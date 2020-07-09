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

#ifndef ART_COMPILER_OPTIMIZING_EXECUTION_SUBGRAPH_H_
#define ART_COMPILER_OPTIMIZING_EXECUTION_SUBGRAPH_H_

#include <algorithm>
#include <sstream>

#include "base/arena_allocator.h"
#include "base/arena_bit_vector.h"
#include "base/arena_containers.h"
#include "base/array_ref.h"
#include "base/bit_vector-inl.h"
#include "base/globals.h"
#include "base/iteration_range.h"
#include "base/scoped_arena_allocator.h"
#include "base/scoped_arena_containers.h"
#include "base/stl_util.h"
#include "base/transform_iterator.h"
#include "nodes.h"

namespace art {

// Helper for transforming block ids to blocks.
class BlockIdToBlockTransformer {
 public:
  BlockIdToBlockTransformer(BlockIdToBlockTransformer&&) = default;
  BlockIdToBlockTransformer(const BlockIdToBlockTransformer&) = default;
  explicit BlockIdToBlockTransformer(const HGraph* graph) : graph_(graph) {}

  const HGraph* GetGraph() const {
    return graph_;
  }

  HBasicBlock* GetBlock(uint32_t id) const {
    DCHECK_LT(id, graph_->GetBlocks().size()) << graph_->PrettyMethod();
    HBasicBlock* blk = graph_->GetBlocks()[id];
    DCHECK(blk != nullptr);
    return blk;
  }

  HBasicBlock* operator()(uint32_t id) const {
    return GetBlock(id);
  }

 private:
  const HGraph* const graph_;
};

// A representation of a particular section of the graph. The graph is split
// into an excluded and included area and is used to track escapes.
//
// This object is a view of the graph and is not updated as the graph is
// changed.
//
// This is implemented by removing various escape points from the subgraph using
// the 'RemoveBlock' function. Once all required blocks are removed one will
// 'Finalize' the subgraph. This will extend the removed area (1) any block
// which inevitably leads to a removed block is also removed and (2) any block
// which is between 2 removed blocks is removed.
//
// This allows us to create a set of 'ExcludedCohorts' which are the
// well-connected subsets of the graph made up of removed blocks. These cohorts
// have a set of entry and exit blocks which act as the boundary of the cohort.
// Since we removed blocks between 2 excluded blocks it is impossible for any
// cohort-exit block to reach any cohort-entry block. This means we can use the
// boundary between the cohort and the rest of the graph to insert
// materialization blocks for partial LSE.
class ExecutionSubgraph : public ArenaObject<kArenaAllocLSA> {
 public:
  using BitVecBlockRange =
      IterationRange<TransformIterator<BitVector::IndexIterator, BlockIdToBlockTransformer>>;

  // A set of connected blocks which are connected and removed from the
  // ExecutionSubgraph. See above comment for explanation.
  class ExcludedCohort : public ArenaObject<kArenaAllocLSA> {
   public:
    ExcludedCohort(ExcludedCohort&&) = default;
    ExcludedCohort(const ExcludedCohort&) = delete;
    explicit ExcludedCohort(ScopedArenaAllocator* allocator, HGraph* graph)
        : graph_(graph),
          entry_blocks_(allocator, graph_->GetBlocks().size(), false, kArenaAllocLSE),
          exit_blocks_(allocator, graph_->GetBlocks().size(), false, kArenaAllocLSE),
          blocks_(allocator, graph_->GetBlocks().size(), false, kArenaAllocLSE) {}

    ~ExcludedCohort() = default;

   private:
    BitVecBlockRange BlockIterRange(const ArenaBitVector& bv) const {
      auto indexes = bv.Indexes();
      BitVecBlockRange res = MakeTransformRange(indexes, BlockIdToBlockTransformer(graph_));
      return res;
    }

   public:
    // All blocks in the cohort.
    BitVecBlockRange Blocks() const {
      return BlockIterRange(blocks_);
    }

    // Blocks that have predecessors outside of the cohort. These blocks will
    // need to have PHIs/control-flow added to create the escaping value.
    BitVecBlockRange EntryBlocks() const {
      return BlockIterRange(entry_blocks_);
    }

    // Blocks that have successors outside of the cohort. The successors of
    // these blocks will need to have PHI's to restore state.
    BitVecBlockRange ExitBlocks() const {
      return BlockIterRange(exit_blocks_);
    }

    bool operator==(const ExcludedCohort& other) const {
      return blocks_.Equal(&other.blocks_);
    }

    bool ContainsBlock(const HBasicBlock* blk) const {
      return blocks_.IsBitSet(blk->GetBlockId());
    }

    // Returns true if there is a path from 'blk' to any block in this cohort.
    // NB blocks contained within the cohort are excluded and will never be
    // considered to succeed the cohort.
    bool SucceedsBlock(const HBasicBlock* blk) const {
      if (ContainsBlock(blk)) {
        return false;
      }
      auto idxs = entry_blocks_.Indexes();
      return std::any_of(idxs.begin(), idxs.end(), [&](uint32_t entry) -> bool {
        return blk->GetGraph()->PathBetween(blk->GetBlockId(), entry);
      });
    }
    // Returns true if there is a path from any block in this cohort to 'blk'.
    // NB blocks contained within the cohort are excluded and will never be
    // considered to preced the cohort.
    bool PrecedesBlock(const HBasicBlock* blk) const {
      if (ContainsBlock(blk)) {
        return false;
      }
      auto idxs = exit_blocks_.Indexes();
      return std::any_of(idxs.begin(), idxs.end(), [&](uint32_t exit) -> bool {
        return blk->GetGraph()->PathBetween(exit, blk->GetBlockId());
      });
    }

    void Dump(std::ostream& os) const;

   private:
    ExcludedCohort() = delete;

    HGraph* graph_;
    ArenaBitVector entry_blocks_;
    ArenaBitVector exit_blocks_;
    ArenaBitVector blocks_;

    friend class ExecutionSubgraph;
    friend class LoadStoreAnalysisTest;
  };

  // The number of successors we can track on a single block. Graphs which
  // contain a block with a branching factor greater than this will not be
  // analysed. This is used to both limit the memory usage of analysis to
  // reasonable levels and ensure that the analysis will complete in a
  // reasonable amount of time. It also simplifies the implementation somewhat
  // to have a constant branching factor.
  static constexpr uint32_t kMaxFilterableSuccessors = 8;

  // Instantiate a subgraph. Valid is whether or not to even attempt
  // partial-escape analysis. It should be false if partial-escape analysis is
  // not desired (eg when being used for instruction scheduling) or when the
  // branching factor in the graph is too high. This is calculated once and
  // passed down for performance reasons.
  ExecutionSubgraph(HGraph* graph, bool valid, ScopedArenaAllocator* allocator)
      : graph_(graph),
        allocator_(allocator),
        allowed_successors_(graph_->GetBlocks().size(),
                            ~(std::bitset<kMaxFilterableSuccessors> {}),
                            allocator_->Adapter(kArenaAllocLSA)),
        unreachable_blocks_vec_(
            allocator_, valid ? graph_->GetBlocks().size() : 0, false, kArenaAllocLSA),
        valid_(valid),
        needs_prune_(false),
        finalized_(false) {
    if (valid_) {
      DCHECK(std::all_of(graph->GetBlocks().begin(), graph->GetBlocks().end(), [](HBasicBlock* it) {
        return it == nullptr || it->GetSuccessors().size() <= kMaxFilterableSuccessors;
      }));
    }
  }

  void Invalidate() {
    valid_ = false;
  }

  bool ContainsBlock(const HBasicBlock* blk) const {
    DCHECK(!finalized_ || !needs_prune_) << "finalized: " << finalized_;
    if (!valid_) {
      return false;
    }
    return !unreachable_blocks_vec_.IsBitSet(blk->GetBlockId());
  }

  void RemoveBlock(const HBasicBlock* to_remove) {
    if (!valid_) {
      return;
    }
    uint32_t id = to_remove->GetBlockId();
    if (unreachable_blocks_vec_.IsBitSet(id)) {
      if (kIsDebugBuild) {
        needs_prune_ = true;
      }
      return;
    }
    unreachable_blocks_vec_.SetBit(id);
    for (HBasicBlock* pred : to_remove->GetPredecessors()) {
      std::bitset<kMaxFilterableSuccessors> set {};
      for (auto [succ, i] : ZipCount(MakeIterationRange(pred->GetSuccessors()))) {
        if (succ != to_remove) {
          set.set(i);
        }
      }
      LimitBlockSuccessors(pred, set);
    }
  }

  void Finalize() {
    Prune();
    RemoveConcavity();
    finalized_ = true;
  }

  BitVecBlockRange UnreachableBlocks() const {
    auto idxs = unreachable_blocks_vec_.Indexes();
    return MakeTransformRange(idxs, BlockIdToBlockTransformer(graph_));
  }

  // Returns true if all allowed execution paths from start eventually reach the
  // graph's exit block (or diverge).
  bool IsValid() const {
    return valid_;
  }

  ArrayRef<const ExcludedCohort> GetExcludedCohorts() const {
    DCHECK(!valid_ || !needs_prune_);
    if (!valid_ || !unreachable_blocks_vec_.IsAnyBitSet()) {
      return ArrayRef<const ExcludedCohort>();
    } else {
      return ArrayRef<const ExcludedCohort>(*excluded_list_);
    }
  }

  // Helper class to create reachable blocks iterator.
  class ContainsFunctor {
   public:
    bool operator()(HBasicBlock* blk) const {
      return subgraph_->ContainsBlock(blk);
    }

   private:
    explicit ContainsFunctor(const ExecutionSubgraph* subgraph) : subgraph_(subgraph) {}
    const ExecutionSubgraph* const subgraph_;
    friend class ExecutionSubgraph;
  };
  // Returns an iterator over reachable blocks (filtered as we go). This is primarilly for testing.
  IterationRange<
      FilterIterator<typename ArenaVector<HBasicBlock*>::const_iterator, ContainsFunctor>>
  ReachableBlocks() const {
    return Filter(MakeIterationRange(graph_->GetBlocks()), ContainsFunctor(this));
  }

  static bool CanAnalyse(HGraph* graph) {
    // If there are any blocks with more than kMaxFilterableSuccessors we can't
    // analyse the graph. See above for explanation.
    return std::all_of(graph->GetBlocks().begin(), graph->GetBlocks().end(), [](HBasicBlock* blk) {
      return blk == nullptr || blk->GetSuccessors().size() <= kMaxFilterableSuccessors;
    });
  }

 private:
  std::bitset<kMaxFilterableSuccessors> GetAllowedSuccessors(const HBasicBlock* blk) const {
    return allowed_successors_[blk->GetBlockId()];
  }

  void LimitBlockSuccessors(const HBasicBlock* block,
                            std::bitset<kMaxFilterableSuccessors> allowed) {
    needs_prune_ = true;
    allowed_successors_[block->GetBlockId()] &= allowed;
  }

  // Remove nodes which both precede and follow any exclusions. This ensures we don't need to deal
  // with only conditionally materializing objects depending on if we already materialized them
  void RemoveConcavity();

  // Removes sink nodes.
  void Prune();

  void RecalculateExcludedCohort();

  // Helper for testing use.
  static bool CalculateValidity(HGraph* graph, const ExecutionSubgraph* esg);

  HGraph* graph_;
  ScopedArenaAllocator* allocator_;
  // ScopedArenaHashMap<const HBasicBlock*, std::bitset<kMaxFilterableSuccessors>>
  // allowed_successors_;
  ScopedArenaVector<std::bitset<kMaxFilterableSuccessors>> allowed_successors_;
  ArenaBitVector unreachable_blocks_vec_;
  std::optional<ScopedArenaVector<ExcludedCohort>> excluded_list_;
  bool valid_;
  bool needs_prune_;
  bool finalized_;

  friend class ExecutionSubgraphTest;
  friend class LoadStoreAnalysisTest;

  DISALLOW_COPY_AND_ASSIGN(ExecutionSubgraph);
};

std::ostream& operator<<(std::ostream& os, const ExecutionSubgraph::ExcludedCohort& ex);

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_EXECUTION_SUBGRAPH_H_
