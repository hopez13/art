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

#include "managed_register_riscv64.h"

#include "base/globals.h"
#include "gtest/gtest.h"

namespace art HIDDEN {
namespace riscv64 {

// riscv vector registers are not supported now

TEST(Riscv64ManagedRegister, NoRegister) {
  Riscv64ManagedRegister reg = ManagedRegister::NoRegister().AsRiscv64();
  EXPECT_TRUE(reg.IsNoRegister());
  // EXPECT_FALSE(reg.Overlaps(reg));
}

TEST(Riscv64ManagedRegister, GpuRegister) {
  Riscv64ManagedRegister reg = Riscv64ManagedRegister::FromGpuRegister(Zero);
  EXPECT_FALSE(reg.IsNoRegister());
  EXPECT_TRUE(reg.IsGpuRegister());
  EXPECT_FALSE(reg.IsFpuRegister());
  // EXPECT_FALSE(reg.IsVectorRegister());
  EXPECT_EQ(Zero, reg.AsGpuRegister());

  reg = Riscv64ManagedRegister::FromGpuRegister(TP);
  EXPECT_FALSE(reg.IsNoRegister());
  EXPECT_TRUE(reg.IsGpuRegister());
  EXPECT_FALSE(reg.IsFpuRegister());
  // EXPECT_FALSE(reg.IsVectorRegister());
  EXPECT_EQ(TP, reg.AsGpuRegister());

  reg = Riscv64ManagedRegister::FromGpuRegister(A2);
  EXPECT_FALSE(reg.IsNoRegister());
  EXPECT_TRUE(reg.IsGpuRegister());
  EXPECT_FALSE(reg.IsFpuRegister());
  // EXPECT_FALSE(reg.IsVectorRegister());
  EXPECT_EQ(A2, reg.AsGpuRegister());

  reg = Riscv64ManagedRegister::FromGpuRegister(A0);
  EXPECT_FALSE(reg.IsNoRegister());
  EXPECT_TRUE(reg.IsGpuRegister());
  EXPECT_FALSE(reg.IsFpuRegister());
  // EXPECT_FALSE(reg.IsVectorRegister());
  EXPECT_EQ(A0, reg.AsGpuRegister());

  reg = Riscv64ManagedRegister::FromGpuRegister(A7);
  EXPECT_FALSE(reg.IsNoRegister());
  EXPECT_TRUE(reg.IsGpuRegister());
  EXPECT_FALSE(reg.IsFpuRegister());
  // EXPECT_FALSE(reg.IsVectorRegister());
  EXPECT_EQ(A7, reg.AsGpuRegister());

  reg = Riscv64ManagedRegister::FromGpuRegister(T0);
  EXPECT_FALSE(reg.IsNoRegister());
  EXPECT_TRUE(reg.IsGpuRegister());
  EXPECT_FALSE(reg.IsFpuRegister());
  // EXPECT_FALSE(reg.IsVectorRegister());
  EXPECT_EQ(T0, reg.AsGpuRegister());

  reg = Riscv64ManagedRegister::FromGpuRegister(T3);
  EXPECT_FALSE(reg.IsNoRegister());
  EXPECT_TRUE(reg.IsGpuRegister());
  EXPECT_FALSE(reg.IsFpuRegister());
  // EXPECT_FALSE(reg.IsVectorRegister());
  EXPECT_EQ(T3, reg.AsGpuRegister());

  reg = Riscv64ManagedRegister::FromGpuRegister(S0);
  EXPECT_FALSE(reg.IsNoRegister());
  EXPECT_TRUE(reg.IsGpuRegister());
  EXPECT_FALSE(reg.IsFpuRegister());
  // EXPECT_FALSE(reg.IsVectorRegister());
  EXPECT_EQ(S0, reg.AsGpuRegister());

  reg = Riscv64ManagedRegister::FromGpuRegister(GP);
  EXPECT_FALSE(reg.IsNoRegister());
  EXPECT_TRUE(reg.IsGpuRegister());
  EXPECT_FALSE(reg.IsFpuRegister());
  // EXPECT_FALSE(reg.IsVectorRegister());
  EXPECT_EQ(GP, reg.AsGpuRegister());

  reg = Riscv64ManagedRegister::FromGpuRegister(SP);
  EXPECT_FALSE(reg.IsNoRegister());
  EXPECT_TRUE(reg.IsGpuRegister());
  EXPECT_FALSE(reg.IsFpuRegister());
  // EXPECT_FALSE(reg.IsVectorRegister());
  EXPECT_EQ(SP, reg.AsGpuRegister());

  reg = Riscv64ManagedRegister::FromGpuRegister(RA);
  EXPECT_FALSE(reg.IsNoRegister());
  EXPECT_TRUE(reg.IsGpuRegister());
  EXPECT_FALSE(reg.IsFpuRegister());
  // EXPECT_FALSE(reg.IsVectorRegister());
  EXPECT_EQ(RA, reg.AsGpuRegister());
}

TEST(Riscv64ManagedRegister, FpuRegister) {
  Riscv64ManagedRegister reg = Riscv64ManagedRegister::FromFpuRegister(F0);
  // Riscv64ManagedRegister vreg = Riscv64ManagedRegister::FromVectorRegister(W0);
  EXPECT_FALSE(reg.IsNoRegister());
  EXPECT_FALSE(reg.IsGpuRegister());
  EXPECT_TRUE(reg.IsFpuRegister());
  // EXPECT_FALSE(reg.IsVectorRegister());
  // EXPECT_TRUE(reg.Overlaps(vreg));
  EXPECT_EQ(F0, reg.AsFpuRegister());
  // EXPECT_EQ(W0, reg.AsOverlappingVectorRegister());
  EXPECT_TRUE(reg.Equals(Riscv64ManagedRegister::FromFpuRegister(F0)));

  reg = Riscv64ManagedRegister::FromFpuRegister(F1);
  // vreg = Riscv64ManagedRegister::FromVectorRegister(W1);
  EXPECT_FALSE(reg.IsNoRegister());
  EXPECT_FALSE(reg.IsGpuRegister());
  EXPECT_TRUE(reg.IsFpuRegister());
  // EXPECT_FALSE(reg.IsVectorRegister());
  // EXPECT_TRUE(reg.Overlaps(vreg));
  EXPECT_EQ(F1, reg.AsFpuRegister());
  // EXPECT_EQ(W1, reg.AsOverlappingVectorRegister());
  EXPECT_TRUE(reg.Equals(Riscv64ManagedRegister::FromFpuRegister(F1)));

  reg = Riscv64ManagedRegister::FromFpuRegister(F20);
  // vreg = Riscv64ManagedRegister::FromVectorRegister(W20);
  EXPECT_FALSE(reg.IsNoRegister());
  EXPECT_FALSE(reg.IsGpuRegister());
  EXPECT_TRUE(reg.IsFpuRegister());
  // EXPECT_FALSE(reg.IsVectorRegister());
  // EXPECT_TRUE(reg.Overlaps(vreg));
  EXPECT_EQ(F20, reg.AsFpuRegister());
  // EXPECT_EQ(W20, reg.AsOverlappingVectorRegister());
  EXPECT_TRUE(reg.Equals(Riscv64ManagedRegister::FromFpuRegister(F20)));

  reg = Riscv64ManagedRegister::FromFpuRegister(F31);
  // vreg = Riscv64ManagedRegister::FromVectorRegister(W31);
  EXPECT_FALSE(reg.IsNoRegister());
  EXPECT_FALSE(reg.IsGpuRegister());
  EXPECT_TRUE(reg.IsFpuRegister());
  // EXPECT_FALSE(reg.IsVectorRegister());
  // EXPECT_TRUE(reg.Overlaps(vreg));
  EXPECT_EQ(F31, reg.AsFpuRegister());
  // EXPECT_EQ(W31, reg.AsOverlappingVectorRegister());
  EXPECT_TRUE(reg.Equals(Riscv64ManagedRegister::FromFpuRegister(F31)));
}

// TEST(Riscv64ManagedRegister, VectorRegister) {
//   Riscv64ManagedRegister reg = Riscv64ManagedRegister::FromVectorRegister(W0);
//   Riscv64ManagedRegister freg = Riscv64ManagedRegister::FromFpuRegister(F0);
//   EXPECT_FALSE(reg.IsNoRegister());
//   EXPECT_FALSE(reg.IsGpuRegister());
//   EXPECT_FALSE(reg.IsFpuRegister());
//  // EXPECT_TRUE(reg.IsVectorRegister());
//   EXPECT_TRUE(reg.Overlaps(freg));
//   EXPECT_EQ(W0, reg.AsVectorRegister());
//   EXPECT_EQ(F0, reg.AsOverlappingFpuRegister());
//   EXPECT_TRUE(reg.Equals(Riscv64ManagedRegister::FromVectorRegister(W0)));

//   reg = Riscv64ManagedRegister::FromVectorRegister(W2);
//   freg = Riscv64ManagedRegister::FromFpuRegister(F2);
//   EXPECT_FALSE(reg.IsNoRegister());
//   EXPECT_FALSE(reg.IsGpuRegister());
//   EXPECT_FALSE(reg.IsFpuRegister());
//   EXPECT_TRUE(reg.IsVectorRegister());
//   EXPECT_TRUE(reg.Overlaps(freg));
//   EXPECT_EQ(W2, reg.AsVectorRegister());
//   EXPECT_EQ(F2, reg.AsOverlappingFpuRegister());
//   EXPECT_TRUE(reg.Equals(Riscv64ManagedRegister::FromVectorRegister(W2)));

//   reg = Riscv64ManagedRegister::FromVectorRegister(W13);
//   freg = Riscv64ManagedRegister::FromFpuRegister(F13);
//   EXPECT_FALSE(reg.IsNoRegister());
//   EXPECT_FALSE(reg.IsGpuRegister());
//   EXPECT_FALSE(reg.IsFpuRegister());
//   EXPECT_TRUE(reg.IsVectorRegister());
//   EXPECT_TRUE(reg.Overlaps(freg));
//   EXPECT_EQ(W13, reg.AsVectorRegister());
//   EXPECT_EQ(F13, reg.AsOverlappingFpuRegister());
//   EXPECT_TRUE(reg.Equals(Riscv64ManagedRegister::FromVectorRegister(W13)));

//   reg = Riscv64ManagedRegister::FromVectorRegister(W29);
//   freg = Riscv64ManagedRegister::FromFpuRegister(F29);
//   EXPECT_FALSE(reg.IsNoRegister());
//   EXPECT_FALSE(reg.IsGpuRegister());
//   EXPECT_FALSE(reg.IsFpuRegister());
//   EXPECT_TRUE(reg.IsVectorRegister());
//   EXPECT_TRUE(reg.Overlaps(freg));
//   EXPECT_EQ(W29, reg.AsVectorRegister());
//   EXPECT_EQ(F29, reg.AsOverlappingFpuRegister());
//   EXPECT_TRUE(reg.Equals(Riscv64ManagedRegister::FromVectorRegister(W29)));
// }

TEST(Riscv64ManagedRegister, Equals) {
  ManagedRegister no_reg = ManagedRegister::NoRegister();
  EXPECT_TRUE(no_reg.Equals(Riscv64ManagedRegister::NoRegister()));
  EXPECT_FALSE(no_reg.Equals(Riscv64ManagedRegister::FromGpuRegister(Zero)));
  EXPECT_FALSE(no_reg.Equals(Riscv64ManagedRegister::FromGpuRegister(A1)));
  EXPECT_FALSE(no_reg.Equals(Riscv64ManagedRegister::FromGpuRegister(S2)));
  EXPECT_FALSE(no_reg.Equals(Riscv64ManagedRegister::FromFpuRegister(F0)));
  // EXPECT_FALSE(no_reg.Equals(Riscv64ManagedRegister::FromVectorRegister(W0)));

  Riscv64ManagedRegister reg_ZERO = Riscv64ManagedRegister::FromGpuRegister(Zero);
  EXPECT_FALSE(reg_ZERO.Equals(Riscv64ManagedRegister::NoRegister()));
  EXPECT_TRUE(reg_ZERO.Equals(Riscv64ManagedRegister::FromGpuRegister(Zero)));
  EXPECT_FALSE(reg_ZERO.Equals(Riscv64ManagedRegister::FromGpuRegister(A1)));
  EXPECT_FALSE(reg_ZERO.Equals(Riscv64ManagedRegister::FromGpuRegister(S2)));
  EXPECT_FALSE(reg_ZERO.Equals(Riscv64ManagedRegister::FromFpuRegister(F0)));
  // EXPECT_FALSE(reg_ZERO.Equals(Riscv64ManagedRegister::FromVectorRegister(W0)));

  Riscv64ManagedRegister reg_A1 = Riscv64ManagedRegister::FromGpuRegister(A1);
  EXPECT_FALSE(reg_A1.Equals(Riscv64ManagedRegister::NoRegister()));
  EXPECT_FALSE(reg_A1.Equals(Riscv64ManagedRegister::FromGpuRegister(Zero)));
  EXPECT_FALSE(reg_A1.Equals(Riscv64ManagedRegister::FromGpuRegister(A0)));
  EXPECT_TRUE(reg_A1.Equals(Riscv64ManagedRegister::FromGpuRegister(A1)));
  EXPECT_FALSE(reg_A1.Equals(Riscv64ManagedRegister::FromGpuRegister(S2)));
  EXPECT_FALSE(reg_A1.Equals(Riscv64ManagedRegister::FromFpuRegister(F0)));
  // EXPECT_FALSE(reg_A1.Equals(Riscv64ManagedRegister::FromVectorRegister(W0)));

  Riscv64ManagedRegister reg_S2 = Riscv64ManagedRegister::FromGpuRegister(S2);
  EXPECT_FALSE(reg_S2.Equals(Riscv64ManagedRegister::NoRegister()));
  EXPECT_FALSE(reg_S2.Equals(Riscv64ManagedRegister::FromGpuRegister(Zero)));
  EXPECT_FALSE(reg_S2.Equals(Riscv64ManagedRegister::FromGpuRegister(A1)));
  EXPECT_FALSE(reg_S2.Equals(Riscv64ManagedRegister::FromGpuRegister(S1)));
  EXPECT_TRUE(reg_S2.Equals(Riscv64ManagedRegister::FromGpuRegister(S2)));
  EXPECT_FALSE(reg_S2.Equals(Riscv64ManagedRegister::FromFpuRegister(F0)));
  // EXPECT_FALSE(reg_S2.Equals(Riscv64ManagedRegister::FromVectorRegister(W0)));

  Riscv64ManagedRegister reg_F0 = Riscv64ManagedRegister::FromFpuRegister(F0);
  EXPECT_FALSE(reg_F0.Equals(Riscv64ManagedRegister::NoRegister()));
  EXPECT_FALSE(reg_F0.Equals(Riscv64ManagedRegister::FromGpuRegister(Zero)));
  EXPECT_FALSE(reg_F0.Equals(Riscv64ManagedRegister::FromGpuRegister(A1)));
  EXPECT_FALSE(reg_F0.Equals(Riscv64ManagedRegister::FromGpuRegister(S2)));
  EXPECT_TRUE(reg_F0.Equals(Riscv64ManagedRegister::FromFpuRegister(F0)));
  EXPECT_FALSE(reg_F0.Equals(Riscv64ManagedRegister::FromFpuRegister(F1)));
  EXPECT_FALSE(reg_F0.Equals(Riscv64ManagedRegister::FromFpuRegister(F31)));
  // EXPECT_FALSE(reg_F0.Equals(Riscv64ManagedRegister::FromVectorRegister(W0)));

  Riscv64ManagedRegister reg_F31 = Riscv64ManagedRegister::FromFpuRegister(F31);
  EXPECT_FALSE(reg_F31.Equals(Riscv64ManagedRegister::NoRegister()));
  EXPECT_FALSE(reg_F31.Equals(Riscv64ManagedRegister::FromGpuRegister(Zero)));
  EXPECT_FALSE(reg_F31.Equals(Riscv64ManagedRegister::FromGpuRegister(A1)));
  EXPECT_FALSE(reg_F31.Equals(Riscv64ManagedRegister::FromGpuRegister(S2)));
  EXPECT_FALSE(reg_F31.Equals(Riscv64ManagedRegister::FromFpuRegister(F0)));
  EXPECT_FALSE(reg_F31.Equals(Riscv64ManagedRegister::FromFpuRegister(F1)));
  EXPECT_TRUE(reg_F31.Equals(Riscv64ManagedRegister::FromFpuRegister(F31)));
  // EXPECT_FALSE(reg_F31.Equals(Riscv64ManagedRegister::FromVectorRegister(W0)));

  //   Riscv64ManagedRegister reg_W0 = Riscv64ManagedRegister::FromVectorRegister(W0);
  //   EXPECT_FALSE(reg_W0.Equals(Riscv64ManagedRegister::NoRegister()));
  //   EXPECT_FALSE(reg_W0.Equals(Riscv64ManagedRegister::FromGpuRegister(Zero)));
  //   EXPECT_FALSE(reg_W0.Equals(Riscv64ManagedRegister::FromGpuRegister(A1)));
  //   EXPECT_FALSE(reg_W0.Equals(Riscv64ManagedRegister::FromGpuRegister(S1)));
  //   EXPECT_FALSE(reg_W0.Equals(Riscv64ManagedRegister::FromFpuRegister(F0)));
  //   EXPECT_TRUE(reg_W0.Equals(Riscv64ManagedRegister::FromVectorRegister(W0)));
  //   EXPECT_FALSE(reg_W0.Equals(Riscv64ManagedRegister::FromVectorRegister(W1)));
  //   EXPECT_FALSE(reg_W0.Equals(Riscv64ManagedRegister::FromVectorRegister(W31)));

  //   Riscv64ManagedRegister reg_W31 = Riscv64ManagedRegister::FromVectorRegister(W31);
  //   EXPECT_FALSE(reg_W31.Equals(Riscv64ManagedRegister::NoRegister()));
  //   EXPECT_FALSE(reg_W31.Equals(Riscv64ManagedRegister::FromGpuRegister(Zero)));
  //   EXPECT_FALSE(reg_W31.Equals(Riscv64ManagedRegister::FromGpuRegister(A1)));
  //   EXPECT_FALSE(reg_W31.Equals(Riscv64ManagedRegister::FromGpuRegister(S1)));
  //   EXPECT_FALSE(reg_W31.Equals(Riscv64ManagedRegister::FromFpuRegister(F0)));
  //   EXPECT_FALSE(reg_W31.Equals(Riscv64ManagedRegister::FromVectorRegister(W0)));
  //   EXPECT_FALSE(reg_W31.Equals(Riscv64ManagedRegister::FromVectorRegister(W1)));
  //   EXPECT_TRUE(reg_W31.Equals(Riscv64ManagedRegister::FromVectorRegister(W31)));
}

TEST(Riscv64ManagedRegister, Overlaps) {
  Riscv64ManagedRegister reg = Riscv64ManagedRegister::FromFpuRegister(F0);
  //   Riscv64ManagedRegister reg_o = Riscv64ManagedRegister::FromVectorRegister(W0);
  //   EXPECT_TRUE(reg.Overlaps(reg_o));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromGpuRegister(Zero)));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromGpuRegister(A0)));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromGpuRegister(S0)));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromGpuRegister(RA)));
  //   EXPECT_EQ(F0, reg_o.AsOverlappingFpuRegister());
  //   EXPECT_EQ(W0, reg.AsOverlappingVectorRegister());
  EXPECT_TRUE(reg.Overlaps(Riscv64ManagedRegister::FromFpuRegister(F0)));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromFpuRegister(F4)));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromFpuRegister(F16)));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromFpuRegister(F31)));
  //  EXPECT_TRUE(reg.Overlaps(Riscv64ManagedRegister::FromVectorRegister(W0)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromVectorRegister(W4)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromVectorRegister(W16)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromVectorRegister(W31)));

  reg = Riscv64ManagedRegister::FromFpuRegister(F4);
  //   reg_o = Riscv64ManagedRegister::FromVectorRegister(W4);
  //   EXPECT_TRUE(reg.Overlaps(reg_o));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromGpuRegister(Zero)));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromGpuRegister(A0)));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromGpuRegister(S0)));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromGpuRegister(RA)));
  //   EXPECT_EQ(F4, reg_o.AsOverlappingFpuRegister());
  //   EXPECT_EQ(W4, reg.AsOverlappingVectorRegister());
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromFpuRegister(F0)));
  EXPECT_TRUE(reg.Overlaps(Riscv64ManagedRegister::FromFpuRegister(F4)));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromFpuRegister(F16)));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromFpuRegister(F31)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromVectorRegister(W0)));
  //   EXPECT_TRUE(reg.Overlaps(Riscv64ManagedRegister::FromVectorRegister(W4)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromVectorRegister(W16)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromVectorRegister(W31)));

  reg = Riscv64ManagedRegister::FromFpuRegister(F16);
  //   reg_o = Riscv64ManagedRegister::FromVectorRegister(W16);
  //   EXPECT_TRUE(reg.Overlaps(reg_o));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromGpuRegister(Zero)));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromGpuRegister(A0)));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromGpuRegister(S0)));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromGpuRegister(RA)));
  //   EXPECT_EQ(F16, reg_o.AsOverlappingFpuRegister());
  //   EXPECT_EQ(W16, reg.AsOverlappingVectorRegister());
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromFpuRegister(F0)));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromFpuRegister(F4)));
  EXPECT_TRUE(reg.Overlaps(Riscv64ManagedRegister::FromFpuRegister(F16)));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromFpuRegister(F31)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromVectorRegister(W0)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromVectorRegister(W4)));
  //   EXPECT_TRUE(reg.Overlaps(Riscv64ManagedRegister::FromVectorRegister(W16)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromVectorRegister(W31)));

  reg = Riscv64ManagedRegister::FromFpuRegister(F31);
  //   reg_o = Riscv64ManagedRegister::FromVectorRegister(W31);
  //   EXPECT_TRUE(reg.Overlaps(reg_o));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromGpuRegister(Zero)));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromGpuRegister(A0)));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromGpuRegister(S0)));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromGpuRegister(RA)));
  //   EXPECT_EQ(F31, reg_o.AsOverlappingFpuRegister());
  //   EXPECT_EQ(W31, reg.AsOverlappingVectorRegister());
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromFpuRegister(F0)));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromFpuRegister(F4)));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromFpuRegister(F16)));
  EXPECT_TRUE(reg.Overlaps(Riscv64ManagedRegister::FromFpuRegister(F31)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromVectorRegister(W0)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromVectorRegister(W4)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromVectorRegister(W16)));
  //   EXPECT_TRUE(reg.Overlaps(Riscv64ManagedRegister::FromVectorRegister(W31)));

  //   reg = Riscv64ManagedRegister::FromVectorRegister(W0);
  //   reg_o = Riscv64ManagedRegister::FromFpuRegister(F0);
  //   EXPECT_TRUE(reg.Overlaps(reg_o));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromGpuRegister(Zero)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromGpuRegister(A0)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromGpuRegister(S0)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromGpuRegister(RA)));
  //   EXPECT_EQ(W0, reg_o.AsOverlappingVectorRegister());
  //   EXPECT_EQ(F0, reg.AsOverlappingFpuRegister());
  //   EXPECT_TRUE(reg.Overlaps(Riscv64ManagedRegister::FromFpuRegister(F0)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromFpuRegister(F4)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromFpuRegister(F16)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromFpuRegister(F31)));
  //   EXPECT_TRUE(reg.Overlaps(Riscv64ManagedRegister::FromVectorRegister(W0)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromVectorRegister(W4)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromVectorRegister(W16)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromVectorRegister(W31)));

  //   reg = Riscv64ManagedRegister::FromVectorRegister(W4);
  //   reg_o = Riscv64ManagedRegister::FromFpuRegister(F4);
  //   EXPECT_TRUE(reg.Overlaps(reg_o));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromGpuRegister(Zero)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromGpuRegister(A0)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromGpuRegister(S0)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromGpuRegister(RA)));
  //   EXPECT_EQ(W4, reg_o.AsOverlappingVectorRegister());
  //   EXPECT_EQ(F4, reg.AsOverlappingFpuRegister());
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromFpuRegister(F0)));
  //   EXPECT_TRUE(reg.Overlaps(Riscv64ManagedRegister::FromFpuRegister(F4)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromFpuRegister(F16)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromFpuRegister(F31)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromVectorRegister(W0)));
  //   EXPECT_TRUE(reg.Overlaps(Riscv64ManagedRegister::FromVectorRegister(W4)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromVectorRegister(W16)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromVectorRegister(W31)));

  //   reg = Riscv64ManagedRegister::FromVectorRegister(W16);
  //   reg_o = Riscv64ManagedRegister::FromFpuRegister(F16);
  //   EXPECT_TRUE(reg.Overlaps(reg_o));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromGpuRegister(Zero)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromGpuRegister(A0)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromGpuRegister(S0)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromGpuRegister(RA)));
  //   EXPECT_EQ(W16, reg_o.AsOverlappingVectorRegister());
  //   EXPECT_EQ(F16, reg.AsOverlappingFpuRegister());
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromFpuRegister(F0)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromFpuRegister(F4)));
  //   EXPECT_TRUE(reg.Overlaps(Riscv64ManagedRegister::FromFpuRegister(F16)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromFpuRegister(F31)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromVectorRegister(W0)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromVectorRegister(W4)));
  //   EXPECT_TRUE(reg.Overlaps(Riscv64ManagedRegister::FromVectorRegister(W16)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromVectorRegister(W31)));

  //   reg = Riscv64ManagedRegister::FromVectorRegister(W31);
  //   reg_o = Riscv64ManagedRegister::FromFpuRegister(F31);
  //   EXPECT_TRUE(reg.Overlaps(reg_o));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromGpuRegister(Zero)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromGpuRegister(A0)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromGpuRegister(S0)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromGpuRegister(RA)));
  //   EXPECT_EQ(W31, reg_o.AsOverlappingVectorRegister());
  //   EXPECT_EQ(F31, reg.AsOverlappingFpuRegister());
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromFpuRegister(F0)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromFpuRegister(F4)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromFpuRegister(F16)));
  //   EXPECT_TRUE(reg.Overlaps(Riscv64ManagedRegister::FromFpuRegister(F31)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromVectorRegister(W0)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromVectorRegister(W4)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromVectorRegister(W16)));
  //   EXPECT_TRUE(reg.Overlaps(Riscv64ManagedRegister::FromVectorRegister(W31)));

  reg = Riscv64ManagedRegister::FromGpuRegister(Zero);
  EXPECT_TRUE(reg.Overlaps(Riscv64ManagedRegister::FromGpuRegister(Zero)));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromGpuRegister(A0)));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromGpuRegister(S0)));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromGpuRegister(RA)));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromFpuRegister(F0)));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromFpuRegister(F4)));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromFpuRegister(F16)));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromFpuRegister(F31)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromVectorRegister(W0)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromVectorRegister(W4)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromVectorRegister(W16)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromVectorRegister(W31)));

  reg = Riscv64ManagedRegister::FromGpuRegister(A0);
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromGpuRegister(Zero)));
  EXPECT_TRUE(reg.Overlaps(Riscv64ManagedRegister::FromGpuRegister(A0)));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromGpuRegister(S0)));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromGpuRegister(RA)));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromFpuRegister(F0)));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromFpuRegister(F4)));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromFpuRegister(F16)));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromFpuRegister(F31)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromVectorRegister(W0)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromVectorRegister(W4)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromVectorRegister(W16)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromVectorRegister(W31)));

  reg = Riscv64ManagedRegister::FromGpuRegister(S0);
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromGpuRegister(Zero)));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromGpuRegister(A0)));
  EXPECT_TRUE(reg.Overlaps(Riscv64ManagedRegister::FromGpuRegister(S0)));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromGpuRegister(RA)));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromFpuRegister(F0)));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromFpuRegister(F4)));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromFpuRegister(F16)));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromFpuRegister(F31)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromVectorRegister(W0)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromVectorRegister(W4)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromVectorRegister(W16)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromVectorRegister(W31)));

  reg = Riscv64ManagedRegister::FromGpuRegister(RA);
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromGpuRegister(Zero)));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromGpuRegister(A0)));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromGpuRegister(S0)));
  EXPECT_TRUE(reg.Overlaps(Riscv64ManagedRegister::FromGpuRegister(RA)));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromFpuRegister(F0)));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromFpuRegister(F4)));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromFpuRegister(F16)));
  EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromFpuRegister(F31)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromVectorRegister(W0)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromVectorRegister(W4)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromVectorRegister(W16)));
  //   EXPECT_FALSE(reg.Overlaps(Riscv64ManagedRegister::FromVectorRegister(W31)));
}

}  // namespace riscv64
}  // namespace art
