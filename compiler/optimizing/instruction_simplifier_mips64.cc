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

#include "instruction_simplifier_mips64.h"

namespace art {
namespace mips64 {

bool InstructionSimplifierMips64Visitor::TryCombineVecMulAcc(HVecMul* mul) {
  Primitive::Type type = mul->GetPackedType();
  if (!(type == Primitive::kPrimByte ||
        type == Primitive::kPrimChar ||
        type == Primitive::kPrimShort ||
        type == Primitive::kPrimInt ||
        type == Primitive::kPrimLong ||
        type == Primitive::kPrimFloat ||
        type == Primitive::kPrimDouble)) {
        return false;
    }

  ArenaAllocator* arena = mul->GetBlock()->GetGraph()->GetArena();

  if (mul->HasOnlyOneNonEnvironmentUse()) {
      HInstruction* use = mul->GetUses().front().GetUser();
      if (use->IsVecAdd() || use->IsVecSub()) {
      // Replace code looking like
      //    VECMUL tmp, x, y
      //    VECADD/SUB dst, acc, tmp
      // with
      //    VECMULACC dst, acc, x, y
      // Note that we do not want to (unconditionally) perform the merge when the
      // multiplication has multiple uses and it can be merged in all of them.
      // Multiple uses could happen on the same control-flow path, and we would
      // then increase the amount of work. In the future we could try to evaluate
      // whether all uses are on different control-flow paths (using dominance and
      // reverse-dominance information) and only perform the merge when they are.
      HInstruction* accumulator = nullptr;
      HVecBinaryOperation* binop = use->AsVecBinaryOperation();
      HInstruction* binop_left = binop->GetLeft();
      HInstruction* binop_right = binop->GetRight();
  //     // This is always true since the `HVecMul` has only one use (which is checked above).
      DCHECK_NE(binop_left, binop_right);
      if (binop_right == mul) {
        accumulator = binop_left;
      } else if (use->IsVecAdd()) {
        DCHECK_EQ(binop_left, mul);
        accumulator = binop_right;
      }

      HInstruction::InstructionKind kind =
          use->IsVecAdd() ? HInstruction::kAdd : HInstruction::kSub;
      if (accumulator != nullptr) {
        HVecMultiplyAccumulate* mulacc =
            new (arena) HVecMultiplyAccumulate(arena,
                                               kind,
                                               accumulator,
                                               mul->GetLeft(),
                                               mul->GetRight(),
                                               binop->GetPackedType(),
                                               binop->GetVectorLength());

        binop->GetBlock()->ReplaceAndRemoveInstructionWith(binop, mulacc);
        DCHECK(!mul->HasUses());
        mul->GetBlock()->RemoveInstruction(mul);
        return true;
      }
    }
  }

  return false;
}

void InstructionSimplifierMips64Visitor::VisitVecMul(HVecMul* instruction) {
  if (TryCombineVecMulAcc(instruction)) {
    RecordSimplification();
  }
}

}  // namespace mips64
}  // namespace art
