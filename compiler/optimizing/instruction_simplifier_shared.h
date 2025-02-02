/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef ART_COMPILER_OPTIMIZING_INSTRUCTION_SIMPLIFIER_SHARED_H_
#define ART_COMPILER_OPTIMIZING_INSTRUCTION_SIMPLIFIER_SHARED_H_

#include "base/macros.h"
#include "nodes.h"

namespace art HIDDEN {

class CodeGenerator;

namespace helpers {

inline bool CanFitInShifterOperand(HInstruction* instruction) {
  if (instruction->IsTypeConversion()) {
    HTypeConversion* conversion = instruction->AsTypeConversion();
    DataType::Type result_type = conversion->GetResultType();
    DataType::Type input_type = conversion->GetInputType();
    // We don't expect to see the same type as input and result.
    return DataType::IsIntegralType(result_type) && DataType::IsIntegralType(input_type) &&
        (result_type != input_type);
  } else {
    return (instruction->IsShl() && instruction->AsShl()->InputAt(1)->IsIntConstant()) ||
        (instruction->IsShr() && instruction->AsShr()->InputAt(1)->IsIntConstant()) ||
        (instruction->IsUShr() && instruction->AsUShr()->InputAt(1)->IsIntConstant());
  }
}

inline bool HasShifterOperand(HInstruction* instr, InstructionSet isa) {
  // On ARM64 `neg` instructions are an alias of `sub` using the zero register
  // as the first register input.
  bool res = instr->IsAdd() || instr->IsAnd() ||
      (isa == InstructionSet::kArm64 && instr->IsNeg()) ||
      instr->IsOr() || instr->IsSub() || instr->IsXor();
  return res;
}

// Check the specified sub is the last operation of the sequence:
//   t1 = Shl
//   t2 = Sub(t1, *)
//   t3 = Sub(*, t2)
inline bool IsSubRightSubLeftShl(HSub *sub) {
  HInstruction* right = sub->GetRight();
  return right->IsSub() && right->AsSub()->GetLeft()->IsShl();
}

}  // namespace helpers

bool TryCombineMultiplyAccumulate(HMul* mul, InstructionSet isa);

bool TryExtractArrayAccessAddress(CodeGenerator* codegen,
                                  HInstruction* access,
                                  HInstruction* array,
                                  HInstruction* index,
                                  size_t data_offset);

bool TryExtractVecArrayAccessAddress(HVecMemoryOperation* access, HInstruction* index);

// Try to replace
//   Sub(c, Sub(a, b))
// with
//   Add(c, Sub(b, a))
bool TryReplaceSubSubWithSubAdd(HSub* last_sub);

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_INSTRUCTION_SIMPLIFIER_SHARED_H_
