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

#include "interpreter/interpreter_common.h"
#include "interpreter/interpreter_intrinsics.h"

namespace art {
namespace interpreter {

#define BINARY_SIMPLE_INTRINSIC(name, op, get, set, offset)  \
bool name(ShadowFrame* shadow_frame,                         \
          const Instruction* inst,                           \
          uint16_t inst_data,                                \
          JValue* result_register)                           \
    REQUIRES_SHARED(Locks::mutator_lock_) {                  \
  uint32_t arg[Instruction::kMaxVarArgRegs] = {};            \
  inst->GetVarArgs(arg, inst_data);                          \
  result_register->set(op(shadow_frame->get(arg[0]), shadow_frame->get(arg[offset]))); \
  return true;                                               \
}

#define UNARY_SIMPLE_INTRINSIC(name, op, get, set)           \
bool name(ShadowFrame* shadow_frame,                         \
          const Instruction* inst,                           \
          uint16_t inst_data,                                \
          JValue* result_register)                           \
    REQUIRES_SHARED(Locks::mutator_lock_) {                  \
  uint32_t arg[Instruction::kMaxVarArgRegs] = {};            \
  inst->GetVarArgs(arg, inst_data);                          \
  result_register->set(op(shadow_frame->get(arg[0])));       \
  return true;                                               \
}

// java.lang.Math.min(II)I
BINARY_SIMPLE_INTRINSIC(MterpMathMinIntInt, std::min, GetVReg, SetI, 1);
// java.lang.Math.min(JJ)J
BINARY_SIMPLE_INTRINSIC(MterpMathMinLongLong, std::min, GetVRegLong, SetJ, 2);
// java.lang.Math.max(II)I
BINARY_SIMPLE_INTRINSIC(MterpMathMaxIntInt, std::max, GetVReg, SetI, 1);
// java.lang.Math.max(JJ)J
BINARY_SIMPLE_INTRINSIC(MterpMathMaxLongLong, std::max, GetVRegLong, SetJ, 2);
// java.lang.Math.abs(I)I
UNARY_SIMPLE_INTRINSIC(MterpMathAbsInt, std::abs, GetVReg, SetI);
// java.lang.Math.abs(J)J
UNARY_SIMPLE_INTRINSIC(MterpMathAbsLong, std::abs, GetVRegLong, SetJ);
// java.lang.Math.abs(F)F
UNARY_SIMPLE_INTRINSIC(MterpMathAbsFloat, std::abs, GetVRegFloat, SetF);
// java.lang.Math.abs(D)D
UNARY_SIMPLE_INTRINSIC(MterpMathAbsDouble, std::abs, GetVRegDouble, SetD);
// java.lang.Math.sqrt(D)D
UNARY_SIMPLE_INTRINSIC(MterpMathSqrtDouble, std::sqrt, GetVRegDouble, SetD);
// java.lang.Math.ceil(D)D
UNARY_SIMPLE_INTRINSIC(MterpMathCeilDouble, std::ceil, GetVRegDouble, SetD);
// java.lang.Math.floor(D)D
UNARY_SIMPLE_INTRINSIC(MterpMathFloorDouble, std::floor, GetVRegDouble, SetD);
// java.lang.Math.sin(D)D
UNARY_SIMPLE_INTRINSIC(MterpMathSinDouble, std::sin, GetVRegDouble, SetD);
// java.lang.Math.cos(D)D
UNARY_SIMPLE_INTRINSIC(MterpMathCosDouble, std::cos, GetVRegDouble, SetD);
// java.lang.Math.tan(D)D
UNARY_SIMPLE_INTRINSIC(MterpMathTanDouble, std::tan, GetVRegDouble, SetD);
// java.lang.Math.asin(D)D
UNARY_SIMPLE_INTRINSIC(MterpMathAsinDouble, std::asin, GetVRegDouble, SetD);
// java.lang.Math.acos(D)D
UNARY_SIMPLE_INTRINSIC(MterpMathAcosDouble, std::acos, GetVRegDouble, SetD);
// java.lang.Math.atan(D)D
UNARY_SIMPLE_INTRINSIC(MterpMathAtanDouble, std::atan, GetVRegDouble, SetD);

bool MterpHandleIntrinsic(ShadowFrame* shadow_frame,
                          ArtMethod* const called_method,
                          const Instruction* inst,
                          uint16_t inst_data,
                          JValue* result_register)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  Intrinsics intrinsic = static_cast<Intrinsics>(called_method->GetIntrinsic());
  bool res = false;  // Assume failure
  switch (intrinsic) {
    case Intrinsics::kMathMinIntInt:
      res = MterpMathMinIntInt(shadow_frame, inst, inst_data, result_register);
      break;
    case Intrinsics::kMathMinLongLong:
      res = MterpMathMinLongLong(shadow_frame, inst, inst_data, result_register);
      break;
    case Intrinsics::kMathMaxIntInt:
      res = MterpMathMaxIntInt(shadow_frame, inst, inst_data, result_register);
      break;
    case Intrinsics::kMathMaxLongLong:
      res = MterpMathMaxLongLong(shadow_frame, inst, inst_data, result_register);
      break;
    case Intrinsics::kMathAbsInt:
      res = MterpMathAbsInt(shadow_frame, inst, inst_data, result_register);
      break;
    case Intrinsics::kMathAbsLong:
      res = MterpMathAbsLong(shadow_frame, inst, inst_data, result_register);
      break;
    case Intrinsics::kMathAbsFloat:
      res = MterpMathAbsFloat(shadow_frame, inst, inst_data, result_register);
      break;
    case Intrinsics::kMathAbsDouble:
      res = MterpMathAbsDouble(shadow_frame, inst, inst_data, result_register);
      break;
    case Intrinsics::kMathSqrt:
      res = MterpMathSqrtDouble(shadow_frame, inst, inst_data, result_register);
      break;
    case Intrinsics::kMathCeil:
      res = MterpMathCeilDouble(shadow_frame, inst, inst_data, result_register);
      break;
    case Intrinsics::kMathFloor:
      res = MterpMathFloorDouble(shadow_frame, inst, inst_data, result_register);
      break;
    case Intrinsics::kMathSin:
      res = MterpMathSinDouble(shadow_frame, inst, inst_data, result_register);
      break;
    case Intrinsics::kMathCos:
      res = MterpMathCosDouble(shadow_frame, inst, inst_data, result_register);
      break;
    case Intrinsics::kMathTan:
      res = MterpMathTanDouble(shadow_frame, inst, inst_data, result_register);
      break;
    case Intrinsics::kMathAsin:
      res = MterpMathAsinDouble(shadow_frame, inst, inst_data, result_register);
      break;
    case Intrinsics::kMathAcos:
      res = MterpMathAcosDouble(shadow_frame, inst, inst_data, result_register);
      break;
    case Intrinsics::kMathAtan:
      res = MterpMathAtanDouble(shadow_frame, inst, inst_data, result_register);
      break;
    default:
      res = false;  // Punt
      break;
  }
  return res;
}

}  // namespace interpreter
}  // namespace art
