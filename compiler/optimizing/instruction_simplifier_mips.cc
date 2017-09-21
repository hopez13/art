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

#include "instruction_simplifier_mips.h"
#include "mirror/array-inl.h"
#include "arch/mips/instruction_set_features_mips.h"

namespace art {
namespace mips {

bool InstructionSimplifierMipsVisitor::TryExtractArrayAccessIndex(HInstruction* access,
  HInstruction* index, Primitive::Type packed_type) {
  if (codegen_->GetInstructionSetFeatures().IsR6()) {
    return false;
  }
  if (index->IsConstant()) {
    // If index is constant the whole address calculation often can be done by LDR/STR themselves.
    // TODO: Treat the case with not-embedable constant.
    return false;
  }

  if (packed_type != Primitive::kPrimShort && packed_type != Primitive::kPrimInt &&
      packed_type != Primitive::kPrimLong && packed_type != Primitive::kPrimChar &&
      packed_type != Primitive::kPrimFloat && packed_type != Primitive::kPrimDouble) {
    return false;
  }

  if (access->IsArrayGet() && access->AsArrayGet()->IsStringCharAt()) {
    return false;
  }

  HGraph* graph = access->GetBlock()->GetGraph();
  ArenaAllocator* arena = graph->GetArena();
  size_t component_shift = Primitive::ComponentSizeShift(packed_type);

  bool is_extracting_beneficial = false;
  // It is beneficial to extract index intermediate address only if there are at least 2 users.
  for (const HUseListNode<HInstruction*>& use : index->GetUses()) {
    HInstruction* user = use.GetUser();
    if (user->IsArrayGet() && user != access && !user->AsArrayGet()->IsStringCharAt()) {
      HArrayGet* another_access = user->AsArrayGet();
      Primitive::Type another_packed_type = another_access->GetType();
      size_t another_component_shift = Primitive::ComponentSizeShift(another_packed_type);
      if (another_component_shift == component_shift) {
        is_extracting_beneficial = true;
        break;
      }
    } else if (user->IsArraySet() && user != access) {
      HArraySet* another_access = user->AsArraySet();
      Primitive::Type another_packed_type = another_access->GetType();
      size_t another_component_shift = Primitive::ComponentSizeShift(another_packed_type);
      if (another_component_shift == component_shift) {
        is_extracting_beneficial = true;
        break;
      }
    } else if (user->IsIntermediateArrayAddressIndex()) {
      HIntermediateArrayAddressIndex* another_access = user->AsIntermediateArrayAddressIndex();
      size_t another_component_shift = another_access->GetShift()->AsIntConstant()->GetValue();
      if (another_component_shift == component_shift) {
        is_extracting_beneficial = true;
        break;
      }
    }
  }

  if (!is_extracting_beneficial) {
    return false;
  }

  HIntConstant* shift = graph->GetIntConstant(component_shift);
  HIntermediateArrayAddressIndex* address =
      new (arena) HIntermediateArrayAddressIndex(index, shift, kNoDexPc);
  access->GetBlock()->InsertInstructionBefore(address, access);
  access->ReplaceInput(address, 1);

  return true;
}


void InstructionSimplifierMipsVisitor::VisitArrayGet(HArrayGet* instruction) {
  Primitive::Type packed_type = instruction->GetType();
  if (TryExtractArrayAccessIndex(instruction, instruction->GetIndex(), packed_type)) {
    RecordSimplification();
  }
}

void InstructionSimplifierMipsVisitor::VisitArraySet(HArraySet* instruction) {
  Primitive::Type packed_type = instruction->GetComponentType();
  if (TryExtractArrayAccessIndex(instruction, instruction->GetIndex(), packed_type)) {
    RecordSimplification();
  }
}


}  // namespace mips
}  // namespace art
