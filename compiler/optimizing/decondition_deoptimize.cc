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
  for (const PredicatedDeoptimize& pd : required_deopts_) {
    HBasicBlock* new_pred = pd.existing_deopt_->GetBlock();
    HBasicBlock* new_block = new_pred->SplitBefore(pd.existing_deopt_);
    DCHECK(new_pred->GetLastInstruction()->IsGoto());
    HInstruction* cond = pd.existing_deopt_->InputAt(0);
    HInstruction* goto_inst = new_pred->GetLastInstruction();
    HInstruction* if_inst = new (graph->GetAllocator()) HIf(cond);

    new_pred->ReplaceAndRemoveInstructionWith(goto_inst, if_inst);

    // Make a deopt block.
    HBasicBlock* deopt_block =
        new (graph->GetAllocator()) HBasicBlock(graph, pd.existing_deopt_->GetDexPc());
    graph->AddBlock(deopt_block);
    HDeoptimize* new_deopt =
        new (graph->GetAllocator()) HDeoptimize(graph->GetAllocator(),
                                                graph->GetConstant(DataType::Type::kBool, 1),
                                                pd.existing_deopt_->GetDeoptimizationKind(),
                                                pd.existing_deopt_->GetDexPc());
    deopt_block->AddInstruction(new_deopt);
    new_deopt->CopyEnvironmentFrom(pd.existing_deopt_->GetEnvironment());
    deopt_block->AddSuccessor(graph->GetExitBlock());
    new_pred->AddSuccessor(deopt_block);
    // The true branch is the deopt.
    new_pred->SwapSuccessors();
    new_block->RemoveInstruction(pd.existing_deopt_);
  }
  GetGraph()->ClearLoopInformation();
  GetGraph()->ClearDominanceInformation();
  GetGraph()->BuildDominatorTree();
  required_deopts_.clear();
}

template <typename StorageType>
void BaseDeoptimizationRemover<StorageType>::AddPredicatedDeoptimization(HDeoptimize* deopt,
                                                                         HInstruction* condition) {
  required_deopts_.emplace_back(deopt, condition);
}

// Initialize all the templates.
template void BaseDeoptimizationRemover<ScopedStorageType>::Finalize();
template void BaseDeoptimizationRemover<UnscopedStorageType>::Finalize();
template void BaseDeoptimizationRemover<ScopedStorageType>::AddPredicatedDeoptimization(
    HDeoptimize* deopt, HInstruction* condition);
template void BaseDeoptimizationRemover<UnscopedStorageType>::AddPredicatedDeoptimization(
    HDeoptimize* deopt, HInstruction* condition);

}  // namespace art
