/*
 * Copyright (C) 2023 The Android Open Source Project
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

#ifndef ART_COMPILER_UTILS_RISCV64_MANAGED_REGISTER_RISCV64_H_
#define ART_COMPILER_UTILS_RISCV64_MANAGED_REGISTER_RISCV64_H_

#include "constants_riscv64.h"
#include "utils/managed_register.h"

namespace art HIDDEN {
namespace riscv64 {

const int kNumberOfGpuRegIds = kNumberOfXRegisters;
const int kNumberOfGpuAllocIds = kNumberOfXRegisters;

const int kNumberOfFpuRegIds = kNumberOfFRegisters;
const int kNumberOfFpuAllocIds = kNumberOfFRegisters;
// TODO: riscv didn't support the Vector
// const int kNumberOfVecRegIds = kNumberOfVectorRegisters;
// const int kNumberOfVecAllocIds = kNumberOfVectorRegisters;

const int kNumberOfRegIds = kNumberOfGpuRegIds + kNumberOfFpuRegIds;  // + kNumberOfVecRegIds;
const int kNumberOfAllocIds =
    kNumberOfGpuAllocIds + kNumberOfFpuAllocIds;  // + kNumberOfVecAllocIds;

// Register ids map:
//   [0..R[  core registers (enum XRegister)
//   [R..F[  floating-point registers (enum FRegister)
//   [F..W[  MSA vector registers (enum VectorRegister)
// where
//   R = kNumberOfGpuRegIds
//   F = R + kNumberOfFpuRegIds
//   W = F + kNumberOfVecRegIds

// An instance of class 'ManagedRegister' represents a single Riscv64 register.
// A register can be one of the following:
//  * core register (enum XRegister)
//  * floating-point register (enum FRegister)
//  * MSA vector register (enum VectorRegister)
//
// 'ManagedRegister::NoRegister()' provides an invalid register.
// There is a one-to-one mapping between ManagedRegister and register id.
class Riscv64ManagedRegister : public ManagedRegister {
 public:
  constexpr XRegister AsGpuRegister() const {
    CHECK(IsGpuRegister());
    return static_cast<XRegister>(id_);
  }

  constexpr FRegister AsFpuRegister() const {
    CHECK(IsFpuRegister());
    return static_cast<FRegister>(id_ - kNumberOfGpuRegIds);
  }
  /*
    constexpr VectorRegister AsVectorRegister() const {
      CHECK(IsVectorRegister());
      return static_cast<VectorRegister>(id_ - (kNumberOfGpuRegIds + kNumberOfFRegisters));
    }

    constexpr FRegister AsOverlappingFpuRegister() const {
      CHECK(IsValidManagedRegister());
      return static_cast<FRegister>(AsVectorRegister());
    }

    constexpr VectorRegister AsOverlappingVectorRegister() const {
      CHECK(IsValidManagedRegister());
      return static_cast<VectorRegister>(AsFpuRegister());
    }
  */
  constexpr bool IsGpuRegister() const {
    CHECK(IsValidManagedRegister());
    return (0 <= id_) && (id_ < kNumberOfGpuRegIds);
  }

  constexpr bool IsFpuRegister() const {
    CHECK(IsValidManagedRegister());
    const int test = id_ - kNumberOfGpuRegIds;
    return (0 <= test) && (test < kNumberOfFpuRegIds);
  }
  /*
    constexpr bool IsVectorRegister() const {
      CHECK(IsValidManagedRegister());
      const int test = id_ - (kNumberOfGpuRegIds + kNumberOfFpuRegIds);
      return (0 <= test) && (test < kNumberOfVecRegIds);
    }
  */
  void Print(std::ostream& os) const;

  // Returns true if the two managed-registers ('this' and 'other') overlap.
  // Either managed-register may be the NoRegister. If both are the NoRegister
  // then false is returned.
  bool Overlaps(const Riscv64ManagedRegister& other) const;

  static constexpr Riscv64ManagedRegister FromGpuRegister(XRegister r) {
    CHECK_NE(r, kNoXRegister);
    return FromRegId(r);
  }

  static constexpr Riscv64ManagedRegister FromFpuRegister(FRegister r) {
    CHECK_NE(r, kNoFRegister);
    return FromRegId(r + kNumberOfGpuRegIds);
  }
  /*
    static constexpr Riscv64ManagedRegister FromVectorRegister(VectorRegister r) {
      CHECK_NE(r, kNoVectorRegister);
      return FromRegId(r + kNumberOfGpuRegIds + kNumberOfFpuRegIds);
    }
  */
 private:
  constexpr bool IsValidManagedRegister() const { return (0 <= id_) && (id_ < kNumberOfRegIds); }

  constexpr int RegId() const {
    CHECK(!IsNoRegister());
    return id_;
  }

  int AllocId() const {
    CHECK(IsValidManagedRegister());
    CHECK_LT(id_, kNumberOfAllocIds);
    return id_;
  }

  friend class ManagedRegister;

  explicit constexpr Riscv64ManagedRegister(int reg_id) : ManagedRegister(reg_id) {}

  static constexpr Riscv64ManagedRegister FromRegId(int reg_id) {
    Riscv64ManagedRegister reg(reg_id);
    CHECK(reg.IsValidManagedRegister());
    return reg;
  }
};

std::ostream& operator<<(std::ostream& os, const Riscv64ManagedRegister& reg);

}  // namespace riscv64

constexpr inline riscv64::Riscv64ManagedRegister ManagedRegister::AsRiscv64() const {
  riscv64::Riscv64ManagedRegister reg(id_);
  CHECK(reg.IsNoRegister() || reg.IsValidManagedRegister());
  return reg;
}

}  // namespace art

#endif  // ART_COMPILER_UTILS_RISCV64_MANAGED_REGISTER_RISCV64_H_
