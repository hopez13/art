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

#include "decondition_deoptimize.h"

#include "android-base/stringprintf.h"
#include "base/arena_bit_vector.h"
#include "base/bit_vector-inl.h"
#include "base/scoped_arena_allocator.h"
#include "base/scoped_arena_containers.h"
#include "base/utils.h"
#include "base/value_object.h"
#include "nodes.h"
#include "side_effects_analysis.h"

using android::base::StringPrintf;
namespace art {

template <typename StorageType>
void BaseDeoptimizationRemover<StorageType>::Finalize() {
  if (required_deopts_.empty()) {
    return;
  }
  HGraph* graph = GetGraph();
  for (HDeoptimizeMarker* pd : required_deopts_) {
    HBasicBlock* new_pred = pd->GetBlock();
    HBasicBlock* new_block = new_pred->SplitBefore(pd);
    DCHECK(new_pred->GetLastInstruction()->IsGoto());
    HInstruction* cond = pd->GetCondition();
    HInstruction* goto_inst = new_pred->GetLastInstruction();
    HInstruction* if_inst = new (graph->GetAllocator()) HIf(cond);

    new_pred->ReplaceAndRemoveInstructionWith(goto_inst, if_inst);

    // Make a deopt block.
    HBasicBlock* deopt_block = new (graph->GetAllocator()) HBasicBlock(graph, pd->GetDexPc());
    graph->AddBlock(deopt_block);
    HDeoptimize* new_deopt =
        new (graph->GetAllocator()) HDeoptimize(pd->GetDeoptimizationKind(), pd->GetDexPc());
    deopt_block->AddInstruction(new_deopt);
    new_deopt->CopyEnvironmentFrom(pd->GetEnvironment());
    deopt_block->AddSuccessor(graph->GetExitBlock());
    new_pred->AddSuccessor(deopt_block);
    // The true branch is the deopt.
    new_pred->SwapSuccessors();
    new_block->RemoveInstruction(pd);
  }
  GetGraph()->ClearLoopInformation();
  GetGraph()->ClearDominanceInformation();
  GetGraph()->BuildDominatorTree();
  required_deopts_.clear();
}

template <typename StorageType>
void BaseDeoptimizationRemover<StorageType>::AddPredicatedDeoptimization(HDeoptimizeMarker* deopt) {
  required_deopts_.push_back(deopt);
}

// Initialize all the templates.
template void BaseDeoptimizationRemover<ScopedStorageType>::Finalize();
template void BaseDeoptimizationRemover<UnscopedStorageType>::Finalize();
template void BaseDeoptimizationRemover<ScopedStorageType>::AddPredicatedDeoptimization(
    HDeoptimizeMarker* deopt);
template void BaseDeoptimizationRemover<UnscopedStorageType>::AddPredicatedDeoptimization(
    HDeoptimizeMarker* deopt);

class GuardRemover : public ValueObject {
 public:
  explicit GuardRemover(HGraph* graph)
      : graph_(graph), deopt_remover_(graph_, ArenaAllocKind::kArenaAllocMisc) {}

  void Run() {
    for (HBasicBlock* bb : graph_->GetReversePostOrderSkipEntryBlock()) {
      VisitBasicBlock(bb);
    }
    deopt_remover_.Finalize();
  }

 private:
  void VisitBasicBlock(HBasicBlock* bb) {
    HInstruction* current_instruction = bb->GetFirstInstruction();
    DCHECK(current_instruction != nullptr)
        << "Block without instructions found! " << bb->GetBlockId();
    HInstruction* next;
    do {
      next = current_instruction->GetNext();
      if (current_instruction->IsDeoptimizeGuard()) {
        HDeoptimizeGuard* guard = current_instruction->AsDeoptimizeGuard();
        HInstruction* guarded = guard->GuardedInput();
        HInstruction* cond = guard->Condition();
        // restore the original value in users.
        guard->ReplaceWith(guarded);
        // Make the deopt we will actually use.
        HDeoptimizeMarker* deopt = new (graph_->GetAllocator())
            HDeoptimizeMarker(cond, guard->GetDeoptimizationKind(), guard->GetDexPc());
        bb->InsertInstructionBefore(deopt, guard);
        deopt->CopyEnvironmentFrom(guard->GetEnvironment());
        bb->RemoveInstruction(guard);
        deopt_remover_.AddPredicatedDeoptimization(deopt);
      }
    } while ((current_instruction = next) != nullptr);
  }

  HGraph* graph_;
  UnscopedDeoptimizationRemover deopt_remover_;
};

bool DeconditionDeoptimize::Run() {
  GuardRemover dr(graph_);
  dr.Run();
  return true;
}

}  // namespace art
