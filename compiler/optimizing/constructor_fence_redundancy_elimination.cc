/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "constructor_fence_redundancy_elimination.h"
#include "escape.h"

// TODO: remove these includes before merging.
#include <stdlib.h>
#include <string>

namespace art {

class CFREVisitor : public EscapeVisitor {
 public:
  CFREVisitor(HGraph* graph,
              OptimizingCompilerStats* stats)
      : EscapeVisitor(graph),
        candidate_fences_(graph->GetArena()->Adapter(kArenaAllocCFRE)),
        stats_(stats) {}

  void VisitReversePostOrder() {
    for (HBasicBlock* block : graph_->GetReversePostOrder()) {
      // Visit all instructions in block.
      VisitBasicBlock(block);

      // If there were any unmerged fences left, merge them together,
      // all objects are considered 'published' at the end of the block.
      MergeCandidateFences();
    }
  }

  virtual void VisitInstruction(HInstruction* instruction) OVERRIDE {
    if (instruction->IsConstructorFence()) {
      HConstructorFence* constructor_fence = instruction->AsConstructorFence();

      // Mark this fence to be part of the merge list when MergeCandidateFences is called later.
      candidate_fences_.push_back(constructor_fence);
      // Mark the constructor fence targets as being tracked by the escape analysis.
      // VisitEscapee(?, AliasOf(target)) will be called if it escapes.
      for (size_t input_idx = 0; input_idx < constructor_fence->InputCount(); ++input_idx) {
        AddEscapeeTracking(constructor_fence->InputAt(input_idx));
      }
    } else if (instruction->IsDeoptimize()) {
      // Pessimize: Merge any constructor fence prior to Deopt.
      MergeCandidateFences();
    } else if (instruction->IsClinitCheck()) {
      // Pessimize: Merge any constructor fence prior to ClinitCheck.
      // XX: Should the escape analysis treat the ClinitCheck as an escape-to-heap instead?
      MergeCandidateFences();
    }
  }

  // One of the (potentially aliased) candidate fence targets (i.e. `escapee`)
  // has escaped into the heap.
  virtual bool VisitEscaped(HInstruction* instruction ATTRIBUTE_UNUSED,
                            HInstruction* escapee ATTRIBUTE_UNUSED) OVERRIDE {
    // An object is considered "published" if it escapes.
    //
    // Greedily merge all constructor fences that we've seen since
    // the tracked escape (or since the beginning).
    MergeCandidateFences();

    // Always clear all the escaping references being tracked.
    return true;
  }

 private:
  // Merges all the existing fences we've seen so far into the last-most fence.
  //
  // This resets the list of candidate fences back to {}.
  void MergeCandidateFences() {
    if (candidate_fences_.empty()) {
      // Nothing to do, need 1+ fences to merge.
      return;
    }

    // The merge target is always the "last" candidate fence.
    HConstructorFence* merge_target = candidate_fences_[candidate_fences_.size() - 1];

    for (HConstructorFence* fence : candidate_fences_) {
      MaybeMerge(merge_target, fence);
    }

    candidate_fences_.clear();
  }

  void MaybeMerge(HConstructorFence* target, HConstructorFence* src) {
    if (target == src) {
      return;  // Don't merge a fence into itself.
      // This is mostly for stats-purposes, we don't want to count merge(x,x)
      // as removing a fence because it's a no-op.
    }

    target->Merge(src);

    MaybeRecordStat(stats_,
                    MethodCompilationStat::kConstructorFenceRemovedCFRE);
  }

  // Set of constructor fences that we've seen in the current block.
  // Each constructor fences acts as a guard for one or more `targets`.
  // There exist no stores to any `targets` between any of these fences.
  //
  // Fences are in succession order (e.g. fence[i] succeeds fence[i-1]
  // within the same basic block).
  ArenaVector<HConstructorFence*> candidate_fences_;

  // Used to record stats about the optimization.
  OptimizingCompilerStats* const stats_;

  DISALLOW_COPY_AND_ASSIGN(CFREVisitor);
};

void ConstructorFenceRedundancyElimination::Run() {

  // TODO: remove this block of code before merging
  {
    char* should_disable_env = getenv("ART_OPT_CFRE");
    if (should_disable_env != nullptr) {
      std::string should_disable(should_disable_env);

      if (should_disable == "0" || should_disable == "false") {
        LOG(INFO) << "ART_OPT_CFRE set to false, skip CFRE";
        return;
      }
    }
  }

  CFREVisitor cfre_visitor(graph_, stats_);

  // Arbitrarily visit in reverse-post order.
  // The exact block visitation order does not matter, as the algorithm
  // only operates on a single block at a time.
  cfre_visitor.VisitReversePostOrder();

}

}  // namespace art
