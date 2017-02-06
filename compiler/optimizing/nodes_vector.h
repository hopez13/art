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

#ifndef ART_COMPILER_OPTIMIZING_NODES_VECTOR_H_
#define ART_COMPILER_OPTIMIZING_NODES_VECTOR_H_

// This #include should never be used by compilation, because this header file (nodes_vector.h)
// is included in the header file nodes.h itself. However it gives editing tools better context.
#include "nodes.h"

namespace art {

// Memory alignment, represented as an offset relative to a base, where 0 <= offset < base,
// and base is a power of two. For example, the value Alignment(16, 0) means memory is
// perfectly aligned at a 16-byte boundary, whereas the value Alignment(16, 4) means
// memory is always exactly 4 bytes above such a boundary.
class Alignment {
 public:
  Alignment(size_t base, size_t offset) : base_(base), offset_(offset) {
    DCHECK_LE(0u, offset);
    DCHECK_LT(offset, base);
    DCHECK(IsPowerOfTwo(base));
  }

  // Returns true if memory is "at least" aligned at the given boundary.
  bool IsAlignedAt(size_t base) const {
    DCHECK_LT(0u, base);
    return (offset_ % base) == 0 && (base_ % base) == 0;
  }

  std::string ToString() const {
    return "ALIGN(" + std::to_string(base_) + "," + std::to_string(offset_) + ")";
  }

 private:
  size_t base_;
  size_t offset_;
};

//
// Definitions of abstract vector operations in HIR.
//

// Abstraction of a vector operation, i.e., an operation that performs
// GetVectorLength() x GetPackedType() operations simultaneously.
class HVecOperation : public HVariableInputSizeInstruction {
 public:
  HVecOperation(ArenaAllocator* arena,
                Primitive::Type packed_type,
                SideEffects side_effects,
                size_t number_of_inputs,
                size_t vector_length,
                uint32_t dex_pc)
      : HVariableInputSizeInstruction(side_effects,
                                      dex_pc,
                                      arena,
                                      number_of_inputs,
                                      kArenaAllocVectorNode),
        vector_length_(vector_length) {
    SetPackedField<TypeField>(packed_type);
    DCHECK_LT(1u, vector_length);
  }

  size_t GetVectorLength() const { return vector_length_; }

  // A SIMD operation currently always looks like a FPU location.
  // TODO: we could introduce SIMD types in HIR.
  Primitive::Type GetType() const OVERRIDE { return Primitive::kPrimDouble; }

  // The true component type packed in a vector.
  Primitive::Type GetPackedType() const { return GetPackedField<TypeField>(); }

  DECLARE_ABSTRACT_INSTRUCTION(VecOperation);

 private:
  // Additional packed bits.
  static constexpr size_t kFieldType = HInstruction::kNumberOfGenericPackedBits;
  static constexpr size_t kFieldTypeSize = MinimumBitsToStore(static_cast<size_t>(Primitive::kPrimLast));
  static constexpr size_t kNumberOfVectorOpPackedBits = kFieldType + kFieldTypeSize;
  static_assert(kNumberOfVectorOpPackedBits <= kMaxNumberOfPackedBits, "Too many packed fields.");
  using TypeField = BitField<Primitive::Type, kFieldType, kFieldTypeSize>;

  const size_t vector_length_;

  DISALLOW_COPY_AND_ASSIGN(HVecOperation);
};

// Abstraction of a unary vector operation.
class HVecUnaryOperation : public HVecOperation {
 public:
  HVecUnaryOperation(ArenaAllocator* arena,
                     Primitive::Type packed_type,
                     size_t vector_length,
                     uint32_t dex_pc)
      : HVecOperation(arena, packed_type, SideEffects::None(), 1, vector_length, dex_pc) { }
  DECLARE_ABSTRACT_INSTRUCTION(VecUnaryOperation);
 private:
  DISALLOW_COPY_AND_ASSIGN(HVecUnaryOperation);
};

// Abstraction of a binary vector operation.
class HVecBinaryOperation : public HVecOperation {
 public:
  HVecBinaryOperation(ArenaAllocator* arena,
                      Primitive::Type packed_type,
                      size_t vector_length,
                      uint32_t dex_pc)
      : HVecOperation(arena, packed_type, SideEffects::None(), 2, vector_length, dex_pc) { }
  DECLARE_ABSTRACT_INSTRUCTION(VecBinaryOperation);
 private:
  DISALLOW_COPY_AND_ASSIGN(HVecBinaryOperation);
};

// Abstraction of a vector operation that references memory, with an alignment.
// The Android runtime guarantees at least "component size" alignment for array
// elements and, thus, vectors.
class HVecMemoryOperation : public HVecOperation {
 public:
  HVecMemoryOperation(ArenaAllocator* arena,
                      Primitive::Type packed_type,
                      SideEffects side_effects,
                      size_t number_of_inputs,
                      size_t vector_length,
                      uint32_t dex_pc)
      : HVecOperation(arena, packed_type, side_effects, number_of_inputs, vector_length, dex_pc),
        alignment_(Primitive::ComponentSize(packed_type), 0) { }

  void SetAlignment(Alignment alignment) { alignment_ = alignment; }

  Alignment GetAlignment() const { return alignment_; }

  DECLARE_ABSTRACT_INSTRUCTION(VecMemoryOperation);

 private:
  Alignment alignment_;

  DISALLOW_COPY_AND_ASSIGN(HVecMemoryOperation);
};

//
// Definitions of concrete vector operations in HIR.
//

class HVecSet1 FINAL : public HVecUnaryOperation {
 public:
  HVecSet1(ArenaAllocator* arena,
           HInstruction* scalar,
           Primitive::Type packed_type,
           size_t vector_length,
           uint32_t dex_pc = kNoDexPc)
      : HVecUnaryOperation(arena, packed_type, vector_length, dex_pc) {
    SetRawInputAt(0, scalar);
  }
  DECLARE_INSTRUCTION(VecSet1);
 private:
  DISALLOW_COPY_AND_ASSIGN(HVecSet1);
};

class HVecCnv FINAL : public HVecUnaryOperation {
 public:
  HVecCnv(ArenaAllocator* arena,
          HInstruction* input,
          Primitive::Type packed_type,
          size_t vector_length,
          uint32_t dex_pc = kNoDexPc)
      : HVecUnaryOperation(arena, packed_type, vector_length, dex_pc) {
    DCHECK(input->IsVecOperation());
    SetRawInputAt(0, input);
  }

  Primitive::Type GetInputType() const { return InputAt(0)->AsVecOperation()->GetPackedType(); }
  Primitive::Type GetResultType() const { return GetPackedType(); }

  DECLARE_INSTRUCTION(VecCnv);

 private:
  DISALLOW_COPY_AND_ASSIGN(HVecCnv);
};

class HVecNeg FINAL : public HVecUnaryOperation {
 public:
  HVecNeg(ArenaAllocator* arena,
          HInstruction* input,
          Primitive::Type packed_type,
          size_t vector_length,
          uint32_t dex_pc = kNoDexPc)
      : HVecUnaryOperation(arena, packed_type, vector_length, dex_pc) {
    DCHECK(input->IsVecOperation());
    SetRawInputAt(0, input);
  }
  DECLARE_INSTRUCTION(VecNeg);
 private:
  DISALLOW_COPY_AND_ASSIGN(HVecNeg);
};

class HVecAdd FINAL : public HVecBinaryOperation {
 public:
  HVecAdd(ArenaAllocator* arena,
          HInstruction* left,
          HInstruction* right,
          Primitive::Type packed_type,
          size_t vector_length,
          uint32_t dex_pc = kNoDexPc)
      : HVecBinaryOperation(arena, packed_type, vector_length, dex_pc) {
    DCHECK(left->IsVecOperation() && right->IsVecOperation());
    SetRawInputAt(0, left);
    SetRawInputAt(1, right);
  }
  DECLARE_INSTRUCTION(VecAdd);
 private:
  DISALLOW_COPY_AND_ASSIGN(HVecAdd);
};

class HVecSub FINAL : public HVecBinaryOperation {
 public:
  HVecSub(ArenaAllocator* arena,
          HInstruction* left,
          HInstruction* right,
          Primitive::Type packed_type,
          size_t vector_length,
          uint32_t dex_pc = kNoDexPc)
      : HVecBinaryOperation(arena, packed_type, vector_length, dex_pc) {
    DCHECK(left->IsVecOperation() && right->IsVecOperation());
    SetRawInputAt(0, left);
    SetRawInputAt(1, right);
  }
  DECLARE_INSTRUCTION(VecSub);
 private:
  DISALLOW_COPY_AND_ASSIGN(HVecSub);
};

class HVecMul FINAL : public HVecBinaryOperation {
 public:
  HVecMul(ArenaAllocator* arena,
          HInstruction* left,
          HInstruction* right,
          Primitive::Type packed_type,
          size_t vector_length,
          uint32_t dex_pc = kNoDexPc)
      : HVecBinaryOperation(arena, packed_type, vector_length, dex_pc) {
    DCHECK(left->IsVecOperation() && right->IsVecOperation());
    SetRawInputAt(0, left);
    SetRawInputAt(1, right);
  }
  DECLARE_INSTRUCTION(VecMul);
 private:
  DISALLOW_COPY_AND_ASSIGN(HVecMul);
};

class HVecDiv FINAL : public HVecBinaryOperation {
 public:
  HVecDiv(ArenaAllocator* arena,
          HInstruction* left,
          HInstruction* right,
          Primitive::Type packed_type,
          size_t vector_length,
          uint32_t dex_pc = kNoDexPc)
      : HVecBinaryOperation(arena, packed_type, vector_length, dex_pc) {
    DCHECK(left->IsVecOperation() && right->IsVecOperation());
    SetRawInputAt(0, left);
    SetRawInputAt(1, right);
  }
  DECLARE_INSTRUCTION(VecDiv);
 private:
  DISALLOW_COPY_AND_ASSIGN(HVecDiv);
};

class HVecAnd FINAL : public HVecBinaryOperation {
 public:
  HVecAnd(ArenaAllocator* arena,
          HInstruction* left,
          HInstruction* right,
          Primitive::Type packed_type,
          size_t vector_length,
          uint32_t dex_pc = kNoDexPc)
      : HVecBinaryOperation(arena, packed_type, vector_length, dex_pc) {
    DCHECK(left->IsVecOperation() && right->IsVecOperation());
    SetRawInputAt(0, left);
    SetRawInputAt(1, right);
  }
  DECLARE_INSTRUCTION(VecAnd);
 private:
  DISALLOW_COPY_AND_ASSIGN(HVecAnd);
};

class HVecOr FINAL : public HVecBinaryOperation {
 public:
  HVecOr(ArenaAllocator* arena,
         HInstruction* left,
         HInstruction* right,
         Primitive::Type packed_type,
         size_t vector_length,
         uint32_t dex_pc = kNoDexPc)
      : HVecBinaryOperation(arena, packed_type, vector_length, dex_pc) {
    DCHECK(left->IsVecOperation() && right->IsVecOperation());
    SetRawInputAt(0, left);
    SetRawInputAt(1, right);
  }
  DECLARE_INSTRUCTION(VecOr);
 private:
  DISALLOW_COPY_AND_ASSIGN(HVecOr);
};

class HVecXor FINAL : public HVecBinaryOperation {
 public:
  HVecXor(ArenaAllocator* arena,
          HInstruction* left,
          HInstruction* right,
          Primitive::Type packed_type,
          size_t vector_length,
          uint32_t dex_pc = kNoDexPc)
      : HVecBinaryOperation(arena, packed_type, vector_length, dex_pc) {
    DCHECK(left->IsVecOperation() && right->IsVecOperation());
    SetRawInputAt(0, left);
    SetRawInputAt(1, right);
  }
  DECLARE_INSTRUCTION(VecXor);
 private:
  DISALLOW_COPY_AND_ASSIGN(HVecXor);
};

class HVecShl FINAL : public HVecBinaryOperation {
 public:
  HVecShl(ArenaAllocator* arena,
          HInstruction* left,
          HInstruction* right,
          Primitive::Type packed_type,
          size_t vector_length,
          uint32_t dex_pc = kNoDexPc)
      : HVecBinaryOperation(arena, packed_type, vector_length, dex_pc) {
    DCHECK(left->IsVecOperation());
    SetRawInputAt(0, left);
    SetRawInputAt(1, right);
  }
  DECLARE_INSTRUCTION(VecShl);
 private:
  DISALLOW_COPY_AND_ASSIGN(HVecShl);
};

class HVecShr FINAL : public HVecBinaryOperation {
 public:
  HVecShr(ArenaAllocator* arena,
          HInstruction* left,
          HInstruction* right,
          Primitive::Type packed_type,
          size_t vector_length,
          uint32_t dex_pc = kNoDexPc)
      : HVecBinaryOperation(arena, packed_type, vector_length, dex_pc) {
    DCHECK(left->IsVecOperation());
    SetRawInputAt(0, left);
    SetRawInputAt(1, right);
  }
  DECLARE_INSTRUCTION(VecShr);
 private:
  DISALLOW_COPY_AND_ASSIGN(HVecShr);
};

class HVecUShr FINAL : public HVecBinaryOperation {
 public:
  HVecUShr(ArenaAllocator* arena,
           HInstruction* left,
           HInstruction* right,
           Primitive::Type packed_type,
           size_t vector_length,
           uint32_t dex_pc = kNoDexPc)
      : HVecBinaryOperation(arena, packed_type, vector_length, dex_pc) {
    DCHECK(left->IsVecOperation());
    SetRawInputAt(0, left);
    SetRawInputAt(1, right);
  }
  DECLARE_INSTRUCTION(VecUShr);
 private:
  DISALLOW_COPY_AND_ASSIGN(HVecUShr);
};

class HVecLoad FINAL : public HVecMemoryOperation {
 public:
  HVecLoad(ArenaAllocator* arena,
           HInstruction* base,
           HInstruction* index,
           Primitive::Type packed_type,
           size_t vector_length,
           uint32_t dex_pc = kNoDexPc)
      : HVecMemoryOperation(arena,
                            packed_type,
                            SideEffects::ArrayReadOfType(packed_type),
                            /*number_of_inputs*/ 2,
                            vector_length,
                            dex_pc) {
    SetRawInputAt(0, base);
    SetRawInputAt(1, index);
  }
  DECLARE_INSTRUCTION(VecLoad);
 private:
  DISALLOW_COPY_AND_ASSIGN(HVecLoad);
};

class HVecStore FINAL : public HVecMemoryOperation {
 public:
  HVecStore(ArenaAllocator* arena,
            HInstruction* base,
            HInstruction* index,
            HInstruction* value,
            Primitive::Type packed_type,
            size_t vector_length,
            uint32_t dex_pc = kNoDexPc)
      : HVecMemoryOperation(arena,
                            packed_type,
                            SideEffects::ArrayWriteOfType(packed_type),
                            /*number_of_inputs*/ 3,
                            vector_length,
                            dex_pc) {
    DCHECK(value->IsVecOperation());
    SetRawInputAt(0, base);
    SetRawInputAt(1, index);
    SetRawInputAt(2, value);
  }
  DECLARE_INSTRUCTION(VecStore);
 private:
  DISALLOW_COPY_AND_ASSIGN(HVecStore);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_NODES_VECTOR_H_
