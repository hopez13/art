/*
 * Copyright (C) 2024 The Android Open Source Project
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

#ifndef ART_COMPILER_OPTIMIZING_NODES_RISCV64_H_
#define ART_COMPILER_OPTIMIZING_NODES_RISCV64_H_

namespace art HIDDEN {

class HRiscv64ShiftAdd final : public HBinaryOperation {
 public:
  HRiscv64ShiftAdd(DataType::Type result_type,
                   HInstruction* left,
                   HInstruction* right,
                   uint32_t distance,
                   uint32_t dex_pc = kNoDexPc)
      : HBinaryOperation(kRiscv64ShiftAdd, result_type, left, right, SideEffects::None(), dex_pc),
        distance_(distance) {
    DCHECK_GE(distance, 1u);
    DCHECK_LE(distance, 3u);
  }

  uint32_t GetDistance() const { return distance_; }

  bool IsCommutative() const override { return false; }

  template <typename T>
  T Compute(T x, T y) const {
    return y + (x << distance_);
  }

  HConstant* Evaluate(HIntConstant* x, HIntConstant* y) const override {
    return GetBlock()->GetGraph()->GetIntConstant(Compute(x->GetValue(), y->GetValue()),
                                                  GetDexPc());
  }

  HConstant* Evaluate(HLongConstant* x, HLongConstant* y) const override {
    return GetBlock()->GetGraph()->GetLongConstant(Compute(x->GetValue(), y->GetValue()),
                                                   GetDexPc());
  }

  HConstant* Evaluate([[maybe_unused]] HFloatConstant* x,
                      [[maybe_unused]] HFloatConstant* y) const override {
    LOG(FATAL) << DebugName() << " is not defined for float values";
    UNREACHABLE();
  }

  HConstant* Evaluate([[maybe_unused]] HDoubleConstant* x,
                      [[maybe_unused]] HDoubleConstant* y) const override {
    LOG(FATAL) << DebugName() << " is not defined for double values";
    UNREACHABLE();
  }

 private:
  HRiscv64ShiftAdd& operator=(const HRiscv64ShiftAdd&) = delete;

 public:
  const char* DebugName() const override {
    constexpr const char* names[] = {
        nullptr, "Riscv64Shift1Add", "Riscv64Shift2Add", "Riscv64Shift3Add"
    };
    return names[distance_];
  }

  HInstruction* Clone(ArenaAllocator* arena) const override {
    DCHECK(IsClonable());
    return new (arena) HRiscv64ShiftAdd(*this);
  }
  void Accept(HGraphVisitor* visitor) override;

 protected:
  DEFAULT_COPY_CONSTRUCTOR(Riscv64ShiftAdd);

 private:
  const uint32_t distance_{UINT32_MAX};
};

}  // namespace art HIDDEN

#endif  // ART_COMPILER_OPTIMIZING_NODES_RISCV64_H_
