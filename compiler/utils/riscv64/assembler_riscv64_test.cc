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

#include "assembler_riscv64.h"

#include <inttypes.h>

#include <map>
#include <random>

#include "base/bit_utils.h"
#include "base/malloc_arena_pool.h"
#include "base/stl_util.h"
#include "utils/assembler_test.h"

#define TEST_RV_ALL 1

#define TEST_RV64_I     TEST_RV_ALL
#define TEST_RV64_M     TEST_RV_ALL
#define TEST_RV64_A     TEST_RV_ALL
#define TEST_RV64_F     TEST_RV_ALL
#define TEST_RV64_D     TEST_RV_ALL
#define TEST_RV64_V     0
#define TEST_RV64_THEAD 0

#define TEST_RV32A_R 0  // passed
#define TEST_RV64A_R 0  // passed

#define __ GetAssembler()->

namespace art {

struct RISCV64CpuRegisterCompare {
  bool operator()(const riscv64::XRegister& a, const riscv64::XRegister& b) const { return a < b; }
};

class AssemblerRISCV64Test : public AssemblerTest<riscv64::Riscv64Assembler,
                                                  riscv64::Riscv64Label,
                                                  riscv64::XRegister,
                                                  riscv64::FRegister,
                                                  uint32_t,
                                                  riscv64::VectorRegister> {
 public:
  using Base = AssemblerTest<riscv64::Riscv64Assembler,
                             riscv64::Riscv64Label,
                             riscv64::XRegister,
                             riscv64::FRegister,
                             uint32_t,
                             riscv64::VectorRegister>;
  /*
    // These tests were taking too long, so we hide the DriverStr() from AssemblerTest<>
    // and reimplement it without the verification against `assembly_string`. b/73903608
    void DriverStr(const std::string& assembly_string ATTRIBUTE_UNUSED,
                   const std::string& test_name ATTRIBUTE_UNUSED) {
      GetAssembler()->FinalizeCode();
      std::vector<uint8_t> data(GetAssembler()->CodeSize());
      MemoryRegion code(data.data(), data.size());
      GetAssembler()->FinalizeInstructions(code);
    }*/

  AssemblerRISCV64Test()
      : instruction_set_features_(Riscv64InstructionSetFeatures::FromVariant("default", nullptr)) {}

 protected:
  /*
  std::string GetAssemblerParameters() override {
    // We assemble and link for RISCV64R6. The reason is that object files produced for RISCV64R6
    // (and MIPS32R6) with the GNU assembler don't have correct final offsets in PC-relative
    // branches in the .text section and so they require a relocation pass (there's a relocation
    // section, .rela.text, that has the needed info to fix up the branches).
    // return " -march=mips64r6 -mmsa -Wa,--no-warn -Wl,-Ttext=0 -Wl,-e0 -nostdlib";
    return " -march=rv64imafd -mabi=lp64 -Wa,--no-warn -Wl,-Ttext=0 -Wl,-e0 -nostdlib";
  }

  void Pad(std::vector<uint8_t>& data ATTRIBUTE_UNUSED) override {
    // The GNU linker unconditionally pads the code segment with NOPs to a size that is a multiple
    // of 16 and there doesn't appear to be a way to suppress this padding. Our assembler doesn't
    // pad, so, in order for two assembler outputs to match, we need to match the padding as well.
    // NOP is encoded as four zero bytes on MIPS.
    // size_t pad_size = RoundUp(data.size(), 16u) - data.size();
    // data.insert(data.end(), pad_size, 0);
  }

  std::string GetDisassembleParameters() override {
    return " -D -bbinary -mriscv:rv64";
  }
  */
  riscv64::Riscv64Assembler* CreateAssembler(ArenaAllocator* allocator) override {
    return new (allocator) riscv64::Riscv64Assembler(allocator, instruction_set_features_.get());
  }

  InstructionSet GetIsa() override { return InstructionSet::kRiscv64; }

  void SetUpHelpers() override {
    if (registers_.size() == 0) {
      registers_.push_back(new riscv64::XRegister(riscv64::ZERO));
      registers_.push_back(new riscv64::XRegister(riscv64::RA));
      registers_.push_back(new riscv64::XRegister(riscv64::SP));
      registers_.push_back(new riscv64::XRegister(riscv64::GP));
      registers_.push_back(new riscv64::XRegister(riscv64::TP));
      registers_.push_back(new riscv64::XRegister(riscv64::T0));
      registers_.push_back(new riscv64::XRegister(riscv64::T1));
      registers_.push_back(new riscv64::XRegister(riscv64::T2));
      registers_.push_back(new riscv64::XRegister(riscv64::S0));
      registers_.push_back(new riscv64::XRegister(riscv64::S1));
      registers_.push_back(new riscv64::XRegister(riscv64::A0));
      registers_.push_back(new riscv64::XRegister(riscv64::A1));
      registers_.push_back(new riscv64::XRegister(riscv64::A2));
      registers_.push_back(new riscv64::XRegister(riscv64::A3));
      registers_.push_back(new riscv64::XRegister(riscv64::A4));
      registers_.push_back(new riscv64::XRegister(riscv64::A5));
      registers_.push_back(new riscv64::XRegister(riscv64::A6));
      registers_.push_back(new riscv64::XRegister(riscv64::A7));
      registers_.push_back(new riscv64::XRegister(riscv64::S2));
      registers_.push_back(new riscv64::XRegister(riscv64::S3));
      registers_.push_back(new riscv64::XRegister(riscv64::S4));
      registers_.push_back(new riscv64::XRegister(riscv64::S5));
      registers_.push_back(new riscv64::XRegister(riscv64::S6));
      registers_.push_back(new riscv64::XRegister(riscv64::S7));
      registers_.push_back(new riscv64::XRegister(riscv64::S8));
      registers_.push_back(new riscv64::XRegister(riscv64::S9));
      registers_.push_back(new riscv64::XRegister(riscv64::S10));
      registers_.push_back(new riscv64::XRegister(riscv64::S11));
      registers_.push_back(new riscv64::XRegister(riscv64::T3));
      registers_.push_back(new riscv64::XRegister(riscv64::T4));
      registers_.push_back(new riscv64::XRegister(riscv64::T5));
      registers_.push_back(new riscv64::XRegister(riscv64::T6));

      secondary_register_names_.emplace(riscv64::XRegister(riscv64::ZERO), "zero");
      secondary_register_names_.emplace(riscv64::XRegister(riscv64::RA), "ra");
      secondary_register_names_.emplace(riscv64::XRegister(riscv64::SP), "sp");
      secondary_register_names_.emplace(riscv64::XRegister(riscv64::GP), "gp");
      secondary_register_names_.emplace(riscv64::XRegister(riscv64::TP), "tp");
      secondary_register_names_.emplace(riscv64::XRegister(riscv64::T0), "t0");
      secondary_register_names_.emplace(riscv64::XRegister(riscv64::T1), "t1");
      secondary_register_names_.emplace(riscv64::XRegister(riscv64::T2), "t2");
      secondary_register_names_.emplace(riscv64::XRegister(riscv64::S0), "s0");  // s0/fp
      secondary_register_names_.emplace(riscv64::XRegister(riscv64::S1), "s1");
      secondary_register_names_.emplace(riscv64::XRegister(riscv64::A0), "a0");
      secondary_register_names_.emplace(riscv64::XRegister(riscv64::A1), "a1");
      secondary_register_names_.emplace(riscv64::XRegister(riscv64::A2), "a2");
      secondary_register_names_.emplace(riscv64::XRegister(riscv64::A3), "a3");
      secondary_register_names_.emplace(riscv64::XRegister(riscv64::A4), "a4");
      secondary_register_names_.emplace(riscv64::XRegister(riscv64::A5), "a5");
      secondary_register_names_.emplace(riscv64::XRegister(riscv64::A6), "a6");
      secondary_register_names_.emplace(riscv64::XRegister(riscv64::A7), "a7");
      secondary_register_names_.emplace(riscv64::XRegister(riscv64::S2), "s2");
      secondary_register_names_.emplace(riscv64::XRegister(riscv64::S3), "s3");
      secondary_register_names_.emplace(riscv64::XRegister(riscv64::S4), "s4");
      secondary_register_names_.emplace(riscv64::XRegister(riscv64::S5), "s5");
      secondary_register_names_.emplace(riscv64::XRegister(riscv64::S6), "s6");
      secondary_register_names_.emplace(riscv64::XRegister(riscv64::S7), "s7");
      secondary_register_names_.emplace(riscv64::XRegister(riscv64::S8), "s8");
      secondary_register_names_.emplace(riscv64::XRegister(riscv64::S9), "s9");
      secondary_register_names_.emplace(riscv64::XRegister(riscv64::S10), "s10");
      secondary_register_names_.emplace(riscv64::XRegister(riscv64::S11), "s11");
      secondary_register_names_.emplace(riscv64::XRegister(riscv64::T3), "t3");
      secondary_register_names_.emplace(riscv64::XRegister(riscv64::T4), "t4");
      secondary_register_names_.emplace(riscv64::XRegister(riscv64::T5), "t5");
      secondary_register_names_.emplace(riscv64::XRegister(riscv64::T6), "t6");

      fp_registers_.push_back(new riscv64::FRegister(riscv64::FT0));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::FT1));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::FT2));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::FT3));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::FT4));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::FT5));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::FT6));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::FT7));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::FS0));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::FS1));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::FA0));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::FA1));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::FA2));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::FA3));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::FA4));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::FA5));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::FA6));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::FA7));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::FS2));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::FS3));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::FS4));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::FS5));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::FS6));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::FS7));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::FS8));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::FS9));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::FS10));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::FS11));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::FT8));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::FT9));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::FT10));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::FT11));

      vec_registers_.push_back(new riscv64::VectorRegister(riscv64::W0));
      vec_registers_.push_back(new riscv64::VectorRegister(riscv64::W1));
      vec_registers_.push_back(new riscv64::VectorRegister(riscv64::W2));
      vec_registers_.push_back(new riscv64::VectorRegister(riscv64::W3));
      vec_registers_.push_back(new riscv64::VectorRegister(riscv64::W4));
      vec_registers_.push_back(new riscv64::VectorRegister(riscv64::W5));
      vec_registers_.push_back(new riscv64::VectorRegister(riscv64::W6));
      vec_registers_.push_back(new riscv64::VectorRegister(riscv64::W7));
      vec_registers_.push_back(new riscv64::VectorRegister(riscv64::W8));
      vec_registers_.push_back(new riscv64::VectorRegister(riscv64::W9));
      vec_registers_.push_back(new riscv64::VectorRegister(riscv64::W10));
      vec_registers_.push_back(new riscv64::VectorRegister(riscv64::W11));
      vec_registers_.push_back(new riscv64::VectorRegister(riscv64::W12));
      vec_registers_.push_back(new riscv64::VectorRegister(riscv64::W13));
      vec_registers_.push_back(new riscv64::VectorRegister(riscv64::W14));
      vec_registers_.push_back(new riscv64::VectorRegister(riscv64::W15));
      vec_registers_.push_back(new riscv64::VectorRegister(riscv64::W16));
      vec_registers_.push_back(new riscv64::VectorRegister(riscv64::W17));
      vec_registers_.push_back(new riscv64::VectorRegister(riscv64::W18));
      vec_registers_.push_back(new riscv64::VectorRegister(riscv64::W19));
      vec_registers_.push_back(new riscv64::VectorRegister(riscv64::W20));
      vec_registers_.push_back(new riscv64::VectorRegister(riscv64::W21));
      vec_registers_.push_back(new riscv64::VectorRegister(riscv64::W22));
      vec_registers_.push_back(new riscv64::VectorRegister(riscv64::W23));
      vec_registers_.push_back(new riscv64::VectorRegister(riscv64::W24));
      vec_registers_.push_back(new riscv64::VectorRegister(riscv64::W25));
      vec_registers_.push_back(new riscv64::VectorRegister(riscv64::W26));
      vec_registers_.push_back(new riscv64::VectorRegister(riscv64::W27));
      vec_registers_.push_back(new riscv64::VectorRegister(riscv64::W28));
      vec_registers_.push_back(new riscv64::VectorRegister(riscv64::W29));
      vec_registers_.push_back(new riscv64::VectorRegister(riscv64::W30));
      vec_registers_.push_back(new riscv64::VectorRegister(riscv64::W31));
    }
  }

  void TearDown() override {
    AssemblerTest::TearDown();
    STLDeleteElements(&registers_);
    STLDeleteElements(&fp_registers_);
    STLDeleteElements(&vec_registers_);
  }

  std::vector<riscv64::Riscv64Label> GetAddresses() override {
    UNIMPLEMENTED(FATAL) << "Feature not implemented yet";
    UNREACHABLE();
  }

  std::vector<riscv64::XRegister*> GetRegisters() override { return registers_; }

  std::vector<riscv64::FRegister*> GetFPRegisters() override { return fp_registers_; }

  std::vector<riscv64::VectorRegister*> GetVectorRegisters() override { return vec_registers_; }

  uint32_t CreateImmediate(int64_t imm_value) override { return imm_value; }

  std::string GetSecondaryRegisterName(const riscv64::XRegister& reg) override {
    CHECK(secondary_register_names_.find(reg) != secondary_register_names_.end());
    return secondary_register_names_[reg];
  }

  std::string RepeatInsn(size_t count, const std::string& insn) {
    std::string result;
    for (; count != 0u; --count) {
      result += insn;
    }
    return result;
  }

  void BranchHelper(void (riscv64::Riscv64Assembler::*f)(riscv64::Riscv64Label*, bool),
                    const std::string& instr_name,
                    bool is_bare = false) {
    riscv64::Riscv64Label label1, label2;
    (Base::GetAssembler()->*f)(&label1, is_bare);
    constexpr size_t kAdduCount1 = 63;
    for (size_t i = 0; i != kAdduCount1; ++i) {
      __ Addw(riscv64::ZERO, riscv64::ZERO, riscv64::ZERO);
    }
    __ Bind(&label1);
    (Base::GetAssembler()->*f)(&label2, is_bare);
    constexpr size_t kAdduCount2 = 64;
    for (size_t i = 0; i != kAdduCount2; ++i) {
      __ Addw(riscv64::ZERO, riscv64::ZERO, riscv64::ZERO);
    }
    __ Bind(&label2);
    (Base::GetAssembler()->*f)(&label1, is_bare);
    __ Addw(riscv64::ZERO, riscv64::ZERO, riscv64::ZERO);

    std::string expected = ".set noreorder\n" + instr_name + " 1f\n" +
                           RepeatInsn(kAdduCount1, "addu $zero, $zero, $zero\n") + "1:\n" +
                           instr_name + " 2f\n" +
                           RepeatInsn(kAdduCount2, "addu $zero, $zero, $zero\n") + "2:\n" +
                           instr_name + " 1b\n" + "addu $zero, $zero, $zero\n";
    DriverStr(expected, instr_name);
  }

  void BranchHelper1(void (riscv64::Riscv64Assembler::*f)(riscv64::Riscv64Label*, bool),
                     const std::string& instr_name,
                     bool is_bare = false) {
    riscv64::Riscv64Label label1, label2;
    (Base::GetAssembler()->*f)(&label1, is_bare);
    constexpr size_t kAdduCount1 = 63;
    for (size_t i = 0; i != kAdduCount1; ++i) {
      __ Add(riscv64::ZERO, riscv64::ZERO, riscv64::ZERO);
    }
    __ Bind(&label1);
    (Base::GetAssembler()->*f)(&label2, is_bare);
    constexpr size_t kAdduCount2 = 64;
    for (size_t i = 0; i != kAdduCount2; ++i) {
      __ Add(riscv64::ZERO, riscv64::ZERO, riscv64::ZERO);
    }
    __ Bind(&label2);
    (Base::GetAssembler()->*f)(&label1, is_bare);
    __ Add(riscv64::ZERO, riscv64::ZERO, riscv64::ZERO);

    std::string expected = instr_name + " 1f\n" +
                           RepeatInsn(kAdduCount1, "add zero, zero, zero\n") + "1:\n" + instr_name +
                           " 2f\n" + RepeatInsn(kAdduCount2, "add zero, zero, zero\n") + "2:\n" +
                           instr_name + " 1b\n" + "add zero, zero, zero\n";
    DriverStr(expected, instr_name);
  }

  void BranchCondOneRegHelper(void (riscv64::Riscv64Assembler::*f)(riscv64::XRegister,
                                                                   riscv64::Riscv64Label*,
                                                                   bool),
                              const std::string& instr_name,
                              bool is_bare = false) {
    riscv64::Riscv64Label label;
    (Base::GetAssembler()->*f)(riscv64::A0, &label, is_bare);
    constexpr size_t kAdduCount1 = 63;
    for (size_t i = 0; i != kAdduCount1; ++i) {
      __ Addw(riscv64::ZERO, riscv64::ZERO, riscv64::ZERO);
    }
    __ Bind(&label);
    constexpr size_t kAdduCount2 = 64;
    for (size_t i = 0; i != kAdduCount2; ++i) {
      __ Addw(riscv64::ZERO, riscv64::ZERO, riscv64::ZERO);
    }
    (Base::GetAssembler()->*f)(riscv64::A1, &label, is_bare);
    __ Addw(riscv64::ZERO, riscv64::ZERO, riscv64::ZERO);

    std::string expected = ".set noreorder\n" + instr_name + " $a0, 1f\n" +
                           (is_bare ? "" : "nop\n") +
                           RepeatInsn(kAdduCount1, "addu $zero, $zero, $zero\n") + "1:\n" +
                           RepeatInsn(kAdduCount2, "addu $zero, $zero, $zero\n") + instr_name +
                           " $a1, 1b\n" + (is_bare ? "" : "nop\n") + "addu $zero, $zero, $zero\n";
    DriverStr(expected, instr_name);
  }

  void BranchCondTwoRegsHelper(void (riscv64::Riscv64Assembler::*f)(riscv64::XRegister,
                                                                    riscv64::XRegister,
                                                                    riscv64::Riscv64Label*,
                                                                    bool),
                               const std::string& instr_name,
                               bool is_bare = false) {
    riscv64::Riscv64Label label;
    (Base::GetAssembler()->*f)(riscv64::A0, riscv64::A1, &label, is_bare);
    constexpr size_t kAdduCount1 = 63;
    for (size_t i = 0; i != kAdduCount1; ++i) {
      __ Addw(riscv64::ZERO, riscv64::ZERO, riscv64::ZERO);
    }
    __ Bind(&label);
    constexpr size_t kAdduCount2 = 64;
    for (size_t i = 0; i != kAdduCount2; ++i) {
      __ Addw(riscv64::ZERO, riscv64::ZERO, riscv64::ZERO);
    }
    (Base::GetAssembler()->*f)(riscv64::A2, riscv64::A3, &label, is_bare);
    __ Addw(riscv64::ZERO, riscv64::ZERO, riscv64::ZERO);

    std::string expected =
        ".set noreorder\n" + instr_name + " $a0, $a1, 1f\n" + (is_bare ? "" : "nop\n") +
        RepeatInsn(kAdduCount1, "addu $zero, $zero, $zero\n") + "1:\n" +
        RepeatInsn(kAdduCount2, "addu $zero, $zero, $zero\n") + instr_name + " $a2, $a3, 1b\n" +
        (is_bare ? "" : "nop\n") + "addu $zero, $zero, $zero\n";
    DriverStr(expected, instr_name);
  }

  void BranchCondTwoRegsHelper1(void (riscv64::Riscv64Assembler::*f)(riscv64::XRegister,
                                                                     riscv64::XRegister,
                                                                     riscv64::Riscv64Label*,
                                                                     bool),
                                const std::string& instr_name,
                                bool is_bare = false) {
    riscv64::Riscv64Label label;
    (Base::GetAssembler()->*f)(riscv64::A0, riscv64::A1, &label, is_bare);
    constexpr size_t kAdduCount1 = 63;
    for (size_t i = 0; i != kAdduCount1; ++i) {
      __ Add(riscv64::ZERO, riscv64::ZERO, riscv64::ZERO);
    }
    __ Bind(&label);
    constexpr size_t kAdduCount2 = 64;
    for (size_t i = 0; i != kAdduCount2; ++i) {
      __ Add(riscv64::ZERO, riscv64::ZERO, riscv64::ZERO);
    }
    (Base::GetAssembler()->*f)(riscv64::A2, riscv64::A3, &label, is_bare);
    __ Add(riscv64::ZERO, riscv64::ZERO, riscv64::ZERO);

    std::string expected = instr_name + " a0, a1, 1f\n" + (is_bare ? "" : "nop\n") +
                           RepeatInsn(kAdduCount1, "add zero, zero, zero\n") + "1:\n" +
                           RepeatInsn(kAdduCount2, "add zero, zero, zero\n") + instr_name +
                           " a2, a3, 1b\n" + (is_bare ? "" : "nop\n") + "add zero, zero, zero\n";
    DriverStr(expected, instr_name);
  }

  void BranchFpuCondHelper(void (riscv64::Riscv64Assembler::*f)(riscv64::FRegister,
                                                                riscv64::Riscv64Label*,
                                                                bool),
                           const std::string& instr_name,
                           bool is_bare = false) {
    riscv64::Riscv64Label label;
    (Base::GetAssembler()->*f)(riscv64::F0, &label, is_bare);
    constexpr size_t kAdduCount1 = 63;
    for (size_t i = 0; i != kAdduCount1; ++i) {
      __ Addw(riscv64::ZERO, riscv64::ZERO, riscv64::ZERO);
    }
    __ Bind(&label);
    constexpr size_t kAdduCount2 = 64;
    for (size_t i = 0; i != kAdduCount2; ++i) {
      __ Addw(riscv64::ZERO, riscv64::ZERO, riscv64::ZERO);
    }
    (Base::GetAssembler()->*f)(riscv64::FT11, &label, is_bare);
    __ Addw(riscv64::ZERO, riscv64::ZERO, riscv64::ZERO);

    std::string expected = ".set noreorder\n" + instr_name + " $f0, 1f\n" +
                           (is_bare ? "" : "nop\n") +
                           RepeatInsn(kAdduCount1, "addu $zero, $zero, $zero\n") + "1:\n" +
                           RepeatInsn(kAdduCount2, "addu $zero, $zero, $zero\n") + instr_name +
                           " $f31, 1b\n" + (is_bare ? "" : "nop\n") + "addu $zero, $zero, $zero\n";
    DriverStr(expected, instr_name);
  }

 private:
  std::vector<riscv64::XRegister*> registers_;
  std::map<riscv64::XRegister, std::string, RISCV64CpuRegisterCompare> secondary_register_names_;

  std::vector<riscv64::FRegister*> fp_registers_;
  std::vector<riscv64::VectorRegister*> vec_registers_;

  std::unique_ptr<const Riscv64InstructionSetFeatures> instruction_set_features_;
};

TEST_F(AssemblerRISCV64Test, Toolchain) { EXPECT_TRUE(CheckTools()); }

#if TEST_RV64_I
TEST_F(AssemblerRISCV64Test, Add) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Add, "add {reg1}, {reg2}, {reg3}"), "Add");
}

TEST_F(AssemblerRISCV64Test, Addi) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Addi, -11, "addi {reg1}, {reg2}, {imm}"),
            "Addi");
}

TEST_F(AssemblerRISCV64Test, Addiw) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Addiw, -11, "addiw {reg1}, {reg2}, {imm}"),
            "Addiw");
}

TEST_F(AssemblerRISCV64Test, Addw) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Addw, "addw {reg1}, {reg2}, {reg3}"), "Addw");
}

TEST_F(AssemblerRISCV64Test, And) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::And, "and {reg1}, {reg2}, {reg3}"), "And");
}

TEST_F(AssemblerRISCV64Test, Andi) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Andi, -11, "andi {reg1}, {reg2}, {imm}"),
            "Andi");
}

TEST_F(AssemblerRISCV64Test, Auipc) {
  DriverStr(RepeatRIb(&riscv64::Riscv64Assembler::Auipc, 20, "auipc {reg},  {imm}"), "Auipc");
}

// XC-TODO: Branch instrs

TEST_F(AssemblerRISCV64Test, Lb) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Lb, -11, "lb {reg1}, {imm}({reg2})"), "Lb");
}

TEST_F(AssemblerRISCV64Test, Lbu) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Lbu, -11, "lbu {reg1}, {imm}({reg2})"), "Lbu");
}

TEST_F(AssemblerRISCV64Test, Ld) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Ld, -11, "ld {reg1}, {imm}({reg2})"), "Ld");
}

TEST_F(AssemblerRISCV64Test, Lh) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Lh, -11, "lh {reg1}, {imm}({reg2})"), "Lh");
}

TEST_F(AssemblerRISCV64Test, Lhu) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Lhu, -11, "lhu {reg1}, {imm}({reg2})"), "Lhu");
}

TEST_F(AssemblerRISCV64Test, Lui) {
  DriverStr(RepeatRIb(&riscv64::Riscv64Assembler::Lui, 20, "lui {reg}, {imm}"), "Lui");
}

TEST_F(AssemblerRISCV64Test, Lw) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Lw, -11, "lw {reg1}, {imm}({reg2})"), "Lw");
}

TEST_F(AssemblerRISCV64Test, Lwu) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Lwu, -11, "lwu {reg1}, {imm}({reg2})"), "Lwu");
}

TEST_F(AssemblerRISCV64Test, Or) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Or, "or {reg1}, {reg2}, {reg3}"), "Or");
}

TEST_F(AssemblerRISCV64Test, Ori) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Ori, -11, "ori {reg1}, {reg2}, {imm}"), "Ori");
}

TEST_F(AssemblerRISCV64Test, Sb) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Sb, -11, "sb {reg1}, {imm}({reg2})"), "Sb");
}

TEST_F(AssemblerRISCV64Test, Sd) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Sd, -11, "sd {reg1}, {imm}({reg2})"), "Sd");
}

TEST_F(AssemblerRISCV64Test, Sh) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Sh, -11, "sh {reg1}, {imm}({reg2})"), "Sh");
}

TEST_F(AssemblerRISCV64Test, Sll) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Sll, "sll {reg1}, {reg2}, {reg3}"), "Sll");
}

TEST_F(AssemblerRISCV64Test, Slli) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Slli, 6, "slli {reg1}, {reg2}, {imm}"), "Slli");
}

TEST_F(AssemblerRISCV64Test, Slliw) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Slliw, 5, "slliw {reg1}, {reg2}, {imm}"),
            "Slliw");
}

TEST_F(AssemblerRISCV64Test, Sllw) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Sllw, "sllw {reg1}, {reg2}, {reg3}"), "Sllw");
}

TEST_F(AssemblerRISCV64Test, Slt) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Slt, "slt {reg1}, {reg2}, {reg3}"), "Slt");
}

TEST_F(AssemblerRISCV64Test, Slti) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Slti, -11, "slti {reg1}, {reg2}, {imm}"),
            "Slti");
}

TEST_F(AssemblerRISCV64Test, Sltiu) {
  // XC-TODO: clang erro?
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Sltiu, -11, "sltiu {reg1}, {reg2}, {imm}"),
            "Sltiu");
}

TEST_F(AssemblerRISCV64Test, Sltu) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Sltu, "sltu {reg1}, {reg2}, {reg3}"), "Sltu");
}

TEST_F(AssemblerRISCV64Test, Sra) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Sra, "sra {reg1}, {reg2}, {reg3}"), "Sra");
}

TEST_F(AssemblerRISCV64Test, Srai) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Srai, 6, "srai {reg1}, {reg2}, {imm}"), "Srai");
}

TEST_F(AssemblerRISCV64Test, Sraiw) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Sraiw, 5, "sraiw {reg1}, {reg2}, {imm}"),
            "Sraiw");
}

TEST_F(AssemblerRISCV64Test, Sraw) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Sraw, "sraw {reg1}, {reg2}, {reg3}"), "Sraw");
}

TEST_F(AssemblerRISCV64Test, Srl) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Srl, "srl {reg1}, {reg2}, {reg3}"), "Srl");
}

TEST_F(AssemblerRISCV64Test, Srli) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Srli, 6, "srli {reg1}, {reg2}, {imm}"), "Srli");
}

TEST_F(AssemblerRISCV64Test, Srliw) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Srliw, 5, "srliw {reg1}, {reg2}, {imm}"),
            "Srliw");
}

TEST_F(AssemblerRISCV64Test, Srlw) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Srlw, "srlw {reg1}, {reg2}, {reg3}"), "Srlw");
}

TEST_F(AssemblerRISCV64Test, Sub) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Sub, "sub {reg1}, {reg2}, {reg3}"), "Sub");
}

TEST_F(AssemblerRISCV64Test, Subw) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Subw, "subw {reg1}, {reg2}, {reg3}"), "Subw");
}

TEST_F(AssemblerRISCV64Test, Sw) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Sw, -11, "sw {reg1}, {imm}({reg2})"), "Sw");
}

TEST_F(AssemblerRISCV64Test, Xor) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Xor, "xor {reg1}, {reg2}, {reg3}"), "Xor");
}

TEST_F(AssemblerRISCV64Test, Xori) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Xori, 11, "xori {reg1}, {reg2}, {imm}"), "Xori");
}
#endif

#if TEST_RV64_M
TEST_F(AssemblerRISCV64Test, Div) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Div, "div {reg1}, {reg2}, {reg3}"), "Div");
}

TEST_F(AssemblerRISCV64Test, Divu) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Divu, "divu {reg1}, {reg2}, {reg3}"), "Divu");
}
TEST_F(AssemblerRISCV64Test, Divuw) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Divuw, "div {reg1}, {reg2}, {reg3}"), "Divuw");
}

TEST_F(AssemblerRISCV64Test, Divw) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Divw, "divw {reg1}, {reg2}, {reg3}"), "Divw");
}

TEST_F(AssemblerRISCV64Test, Mul) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Mul, "mul {reg1}, {reg2}, {reg3}"), "Mul");
}

TEST_F(AssemblerRISCV64Test, Mulh) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Mulh, "mul {reg1}, {reg2}, {reg3}"), "Mulh");
}

TEST_F(AssemblerRISCV64Test, Mulhsu) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Mulhsu, "mul {reg1}, {reg2}, {reg3}"), "Mulhsu");
}

TEST_F(AssemblerRISCV64Test, Mulhu) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Mulhu, "mul {reg1}, {reg2}, {reg3}"), "Mulhu");
}

TEST_F(AssemblerRISCV64Test, Mulw) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Mulw, "mulw {reg1}, {reg2}, {reg3}"), "Mulw");
}

TEST_F(AssemblerRISCV64Test, Rem) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Rem, "remw {reg1}, {reg2}, {reg3}"), "Rem");
}

TEST_F(AssemblerRISCV64Test, Remu) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Remu, "remw {reg1}, {reg2}, {reg3}"), "Remu");
}

TEST_F(AssemblerRISCV64Test, Remuw) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Remuw, "remuw {reg1}, {reg2}, {reg3}"), "Remuw");
}

TEST_F(AssemblerRISCV64Test, Remw) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Remw, "remw {reg1}, {reg2}, {reg3}"), "Remw");
}
#endif

#if TEST_RV64_A
TEST_F(AssemblerRISCV64Test, AmoAddD) {
  DriverStr(
      RepeatRRRIb(&riscv64::Riscv64Assembler::AmoAddD, 1, "amoadd.d {reg1}, {reg2}, ({reg3})"),
      "AmoAddD");
}

TEST_F(AssemblerRISCV64Test, AmoAddW) {
  DriverStr(
      RepeatRRRIb(&riscv64::Riscv64Assembler::AmoAddW, 1, "amoadd.w {reg1}, {reg2}, ({reg3})"),
      "AmoAddW");
}

TEST_F(AssemblerRISCV64Test, AmoAndD) {
  DriverStr(
      RepeatRRRIb(&riscv64::Riscv64Assembler::AmoAndD, 1, "amoand.d {reg1}, {reg2}, ({reg3})"),
      "AmoAndD");
}

TEST_F(AssemblerRISCV64Test, AmoAndW) {
  DriverStr(
      RepeatRRRIb(&riscv64::Riscv64Assembler::AmoAndW, 1, "amoand.w {reg1}, {reg2}, ({reg3})"),
      "AmoAndW");
}

TEST_F(AssemblerRISCV64Test, AmoMaxD) {
  DriverStr(
      RepeatRRRIb(&riscv64::Riscv64Assembler::AmoMaxD, 1, "amomax.d {reg1}, {reg2}, ({reg3})"),
      "AmoMaxD");
}

TEST_F(AssemblerRISCV64Test, AmoMaxW) {
  DriverStr(
      RepeatRRRIb(&riscv64::Riscv64Assembler::AmoMaxW, 1, "amomax.w {reg1}, {reg2}, ({reg3})"),
      "AmoMaxW");
}

TEST_F(AssemblerRISCV64Test, AmoMaxuD) {
  DriverStr(
      RepeatRRRIb(&riscv64::Riscv64Assembler::AmoMaxuD, 1, "amomaxu.d {reg1}, {reg2}, ({reg3})"),
      "AmoMaxuD");
}

TEST_F(AssemblerRISCV64Test, AmoMaxuW) {
  DriverStr(
      RepeatRRRIb(&riscv64::Riscv64Assembler::AmoMaxuW, 1, "amomaxu.w {reg1}, {reg2}, ({reg3})"),
      "AmoMaxuW");
}

TEST_F(AssemblerRISCV64Test, AmoMinD) {
  DriverStr(
      RepeatRRRIb(&riscv64::Riscv64Assembler::AmoMinD, 1, "amomin.d {reg1}, {reg2}, ({reg3})"),
      "AmoMinD");
}

TEST_F(AssemblerRISCV64Test, AmoMinW) {
  DriverStr(
      RepeatRRRIb(&riscv64::Riscv64Assembler::AmoMinW, 1, "amomin.w {reg1}, {reg2}, ({reg3})"),
      "AmoMinW");
}

TEST_F(AssemblerRISCV64Test, AmoMinuD) {
  DriverStr(
      RepeatRRRIb(&riscv64::Riscv64Assembler::AmoMinuD, 1, "amominu.d {reg1}, {reg2}, ({reg3})"),
      "AmoMinuD");
}

TEST_F(AssemblerRISCV64Test, AmoMinuW) {
  DriverStr(
      RepeatRRRIb(&riscv64::Riscv64Assembler::AmoMinuW, 1, "amominu.w {reg1}, {reg2}, ({reg3})"),
      "AmoMinuW");
}

TEST_F(AssemblerRISCV64Test, AmoOrD) {
  DriverStr(RepeatRRRIb(&riscv64::Riscv64Assembler::AmoOrD, 1, "amoor.d {reg1}, {reg2}, ({reg3})"),
            "AmoOrD");
}

TEST_F(AssemblerRISCV64Test, AmoOrW) {
  DriverStr(RepeatRRRIb(&riscv64::Riscv64Assembler::AmoOrW, 1, "amoor.w {reg1}, {reg2}, ({reg3})"),
            "AmoOrW");
}

TEST_F(AssemblerRISCV64Test, AmoSwapD) {
  DriverStr(
      RepeatRRRIb(&riscv64::Riscv64Assembler::AmoSwapD, 1, "amoswap.d {reg1}, {reg2}, ({reg3})"),
      "AmoSwapD");
}

TEST_F(AssemblerRISCV64Test, AmoSwapW) {
  DriverStr(
      RepeatRRRIb(&riscv64::Riscv64Assembler::AmoSwapW, 1, "amoswap.w {reg1}, {reg2}, ({reg3})"),
      "AmoSwapW");
}

TEST_F(AssemblerRISCV64Test, AmoXorD) {
  DriverStr(
      RepeatRRRIb(&riscv64::Riscv64Assembler::AmoXorD, 1, "amoxor.d {reg1}, {reg2}, ({reg3})"),
      "AmoXorD");
}

TEST_F(AssemblerRISCV64Test, AmoXorW) {
  DriverStr(
      RepeatRRRIb(&riscv64::Riscv64Assembler::AmoXorW, 1, "amoxor.w {reg1}, {reg2}, ({reg3})"),
      "AmoXorW");
}

TEST_F(AssemblerRISCV64Test, LrD) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::LrD, 1, "lr.d {reg1}, ({reg2})"), "LrD");
}

TEST_F(AssemblerRISCV64Test, LrW) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::LrW, 1, "lr.w {reg1}, ({reg2})"), "LrW");
}

TEST_F(AssemblerRISCV64Test, ScD) {
  DriverStr(RepeatRRRIb(&riscv64::Riscv64Assembler::ScD, 1, "sc.d {reg1}, {reg2}, ({reg3})"),
            "ScD");
}

TEST_F(AssemblerRISCV64Test, ScW) {
  DriverStr(RepeatRRRIb(&riscv64::Riscv64Assembler::ScW, 1, "sc.w {reg1}, {reg2}, ({reg3})"),
            "ScW");
}
#endif

#if TEST_RV64_F
TEST_F(AssemblerRISCV64Test, FAddS) {
  DriverStr(RepeatFFF(&riscv64::Riscv64Assembler::FAddS, "fadd.s {reg1}, {reg2}, {reg3}"), "FAddS");
}

TEST_F(AssemblerRISCV64Test, FClassS) {
  DriverStr(RepeatRF(&riscv64::Riscv64Assembler::FClassS, "fclass.s {reg1}, {reg2}"), "FClassS");
}

TEST_F(AssemblerRISCV64Test, FCvtLS) {
  // FPRoundingMode can not be replaced with unsigned int
  // DriverStr(RepeatRFIb(&riscv64::Riscv64Assembler::FCvtLS, 2, "fcvt.l.s {reg1}, {reg2}, {imm}"),
  // "FCvtLS");
}

TEST_F(AssemblerRISCV64Test, FCvtLuS) {
  DriverStr(RepeatRF(&riscv64::Riscv64Assembler::FCvtLuS, "fcvt.lu.s {reg1}, {reg2}"), "FCvtLuS");
}

TEST_F(AssemblerRISCV64Test, FCvtSL) {
  DriverStr(RepeatFR(&riscv64::Riscv64Assembler::FCvtSL, "fcvt.s.l {reg1}, {reg2}"), "FCvtSL");
}
TEST_F(AssemblerRISCV64Test, FCvtSLu) {
  DriverStr(RepeatFR(&riscv64::Riscv64Assembler::FCvtSLu, "fcvt.s.lu {reg1}, {reg2}"), "FCvtSLu");
}

TEST_F(AssemblerRISCV64Test, FCvtSW) {
  DriverStr(RepeatFR(&riscv64::Riscv64Assembler::FCvtSW, "fcvt.s.w {reg1}, {reg2}"), "FCvtSW");
}

TEST_F(AssemblerRISCV64Test, FCvtSWu) {
  DriverStr(RepeatFR(&riscv64::Riscv64Assembler::FCvtSWu, "fcvt.s.wu {reg1}, {reg2}"), "FCvtSWu");
}

TEST_F(AssemblerRISCV64Test, FCvtWS) {
  // DriverStr(RepeatrF(&riscv64::Riscv64Assembler::FCvtWS, "fcvt.w.s {reg1}, {reg2}, {reg3}"),
  // "FCvtWS");
}

TEST_F(AssemblerRISCV64Test, FCvtWuS) {
  DriverStr(RepeatrF(&riscv64::Riscv64Assembler::FCvtWuS, "fcvt.wu.s {reg1}, {reg2}"), "FCvtWuS");
}

TEST_F(AssemblerRISCV64Test, FDivS) {
  DriverStr(RepeatFFF(&riscv64::Riscv64Assembler::FDivS, "fdiv.s {reg1}, {reg2}, {reg3}"), "FDivS");
}

TEST_F(AssemblerRISCV64Test, FEqS) {
  DriverStr(RepeatRFF(&riscv64::Riscv64Assembler::FEqS, "feq.s {reg1}, {reg2}, {reg3}"), "FEqS");
}

TEST_F(AssemblerRISCV64Test, FLeS) {
  DriverStr(RepeatRFF(&riscv64::Riscv64Assembler::FLeS, "fle.s {reg1}, {reg2}, {reg3}"), "FLeS");
}

TEST_F(AssemblerRISCV64Test, FLtS) {
  DriverStr(RepeatRFF(&riscv64::Riscv64Assembler::FLtS, "flt.s {reg1}, {reg2}, {reg3}"), "FLtS");
}

TEST_F(AssemblerRISCV64Test, FLw) {
  DriverStr(RepeatFRIb(&riscv64::Riscv64Assembler::FLw, -11, "flw {reg1}, {imm}({reg2})"), "FLw");
}

TEST_F(AssemblerRISCV64Test, FMAddS) {
  // DriverStr(RepeatFFFF(&riscv64::Riscv64Assembler::FMAddS, "fmadd.s {reg1}, {reg2}, {reg3},
  // {reg4}"), "FMAddS");
}

TEST_F(AssemblerRISCV64Test, FMaxS) {
  DriverStr(RepeatFFF(&riscv64::Riscv64Assembler::FMaxS, "fmax.s {reg1}, {reg2}, {reg3}"), "FMaxS");
}

TEST_F(AssemblerRISCV64Test, FMinS) {
  DriverStr(RepeatFFF(&riscv64::Riscv64Assembler::FMinS, "fmin.s {reg1}, {reg2}, {reg3}"), "FMinS");
}

TEST_F(AssemblerRISCV64Test, FMSubS) {
  // DriverStr(RepeatFFFF(&riscv64::Riscv64Assembler::FMSubS, "fmsub.s {reg1}, {reg2}, {reg3},
  // {reg4}"), "FMSubS");
}

TEST_F(AssemblerRISCV64Test, FMulS) {
  DriverStr(RepeatFFF(&riscv64::Riscv64Assembler::FMulS, "fmul.s {reg1}, {reg2}, {reg3}"), "FMulS");
}

TEST_F(AssemblerRISCV64Test, FMvWX) {
  DriverStr(RepeatFR(&riscv64::Riscv64Assembler::FMvWX, "fmv.w.x {reg1}, {reg2}"), "FMvWX");
}

TEST_F(AssemblerRISCV64Test, FMvXW) {
  DriverStr(RepeatRF(&riscv64::Riscv64Assembler::FMvXW, "fmv.x.w {reg1}, {reg2}"), "FMvXW");
}

TEST_F(AssemblerRISCV64Test, FNMAddS) {
  // DriverStr(RepeatFFFF(&riscv64::Riscv64Assembler::FNMAddS, "fnmadd.s {reg1}, {reg2}, {reg3},
  // {reg4}"), "FNMAddS");
}

TEST_F(AssemblerRISCV64Test, FNMSubS) {
  // DriverStr(RepeatFFFF(&riscv64::Riscv64Assembler::FNMSubS, "fnmsub.s {reg1}, {reg2}, {reg3},
  // {reg4}"), "FNMSubS");
}

TEST_F(AssemblerRISCV64Test, FSgnjS) {
  DriverStr(RepeatFFF(&riscv64::Riscv64Assembler::FSgnjS, "fsgnj.s {reg1}, {reg2}, {reg3}"),
            "FSgnjS");
}

TEST_F(AssemblerRISCV64Test, FSgnjnS) {
  DriverStr(RepeatFFF(&riscv64::Riscv64Assembler::FSgnjnS, "fsgnjn.s {reg1}, {reg2}, {reg3}"),
            "FSgnjnS");
}

TEST_F(AssemblerRISCV64Test, FSgnjxS) {
  DriverStr(RepeatFFF(&riscv64::Riscv64Assembler::FSgnjxS, "fsgnjx.s {reg1}, {reg2}, {reg3}"),
            "FSgnjxS");
}

TEST_F(AssemblerRISCV64Test, FSqrtS) {
  DriverStr(RepeatFF(&riscv64::Riscv64Assembler::FSqrtS, "fsqrt.s {reg1}, {reg2}"), "FSqrtS");
}

TEST_F(AssemblerRISCV64Test, FSubS) {
  DriverStr(RepeatFFF(&riscv64::Riscv64Assembler::FSubS, "fsub.s {reg1}, {reg2}, {reg3}"), "FSubS");
}

TEST_F(AssemblerRISCV64Test, FSw) {
  DriverStr(RepeatFRIb(&riscv64::Riscv64Assembler::FSw, 2, "fsw {reg1}, {imm}({reg2})"), "FSw");
}
#endif

#if TEST_RV64_D
TEST_F(AssemblerRISCV64Test, FAddD) {
  DriverStr(RepeatFFF(&riscv64::Riscv64Assembler::FAddD, "fadd.d {reg1}, {reg2}, {reg3}"), "FAddD");
}

TEST_F(AssemblerRISCV64Test, FClassD) {
  DriverStr(RepeatrF(&riscv64::Riscv64Assembler::FClassD, "fclass.d {reg1}, {reg2}"), "FClassD");
}

TEST_F(AssemblerRISCV64Test, FCvtDL) {
  DriverStr(RepeatFR(&riscv64::Riscv64Assembler::FCvtDL, "fcvt.d.l {reg1}, {reg2}"), "FCvtDL");
}

TEST_F(AssemblerRISCV64Test, FCvtDLu) {
  DriverStr(RepeatFR(&riscv64::Riscv64Assembler::FCvtDLu, "fcvt.d.lu {reg1}, {reg2}"), "FCvtDLu");
}

TEST_F(AssemblerRISCV64Test, FCvtDS) {
  DriverStr(RepeatFF(&riscv64::Riscv64Assembler::FCvtDS, "fcvt.d.s {reg1}, {reg2}"), "FCvtDS");
}

TEST_F(AssemblerRISCV64Test, FCvtDW) {
  DriverStr(RepeatFR(&riscv64::Riscv64Assembler::FCvtDW, "fcvt.d.w {reg1}, {reg2}"), "FCvtDW");
}

TEST_F(AssemblerRISCV64Test, FCvtDWu) {
  DriverStr(RepeatFR(&riscv64::Riscv64Assembler::FCvtDWu, "fcvt.d.wu {reg1}, {reg2}"), "FCvtDWu");
}

TEST_F(AssemblerRISCV64Test, FCvtLD) {
  // DriverStr(RepeatRF(&riscv64::Riscv64Assembler::FCvtLD, "fcvt.l.d {reg1}, {reg2}, {reg3}"),
  // "FCvtLD");
}

TEST_F(AssemblerRISCV64Test, FCvtLuD) {
  DriverStr(RepeatRF(&riscv64::Riscv64Assembler::FCvtLuD, "fcvt.lu.d {reg1}, {reg2}"), "FCvtLuD");
}

TEST_F(AssemblerRISCV64Test, FCvtSD) {
  DriverStr(RepeatFF(&riscv64::Riscv64Assembler::FCvtSD, "fcvt.s.d {reg1}, {reg2}"), "FCvtSD");
}

TEST_F(AssemblerRISCV64Test, FCvtWD) {
  // DriverStr(RepeatrF(&riscv64::Riscv64Assembler::FCvtWD, "fcvt.w.d {reg1}, {reg2}, {reg3}"),
  // "FCvtWD");
}

TEST_F(AssemblerRISCV64Test, FCvtWuD) {
  DriverStr(RepeatrF(&riscv64::Riscv64Assembler::FCvtWuD, "fcvt.wu.d {reg1}, {reg2}"), "FCvtWuD");
}

TEST_F(AssemblerRISCV64Test, FDivD) {
  DriverStr(RepeatFFF(&riscv64::Riscv64Assembler::FDivD, "fdiv.d {reg1}, {reg2}, {reg3}"), "FDivD");
}

TEST_F(AssemblerRISCV64Test, FEqD) {
  DriverStr(RepeatRFF(&riscv64::Riscv64Assembler::FEqD, "feq.d {reg1}, {reg2}, {reg3}"), "FEqD");
}

TEST_F(AssemblerRISCV64Test, FLd) {
  DriverStr(RepeatFRIb(&riscv64::Riscv64Assembler::FLd, -11, "fld {reg1}, {imm}({reg2})"), "FLw");
}

TEST_F(AssemblerRISCV64Test, FLeD) {
  DriverStr(RepeatRFF(&riscv64::Riscv64Assembler::FLeD, "fle.d {reg1}, {reg2}, {reg3}"), "FLeD");
}

TEST_F(AssemblerRISCV64Test, FLtD) {
  DriverStr(RepeatRFF(&riscv64::Riscv64Assembler::FLtD, "flt.d {reg1}, {reg2}, {reg3}"), "FLtD");
}

TEST_F(AssemblerRISCV64Test, FMAddD) {
  // DriverStr(RepeatFFFF(&riscv64::Riscv64Assembler::FMAddD, "fmadd.d {reg1}, {reg2}, {reg3},
  // {reg4}"), "FMAddD");
}

TEST_F(AssemblerRISCV64Test, FMaxD) {
  DriverStr(RepeatFFF(&riscv64::Riscv64Assembler::FMaxD, "fmax.d {reg1}, {reg2}, {reg3}"), "FMaxD");
}

TEST_F(AssemblerRISCV64Test, FMinD) {
  DriverStr(RepeatFFF(&riscv64::Riscv64Assembler::FMinD, "fmin.d {reg1}, {reg2}, {reg3}"), "FMinD");
}

TEST_F(AssemblerRISCV64Test, FMSubD) {
  // DriverStr(RepeatFFFF(&riscv64::Riscv64Assembler::FMSubD, "fmsub.d {reg1}, {reg2}, {reg3},
  // {reg4}"), "FMSubD");
}

TEST_F(AssemblerRISCV64Test, FMulD) {
  DriverStr(RepeatFFF(&riscv64::Riscv64Assembler::FMulD, "fmul.d {reg1}, {reg2}, {reg3}"), "FMulD");
}

TEST_F(AssemblerRISCV64Test, FMvDX) {
  DriverStr(RepeatFR(&riscv64::Riscv64Assembler::FMvDX, "fmv.d.x {reg1}, {reg2}"), "FMvDX");
}

TEST_F(AssemblerRISCV64Test, FMvXD) {
  DriverStr(RepeatRF(&riscv64::Riscv64Assembler::FMvXD, "fmv.x.d {reg1}, {reg2}"), "FMvXD");
}

TEST_F(AssemblerRISCV64Test, FNMAddD) {
  // DriverStr(RepeatFFFF(&riscv64::Riscv64Assembler::FNMAddD, "fnmadd.d {reg1}, {reg2}, {reg3},
  // {reg4}"), "FNMAddD");
}

TEST_F(AssemblerRISCV64Test, FNMSubD) {
  // DriverStr(RepeatFFFF(&riscv64::Riscv64Assembler::FNMSubD, "fnmsub.d {reg1}, {reg2}, {reg3},
  // {reg4}"), "FNMSubD");
}

TEST_F(AssemblerRISCV64Test, FSd) {
  DriverStr(RepeatFRIb(&riscv64::Riscv64Assembler::FSd, 2, "fsd {reg1}, {imm}({reg2})"), "FSd");
}

TEST_F(AssemblerRISCV64Test, FSgnjD) {
  DriverStr(RepeatFFF(&riscv64::Riscv64Assembler::FSgnjD, "fsgnj.d {reg1}, {reg2}, {reg3}"),
            "FSgnjD");
}

TEST_F(AssemblerRISCV64Test, FSgnjnD) {
  DriverStr(RepeatFFF(&riscv64::Riscv64Assembler::FSgnjnD, "fsgnjn.d {reg1}, {reg2}, {reg3}"),
            "FSgnjnD");
}

TEST_F(AssemblerRISCV64Test, FSgnjxD) {
  DriverStr(RepeatFFF(&riscv64::Riscv64Assembler::FSgnjxD, "fsgnjx.d {reg1}, {reg2}, {reg3}"),
            "FSgnjxD");
}

TEST_F(AssemblerRISCV64Test, FSqrtD) {
  DriverStr(RepeatFF(&riscv64::Riscv64Assembler::FSqrtD, "fsqrt.d {reg1}, {reg2}"), "FSqrtD");
}

TEST_F(AssemblerRISCV64Test, FSubD) {
  DriverStr(RepeatFFF(&riscv64::Riscv64Assembler::FSubD, "fsub.d {reg1}, {reg2}, {reg3}"), "FSubD");
}
#endif

#undef __

}  // namespace art
