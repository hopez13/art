/*
 * Copyright (C) 2014 The Android Open Source Project
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

#include "base/arena_bit_vector.h"
#include "base/bit_vector-inl.h"
#include "base/scoped_arena_allocator.h"
#include "base/scoped_arena_containers.h"
#include "base/utils.h"
#include "base/value_object.h"
#include "optimizing/nodes.h"
#include "side_effects_analysis.h"

namespace art {

class DeoptimizeRemover : public ValueObject {
 public:
  explicit DeoptimizeRemover(HGraph* graph) : graph_(graph), needs_recalculate_(false) {}

  bool Run() {
    // Use the reverse post order to ensure the non back-edge predecessors of a block are
    // visited before the block itself.
    HBasicBlock* predecessor = nullptr;
    // LOG(INFO) << "Method is " << graph_->GetMethodName();
    // LOG(INFO) << "Entry block is " << graph_->GetEntryBlock();
    // LOG(INFO) << "Exit block is " << graph_->GetExitBlock();
    for (HBasicBlock* block : graph_->GetReversePostOrder()) {
      // LOG(INFO) << "Visiting block " << block << " (" << block->GetBlockId() << ")";
      VisitBasicBlock(block, predecessor);
      predecessor = block;
    }
    if (needs_recalculate_) {
      // TODO Can we be smarter about this?
      graph_->ClearLoopInformation();
      graph_->ClearDominanceInformation();
      graph_->BuildDominatorTree();
    }
    return true;
  }

  // TODO Doing this as a pass is stupid and lazy. We should just do it as we go.
  void VisitBasicBlock(HBasicBlock* block, HBasicBlock* predecessor) {
    HInstruction* current = block->GetFirstInstruction();
    const HTryBoundary* try_block = block->IsTryBlock() ? &block->GetTryCatchInformation()->GetTryEntry() : static_cast<const HTryBoundary*>(nullptr);
    while (current != nullptr) {
      // TODO Support movable deoptimizes once I figure out what they do!
      if (current->IsDeoptimize() && !current->AsDeoptimize()->IsUnconditional() && !current->AsDeoptimize()->CanBeMoved()) {
        // We only want to have unconditional deoptimizes.
        needs_recalculate_ = true;
        HInstruction* next = current->GetNext();
        SplitBlock(&block, &predecessor, try_block, current->AsDeoptimize());
        current = next;
        DCHECK_EQ(next, block->GetFirstInstruction());
      } else {
        current = current->GetNext();
      }
    }
  }

  HBasicBlock* GetDeoptBlockFor(HDeoptimize* deopt, const HTryBoundary* try_entry) {
    HBasicBlock* real_deopt_block = FindOrCreateDeoptBlock(deopt);
    if (try_entry != nullptr) {
      // We need to remove the try first. This should never actually get
      // triggered but is needed for graph correctness.
      HBasicBlock* remove_try = new (graph_->GetAllocator()) HBasicBlock(graph_);
      graph_->AddBlock(remove_try);
      HTryBoundary* exit_try = new (graph_->GetAllocator()) HTryBoundary(HTryBoundary::BoundaryKind::kExit);
      remove_try->AddInstruction(exit_try);
      remove_try->AddSuccessor(real_deopt_block);
      for (HBasicBlock* handler : try_entry->GetExceptionHandlers()) {
        exit_try->AddExceptionHandler(handler);
      }
      return remove_try;
    } else {
      return real_deopt_block;
    }
  }

  HBasicBlock* FindOrCreateDeoptBlock(HDeoptimize* deopt) {
    // auto iter = std::find_if(deopts_.begin(), deopts_.end(), [&](std::pair<HDeoptimize*, HBasicBlock*> p) {
    //   if (p.first->GetDexPc() != deopt->GetDexPc() || p.first->GetEnvironment()->Size() != deopt->GetEnvironment()->Size()) {
    //     return false;
    //   }
    //   if (p.first->HasEnvironment() != deopt->HasEnvironment()) {
    //     return false;
    //   } else if (!deopt->HasEnvironment()) {
    //     return true;
    //   }
    //   for (size_t i = 0; i < deopt->GetEnvironment()->Size(); i++) {
    //     auto my_inst = deopt->GetEnvironment()->GetInstructionAt(i);
    //     auto other_inst = p.first->GetEnvironment()->GetInstructionAt(i);
    //     if (my_inst == nullptr || other_inst == nullptr) {
    //       if (my_inst != other_inst) {
    //         return false;
    //       } else {
    //         continue;
    //       }
    //     }
    //     if (!my_inst->Equals(other_inst)) {
    //       return false;
    //     }
    //   }
    //   return true;
    // });
    // if (iter != deopts_.end()) {
    //   return iter->second;
    // }
    HBasicBlock* deopt_block =
        new (graph_->GetAllocator()) HBasicBlock(graph_, deopt->GetDexPc());
    graph_->AddBlock(deopt_block);
    HDeoptimize* new_deopt = new (graph_->GetAllocator()) HDeoptimize(graph_->GetAllocator(), graph_->GetConstant(DataType::Type::kBool, 1), deopt->GetDeoptimizationKind(), deopt->GetDexPc());
    deopt_block->AddInstruction(new_deopt);
    new_deopt->CopyEnvironmentFrom(deopt->GetEnvironment());
    deopt_block->AddSuccessor(graph_->GetExitBlock());
    // deopts_.emplace_back(new_deopt, deopt_block);
    return deopt_block;
  }

  void SplitBlock(HBasicBlock** current_block,
                  HBasicBlock** current_predecessor,
                  // HInstruction* new_block_inst_start,
                  const HTryBoundary* try_boundary,
                  HDeoptimize* split_deoptimize) {
    // Allocate the new predecessor block and update caller with the new current
    // block and predecessor.
    HBasicBlock* new_pred = *current_block;
    HBasicBlock* new_block = new_pred->SplitBefore(split_deoptimize);
    *current_block = new_block;
    *current_predecessor = new_pred;

    // Now we have:
    // 'block' = 'new_pred': [ ins1, ins2, ..., ins_deopt-1, goto_new_block]
    // 'new_block': [ins_deoptimize, ..., ins_last]

    // Replace the GOTO with an 'IF'.
    DCHECK(new_pred->GetLastInstruction()->IsGoto());
    HInstruction* cond = split_deoptimize->InputAt(0);
    HInstruction* goto_inst = new_pred->GetLastInstruction();
    HInstruction* if_inst = new (graph_->GetAllocator()) HIf(cond);
    // new_pred->InsertInstructionBefore(if_inst, goto_inst);
    new_pred->ReplaceAndRemoveInstructionWith(goto_inst, if_inst);
    // new_pred->GetLastInstruction()->ReplaceWith(if_inst);

    // LOG(WARNING) << "last inst!" << new_pred->GetLastInstruction()->DebugName();

    // Make a deopt block.
    HBasicBlock* deopt_block = GetDeoptBlockFor(split_deoptimize, try_boundary);
    new_pred->AddSuccessor(deopt_block);
    // The true branch is the deopt.
    new_pred->SwapSuccessors();
    // TODO Currently we can just remove the deoptimize instruction since its
    // not got any value (which we check by making sure it's immobile)
    new_block->RemoveInstruction(split_deoptimize);
    // LOG(INFO) << "Did modifications! new_pred = " << new_pred << "(" << new_pred->GetBlockId() << ") new_block = " << new_block << "(" << new_block->GetBlockId() << ") deopt_block = " << deopt_block << " (" << deopt_block->GetBlockId() << ")";

#if 0
    // Stupid
    // HBasicBlock* new_pred = graph_->SplitEdge(current_predecessor, block);
    // HBasicBlock* new_deopt_block = new (graph_->GetAllocator()) HBasicBlock(graph_, block->GetDexPc());

    // // Both the deopt and current-block are successors.
    // new_pred->AddSuccessor(new_deopt_block);

    // // The deopt is the 'true' branch, swap to preserve this.
    // new_pred->SwapSuccessors();

    // // Setup the instructions in the deopt block!
    // HInstruction* uncond_deopt = split_deoptimize->Clone(graph_->GetAllocator());
    // uncond_deopt->SetRawInputAt(0, graph_->GetConstant(DataType::Type::kBool, 1));
    // new_deopt_block->AddInstruction(uncond_deopt);
    // new_deopt_block->AddSuccessor(graph_->GetExitBlock());

    // // Copy the instructions to the new_pred.
    // HInstruction* new_block_inst = new_block_inst_start;
    // while (new_block_inst != split_deoptimize) {
    //   DCHECK(new_block_inst != nullptr) << "Didn't reach deoptimize to split on!";
    //   block->RemoveInstructionOrPhi(new_block_inst, /*ensure_safety=*/ false);
    //   new_pred->AddInstruction(new_block_inst);
    // }
    // return new_pred;
  #endif
  }

 private:
  HGraph* graph_;
  // std::vector<std::pair<HDeoptimize*, HBasicBlock*>> deopts_;
  bool needs_recalculate_;
};

bool DeconditionDeoptimize::Run() {
  DeoptimizeRemover dr(graph_);
  return dr.Run();
}
}  // namespace art
