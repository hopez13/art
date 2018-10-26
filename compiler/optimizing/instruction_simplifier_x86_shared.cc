/* Copyright (C) 2018 The Android Open Source Project
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

#include "instruction_simplifier_x86_shared.h"
#include "nodes_x86.h"

namespace art {

bool TryCombineAndNot(HAnd* instruction) {
  DataType::Type type = instruction->GetType();
  if (!DataType::IsIntOrLongType(type)) {
    return false;
  }
  // Replace code looking like
  //    Not tmp, y
  //    And dst, x, tmp
  //  with
  //    AndNot dst, x, y
  ArenaAllocator* arena = instruction->GetBlock()->GetGraph()->GetAllocator();
  HInstruction* left = instruction->GetLeft();
  HInstruction* right = instruction->GetRight();
  // Perform simplication only when either left or right
  // is Not. When both are Not, instruction should be simplified with
  // DeMorgan's Laws.
  if (left->IsNot() ^ right->IsNot()) {
    bool left_is_not = left->IsNot();
    HInstruction* other_ins = (left_is_not ? right : left);
    HNot* not_ins = (left_is_not ? left: right)->AsNot();
    // Only do the simplification if instruction has only one use
    // and thus can be safely removed.
    if (not_ins->HasOnlyOneNonEnvironmentUse()) {
      HInstruction* use = not_ins->GetUses().front().GetUser();
      DCHECK(use->Equals(instruction));
      HX86AndNot* and_not = new (arena) HX86AndNot(type, not_ins->GetInput(), other_ins, instruction->GetDexPc());
      instruction->GetBlock()->ReplaceAndRemoveInstructionWith(instruction, and_not);
      DCHECK(!not_ins->HasUses());
      not_ins->GetBlock()->RemoveInstruction(not_ins);
      return true;
    }
  }
  return false;
}

bool TryGenerateMaskOrResetLeastSetBit(HAdd* instruction) {
  DataType::Type type = instruction->GetType();
  if (!DataType::IsIntOrLongType(type)) {
    return false;
  }
  ArenaAllocator* arena = instruction->GetBlock()->GetGraph()->GetAllocator();
  HConstant* cst_input = instruction->GetConstantRight();
  HInstruction* other_input = instruction->GetLeastConstantLeft();
  if (cst_input == nullptr) {
     return false;
  }
  if (cst_input->IsMinusOne() && instruction->HasOnlyOneNonEnvironmentUse()) {
    HInstruction* use = instruction->GetUses().front().GetUser();
    if (use->IsAnd()|| use->IsXor()) {
      HInstruction* left = use->AsBinaryOperation()->GetLeft();
      HInstruction* right = use->AsBinaryOperation()->GetRight();
      if (left == other_input || right == other_input) {
        HInstruction::InstructionKind kind = use->IsAnd() ? HInstruction::kAnd : HInstruction::kXor;
        HX86MaskOrResetLeastSetBit* lsb = new (arena)
                                                HX86MaskOrResetLeastSetBit(type, kind,
                                                                           other_input,
                                                                           instruction->GetDexPc());
        use->GetBlock()->ReplaceAndRemoveInstructionWith(use, lsb);
        DCHECK(!instruction->HasUses());
        instruction->GetBlock()->RemoveInstruction(instruction);
        return true;
      }
    }
  }
  return false;
}

}  // namespace art
