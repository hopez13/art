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

#include "android-base/macros.h"
#include "base/arena_allocator.h"
#include "base/arena_bit_vector.h"
#include "base/globals.h"
#include "nodes.h"

namespace art {

// Removes sink nodes.
void ExecutionSubgraph::Prune() {
  if (!valid_) {
    return;
  }
  needs_prune_ = false;
  // Results needs the existing allocator since it needs to be readable at the
  // same point we are allocating for allowed_successors_. This is the record of
  // the edges that were both (1) explored and (2) reached the exit node. The
  // optional is so we can distinguish nodes we've never visited.
  ScopedArenaVector<std::optional<std::bitset<kMaxFilterableSuccessors>>> results(
      graph_->GetBlocks().size(), allocator_->Adapter(kArenaAllocLSA));
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
  bool start_reaches_end = false;
  // This is basically a DFS of the graph with some edges skipped.
  {
    using BlockId = uint32_t;
    const size_t num_blocks = graph_->GetBlocks().size();
    constexpr ssize_t kUnvisitedSuccIdx = -1;
    ScopedArenaAllocator temporaries(graph_->GetArenaStack());
    ArenaBitVector visiting(&temporaries, num_blocks, false, kArenaAllocLSA);
    // How many of the successors of each block we have already examined. This
    // has three states. (1) kUnvisitedSuccIdx: we have not examined any edges,
    // (2) 0 <= val < # of successors: we have examined 'val' successors/are
    // currently examining successors_[val], (3) kMaxFilterableSuccessors: We
    // have examined all of the successors of the block (the 'result' is final).
    ScopedArenaVector<ssize_t> last_succ_seen(
        num_blocks, kUnvisitedSuccIdx, temporaries.Adapter(kArenaAllocLSA));
    // A stack of which blocks we are visiting in this DFS traversal. Does not
    // include the current-block. Used with last_succ_seen to figure out which
    // bits to set if we find a path to the end/loop.
    ScopedArenaVector<BlockId> current_path(temporaries.Adapter(kArenaAllocLSE));
    // Just ensure we have enough space. The allocator will be cleared shortly
    // anyway so this is fast.
    current_path.reserve(num_blocks);
    // Current block we are examining. Modified only by 'push_block' and 'pop_block'
    const HBasicBlock* cur_block = graph_->GetEntryBlock();
    // Used to note a recur where we will start iterating on 'blk' and save
    // where we are. We must 'continue' immediately after this.
    auto push_block = [&](const HBasicBlock* blk) -> void {
      DCHECK(std::find(current_path.cbegin(), current_path.cend(), cur_block->GetBlockId()) ==
             current_path.end());
      if (kIsDebugBuild) {
        std::for_each(current_path.cbegin(), current_path.cend(), [&](auto id) {
          DCHECK_GT(last_succ_seen[id], kUnvisitedSuccIdx) << id;
          DCHECK_LT(last_succ_seen[id], static_cast<ssize_t>(kMaxFilterableSuccessors)) << id;
        });
      }
      current_path.push_back(cur_block->GetBlockId());
      visiting.SetBit(cur_block->GetBlockId());
      cur_block = blk;
    };
    // Used to note that we have fully explored a block and should return back
    // up. Sets cur_block appropriately. We must 'continue' immediately after
    // calling this.
    auto pop_block = [&]() -> void {
      if (UNLIKELY(current_path.empty())) {
        // Should only happen if entry-blocks successors are exhausted.
        if (kIsDebugBuild) {
          BlockId entry_id = graph_->GetEntryBlock()->GetBlockId();
          DCHECK_GE(last_succ_seen[entry_id],
                    static_cast<ssize_t>(graph_->GetEntryBlock()->GetSuccessors().size()));
        }
        cur_block = nullptr;
      } else {
        const HBasicBlock* last = graph_->GetBlocks()[current_path.back()];
        visiting.ClearBit(current_path.back());
        current_path.pop_back();
        cur_block = last;
      }
    };
    // Mark the current path as a path to the end.
    auto propogate_true = [&]() {
      for (BlockId id : current_path) {
        DCHECK_GT(last_succ_seen[id], kUnvisitedSuccIdx);
        DCHECK_LT(last_succ_seen[id], static_cast<ssize_t>(kMaxFilterableSuccessors));
        results[id]->set(last_succ_seen[id]);
      }
    };
    ssize_t num_entry_succ = graph_->GetEntryBlock()->GetSuccessors().size();
    ssize_t* start_succ_seen = &last_succ_seen[graph_->GetEntryBlock()->GetBlockId()];
    while (num_entry_succ > *start_succ_seen) {
      DCHECK(cur_block != nullptr);
      BlockId id = cur_block->GetBlockId();
      DCHECK((current_path.empty() && cur_block == graph_->GetEntryBlock()) ||
             current_path.front() == graph_->GetEntryBlock()->GetBlockId())
          << "current path size: " << current_path.size()
          << " cur_block id: " << cur_block->GetBlockId() << " entry id "
          << graph_->GetEntryBlock()->GetBlockId();
      DCHECK(!visiting.IsBitSet(id))
          << "Somehow ended up in a loop! This should have been caught before now! " << id;
      std::optional<std::bitset<kMaxFilterableSuccessors>>& maybe_result = results[id];
      if (cur_block == graph_->GetExitBlock()) {
        start_reaches_end = true;
        propogate_true();
        pop_block();
        continue;
      } else if (maybe_result && last_succ_seen[id] == kMaxFilterableSuccessors) {
        // Already fully explored.
        if (maybe_result->any()) {
          propogate_true();
        }
        pop_block();
        continue;
      }
      // We either haven't seen this or are still iterating through it's
      // children. Ensure that we have the results in either case.
      if (!maybe_result) {
        maybe_result.emplace();
      }
      // NB This is a reference. Modifications modify the last_succ_seen.
      ssize_t& cur_succ = last_succ_seen[id];
      std::bitset<kMaxFilterableSuccessors> succ_bitmap = GetAllowedSuccessors(cur_block);
      // Get next successor allowed.
      while (++cur_succ < static_cast<ssize_t>(kMaxFilterableSuccessors) &&
             !succ_bitmap.test(cur_succ)) {
        DCHECK_GE(cur_succ, 0);
      }
      if (cur_succ >= static_cast<ssize_t>(cur_block->GetSuccessors().size())) {
        // No more successors. Mark that we've checked everything. Later visits
        // to this node can use the existing data.
        DCHECK_LE(cur_succ, static_cast<ssize_t>(kMaxFilterableSuccessors));
        cur_succ = kMaxFilterableSuccessors;
        pop_block();
        continue;
      }
      const HBasicBlock* nxt = cur_block->GetSuccessors()[cur_succ];
      DCHECK(nxt != nullptr) << "id: " << cur_succ << " max: " << cur_block->GetSuccessors().size();
      if (visiting.IsBitSet(nxt->GetBlockId())) {
        // This is a loop. Mark it and continue on. Mark allowed-successor on
        // this block's results as well.
        maybe_result->set(cur_succ);
        propogate_true();
      } else {
        // Not a loop yet. Recur.
        push_block(nxt);
      }
    }
  }
  // SubgraphPruneHelper helper(this, results, visiting);
  // bool start_reaches_end = helper.ReachesEnd(graph_->GetEntryBlock());
  // If we can't reach the end then there is no path through the graph without
  // hitting excluded blocks
  if (!start_reaches_end) {
    valid_ = false;
    return;
  }
  // Mark blocks we didn't see in the ReachesEnd flood-fill
  for (const HBasicBlock* blk : graph_->GetBlocks()) {
    if (blk != nullptr && !results[blk->GetBlockId()] && blk != graph_->GetEntryBlock()) {
      // We never visited this block, must be unreachable.
      unreachable_blocks_vec_.SetBit(blk->GetBlockId());
    }
  }
  // Clear the last block.
  results[graph_->GetExitBlock()->GetBlockId()].reset();
  allowed_successors_.clear();
  // Update the allowed-successors to take into account the expanded exclusions.
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
  // Use Floyd-Warshall to exclude blocks that are between 2 different
  // exclusions. This ensures we won't need to do any sort of complicated
  // predicated-allocate.
  // TODO It might be worth it to remove this at some point. If we add
  // conditional allocations in some way this is not really needed.
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
