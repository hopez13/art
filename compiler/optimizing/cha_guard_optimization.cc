/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "cha_guard_optimization.h"

namespace art {

// Note we can only do CHA guard elimination/motion in a single pass, since
// if a guard is not removed, another guard might be removed due to
// the existence of the first guard. The first guard should not be further
// removed in another pass.

class CHAGuardVisitor : HGraphVisitor {
 public:
  CHAGuardVisitor(HGraph* graph)
      : HGraphVisitor(graph),
        block_has_cha_guard_(GetGraph()->GetBlocks().size(),
                             0,
                             graph->GetArena()->Adapter(kArenaAllocCHA)) {
    number_of_guards_to_visit_ = GetGraph()->GetNumberOfCHAGuards();
    DCHECK_NE(number_of_guards_to_visit_, 0u);
    // Will recount number of guards during guard optimization.
    GetGraph()->SetNumberOfCHAGuards(0);
  }

  void VisitShouldDeoptimizeFlag(HShouldDeoptimizeFlag* flag) OVERRIDE;

  void VisitBasicBlock(HBasicBlock* block) OVERRIDE;

 private:
  void RemoveGuard(HShouldDeoptimizeFlag* flag);
  // Return true if `flag` is removed.
  bool OptimizeForParameter(HShouldDeoptimizeFlag* flag, HInstruction* receiver);
  // Return true if `flag` is removed.
  bool OptimizeWithDominatingGuard(HShouldDeoptimizeFlag* flag, HInstruction* receiver);
  // Return true if `flag` is hoisted.
  bool HoistGuard(HShouldDeoptimizeFlag* flag, HInstruction* receiver);

  // Record if each block has any CHA guard. It's updated during the
  // reverse post order visit. Use int instead of bool since ArenaVector
  // does not support bool.
  ArenaVector<int> block_has_cha_guard_;

  HInstructionIterator* instruction_iterator_;

  uint32_t number_of_guards_to_visit_;

  DISALLOW_COPY_AND_ASSIGN(CHAGuardVisitor);
};

void CHAGuardVisitor::VisitBasicBlock(HBasicBlock* block) {
  if (number_of_guards_to_visit_ == 0) {
    return;
  }
  // Skip phis, just iterate through instructions.
  HInstructionIterator it(block->GetInstructions());
  instruction_iterator_ = &it;
  for (; !it.Done(); it.Advance()) {
    it.Current()->Accept(this);
  }
}

void CHAGuardVisitor::RemoveGuard(HShouldDeoptimizeFlag* flag) {
  HBasicBlock* block = flag->GetBlock();
  HInstruction* should_deopt = flag->GetNext();
  DCHECK(should_deopt->IsNotEqual());
  HInstruction* deopt = should_deopt->GetNext();
  DCHECK(deopt->IsDeoptimize());

  // Advance instruction iterator first before we remove the guard.
  instruction_iterator_->Advance();
  instruction_iterator_->Advance();
  block->RemoveInstruction(deopt);
  block->RemoveInstruction(should_deopt);
  block->RemoveInstruction(flag);
}

bool CHAGuardVisitor::OptimizeForParameter(HShouldDeoptimizeFlag* flag,
                                           HInstruction* receiver) {
  // If some compiled code is invalidated by CHA due to class loading, the
  // compiled code will not be entered anymore. So the very fact that the
  // compiled code is invoked guarantees that a parameter receiver conforms
  // to all the CHA devirtualization assumptions made by the compiled code,
  // since all parameter receivers pre-exist any (potential) invalidation of
  // the compiled code.
  if (receiver->IsParameterValue()) {
    RemoveGuard(flag);
    return true;
  }
  return false;
}

bool CHAGuardVisitor::OptimizeWithDominatingGuard(HShouldDeoptimizeFlag* flag,
                                                  HInstruction* receiver) {
  // If there is another guard that dominates the current guard, and
  // that guard is dominated by receiver's definition, then the current
  // guard can be eliminated, since receiver must pre-exist that other
  // guard, and passing that guard guarantees that receiver conforms to
  // all the CHA devirtualization assumptions.
  HBasicBlock* dominator = flag->GetBlock();
  HBasicBlock* receiver_def_block = receiver->GetBlock();
  while (dominator != receiver_def_block) {
    if (block_has_cha_guard_[dominator->GetBlockId()] == 1) {
      RemoveGuard(flag);
      return true;
    }
    dominator = dominator->GetDominator();
  }

  // We do a linear search within the block to see if there is a guard after
  // receiver's definition.
  HInstruction* instruction;
  if (dominator == flag->GetBlock()) {
    // receiver is defined in the current block. Search backward from the current
    // guard.
    instruction = flag->GetPrevious();
  } else {
    // receiver is defined in a dominator. Search backward from the last instruction
    // of that dominator.
    instruction = dominator->GetLastInstruction();
  }
  while (instruction != receiver) {
    if (instruction == nullptr) {
      // receiver must be defined in this block, we didn't find it
      // in the instruction list, so it must be a Phi.
      DCHECK(receiver->IsPhi());
      break;
    }
    if (instruction->IsShouldDeoptimizeFlag()) {
      RemoveGuard(flag);
      return true;
    }
    instruction = instruction->GetPrevious();
  }
  return false;
}

bool CHAGuardVisitor::HoistGuard(HShouldDeoptimizeFlag* flag,
                                 HInstruction* receiver) {
  HBasicBlock* block = flag->GetBlock();
  HLoopInformation* loop_info = block->GetLoopInformation();
  if (loop_info != nullptr) {
    HBasicBlock* pre_header = loop_info->GetPreHeader();
    if (loop_info->IsDefinedOutOfTheLoop(receiver)) {
      HInstruction* should_deopt = flag->GetNext();
      DCHECK(should_deopt->IsNotEqual());
      HInstruction* deopt = should_deopt->GetNext();
      DCHECK(deopt->IsDeoptimize());

      // Advance instruction iterator first before we move the guard.
      instruction_iterator_->Advance();
      instruction_iterator_->Advance();

      flag->MoveBefore(pre_header->GetLastInstruction());
      should_deopt->MoveBefore(pre_header->GetLastInstruction());

      block->RemoveInstruction(deopt);
      HInstruction* suspend = loop_info->GetSuspendCheck();
      HDeoptimize* deoptimize =
        new (GetGraph()->GetArena()) HDeoptimize(should_deopt, suspend->GetDexPc());
      pre_header->InsertInstructionBefore(deoptimize, pre_header->GetLastInstruction());
      if (suspend->HasEnvironment()) {
        deoptimize->CopyEnvironmentFromWithLoopPhiAdjustment(
            suspend->GetEnvironment(), loop_info->GetHeader());
      }
      block_has_cha_guard_[pre_header->GetBlockId()] = 1;
      GetGraph()->IncrementNumberOfCHAGuards();
      return true;
    }
  }
  return false;
}

void CHAGuardVisitor::VisitShouldDeoptimizeFlag(HShouldDeoptimizeFlag* flag) {
  number_of_guards_to_visit_--;
  HInstruction* receiver = flag->InputAt(0);
  // Don't need the receiver anymore.
  flag->ReplaceInput(GetGraph()->GetNullConstant(), 0);
  if (receiver->IsNullCheck()) {
    receiver = receiver->InputAt(0);
  }

  if (OptimizeForParameter(flag, receiver)) {
    return;
  }
  if (OptimizeWithDominatingGuard(flag, receiver)) {
    return;
  }
  if (HoistGuard(flag, receiver)) {
    return;
  }

  // Need to keep the CHA guard in place.
  block_has_cha_guard_[flag->GetBlock()->GetBlockId()] = 1;
  GetGraph()->IncrementNumberOfCHAGuards();
}

void CHAGuardOptimization::Run() {
  if (graph_->GetNumberOfCHAGuards() == 0) {
    return;
  }
  CHAGuardVisitor visitor(graph_);
  for (HBasicBlock* block : graph_->GetReversePostOrder()) {
    visitor.VisitBasicBlock(block);
  }
}

} // namespace art
