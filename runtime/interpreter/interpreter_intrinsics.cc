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

template <typename kind>
static ALWAYS_INLINE int32_t Compare(kind lhs, kind rhs) {
  return (lhs < rhs) ? -1 : ((lhs == rhs) ? 0 : 1);
}

template <typename kind>
static ALWAYS_INLINE int32_t Signum(kind opnd) {
  return (opnd < 0) ? -1 : ((opnd == 0) ? 0 : 1);
}

template <typename kind>
static ALWAYS_INLINE kind LowestOneBit(kind opnd) {
  // Hacker's Delight, Section 2-1
  return opnd & -opnd;
}

template <typename kind>
static ALWAYS_INLINE kind HighestOneBit(kind opnd) {
  // Hacker's Delight, Section 3-1
  opnd |= (opnd >> 1);
  opnd |= (opnd >> 2);
  opnd |= (opnd >> 4);
  opnd |= (opnd >> 8);
  opnd |= (opnd >> 16);
  if (sizeof(opnd) == sizeof(int32_t)) {
    return opnd - (static_cast<uint32_t>(opnd) >> 1);
  } else {
    DCHECK(sizeof(opnd) == sizeof(int64_t));
    opnd |= (opnd >> 32);
    return opnd - (static_cast<uint64_t>(opnd) >> 1);
  }
}

template <typename kind, typename unsigned_kind, bool left>
static ALWAYS_INLINE kind Rot(kind opnd, int distance) {
  int mask = (sizeof(opnd) * 8) - 1;
  int unsigned_right_shift = left ? (-distance & mask) : (distance & mask);
  int signed_left_shift = left ? (distance & mask) : (-distance & mask);
  return (static_cast<unsigned_kind>(opnd) >> unsigned_right_shift) | (opnd << signed_left_shift);
}

#define BINARY_SIMPLE_INTRINSIC(name, op, get, set, offset)  \
static ALWAYS_INLINE bool name(ShadowFrame* shadow_frame,    \
                               const Instruction* inst,      \
                               uint16_t inst_data,           \
                               JValue* result_register)      \
    REQUIRES_SHARED(Locks::mutator_lock_) {                  \
  uint32_t arg[Instruction::kMaxVarArgRegs] = {};            \
  inst->GetVarArgs(arg, inst_data);                          \
  result_register->set(op(shadow_frame->get(arg[0]), shadow_frame->get(arg[offset]))); \
  return true;                                               \
}

#define UNARY_SIMPLE_INTRINSIC(name, op, get, set)           \
static ALWAYS_INLINE bool name(ShadowFrame* shadow_frame,    \
                               const Instruction* inst,      \
                               uint16_t inst_data,           \
                               JValue* result_register)      \
    REQUIRES_SHARED(Locks::mutator_lock_) {                  \
  uint32_t arg[Instruction::kMaxVarArgRegs] = {};            \
  inst->GetVarArgs(arg, inst_data);                          \
  result_register->set(op(shadow_frame->get(arg[0])));       \
  return true;                                               \
}

#define ZEROCOUNT_INTRINSIC(name, op, get, opnd_type)                     \
static ALWAYS_INLINE bool name(ShadowFrame* shadow_frame,                 \
                               const Instruction* inst,                   \
                               uint16_t inst_data,                        \
                               JValue* result_register)                   \
    REQUIRES_SHARED(Locks::mutator_lock_) {                               \
  uint32_t arg[Instruction::kMaxVarArgRegs] = {};                         \
  inst->GetVarArgs(arg, inst_data);                                       \
  opnd_type opnd = static_cast<opnd_type>(shadow_frame->get(arg[0]));     \
  result_register->SetI((opnd == 0) ? sizeof(opnd) * 8 : op(opnd));       \
  return true;                                                            \
}

#define BINARY_JJI_INTRINSIC(name, op)                                \
static ALWAYS_INLINE bool name(ShadowFrame* shadow_frame,             \
                               const Instruction* inst,               \
                               uint16_t inst_data,                    \
                               JValue* result_register)               \
    REQUIRES_SHARED(Locks::mutator_lock_) {                           \
  uint32_t arg[Instruction::kMaxVarArgRegs] = {};                     \
  inst->GetVarArgs(arg, inst_data);                                   \
  int64_t arg0 = shadow_frame->GetVRegLong(arg[0]);                   \
  int32_t arg1 = shadow_frame->GetVReg(arg[2]);                       \
  result_register->SetJ(op(arg0, arg1));                              \
  return true;                                                        \
}

// java.lang.Integer.reverse(I)I
static ALWAYS_INLINE int32_t MterpIntegerReverse(ShadowFrame* shadow_frame,
                                                 const Instruction* inst,
                                                 uint16_t inst_data,
                                                 JValue* result_register)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  uint32_t arg[Instruction::kMaxVarArgRegs] = {};
  inst->GetVarArgs(arg, inst_data);
  uint32_t opnd = static_cast<uint32_t>(shadow_frame->GetVReg(arg[0]));
  // Hacker's Delight 7-1
  opnd = ((opnd >>  1) & 0x55555555) | ((opnd & 0x55555555) <<  1);
  opnd = ((opnd >>  2) & 0x33333333) | ((opnd & 0x33333333) <<  2);
  opnd = ((opnd >>  4) & 0x0F0F0F0F) | ((opnd & 0x0F0F0F0F) <<  4);
  opnd = ((opnd >>  8) & 0x00FF00FF) | ((opnd & 0x00FF00FF) <<  8);
  opnd = ((opnd >> 16)) | ((opnd) << 16);
  result_register->SetI(opnd);
  return true;
}
// java.lang.Integer.reverseBytes(I)I
UNARY_SIMPLE_INTRINSIC(MterpIntegerReverseBytes, __builtin_bswap32, GetVReg, SetI);

// java.lang.Integer.bitCount(I)I
UNARY_SIMPLE_INTRINSIC(MterpIntegerBitCount, __builtin_popcount, GetVReg, SetI);

// java.lang.Integer.compare(II)I
BINARY_SIMPLE_INTRINSIC(MterpIntegerCompare, Compare, GetVReg, SetI, 1);

// java.lang.Integer.highestOneBit(I)I
UNARY_SIMPLE_INTRINSIC(MterpIntegerHighestOneBit, HighestOneBit, GetVReg, SetI);

// java.lang.Integer.LowestOneBit(I)I
UNARY_SIMPLE_INTRINSIC(MterpIntegerLowestOneBit, LowestOneBit, GetVReg, SetI);

// java.lang.Integer.numberOfLeadingZeros(I)I
ZEROCOUNT_INTRINSIC(MterpIntegerNumberOfLeadingZeros, __builtin_clz, GetVReg, int32_t);

// java.lang.Integer.numberOfTrailingZeros(I)I
ZEROCOUNT_INTRINSIC(MterpIntegerNumberOfTrailingZeros, __builtin_ctz, GetVReg, int32_t);

// java.lang.Integer.rotateRight(II)I
BINARY_SIMPLE_INTRINSIC(MterpIntegerRotateRight, (Rot<int32_t, uint32_t, false>), GetVReg, SetI, 1);

// java.lang.Integer.rotateLeft(II)I
BINARY_SIMPLE_INTRINSIC(MterpIntegerRotateLeft, (Rot<int32_t, uint32_t, true>), GetVReg, SetI, 1);

// java.lang.Integer.signum(I)I
UNARY_SIMPLE_INTRINSIC(MterpIntegerSignum, Signum, GetVReg, SetI);

// java.lang.Long.reverse(I)I
static ALWAYS_INLINE int64_t MterpLongReverse(ShadowFrame* shadow_frame,
                                              const Instruction* inst,
                                              uint16_t inst_data,
                                              JValue* result_register)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  uint32_t arg[Instruction::kMaxVarArgRegs] = {};
  inst->GetVarArgs(arg, inst_data);
  uint64_t opnd = static_cast<uint64_t>(shadow_frame->GetVRegLong(arg[0]));
  // Hacker's Delight 7-1
  opnd = (opnd & 0x5555555555555555L) << 1 | ((opnd >> 1) & 0x5555555555555555L);
  opnd = (opnd & 0x3333333333333333L) << 2 | ((opnd >> 2) & 0x3333333333333333L);
  opnd = (opnd & 0x0f0f0f0f0f0f0f0fL) << 4 | ((opnd >> 4) & 0x0f0f0f0f0f0f0f0fL);
  opnd = (opnd & 0x00ff00ff00ff00ffL) << 8 | ((opnd >> 8) & 0x00ff00ff00ff00ffL);
  opnd = (opnd << 48) | ((opnd & 0xffff0000L) << 16) | ((opnd >> 16) & 0xffff0000L) | (opnd >> 48);
  result_register->SetJ(opnd);
  return true;
}

// java.lang.Long.reverseBytes(J)J
UNARY_SIMPLE_INTRINSIC(MterpLongReverseBytes, __builtin_bswap64, GetVRegLong, SetJ);

// java.lang.Long.bitCount(J)I
UNARY_SIMPLE_INTRINSIC(MterpLongBitCount, __builtin_popcountll, GetVRegLong, SetI);

// java.lang.Long.compare(JJ)I
BINARY_SIMPLE_INTRINSIC(MterpLongCompare, Compare, GetVRegLong, SetI, 2);

// java.lang.Long.highestOneBit(J)J
UNARY_SIMPLE_INTRINSIC(MterpLongHighestOneBit, HighestOneBit, GetVRegLong, SetJ);

// java.lang.Long.lowestOneBit(J)J
UNARY_SIMPLE_INTRINSIC(MterpLongLowestOneBit, LowestOneBit, GetVRegLong, SetJ);

// java.lang.Long.numberOfLeadingZeros(J)I
ZEROCOUNT_INTRINSIC(MterpLongNumberOfLeadingZeros, __builtin_clzll, GetVRegLong, int64_t);

// java.lang.Long.numberOfTrailingZeros(J)I
ZEROCOUNT_INTRINSIC(MterpLongNumberOfTrailingZeros, __builtin_ctzll, GetVRegLong, int64_t);

// java.lang.Long.rotateRight(JI)J
BINARY_JJI_INTRINSIC(MterpLongRotateRight, (Rot<int64_t, uint64_t, false>));

// java.lang.Long.rotateLeft(JI)J
BINARY_JJI_INTRINSIC(MterpLongRotateLeft, (Rot<int64_t, uint64_t, true>));

// java.lang.Long.signum(J)I
UNARY_SIMPLE_INTRINSIC(MterpLongSignum, Signum, GetVRegLong, SetI);

// java.lang.Short.reverseBytes(S)S
UNARY_SIMPLE_INTRINSIC(MterpShortReverseBytes, __builtin_bswap16, GetVReg, SetS);

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
UNARY_SIMPLE_INTRINSIC(MterpMathAbsFloat, 0x7fffffff&, GetVReg, SetI);

// java.lang.Math.abs(D)D
UNARY_SIMPLE_INTRINSIC(MterpMathAbsDouble, INT64_C(0x7fffffffffffffff)&, GetVRegLong, SetJ);

// java.lang.Math.sqrt(D)D
UNARY_SIMPLE_INTRINSIC(MterpMathSqrt, std::sqrt, GetVRegDouble, SetD);

// java.lang.Math.ceil(D)D
UNARY_SIMPLE_INTRINSIC(MterpMathCeil, std::ceil, GetVRegDouble, SetD);

// java.lang.Math.floor(D)D
UNARY_SIMPLE_INTRINSIC(MterpMathFloor, std::floor, GetVRegDouble, SetD);

// java.lang.Math.sin(D)D
UNARY_SIMPLE_INTRINSIC(MterpMathSin, std::sin, GetVRegDouble, SetD);

// java.lang.Math.cos(D)D
UNARY_SIMPLE_INTRINSIC(MterpMathCos, std::cos, GetVRegDouble, SetD);

// java.lang.Math.tan(D)D
UNARY_SIMPLE_INTRINSIC(MterpMathTan, std::tan, GetVRegDouble, SetD);

// java.lang.Math.asin(D)D
UNARY_SIMPLE_INTRINSIC(MterpMathAsin, std::asin, GetVRegDouble, SetD);

// java.lang.Math.acos(D)D
UNARY_SIMPLE_INTRINSIC(MterpMathAcos, std::acos, GetVRegDouble, SetD);

// java.lang.Math.atan(D)D
UNARY_SIMPLE_INTRINSIC(MterpMathAtan, std::atan, GetVRegDouble, SetD);

// java.lang.String.charAt(I)C
static ALWAYS_INLINE bool MterpStringCharAt(ShadowFrame* shadow_frame,
                                            const Instruction* inst,
                                            uint16_t inst_data,
                                            JValue* result_register)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  uint32_t arg[Instruction::kMaxVarArgRegs] = {};
  inst->GetVarArgs(arg, inst_data);
  mirror::String* str = shadow_frame->GetVRegReference(arg[0])->AsString();
  int length = str->GetLength();
  int index = shadow_frame->GetVReg(arg[1]);
  uint16_t res;
  if (UNLIKELY(index < 0) || (index >= length)) {
    return false;  // Punt and let non-intrinsic version deal with the throw.
  }
  if (str->IsCompressed()) {
    res = str->GetValueCompressed()[index];
  } else {
    res = str->GetValue()[index];
  }
  result_register->SetC(res);
  return true;
}

// java.lang.String.compareTo(Ljava/lang/string)I
static ALWAYS_INLINE bool MterpStringCompareTo(ShadowFrame* shadow_frame,
                                               const Instruction* inst,
                                               uint16_t inst_data,
                                               JValue* result_register)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  uint32_t arg[Instruction::kMaxVarArgRegs] = {};
  inst->GetVarArgs(arg, inst_data);
  mirror::String* str = shadow_frame->GetVRegReference(arg[0])->AsString();
  mirror::Object* arg1 = shadow_frame->GetVRegReference(arg[1]);
  if (arg1 == nullptr) {
    return false;
  }
  result_register->SetI(str->CompareTo(arg1->AsString()));
  return true;
}

#define STRING_INDEXOF_INTRINSIC(name, starting_pos)             \
static ALWAYS_INLINE bool Mterp##name(ShadowFrame* shadow_frame, \
                                      const Instruction* inst,   \
                                      uint16_t inst_data,        \
                                      JValue* result_register)   \
    REQUIRES_SHARED(Locks::mutator_lock_) {                      \
  uint32_t arg[Instruction::kMaxVarArgRegs] = {};                \
  inst->GetVarArgs(arg, inst_data);                              \
  mirror::String* str = shadow_frame->GetVRegReference(arg[0])->AsString(); \
  int ch = shadow_frame->GetVReg(arg[1]);                        \
  if (ch >= 0x10000) {                                           \
    /* Punt if supplementary char. */                            \
    return false;                                                \
  }                                                              \
  result_register->SetI(str->FastIndexOf(ch, starting_pos));     \
  return true;                                                   \
}

// java.lang.String.indexOf(I)I
STRING_INDEXOF_INTRINSIC(StringIndexOf, 0);

// java.lang.String.indexOf(II)I
STRING_INDEXOF_INTRINSIC(StringIndexOfAfter, shadow_frame->GetVReg(arg[2]));

#define SIMPLE_STRING_INTRINSIC(name, operation)                 \
static ALWAYS_INLINE bool Mterp##name(ShadowFrame* shadow_frame, \
                                      const Instruction* inst,   \
                                      uint16_t inst_data,        \
                                      JValue* result_register)   \
    REQUIRES_SHARED(Locks::mutator_lock_) {                      \
  uint32_t arg[Instruction::kMaxVarArgRegs] = {};                \
  inst->GetVarArgs(arg, inst_data);                              \
  mirror::String* str = shadow_frame->GetVRegReference(arg[0])->AsString(); \
  result_register->operation;                                    \
  return true;                                                   \
}

// java.lang.String.isEmpty()Z
SIMPLE_STRING_INTRINSIC(StringIsEmpty, SetZ(str->GetLength() == 0))

// java.lang.String.length()I
SIMPLE_STRING_INTRINSIC(StringLength, SetI(str->GetLength()))

// java.lang.String.getCharsNoCheck(II[CI)V
static ALWAYS_INLINE bool MterpStringGetCharsNoCheck(ShadowFrame* shadow_frame,
                                                     const Instruction* inst,
                                                     uint16_t inst_data,
                                                     JValue* result_register ATTRIBUTE_UNUSED)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  // Start, end & index already checked by caller - won't throw.  Destination is uncompressed.
  uint32_t arg[Instruction::kMaxVarArgRegs] = {};
  inst->GetVarArgs(arg, inst_data);
  mirror::String* str = shadow_frame->GetVRegReference(arg[0])->AsString();
  int32_t start = shadow_frame->GetVReg(arg[1]);
  int32_t end = shadow_frame->GetVReg(arg[2]);
  int32_t index = shadow_frame->GetVReg(arg[4]);
  mirror::CharArray* array = shadow_frame->GetVRegReference(arg[3])->AsCharArray();
  uint16_t* dst = array->GetData() + index;
  int32_t len = (end - start);
  if (str->IsCompressed()) {
    const uint8_t* src_8 = str->GetValueCompressed() + start;
    for (int i = 0; i < len; i++) {
      dst[i] = src_8[i];
    }
  } else {
    uint16_t* src_16 = str->GetValue() + start;
    memcpy(dst, src_16, len * sizeof(uint16_t));
  }
  return true;
}

// java.lang.String.equalsLjava/lang/Object;)Z
static ALWAYS_INLINE bool MterpStringEquals(ShadowFrame* shadow_frame,
                                            const Instruction* inst,
                                            uint16_t inst_data,
                                            JValue* result_register)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  uint32_t arg[Instruction::kMaxVarArgRegs] = {};
  inst->GetVarArgs(arg, inst_data);
  mirror::String* str = shadow_frame->GetVRegReference(arg[0])->AsString();
  mirror::Object* obj = shadow_frame->GetVRegReference(arg[1]);
  bool res = false;  // Assume not equal.
  if ((obj != nullptr) && obj->IsString()) {
    mirror::String* str2 = obj->AsString();
    if (str->GetCount() == str2->GetCount()) {
      // Length & compression status are same.  Can use block compare.
      void* bytes1;
      void* bytes2;
      int len = str->GetLength();
      if (str->IsCompressed()) {
        bytes1 = str->GetValueCompressed();
        bytes2 = str2->GetValueCompressed();
      } else {
        len *= sizeof(uint16_t);
        bytes1 = str->GetValue();
        bytes2 = str2->GetValue();
      }
      res = (memcmp(bytes1, bytes2, len) == 0);
    }
  }
  result_register->SetZ(res);
  return true;
}

// Macro to help keep track of what's left to implement.
#define UNIMPLEMENTED_CASE(name)    \
    case Intrinsics::k##name:       \
      res = false;                  \
      break;

#define INTRINSIC_CASE(name)                                           \
    case Intrinsics::k##name:                                          \
      res = Mterp##name(shadow_frame, inst, inst_data, result_register); \
      break;

bool MterpHandleIntrinsic(ShadowFrame* shadow_frame,
                          ArtMethod* const called_method,
                          const Instruction* inst,
                          uint16_t inst_data,
                          JValue* result_register)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  Intrinsics intrinsic = static_cast<Intrinsics>(called_method->GetIntrinsic());
  bool res = false;  // Assume failure
  switch (intrinsic) {
    UNIMPLEMENTED_CASE(DoubleDoubleToRawLongBits /* (D)J */)
    UNIMPLEMENTED_CASE(DoubleDoubleToLongBits /* (D)J */)
    UNIMPLEMENTED_CASE(DoubleIsInfinite /* (D)Z */)
    UNIMPLEMENTED_CASE(DoubleIsNaN /* (D)Z */)
    UNIMPLEMENTED_CASE(DoubleLongBitsToDouble /* (J)D */)
    UNIMPLEMENTED_CASE(FloatFloatToRawIntBits /* (F)I */)
    UNIMPLEMENTED_CASE(FloatFloatToIntBits /* (F)I */)
    UNIMPLEMENTED_CASE(FloatIsInfinite /* (F)Z */)
    UNIMPLEMENTED_CASE(FloatIsNaN /* (F)Z */)
    UNIMPLEMENTED_CASE(FloatIntBitsToFloat /* (I)F */)
    INTRINSIC_CASE(IntegerReverse)
    INTRINSIC_CASE(IntegerReverseBytes)
    INTRINSIC_CASE(IntegerBitCount)
    INTRINSIC_CASE(IntegerCompare)
    INTRINSIC_CASE(IntegerHighestOneBit)
    INTRINSIC_CASE(IntegerLowestOneBit)
    INTRINSIC_CASE(IntegerNumberOfLeadingZeros)
    INTRINSIC_CASE(IntegerNumberOfTrailingZeros)
    INTRINSIC_CASE(IntegerRotateRight)
    INTRINSIC_CASE(IntegerRotateLeft)
    INTRINSIC_CASE(IntegerSignum)
    INTRINSIC_CASE(LongReverse)
    INTRINSIC_CASE(LongReverseBytes)
    INTRINSIC_CASE(LongBitCount)
    INTRINSIC_CASE(LongCompare)
    INTRINSIC_CASE(LongHighestOneBit)
    INTRINSIC_CASE(LongLowestOneBit)
    INTRINSIC_CASE(LongNumberOfLeadingZeros)
    INTRINSIC_CASE(LongNumberOfTrailingZeros)
    INTRINSIC_CASE(LongRotateRight)
    INTRINSIC_CASE(LongRotateLeft)
    INTRINSIC_CASE(LongSignum)
    INTRINSIC_CASE(ShortReverseBytes)
    INTRINSIC_CASE(MathAbsDouble)
    INTRINSIC_CASE(MathAbsFloat)
    INTRINSIC_CASE(MathAbsLong)
    INTRINSIC_CASE(MathAbsInt)
    UNIMPLEMENTED_CASE(MathMinDoubleDouble /* (DD)D */)
    UNIMPLEMENTED_CASE(MathMinFloatFloat /* (FF)F */)
    INTRINSIC_CASE(MathMinLongLong)
    INTRINSIC_CASE(MathMinIntInt)
    UNIMPLEMENTED_CASE(MathMaxDoubleDouble /* (DD)D */)
    UNIMPLEMENTED_CASE(MathMaxFloatFloat /* (FF)F */)
    INTRINSIC_CASE(MathMaxLongLong)
    INTRINSIC_CASE(MathMaxIntInt)
    INTRINSIC_CASE(MathCos)
    INTRINSIC_CASE(MathSin)
    INTRINSIC_CASE(MathAcos)
    INTRINSIC_CASE(MathAsin)
    INTRINSIC_CASE(MathAtan)
    UNIMPLEMENTED_CASE(MathAtan2 /* (DD)D */)
    UNIMPLEMENTED_CASE(MathCbrt /* (D)D */)
    UNIMPLEMENTED_CASE(MathCosh /* (D)D */)
    UNIMPLEMENTED_CASE(MathExp /* (D)D */)
    UNIMPLEMENTED_CASE(MathExpm1 /* (D)D */)
    UNIMPLEMENTED_CASE(MathHypot /* (DD)D */)
    UNIMPLEMENTED_CASE(MathLog /* (D)D */)
    UNIMPLEMENTED_CASE(MathLog10 /* (D)D */)
    UNIMPLEMENTED_CASE(MathNextAfter /* (DD)D */)
    UNIMPLEMENTED_CASE(MathSinh /* (D)D */)
    INTRINSIC_CASE(MathTan)
    UNIMPLEMENTED_CASE(MathTanh /* (D)D */)
    INTRINSIC_CASE(MathSqrt)
    INTRINSIC_CASE(MathCeil)
    INTRINSIC_CASE(MathFloor)
    UNIMPLEMENTED_CASE(MathRint /* (D)D */)
    UNIMPLEMENTED_CASE(MathRoundDouble /* (D)J */)
    UNIMPLEMENTED_CASE(MathRoundFloat /* (F)I */)
    UNIMPLEMENTED_CASE(SystemArrayCopyChar /* ([CI[CII)V */)
    UNIMPLEMENTED_CASE(SystemArrayCopy /* (Ljava/lang/Object;ILjava/lang/Object;II)V */)
    UNIMPLEMENTED_CASE(ThreadCurrentThread /* ()Ljava/lang/Thread; */)
    UNIMPLEMENTED_CASE(MemoryPeekByte /* (J)B */)
    UNIMPLEMENTED_CASE(MemoryPeekIntNative /* (J)I */)
    UNIMPLEMENTED_CASE(MemoryPeekLongNative /* (J)J */)
    UNIMPLEMENTED_CASE(MemoryPeekShortNative /* (J)S */)
    UNIMPLEMENTED_CASE(MemoryPokeByte /* (JB)V */)
    UNIMPLEMENTED_CASE(MemoryPokeIntNative /* (JI)V */)
    UNIMPLEMENTED_CASE(MemoryPokeLongNative /* (JJ)V */)
    UNIMPLEMENTED_CASE(MemoryPokeShortNative /* (JS)V */)
    INTRINSIC_CASE(StringCharAt)
    INTRINSIC_CASE(StringCompareTo)
    INTRINSIC_CASE(StringEquals)
    INTRINSIC_CASE(StringGetCharsNoCheck)
    INTRINSIC_CASE(StringIndexOf)
    INTRINSIC_CASE(StringIndexOfAfter)
    UNIMPLEMENTED_CASE(StringStringIndexOf /* (Ljava/lang/String;)I */)
    UNIMPLEMENTED_CASE(StringStringIndexOfAfter /* (Ljava/lang/String;I)I */)
    INTRINSIC_CASE(StringIsEmpty)
    INTRINSIC_CASE(StringLength)
    UNIMPLEMENTED_CASE(StringNewStringFromBytes /* ([BIII)Ljava/lang/String; */)
    UNIMPLEMENTED_CASE(StringNewStringFromChars /* (II[C)Ljava/lang/String; */)
    UNIMPLEMENTED_CASE(StringNewStringFromString /* (Ljava/lang/String;)Ljava/lang/String; */)
    UNIMPLEMENTED_CASE(StringBufferAppend /* (Ljava/lang/String;)Ljava/lang/StringBuffer; */)
    UNIMPLEMENTED_CASE(StringBufferLength /* ()I */)
    UNIMPLEMENTED_CASE(StringBufferToString /* ()Ljava/lang/String; */)
    UNIMPLEMENTED_CASE(StringBuilderAppend /* (Ljava/lang/String;)Ljava/lang/StringBuilder; */)
    UNIMPLEMENTED_CASE(StringBuilderLength /* ()I */)
    UNIMPLEMENTED_CASE(StringBuilderToString /* ()Ljava/lang/String; */)
    UNIMPLEMENTED_CASE(UnsafeCASInt /* (Ljava/lang/Object;JII)Z */)
    UNIMPLEMENTED_CASE(UnsafeCASLong /* (Ljava/lang/Object;JJJ)Z */)
    UNIMPLEMENTED_CASE(UnsafeCASObject /* (Ljava/lang/Object;JLjava/lang/Object;Ljava/lang/Object;)Z */)
    UNIMPLEMENTED_CASE(UnsafeGet /* (Ljava/lang/Object;J)I */)
    UNIMPLEMENTED_CASE(UnsafeGetVolatile /* (Ljava/lang/Object;J)I */)
    UNIMPLEMENTED_CASE(UnsafeGetObject /* (Ljava/lang/Object;J)Ljava/lang/Object; */)
    UNIMPLEMENTED_CASE(UnsafeGetObjectVolatile /* (Ljava/lang/Object;J)Ljava/lang/Object; */)
    UNIMPLEMENTED_CASE(UnsafeGetLong /* (Ljava/lang/Object;J)J */)
    UNIMPLEMENTED_CASE(UnsafeGetLongVolatile /* (Ljava/lang/Object;J)J */)
    UNIMPLEMENTED_CASE(UnsafePut /* (Ljava/lang/Object;JI)V */)
    UNIMPLEMENTED_CASE(UnsafePutOrdered /* (Ljava/lang/Object;JI)V */)
    UNIMPLEMENTED_CASE(UnsafePutVolatile /* (Ljava/lang/Object;JI)V */)
    UNIMPLEMENTED_CASE(UnsafePutObject /* (Ljava/lang/Object;JLjava/lang/Object;)V */)
    UNIMPLEMENTED_CASE(UnsafePutObjectOrdered /* (Ljava/lang/Object;JLjava/lang/Object;)V */)
    UNIMPLEMENTED_CASE(UnsafePutObjectVolatile /* (Ljava/lang/Object;JLjava/lang/Object;)V */)
    UNIMPLEMENTED_CASE(UnsafePutLong /* (Ljava/lang/Object;JJ)V */)
    UNIMPLEMENTED_CASE(UnsafePutLongOrdered /* (Ljava/lang/Object;JJ)V */)
    UNIMPLEMENTED_CASE(UnsafePutLongVolatile /* (Ljava/lang/Object;JJ)V */)
    UNIMPLEMENTED_CASE(UnsafeGetAndAddInt /* (Ljava/lang/Object;JI)I */)
    UNIMPLEMENTED_CASE(UnsafeGetAndAddLong /* (Ljava/lang/Object;JJ)J */)
    UNIMPLEMENTED_CASE(UnsafeGetAndSetInt /* (Ljava/lang/Object;JI)I */)
    UNIMPLEMENTED_CASE(UnsafeGetAndSetLong /* (Ljava/lang/Object;JJ)J */)
    UNIMPLEMENTED_CASE(UnsafeGetAndSetObject /* (Ljava/lang/Object;JLjava/lang/Object;)Ljava/lang/Object; */)
    UNIMPLEMENTED_CASE(UnsafeLoadFence /* ()V */)
    UNIMPLEMENTED_CASE(UnsafeStoreFence /* ()V */)
    UNIMPLEMENTED_CASE(UnsafeFullFence /* ()V */)
    UNIMPLEMENTED_CASE(ReferenceGetReferent /* ()Ljava/lang/Object; */)
    UNIMPLEMENTED_CASE(IntegerValueOf /* (I)Ljava/lang/Integer; */)
    case Intrinsics::kNone:
      res = false;
      break;
    // Note: no default case to ensure we catch any newly added intrinsics.
  }
  return res;
}

}  // namespace interpreter
}  // namespace art
