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

#include "arch/arm/instruction_set_features_arm.h"
#include "code_generator_utils.h"
#include "common_arm.h"
#include "mirror/array-inl.h"
#include "scheduler_arm.h"

namespace art {
namespace arm {

using helpers::Int32ConstantFrom;
using helpers::Uint64ConstantFrom;

void SchedulingLatencyVisitorARM::HandleBinaryOperationLantencies(HBinaryOperation* instr) {
  switch (instr->GetResultType()) {
    case Primitive::kPrimLong:

      // HAdd and HSub long operations translate to ADDS+ADC or SUBS+SBC pairs,
      // so a bubble (kArmNopLatency) is added to represent the internal carry flag
      // dependency inside these pairs.
      last_visited_internal_latency_ = HInstructionScheduling::scheduler_ArmIntegerOpLatency;
      last_visited_latency_ = HInstructionScheduling::scheduler_ArmIntegerOpLatency;
      break;
    case Primitive::kPrimFloat:
    case Primitive::kPrimDouble:
      last_visited_latency_ = HInstructionScheduling::scheduler_ArmFloatingPointOpLatency;
      break;
    default:
      last_visited_latency_ = HInstructionScheduling::scheduler_ArmIntegerOpLatency;
      break;
  }
}

void SchedulingLatencyVisitorARM::VisitAdd(HAdd* instr) {
  HandleBinaryOperationLantencies(instr);
}

void SchedulingLatencyVisitorARM::VisitSub(HSub* instr) {
  HandleBinaryOperationLantencies(instr);
}

void SchedulingLatencyVisitorARM::VisitMul(HMul* instr) {
  switch (instr->GetResultType()) {
    case Primitive::kPrimLong:
      last_visited_internal_latency_ = 3 * HInstructionScheduling::scheduler_ArmMulIntegerLatency;
      last_visited_latency_ = HInstructionScheduling::scheduler_ArmIntegerOpLatency;
      break;
    case Primitive::kPrimFloat:
    case Primitive::kPrimDouble:
      last_visited_latency_ = HInstructionScheduling::scheduler_ArmMulFloatingPointLatency;
      break;
    default:
      last_visited_latency_ = HInstructionScheduling::scheduler_ArmMulIntegerLatency;
      break;
  }
}

void SchedulingLatencyVisitorARM::HandleBitwiseOperationLantencies(HBinaryOperation* instr) {
  switch (instr->GetResultType()) {
    case Primitive::kPrimLong:
      last_visited_internal_latency_ = kArmIntegerOpLatency;
      last_visited_latency_ = kArmIntegerOpLatency;
      break;
    case Primitive::kPrimFloat:
    case Primitive::kPrimDouble:
      last_visited_latency_ = kArmFloatingPointOpLatency;
      break;
    default:
      last_visited_latency_ = kArmIntegerOpLatency;
      break;
  }
}

void SchedulingLatencyVisitorARM::VisitAnd(HAnd* instr) {
  HandleBitwiseOperationLantencies(instr);
}

void SchedulingLatencyVisitorARM::VisitOr(HOr* instr) {
  HandleBitwiseOperationLantencies(instr);
}

void SchedulingLatencyVisitorARM::VisitXor(HXor* instr) {
  HandleBitwiseOperationLantencies(instr);
}

void SchedulingLatencyVisitorARM::VisitRor(HRor* instr) {
  switch (instr->GetResultType()) {
    case Primitive::kPrimInt:
      last_visited_latency_ = HInstructionScheduling::scheduler_ArmIntegerOpLatency;
      break;
    case Primitive::kPrimLong: {
      // HandleLongRotate
      HInstruction* rhs = instr->GetRight();
      if (rhs->IsConstant()) {
        uint64_t rot = Uint64ConstantFrom(rhs->AsConstant()) & kMaxLongShiftDistance;
        if (rot != 0u) {
          last_visited_internal_latency_ = 3 * HInstructionScheduling::scheduler_ArmIntegerOpLatency;
          last_visited_latency_ = HInstructionScheduling::scheduler_ArmIntegerOpLatency;
        } else {
          last_visited_internal_latency_ = HInstructionScheduling::scheduler_ArmIntegerOpLatency;
          last_visited_latency_ = HInstructionScheduling::scheduler_ArmIntegerOpLatency;
        }
      } else {
        last_visited_internal_latency_ = 9 * HInstructionScheduling::scheduler_ArmIntegerOpLatency + HInstructionScheduling::scheduler_ArmBranchLatency;
        last_visited_latency_ = HInstructionScheduling::scheduler_ArmBranchLatency;
      }
      break;
    }
    default:
      LOG(FATAL) << "Unexpected operation type " << instr->GetResultType();
      UNREACHABLE();
  }
}

void SchedulingLatencyVisitorARM::HandleShiftLatencies(HBinaryOperation* instr) {
  Primitive::Type type = instr->GetResultType();
  HInstruction* rhs = instr->GetRight();
  switch (type) {
    case Primitive::kPrimInt:
      if (!rhs->IsConstant()) {
        last_visited_internal_latency_ = HInstructionScheduling::scheduler_ArmIntegerOpLatency;
      }
      last_visited_latency_ = HInstructionScheduling::scheduler_ArmIntegerOpLatency;
      break;
    case Primitive::kPrimLong:
      if (!rhs->IsConstant()) {
        last_visited_internal_latency_ = 8 * HInstructionScheduling::scheduler_ArmIntegerOpLatency;
      } else {
        uint32_t shift_value = Int32ConstantFrom(rhs->AsConstant()) & kMaxLongShiftDistance;
        if (shift_value == 1 || shift_value >= 32) {
          last_visited_internal_latency_ = HInstructionScheduling::scheduler_ArmIntegerOpLatency;
        } else {
          last_visited_internal_latency_ = 2 * HInstructionScheduling::scheduler_ArmIntegerOpLatency;
        }
      }
      last_visited_latency_ = HInstructionScheduling::scheduler_ArmIntegerOpLatency;
      break;
    default:
      LOG(FATAL) << "Unexpected operation type " << type;
      UNREACHABLE();
  }
}

void SchedulingLatencyVisitorARM::VisitShl(HShl* instr) {
  HandleShiftLatencies(instr);
}

void SchedulingLatencyVisitorARM::VisitShr(HShr* instr) {
  HandleShiftLatencies(instr);
}

void SchedulingLatencyVisitorARM::VisitUShr(HUShr* instr) {
  HandleShiftLatencies(instr);
}

void SchedulingLatencyVisitorARM::VisitCondition(HCondition* instr) {
  switch (instr->GetLeft()->GetType()) {
    case Primitive::kPrimLong:
      last_visited_internal_latency_ = 4 * HInstructionScheduling::scheduler_ArmIntegerOpLatency;
      break;
    case Primitive::kPrimFloat:
    case Primitive::kPrimDouble:
      last_visited_internal_latency_ = 2 * HInstructionScheduling::scheduler_ArmFloatingPointOpLatency;
      break;
    default:
      last_visited_internal_latency_ = 2 * HInstructionScheduling::scheduler_ArmIntegerOpLatency;
      break;
  }
  last_visited_latency_ = HInstructionScheduling::scheduler_ArmIntegerOpLatency;
}

void SchedulingLatencyVisitorARM::VisitCompare(HCompare* instr) {
  Primitive::Type type = instr->InputAt(0)->GetType();
  switch (type) {
    case Primitive::kPrimBoolean:
    case Primitive::kPrimByte:
    case Primitive::kPrimShort:
    case Primitive::kPrimChar:
    case Primitive::kPrimInt:
      last_visited_internal_latency_ = 2 * HInstructionScheduling::scheduler_ArmIntegerOpLatency;
      break;
    case Primitive::kPrimLong:
      last_visited_internal_latency_ = 2 * HInstructionScheduling::scheduler_ArmIntegerOpLatency + 3 * HInstructionScheduling::scheduler_ArmBranchLatency;
      break;
    case Primitive::kPrimFloat:
    case Primitive::kPrimDouble:
      last_visited_internal_latency_ = HInstructionScheduling::scheduler_ArmIntegerOpLatency + 2 * HInstructionScheduling::scheduler_ArmFloatingPointOpLatency;
      break;
    default:
      last_visited_internal_latency_ = 2 * HInstructionScheduling::scheduler_ArmIntegerOpLatency;
      break;
  }
  last_visited_latency_ = HInstructionScheduling::scheduler_ArmIntegerOpLatency;
}

void SchedulingLatencyVisitorARM::VisitBitwiseNegatedRight(HBitwiseNegatedRight* instruction) {
  if (instruction->GetResultType() == Primitive::kPrimInt) {
    last_visited_latency_ = HInstructionScheduling::scheduler_ArmIntegerOpLatency;
  } else {
    last_visited_internal_latency_ = HInstructionScheduling::scheduler_ArmIntegerOpLatency;
    last_visited_latency_ = HInstructionScheduling::scheduler_ArmIntegerOpLatency;
  }
}

void SchedulingLatencyVisitorARM::HandleGenerateDataProcInstruction(bool internal_latency) {
  if (internal_latency) {
    last_visited_internal_latency_ += HInstructionScheduling::scheduler_ArmIntegerOpLatency;
  } else {
    last_visited_latency_ = HInstructionScheduling::scheduler_ArmDataProcWithShifterOpLatency;
  }
}

void SchedulingLatencyVisitorARM::HandleGenerateDataProc(HDataProcWithShifterOp* instruction) {
  const HInstruction::InstructionKind kind = instruction->GetInstrKind();
  if (kind == HInstruction::kAdd) {
    last_visited_internal_latency_ = HInstructionScheduling::scheduler_ArmIntegerOpLatency;
    last_visited_latency_ = HInstructionScheduling::scheduler_ArmIntegerOpLatency;
  } else if (kind == HInstruction::kSub) {
    last_visited_internal_latency_ = HInstructionScheduling::scheduler_ArmIntegerOpLatency;
    last_visited_latency_ = HInstructionScheduling::scheduler_ArmIntegerOpLatency;
  } else {
    HandleGenerateDataProcInstruction(/* internal_latency */ true);
    HandleGenerateDataProcInstruction();
  }
}

void SchedulingLatencyVisitorARM::HandleGenerateLongDataProc(HDataProcWithShifterOp* instruction) {
  DCHECK_EQ(instruction->GetType(), Primitive::kPrimLong);
  DCHECK(HDataProcWithShifterOp::IsShiftOp(instruction->GetOpKind()));

  const uint32_t shift_value = instruction->GetShiftAmount();
  const HInstruction::InstructionKind kind = instruction->GetInstrKind();

  if (shift_value >= 32) {
    // Different shift types actually generate similar code here,
    // no need to differentiate shift types like the codegen pass does,
    // which also avoids handling shift types from different ARM backends.
    HandleGenerateDataProc(instruction);
  } else {
    DCHECK_GT(shift_value, 1U);
    DCHECK_LT(shift_value, 32U);

    if (kind == HInstruction::kOr || kind == HInstruction::kXor) {
      HandleGenerateDataProcInstruction(/* internal_latency */ true);
      HandleGenerateDataProcInstruction(/* internal_latency */ true);
      HandleGenerateDataProcInstruction();
    } else {
      last_visited_internal_latency_ += 2 * HInstructionScheduling::scheduler_ArmIntegerOpLatency;
      HandleGenerateDataProc(instruction);
    }
  }
}

void SchedulingLatencyVisitorARM::VisitDataProcWithShifterOp(HDataProcWithShifterOp* instruction) {
  const HDataProcWithShifterOp::OpKind op_kind = instruction->GetOpKind();

  if (instruction->GetType() == Primitive::kPrimInt) {
    DCHECK(!HDataProcWithShifterOp::IsExtensionOp(op_kind));
    HandleGenerateDataProcInstruction();
  } else {
    DCHECK_EQ(instruction->GetType(), Primitive::kPrimLong);
    if (HDataProcWithShifterOp::IsExtensionOp(op_kind)) {
      HandleGenerateDataProc(instruction);
    } else {
      HandleGenerateLongDataProc(instruction);
    }
  }
}

void SchedulingLatencyVisitorARM::VisitIntermediateAddress(HIntermediateAddress* ATTRIBUTE_UNUSED) {
  // Although the code generated is a simple `add` instruction, we found through empirical results
  // that spacing it from its use in memory accesses was beneficial.
  last_visited_internal_latency_ = HInstructionScheduling::scheduler_ArmNopLatency;
  last_visited_latency_ = HInstructionScheduling::scheduler_ArmIntegerOpLatency;
}

void SchedulingLatencyVisitorARM::VisitMultiplyAccumulate(HMultiplyAccumulate* ATTRIBUTE_UNUSED) {
  last_visited_latency_ = HInstructionScheduling::scheduler_ArmMulIntegerLatency;
}

void SchedulingLatencyVisitorARM::VisitArrayGet(HArrayGet* instruction) {
  Primitive::Type type = instruction->GetType();
  const bool maybe_compressed_char_at =
      mirror::kUseStringCompression && instruction->IsStringCharAt();
  HInstruction* array_instr = instruction->GetArray();
  bool has_intermediate_address = array_instr->IsIntermediateAddress();
  HInstruction* index = instruction->InputAt(1);

  switch (type) {
    case Primitive::kPrimBoolean:
    case Primitive::kPrimByte:
    case Primitive::kPrimShort:
    case Primitive::kPrimChar:
    case Primitive::kPrimInt: {
      if (maybe_compressed_char_at) {
        last_visited_internal_latency_ += HInstructionScheduling::scheduler_ArmMemoryLoadLatency;
      }
      if (index->IsConstant()) {
        if (maybe_compressed_char_at) {
          last_visited_internal_latency_ +=
              HInstructionScheduling::scheduler_ArmIntegerOpLatency + HInstructionScheduling::scheduler_ArmBranchLatency + HInstructionScheduling::scheduler_ArmMemoryLoadLatency;
          last_visited_latency_ = HInstructionScheduling::scheduler_ArmBranchLatency;
        } else {
          last_visited_latency_ += HInstructionScheduling::scheduler_ArmMemoryLoadLatency;
        }
      } else {
        if (has_intermediate_address) {
        } else {
          last_visited_internal_latency_ += HInstructionScheduling::scheduler_ArmIntegerOpLatency;
        }
        if (maybe_compressed_char_at) {
          last_visited_internal_latency_ +=
              HInstructionScheduling::scheduler_ArmIntegerOpLatency + HInstructionScheduling::scheduler_ArmBranchLatency + HInstructionScheduling::scheduler_ArmMemoryLoadLatency;
          last_visited_latency_ = HInstructionScheduling::scheduler_ArmBranchLatency;
        } else {
          last_visited_latency_ += HInstructionScheduling::scheduler_ArmMemoryLoadLatency;
        }
      }
      break;
    }

    case Primitive::kPrimNot: {
      if (kEmitCompilerReadBarrier && kUseBakerReadBarrier) {
        last_visited_latency_ = HInstructionScheduling::scheduler_ArmLoadWithBakerReadBarrierLatency;
      } else {
        if (index->IsConstant()) {
          last_visited_latency_ = HInstructionScheduling::scheduler_ArmMemoryLoadLatency;
        } else {
          if (has_intermediate_address) {
          } else {
            last_visited_internal_latency_ += HInstructionScheduling::scheduler_ArmIntegerOpLatency;
          }
          last_visited_internal_latency_ = HInstructionScheduling::scheduler_ArmMemoryLoadLatency;
        }
      }
      break;
    }

    case Primitive::kPrimLong: {
      if (index->IsConstant()) {
        last_visited_latency_ = HInstructionScheduling::scheduler_ArmMemoryLoadLatency;
      } else {
        last_visited_internal_latency_ += HInstructionScheduling::scheduler_ArmIntegerOpLatency;
        last_visited_latency_ = HInstructionScheduling::scheduler_ArmMemoryLoadLatency;
      }
      break;
    }

    case Primitive::kPrimFloat: {
      if (index->IsConstant()) {
        last_visited_latency_ = HInstructionScheduling::scheduler_ArmMemoryLoadLatency;
      } else {
        last_visited_internal_latency_ += HInstructionScheduling::scheduler_ArmIntegerOpLatency;
        last_visited_latency_ = HInstructionScheduling::scheduler_ArmMemoryLoadLatency;
      }
      break;
    }

    case Primitive::kPrimDouble: {
      if (index->IsConstant()) {
        last_visited_latency_ = HInstructionScheduling::scheduler_ArmMemoryLoadLatency;
      } else {
        last_visited_internal_latency_ += HInstructionScheduling::scheduler_ArmIntegerOpLatency;
        last_visited_latency_ = HInstructionScheduling::scheduler_ArmMemoryLoadLatency;
      }
      break;
    }

    default:
      LOG(FATAL) << "Unreachable type " << type;
      UNREACHABLE();
  }
}

void SchedulingLatencyVisitorARM::VisitArrayLength(HArrayLength* instruction) {
  last_visited_latency_ = HInstructionScheduling::scheduler_ArmMemoryLoadLatency;
  if (mirror::kUseStringCompression && instruction->IsStringLength()) {
    last_visited_internal_latency_ = HInstructionScheduling::scheduler_ArmMemoryLoadLatency;
    last_visited_latency_ = HInstructionScheduling::scheduler_ArmIntegerOpLatency;
  }
}

void SchedulingLatencyVisitorARM::VisitArraySet(HArraySet* instruction) {
  HInstruction* index = instruction->InputAt(1);
  Primitive::Type value_type = instruction->GetComponentType();
  HInstruction* array_instr = instruction->GetArray();
  bool has_intermediate_address = array_instr->IsIntermediateAddress();

  switch (value_type) {
    case Primitive::kPrimBoolean:
    case Primitive::kPrimByte:
    case Primitive::kPrimShort:
    case Primitive::kPrimChar:
    case Primitive::kPrimInt: {
      if (index->IsConstant()) {
        last_visited_latency_ = HInstructionScheduling::scheduler_ArmMemoryStoreLatency;
      } else {
        if (has_intermediate_address) {
        } else {
          last_visited_internal_latency_ = HInstructionScheduling::scheduler_ArmIntegerOpLatency;
        }
        last_visited_latency_ = HInstructionScheduling::scheduler_ArmMemoryStoreLatency;
      }
      break;
    }

    case Primitive::kPrimNot: {
      if (instruction->InputAt(2)->IsNullConstant()) {
        if (index->IsConstant()) {
          last_visited_latency_ = HInstructionScheduling::scheduler_ArmMemoryStoreLatency;
        } else {
          last_visited_internal_latency_ = HInstructionScheduling::scheduler_ArmIntegerOpLatency;
          last_visited_latency_ = HInstructionScheduling::scheduler_ArmMemoryStoreLatency;
        }
      } else {
        // Following the exact instructions of runtime type checks is too complicated,
        // just giving it a simple slow latency.
        last_visited_latency_ = HInstructionScheduling::scheduler_ArmRuntimeTypeCheckLatency;
      }
      break;
    }

    case Primitive::kPrimLong: {
      if (index->IsConstant()) {
        last_visited_latency_ = HInstructionScheduling::scheduler_ArmMemoryLoadLatency;
      } else {
        last_visited_internal_latency_ = HInstructionScheduling::scheduler_ArmIntegerOpLatency;
        last_visited_latency_ = HInstructionScheduling::scheduler_ArmMemoryLoadLatency;
      }
      break;
    }

    case Primitive::kPrimFloat: {
      if (index->IsConstant()) {
        last_visited_latency_ = HInstructionScheduling::scheduler_ArmMemoryLoadLatency;
      } else {
        last_visited_internal_latency_ = HInstructionScheduling::scheduler_ArmIntegerOpLatency;
        last_visited_latency_ = HInstructionScheduling::scheduler_ArmMemoryLoadLatency;
      }
      break;
    }

    case Primitive::kPrimDouble: {
      if (index->IsConstant()) {
        last_visited_latency_ = HInstructionScheduling::scheduler_ArmMemoryLoadLatency;
      } else {
        last_visited_internal_latency_ = HInstructionScheduling::scheduler_ArmIntegerOpLatency;
        last_visited_latency_ = HInstructionScheduling::scheduler_ArmMemoryLoadLatency;
      }
      break;
    }

    default:
      LOG(FATAL) << "Unreachable type " << value_type;
      UNREACHABLE();
  }
}

void SchedulingLatencyVisitorARM::VisitBoundsCheck(HBoundsCheck* ATTRIBUTE_UNUSED) {
  last_visited_internal_latency_ = HInstructionScheduling::scheduler_ArmIntegerOpLatency;
  // Users do not use any data results.
  last_visited_latency_ = 0;
}

void SchedulingLatencyVisitorARM::HandleDivRemConstantIntegralLatencies(int32_t imm) {
  if (imm == 0) {
    last_visited_internal_latency_ = 0;
    last_visited_latency_ = 0;
  } else if (imm == 1 || imm == -1) {
    last_visited_latency_ = HInstructionScheduling::scheduler_ArmIntegerOpLatency;
  } else if (IsPowerOfTwo(AbsOrMin(imm))) {
    last_visited_internal_latency_ = 3 * HInstructionScheduling::scheduler_ArmIntegerOpLatency;
    last_visited_latency_ = HInstructionScheduling::scheduler_ArmIntegerOpLatency;
  } else {
    last_visited_internal_latency_ = HInstructionScheduling::scheduler_ArmMulIntegerLatency + 2 * HInstructionScheduling::scheduler_ArmIntegerOpLatency;
    last_visited_latency_ = HInstructionScheduling::scheduler_ArmIntegerOpLatency;
  }
}

void SchedulingLatencyVisitorARM::VisitDiv(HDiv* instruction) {
  Primitive::Type type = instruction->GetResultType();
  switch (type) {
    case Primitive::kPrimInt: {
      HInstruction* rhs = instruction->GetRight();
      if (rhs->IsConstant()) {
        int32_t imm = Int32ConstantFrom(rhs->AsConstant());
        HandleDivRemConstantIntegralLatencies(imm);
      } else {
        last_visited_latency_ = HInstructionScheduling::scheduler_ArmDivIntegerLatency;
      }
      break;
    }
    case Primitive::kPrimFloat:
      last_visited_latency_ = HInstructionScheduling::scheduler_ArmDivFloatLatency;
      break;
    case Primitive::kPrimDouble:
      last_visited_latency_ = HInstructionScheduling::scheduler_ArmDivDoubleLatency;
      break;
    default:
      last_visited_internal_latency_ = HInstructionScheduling::scheduler_ArmCallInternalLatency;
      last_visited_latency_ = HInstructionScheduling::scheduler_ArmCallLatency;
      break;
  }
}

void SchedulingLatencyVisitorARM::VisitInstanceFieldGet(HInstanceFieldGet* instruction) {
  HandleFieldGetLatencies(instruction, instruction->GetFieldInfo());
}

void SchedulingLatencyVisitorARM::VisitInstanceFieldSet(HInstanceFieldSet* instruction) {
  HandleFieldSetLatencies(instruction, instruction->GetFieldInfo());
}

void SchedulingLatencyVisitorARM::VisitInstanceOf(HInstanceOf* ATTRIBUTE_UNUSED) {
  last_visited_internal_latency_ = HInstructionScheduling::scheduler_ArmCallInternalLatency;
  last_visited_latency_ = HInstructionScheduling::scheduler_ArmIntegerOpLatency;
}

void SchedulingLatencyVisitorARM::VisitInvoke(HInvoke* ATTRIBUTE_UNUSED) {
  last_visited_internal_latency_ = HInstructionScheduling::scheduler_ArmCallInternalLatency;
  last_visited_latency_ = HInstructionScheduling::scheduler_ArmCallLatency;
}

void SchedulingLatencyVisitorARM::VisitLoadString(HLoadString* ATTRIBUTE_UNUSED) {
  last_visited_internal_latency_ = HInstructionScheduling::scheduler_ArmLoadStringInternalLatency;
  last_visited_latency_ = HInstructionScheduling::scheduler_ArmMemoryLoadLatency;
}

void SchedulingLatencyVisitorARM::VisitNewArray(HNewArray* ATTRIBUTE_UNUSED) {
  last_visited_internal_latency_ = HInstructionScheduling::scheduler_ArmIntegerOpLatency + HInstructionScheduling::scheduler_ArmCallInternalLatency;
  last_visited_latency_ = HInstructionScheduling::scheduler_ArmCallLatency;
}

void SchedulingLatencyVisitorARM::VisitNewInstance(HNewInstance* instruction) {
  if (instruction->IsStringAlloc()) {
    last_visited_internal_latency_ = 2 + HInstructionScheduling::scheduler_ArmMemoryLoadLatency + HInstructionScheduling::scheduler_ArmCallInternalLatency;
  } else {
    last_visited_internal_latency_ = HInstructionScheduling::scheduler_ArmCallInternalLatency;
  }
  last_visited_latency_ = HInstructionScheduling::scheduler_ArmCallLatency;
}

void SchedulingLatencyVisitorARM::VisitRem(HRem* instruction) {
  Primitive::Type type = instruction->GetResultType();
  switch (type) {
    case Primitive::kPrimInt: {
      HInstruction* rhs = instruction->GetRight();
      if (rhs->IsConstant()) {
        int32_t imm = Int32ConstantFrom(rhs->AsConstant());
        HandleDivRemConstantIntegralLatencies(imm);
      } else {
        last_visited_internal_latency_ = HInstructionScheduling::scheduler_ArmDivIntegerLatency;
        last_visited_latency_ = HInstructionScheduling::scheduler_ArmMulIntegerLatency;
      }
      break;
    }
    default:
      last_visited_internal_latency_ = HInstructionScheduling::scheduler_ArmCallInternalLatency;
      last_visited_latency_ = HInstructionScheduling::scheduler_ArmCallLatency;
      break;
  }
}

void SchedulingLatencyVisitorARM::HandleFieldGetLatencies(HInstruction* instruction,
                                                          const FieldInfo& field_info) {
  DCHECK(instruction->IsInstanceFieldGet() || instruction->IsStaticFieldGet());
  DCHECK(codegen_ != nullptr);
  bool is_volatile = field_info.IsVolatile();
  Primitive::Type field_type = field_info.GetFieldType();
  bool atomic_ldrd_strd = codegen_->GetInstructionSetFeatures().HasAtomicLdrdAndStrd();

  switch (field_type) {
    case Primitive::kPrimBoolean:
    case Primitive::kPrimByte:
    case Primitive::kPrimShort:
    case Primitive::kPrimChar:
    case Primitive::kPrimInt:
      last_visited_latency_ = HInstructionScheduling::scheduler_ArmMemoryLoadLatency;
      break;

    case Primitive::kPrimNot:
      if (kEmitCompilerReadBarrier && kUseBakerReadBarrier) {
        last_visited_internal_latency_ = HInstructionScheduling::scheduler_ArmMemoryLoadLatency + HInstructionScheduling::scheduler_ArmIntegerOpLatency;
        last_visited_latency_ = HInstructionScheduling::scheduler_ArmMemoryLoadLatency;
      } else {
        last_visited_latency_ = HInstructionScheduling::scheduler_ArmMemoryLoadLatency;
      }
      break;

    case Primitive::kPrimLong:
      if (is_volatile && !atomic_ldrd_strd) {
        last_visited_internal_latency_ = HInstructionScheduling::scheduler_ArmMemoryLoadLatency + HInstructionScheduling::scheduler_ArmIntegerOpLatency;
        last_visited_latency_ = HInstructionScheduling::scheduler_ArmMemoryLoadLatency;
      } else {
        last_visited_latency_ = HInstructionScheduling::scheduler_ArmMemoryLoadLatency;
      }
      break;

    case Primitive::kPrimFloat:
      last_visited_latency_ = HInstructionScheduling::scheduler_ArmMemoryLoadLatency;
      break;

    case Primitive::kPrimDouble:
      if (is_volatile && !atomic_ldrd_strd) {
        last_visited_internal_latency_ =
            HInstructionScheduling::scheduler_ArmMemoryLoadLatency + HInstructionScheduling::scheduler_ArmIntegerOpLatency + HInstructionScheduling::scheduler_ArmMemoryLoadLatency;
        last_visited_latency_ = HInstructionScheduling::scheduler_ArmIntegerOpLatency;
      } else {
        last_visited_latency_ = HInstructionScheduling::scheduler_ArmMemoryLoadLatency;
      }
      break;

    default:
      last_visited_latency_ = HInstructionScheduling::scheduler_ArmMemoryLoadLatency;
      break;
  }

  if (is_volatile) {
    last_visited_internal_latency_ += HInstructionScheduling::scheduler_ArmMemoryBarrierLatency;
  }
}

void SchedulingLatencyVisitorARM::HandleFieldSetLatencies(HInstruction* instruction,
                                                          const FieldInfo& field_info) {
  DCHECK(instruction->IsInstanceFieldSet() || instruction->IsStaticFieldSet());
  DCHECK(codegen_ != nullptr);
  bool is_volatile = field_info.IsVolatile();
  Primitive::Type field_type = field_info.GetFieldType();
  bool needs_write_barrier =
      CodeGenerator::StoreNeedsWriteBarrier(field_type, instruction->InputAt(1));
  bool atomic_ldrd_strd = codegen_->GetInstructionSetFeatures().HasAtomicLdrdAndStrd();

  switch (field_type) {
    case Primitive::kPrimBoolean:
    case Primitive::kPrimByte:
    case Primitive::kPrimShort:
    case Primitive::kPrimChar:
      if (is_volatile) {
        last_visited_internal_latency_ = HInstructionScheduling::scheduler_ArmMemoryBarrierLatency + HInstructionScheduling::scheduler_ArmMemoryStoreLatency;
        last_visited_latency_ = HInstructionScheduling::scheduler_ArmMemoryBarrierLatency;
      } else {
        last_visited_latency_ = HInstructionScheduling::scheduler_ArmMemoryStoreLatency;
      }
      break;

    case Primitive::kPrimInt:
    case Primitive::kPrimNot:
      if (kPoisonHeapReferences && needs_write_barrier) {
        last_visited_internal_latency_ += HInstructionScheduling::scheduler_ArmIntegerOpLatency * 2;
      }
      last_visited_latency_ = HInstructionScheduling::scheduler_ArmMemoryStoreLatency;
      break;

    case Primitive::kPrimLong:
      if (is_volatile && !atomic_ldrd_strd) {
        last_visited_internal_latency_ =
            HInstructionScheduling::scheduler_ArmIntegerOpLatency + HInstructionScheduling::scheduler_ArmMemoryLoadLatency + HInstructionScheduling::scheduler_ArmMemoryStoreLatency;
        last_visited_latency_ = HInstructionScheduling::scheduler_ArmIntegerOpLatency;
      } else {
        last_visited_latency_ = HInstructionScheduling::scheduler_ArmMemoryStoreLatency;
      }
      break;

    case Primitive::kPrimFloat:
      last_visited_latency_ = HInstructionScheduling::scheduler_ArmMemoryStoreLatency;
      break;

    case Primitive::kPrimDouble:
      if (is_volatile && !atomic_ldrd_strd) {
        last_visited_internal_latency_ = HInstructionScheduling::scheduler_ArmIntegerOpLatency +
            HInstructionScheduling::scheduler_ArmIntegerOpLatency + HInstructionScheduling::scheduler_ArmMemoryLoadLatency + HInstructionScheduling::scheduler_ArmMemoryStoreLatency;
        last_visited_latency_ = HInstructionScheduling::scheduler_ArmIntegerOpLatency;
      } else {
        last_visited_latency_ = HInstructionScheduling::scheduler_ArmMemoryStoreLatency;
      }
      break;

    default:
      last_visited_latency_ = HInstructionScheduling::scheduler_ArmMemoryStoreLatency;
      break;
  }
}

void SchedulingLatencyVisitorARM::VisitStaticFieldGet(HStaticFieldGet* instruction) {
  HandleFieldGetLatencies(instruction, instruction->GetFieldInfo());
}

void SchedulingLatencyVisitorARM::VisitStaticFieldSet(HStaticFieldSet* instruction) {
  HandleFieldSetLatencies(instruction, instruction->GetFieldInfo());
}

void SchedulingLatencyVisitorARM::VisitSuspendCheck(HSuspendCheck* instruction) {
  HBasicBlock* block = instruction->GetBlock();
  DCHECK((block->GetLoopInformation() != nullptr) ||
         (block->IsEntryBlock() && instruction->GetNext()->IsGoto()));
  // Users do not use any data results.
  last_visited_latency_ = 0;
}

void SchedulingLatencyVisitorARM::VisitTypeConversion(HTypeConversion* instr) {
  Primitive::Type result_type = instr->GetResultType();
  Primitive::Type input_type = instr->GetInputType();

  switch (result_type) {
    case Primitive::kPrimByte:
    case Primitive::kPrimChar:
    case Primitive::kPrimShort:
      last_visited_latency_ = HInstructionScheduling::scheduler_ArmIntegerOpLatency;  // SBFX or UBFX
      break;

    case Primitive::kPrimInt:
      switch (input_type) {
        case Primitive::kPrimLong:
          last_visited_latency_ = HInstructionScheduling::scheduler_ArmIntegerOpLatency;  // MOV
          break;
        case Primitive::kPrimFloat:
        case Primitive::kPrimDouble:
          last_visited_internal_latency_ = HInstructionScheduling::scheduler_ArmTypeConversionFloatingPointIntegerLatency;
          last_visited_latency_ = HInstructionScheduling::scheduler_ArmFloatingPointOpLatency;
          break;
        default:
          last_visited_latency_ = HInstructionScheduling::scheduler_ArmIntegerOpLatency;
          break;
      }
      break;

    case Primitive::kPrimLong:
      switch (input_type) {
        case Primitive::kPrimBoolean:
        case Primitive::kPrimByte:
        case Primitive::kPrimChar:
        case Primitive::kPrimShort:
        case Primitive::kPrimInt:
          // MOV and extension
          last_visited_internal_latency_ = HInstructionScheduling::scheduler_ArmIntegerOpLatency;
          last_visited_latency_ = HInstructionScheduling::scheduler_ArmIntegerOpLatency;
          break;
        case Primitive::kPrimFloat:
        case Primitive::kPrimDouble:
          // invokes runtime
          last_visited_internal_latency_ = HInstructionScheduling::scheduler_ArmCallInternalLatency;
          break;
        default:
          last_visited_internal_latency_ = HInstructionScheduling::scheduler_ArmIntegerOpLatency;
          last_visited_latency_ = HInstructionScheduling::scheduler_ArmIntegerOpLatency;
          break;
      }
      break;

    case Primitive::kPrimFloat:
      switch (input_type) {
        case Primitive::kPrimBoolean:
        case Primitive::kPrimByte:
        case Primitive::kPrimChar:
        case Primitive::kPrimShort:
        case Primitive::kPrimInt:
          last_visited_internal_latency_ = HInstructionScheduling::scheduler_ArmTypeConversionFloatingPointIntegerLatency;
          last_visited_latency_ = HInstructionScheduling::scheduler_ArmFloatingPointOpLatency;
          break;
        case Primitive::kPrimLong:
          // invokes runtime
          last_visited_internal_latency_ = HInstructionScheduling::scheduler_ArmCallInternalLatency;
          break;
        case Primitive::kPrimDouble:
          last_visited_latency_ = HInstructionScheduling::scheduler_ArmFloatingPointOpLatency;
          break;
        default:
          last_visited_latency_ = HInstructionScheduling::scheduler_ArmFloatingPointOpLatency;
          break;
      }
      break;

    case Primitive::kPrimDouble:
      switch (input_type) {
        case Primitive::kPrimBoolean:
        case Primitive::kPrimByte:
        case Primitive::kPrimChar:
        case Primitive::kPrimShort:
        case Primitive::kPrimInt:
          last_visited_internal_latency_ = HInstructionScheduling::scheduler_ArmTypeConversionFloatingPointIntegerLatency;
          last_visited_latency_ = HInstructionScheduling::scheduler_ArmFloatingPointOpLatency;
          break;
        case Primitive::kPrimLong:
          last_visited_internal_latency_ = 5 * HInstructionScheduling::scheduler_ArmFloatingPointOpLatency;
          last_visited_latency_ = HInstructionScheduling::scheduler_ArmFloatingPointOpLatency;
          break;
        case Primitive::kPrimFloat:
          last_visited_latency_ = HInstructionScheduling::scheduler_ArmFloatingPointOpLatency;
          break;
        default:
          last_visited_latency_ = HInstructionScheduling::scheduler_ArmFloatingPointOpLatency;
          break;
      }
      break;

    default:
      last_visited_latency_ = HInstructionScheduling::scheduler_ArmTypeConversionFloatingPointIntegerLatency;
      break;
  }
}

void SchedulingLatencyVisitorARM::VisitArmDexCacheArraysBase(art::HArmDexCacheArraysBase*) {
  last_visited_internal_latency_ = HInstructionScheduling::scheduler_ArmIntegerOpLatency;
  last_visited_latency_ = HInstructionScheduling::scheduler_ArmIntegerOpLatency;
}

}  // namespace arm
}  // namespace art
