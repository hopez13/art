/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "execution_subgraph.h"

#include <algorithm>
#include <unordered_set>

#include "nodes.h"

namespace art {

// Removes sink nodes.
void ExecutionSubgraph::Prune() {
  if (!valid_) {
    return;
  }
  needs_prune_ = false;
  // Grab blocks further up the tree.
  ScopedArenaVector<std::optional<std::bitset<kMaxFilterableSuccessors>>> results(
      graph_->GetBlocks().size(), allocator_->Adapter(kArenaAllocLSA));
  ArenaBitVector visiting(allocator_, graph_->GetBlocks().size(), false, kArenaAllocLSE);
  unreachable_blocks_vec_.ClearAllBits();
  // TODO We should support infinite loops as well.
  if (graph_->GetExitBlock() == nullptr) {
    // Infinite loop
    valid_ = false;
    return;
  }
  results[graph_->GetExitBlock()->GetBlockId()] = ~(std::bitset<kMaxFilterableSuccessors>());
  // Fills up the 'results' map with what we need to add to update
  // allowed_successors to in order to prune sink nodes.
  // NB C++ Doesn't like recursive calls of lambdas like this so just pass
  // down the 'reaches_end' function explicitly.
  auto reaches_end = [&](const HBasicBlock* blk, auto& reaches_end_recur) {
    std::optional<std::bitset<kMaxFilterableSuccessors>>& maybe_result = results[blk->GetBlockId()];
    if (visiting.IsBitSet(blk->GetBlockId())) {
      // We are in a loop so the block is live.
      return true;
    } else if (maybe_result) {
      CHECK(maybe_result->any() || unreachable_blocks_vec_.IsBitSet(blk->GetBlockId()));
      return maybe_result->any();
    }
    visiting.SetBit(blk->GetBlockId());
    // what we currently allow.
    std::bitset<kMaxFilterableSuccessors> succ_bitmap = GetAllowedSuccessors(blk);
    // The new allowed successors. We use visiting to break loops so we don't
    // need to figure out how many bits to turn on.
    maybe_result = std::bitset<kMaxFilterableSuccessors>();
    std::bitset<kMaxFilterableSuccessors>& result = maybe_result.value();
    for (auto [succ, i] :
         ZipCount(MakeIterationRange(blk->GetSuccessors().begin(), blk->GetSuccessors().end()))) {
      if (succ_bitmap.test(i) && reaches_end_recur(succ, reaches_end_recur)) {
        result.set(i);
      }
    }
    visiting.ClearBit(blk->GetBlockId());
    bool res = result.any();
    if (!res) {
      // If this is a sink block it will be removed from the successors of all
      // its predecessors and made unreachable.
      CHECK(blk != nullptr);
      unreachable_blocks_vec_.SetBit(blk->GetBlockId());
    }
    return res;
  };
  bool start_reaches_end = reaches_end(graph_->GetEntryBlock(), reaches_end);
  if (!start_reaches_end) {
    valid_ = false;
    return;
  }
  for (const HBasicBlock* blk : graph_->GetBlocks()) {
    if (blk != nullptr && !results[blk->GetBlockId()] && blk != graph_->GetEntryBlock()) {
      // We never visited this block, must be unreachable.
      unreachable_blocks_vec_.SetBit(blk->GetBlockId());
    }
  }
  results[graph_->GetExitBlock()->GetBlockId()].reset();
  allowed_successors_.clear();
  for (auto [v, id] : ZipCount(MakeIterationRange(results))) {
    if (!v) {
      continue;
    }
    HBasicBlock* block = graph_->GetBlocks()[id];
    if (v->count() != block->GetSuccessors().size()) {
      allowed_successors_.Overwrite(block, *v);
    }
  }
  RecalculateExcludedCohort();
}

void ExecutionSubgraph::RemoveConcavity() {
  if (!valid_) {
    return;
  }
  DCHECK(!needs_prune_);
  ArenaBitVector initial(allocator_, graph_->GetBlocks().size(), false, kArenaAllocLSA);
  initial.Copy(&unreachable_blocks_vec_);
  for (const HBasicBlock* blk : graph_->GetBlocks()) {
    if (blk == nullptr || initial.IsBitSet(blk->GetBlockId())) {
      continue;
    }
    for (uint32_t skipped1 : initial.Indexes()) {
      if (LIKELY(!graph_->PathBetween(skipped1, blk->GetBlockId()))) {
        continue;
      }
      for (uint32_t skipped2 : initial.Indexes()) {
        if (graph_->PathBetween(blk->GetBlockId(), skipped2)) {
          RemoveBlock(blk);
        }
      }
    }
  }
  Prune();
}

void ExecutionSubgraph::RecalculateExcludedCohort() {
  DCHECK(!needs_prune_);
  excluded_list_.emplace(allocator_->Adapter(kArenaAllocLSA));
  ScopedArenaVector<ExcludedCohort>& res = excluded_list_.value();
  // Make a copy of unreachable_blocks_;
  ArenaBitVector unreachable(allocator_, graph_->GetBlocks().size(), false, kArenaAllocLSA);
  unreachable.Copy(&unreachable_blocks_vec_);
  // Split cohorts with union-find
  while (unreachable.IsAnyBitSet()) {
    res.emplace_back(allocator_, graph_);
    ExcludedCohort& cohort = res.back();
    // We don't allocate except for the queue beyond here so create another arena to save memory.
    ScopedArenaAllocator alloc(graph_->GetArenaStack());
    ScopedArenaQueue<const HBasicBlock*> worklist(alloc.Adapter(kArenaAllocLSA));
    // Select an arbitrary node
    const HBasicBlock* first = graph_->GetBlocks()[unreachable.GetHighestBitSet()];
    worklist.push(first);
    do {
      // Flood-fill both forwards and backwards.
      const HBasicBlock* cur = worklist.front();
      worklist.pop();
      if (!unreachable.IsBitSet(cur->GetBlockId())) {
        // Already visited or reachable somewhere else.
        continue;
      }
      unreachable.ClearBit(cur->GetBlockId());
      if (cur == nullptr) {
        continue;
      }
      cohort.blocks_.SetBit(cur->GetBlockId());
      // don't bother filtering here, it's done next go-around
      for (const HBasicBlock* pred : cur->GetPredecessors()) {
        worklist.push(pred);
      }
      for (const HBasicBlock* succ : cur->GetSuccessors()) {
        worklist.push(succ);
      }
    } while (!worklist.empty());
  }
  // Figure out entry & exit nodes.
  for (ExcludedCohort& cohort : res) {
    DCHECK(cohort.blocks_.IsAnyBitSet());
    auto is_external = [&](const HBasicBlock* ext) -> bool {
      return !cohort.blocks_.IsBitSet(ext->GetBlockId());
    };
    for (const HBasicBlock* blk : cohort.Blocks()) {
      const auto& preds = blk->GetPredecessors();
      const auto& succs = blk->GetSuccessors();
      if (std::any_of(preds.cbegin(), preds.cend(), is_external)) {
        cohort.entry_blocks_.SetBit(blk->GetBlockId());
      }
      if (std::any_of(succs.cbegin(), succs.cend(), is_external)) {
        cohort.exit_blocks_.SetBit(blk->GetBlockId());
      }
    }
  }
}

std::ostream& operator<<(std::ostream& os, const ExecutionSubgraph::ExcludedCohort& ex) {
  ex.Dump(os);
  return os;
}

void ExecutionSubgraph::ExcludedCohort::Dump(std::ostream& os) const {
  auto dump = [&](BitVecBlockRange arr) {
    os << "[";
    bool first = true;
    for (const HBasicBlock* b : arr) {
      if (!first) {
        os << ", ";
      }
      first = false;
      os << b->GetBlockId();
    }
    os << "]";
  };
  auto dump_blocks = [&]() {
    os << "[";
    bool first = true;
    for (const HBasicBlock* b : Blocks()) {
      if (!entry_blocks_.IsBitSet(b->GetBlockId()) && !exit_blocks_.IsBitSet(b->GetBlockId())) {
        if (!first) {
          os << ", ";
        }
        first = false;
        os << b->GetBlockId();
      }
    }
    os << "]";
  };

  os << "{ entry: ";
  dump(EntryBlocks());
  os << ", interior: ";
  dump_blocks();
  os << ", exit: ";
  dump(ExitBlocks());
  os << "}";
}

// For testing
bool ExecutionSubgraph::CalculateValidity(HGraph* graph, const ExecutionSubgraph* esg) {
  bool reached_end = false;
  std::queue<const HBasicBlock*> worklist;
  std::unordered_set<const HBasicBlock*> visited;
  worklist.push(graph->GetEntryBlock());
  while (!worklist.empty()) {
    const HBasicBlock* cur = worklist.front();
    worklist.pop();
    if (visited.find(cur) != visited.end()) {
      continue;
    } else {
      visited.insert(cur);
    }
    if (cur == graph->GetExitBlock()) {
      reached_end = true;
      continue;
    }
    bool has_succ = false;
    for (const HBasicBlock* succ : cur->GetSuccessors()) {
      if (succ == nullptr || !esg->ContainsBlock(succ)) {
        continue;
      }
      has_succ = true;
      worklist.push(succ);
    }
    if (!has_succ) {
      // We aren't at the end and have nowhere to go so fail.
      return false;
    }
  }
  return reached_end;
}

}  // namespace art
