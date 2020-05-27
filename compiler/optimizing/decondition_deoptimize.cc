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

void DeoptimizationRemover::AdjustControlFlow() {
  if (required_deopts_.empty()) {
    return;
  }
  HGraph* graph = GetGraph();
  for (HDeoptimize* pd : required_deopts_) {
    HBasicBlock* new_pred = pd->GetBlock();
    HBasicBlock* new_block = new_pred->SplitBefore(pd);
    DCHECK(new_pred->GetLastInstruction()->IsGoto());
    HInstruction* cond = pd->InputAt(0);
    HInstruction* goto_inst = new_pred->GetLastInstruction();
    HInstruction* if_inst = new (graph->GetAllocator()) HIf(cond);

    new_pred->ReplaceAndRemoveInstructionWith(goto_inst, if_inst);

    // Make a deopt block.
    HBasicBlock* deopt_block =
        new (graph->GetAllocator()) HBasicBlock(graph, pd->GetDexPc());
    graph->AddBlock(deopt_block);
    HDeoptimize* new_deopt =
        new (graph->GetAllocator()) HDeoptimize(graph->GetAllocator(),
                                                graph->GetConstant(DataType::Type::kBool, 1),
                                                pd->GetDeoptimizationKind(),
                                                pd->GetDexPc());
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

void DeoptimizationRemover::AddPredicatedDeoptimization(HDeoptimize* deopt) {
  required_deopts_.push_back(deopt);
}

}  // namespace art
