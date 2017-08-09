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

namespace art {

class CFREVisitor : public HGraphVisitor {
 public:
  CFREVisitor(HGraph* graph,
              OptimizingCompilerStats* stats)
      : HGraphVisitor(graph),
        candidate_fences_(graph->GetArena()->Adapter(kArenaAllocCFRE)),
        stats_(stats) {}

  void VisitBasicBlock(HBasicBlock* block) OVERRIDE {
    // Visit all instructions in block.
    HGraphVisitor::VisitBasicBlock(block);

    // If there were any unmerged fences left, merge them together,
    // the objects are considered 'published' at the end of the block.
    MergeCandidateFences();
  }

  void VisitConstructorFence(HConstructorFence* constructor_fence) OVERRIDE {
    candidate_fences_.push_back(constructor_fence);
  }

  void VisitInstanceFieldSet(HInstanceFieldSet* instruction) OVERRIDE {
    HInstruction* value = instruction->InputAt(1);
    VisitSetLocation(instruction, value);
  }

  void VisitStaticFieldSet(HStaticFieldSet* instruction) OVERRIDE {
    HInstruction* value = instruction->InputAt(1);
    VisitSetLocation(instruction, value);
  }

  void VisitArraySet(HArraySet* instruction) OVERRIDE {
    HInstruction* value = instruction->InputAt(2);
    VisitSetLocation(instruction, value);
  }

  void VisitDeoptimize(HDeoptimize* instruction ATTRIBUTE_UNUSED) {
    // XX: Do I need to handle DEOPTIMIZE at all?
  }

  void VisitInvokeStaticOrDirect(HInvokeStaticOrDirect* invoke) OVERRIDE {
    HandleInvoke(invoke);
  }

  void VisitInvokeVirtual(HInvokeVirtual* invoke) OVERRIDE {
    HandleInvoke(invoke);
  }

  void VisitInvokeInterface(HInvokeInterface* invoke) OVERRIDE {
    HandleInvoke(invoke);
  }

  void VisitInvokeUnresolved(HInvokeUnresolved* invoke) OVERRIDE {
    HandleInvoke(invoke);
  }

  void VisitInvokePolymorphic(HInvokePolymorphic* invoke) OVERRIDE {
    HandleInvoke(invoke);
  }

  void VisitClinitCheck(HClinitCheck* clinit) OVERRIDE {
    HandleInvoke(clinit);
  }

  void VisitUnresolvedInstanceFieldGet(HUnresolvedInstanceFieldGet* instruction) OVERRIDE {
    // Conservatively treat it as an invocation.
    HandleInvoke(instruction);
  }

  void VisitUnresolvedInstanceFieldSet(HUnresolvedInstanceFieldSet* instruction) OVERRIDE {
    // Conservatively treat it as an invocation.
    HandleInvoke(instruction);
  }

  void VisitUnresolvedStaticFieldGet(HUnresolvedStaticFieldGet* instruction) OVERRIDE {
    // Conservatively treat it as an invocation.
    HandleInvoke(instruction);
  }

  void VisitUnresolvedStaticFieldSet(HUnresolvedStaticFieldSet* instruction) OVERRIDE {
    // Conservatively treat it as an invocation.
    HandleInvoke(instruction);
  }

 private:
  void HandleInvoke(HInstruction* invoke) {
    // An object is considered "published" if it escapes into an invoke as any of the parameters.
    for (size_t input_count = 0; input_count < invoke->InputCount(); ++input_count) {
      if (IsInterestingPublishTarget(invoke->InputAt(input_count))) {
        MergeCandidateFences();
      }
    }
  }

  void VisitSetLocation(HInstruction* inst ATTRIBUTE_UNUSED, HInstruction* store_input) {
    // An object is considered "published" if it's stored onto the heap.
    // Sidenote: A later "LSE" pass can still remove the fence if it proves the
    // object doesn't actually escape.
    if (IsInterestingPublishTarget(store_input)) {
      // Greedily merge all constructor fences that we've seen since
      // the last interesting store (or since the beginning).
      MergeCandidateFences();
    }
  }

  // Merges all the existing fences we've seen so far into the last-most fence.
  //
  // This resets the list of candidate fences back to {}.
  void MergeCandidateFences() {
    if (candidate_fences_.empty()) {
      // Nothing to do, need 2+ fences to merge.
      return;
    }

    // The merge target is always the "last" candidate fence.
    HConstructorFence* merge_target = candidate_fences_[candidate_fences_.size() - 1];

    for (HConstructorFence* fence : candidate_fences_) {
      MaybeMerge(merge_target, fence);
    }

    candidate_fences_.clear();
  }

  // A publishing 'store' is only interesting if the value being stored
  // is one of the fence `targets` in `candidate_fences`.
  bool IsInterestingPublishTarget(HInstruction* store_input) const {
    for (HConstructorFence* fence : candidate_fences_) {
      for (size_t input_count = 0; input_count < fence->InputCount(); ++input_count) {
        if (fence->InputAt(input_count) == store_input) {
          return true;
        }
      }
    }

    return false;
  }

  void MaybeMerge(HConstructorFence* target, HConstructorFence* src) {
    if (target == src) {
      return;  // Don't merge a fence into itself.
      // This is mostly for stats-purposes, we don't want to count merge(x,x)
      // as removing a fence because it's a no-op.
    }

    target->Merge(src);

    OptimizingCompilerStats::MaybeRecordStat(stats_,
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
  CFREVisitor cfre_visitor(graph_, stats_);
  cfre_visitor.VisitReversePostOrder();
}

}  // namespace art
