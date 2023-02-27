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
#include "base/macros.h"
#include "base/stl_util.h"
#include "utils/assembler_test.h"

#define __ GetAssembler()->

namespace art HIDDEN {

struct RISCV64CpuRegisterCompare {
  bool operator()(const riscv64::XRegister& a, const riscv64::XRegister& b) const { return a < b; }
};

struct RISCV64RoundingModeCompare {
  bool operator()(const riscv64::RoundingMode& a, const riscv64::RoundingMode& b) const {
    return a < b;
  }
};

class AssemblerRiscv64Test : public AssemblerTest<riscv64::Riscv64Assembler,
                                                  riscv64::Riscv64Label,
                                                  riscv64::XRegister,
                                                  riscv64::FRegister,
                                                  uint64_t,
                                                  uint32_t,
                                                  riscv64::RoundingMode> {
 public:
  using Base = AssemblerTest<riscv64::Riscv64Assembler,
                             riscv64::Riscv64Label,
                             riscv64::XRegister,
                             riscv64::FRegister,
                             uint64_t,
                             uint32_t,
                             riscv64::RoundingMode>;

  // use base class
  /*
  void DriverStr(const std::string& assembly_string ATTRIBUTE_UNUSED,
                 const std::string& test_name ATTRIBUTE_UNUSED) {
    GetAssembler()->FinalizeCode();
    std::vector<uint8_t> data(GetAssembler()->CodeSize());
    MemoryRegion code(data.data(), data.size());
    GetAssembler()->FinalizeInstructions(code);
  }
  */

  AssemblerRiscv64Test()
      : instruction_set_features_(Riscv64InstructionSetFeatures::FromVariant("generic", nullptr)) {}

 protected:
  InstructionSet GetIsa() override { return InstructionSet::kRiscv64; }

  riscv64::Riscv64Assembler* CreateAssembler(ArenaAllocator* allocator) override {
    return new (allocator) riscv64::Riscv64Assembler(allocator, instruction_set_features_.get());
  }

  void SetUpHelpers() override {
    if (registers_.size() == 0) {
      registers_.push_back(new riscv64::XRegister(riscv64::Zero));
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

      secondary_register_names_.emplace(riscv64::XRegister(riscv64::Zero), "zero");
      secondary_register_names_.emplace(riscv64::XRegister(riscv64::RA), "ra");
      secondary_register_names_.emplace(riscv64::XRegister(riscv64::SP), "sp");
      secondary_register_names_.emplace(riscv64::XRegister(riscv64::GP), "gp");
      secondary_register_names_.emplace(riscv64::XRegister(riscv64::TP), "tp");
      secondary_register_names_.emplace(riscv64::XRegister(riscv64::T0), "t0");
      secondary_register_names_.emplace(riscv64::XRegister(riscv64::T1), "t1");
      secondary_register_names_.emplace(riscv64::XRegister(riscv64::T2), "t2");
      secondary_register_names_.emplace(riscv64::XRegister(riscv64::S0), "s0");
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

      fp_registers_.push_back(new riscv64::FRegister(riscv64::F0));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::F1));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::F2));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::F3));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::F4));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::F5));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::F6));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::F7));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::F8));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::F9));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::F10));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::F11));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::F12));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::F13));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::F14));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::F15));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::F16));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::F17));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::F18));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::F19));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::F20));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::F21));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::F22));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::F23));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::F24));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::F25));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::F26));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::F27));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::F28));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::F29));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::F30));
      fp_registers_.push_back(new riscv64::FRegister(riscv64::F31));
    }

    if (roundingmode_.size() == 0) {
      roundingmode_.push_back(new riscv64::RoundingMode(riscv64::RNE));
      roundingmode_.push_back(new riscv64::RoundingMode(riscv64::RTZ));
      roundingmode_.push_back(new riscv64::RoundingMode(riscv64::RDN));
      roundingmode_.push_back(new riscv64::RoundingMode(riscv64::RUP));
      roundingmode_.push_back(new riscv64::RoundingMode(riscv64::RMM));
      roundingmode_.push_back(new riscv64::RoundingMode(riscv64::DYN));

      roundingmode_names_.emplace(riscv64::RoundingMode(riscv64::RNE), "rne");
      roundingmode_names_.emplace(riscv64::RoundingMode(riscv64::RTZ), "rtz");
      roundingmode_names_.emplace(riscv64::RoundingMode(riscv64::RDN), "rdn");
      roundingmode_names_.emplace(riscv64::RoundingMode(riscv64::RUP), "rup");
      roundingmode_names_.emplace(riscv64::RoundingMode(riscv64::RMM), "rmm");
      roundingmode_names_.emplace(riscv64::RoundingMode(riscv64::DYN), "dyn");
    }
  }
  void TearDown() override {
    AssemblerTest::TearDown();
    STLDeleteElements(&registers_);
    STLDeleteElements(&fp_registers_);
    STLDeleteElements(&roundingmode_);
    // STLDeleteElements(&vec_registers_);
  }

  // std::vector<riscv64::Riscv64Label> GetAddresses() override {
  std::vector<riscv64::Riscv64Label> GetAddresses() override {
    UNIMPLEMENTED(FATAL) << "Feature not implemented yet";
    UNREACHABLE();
  }

  std::vector<riscv64::XRegister*> GetRegisters() override { return registers_; }

  std::vector<riscv64::FRegister*> GetFPRegisters() override { return fp_registers_; }

  std::vector<riscv64::RoundingMode*> GetRoundingMode() override { return roundingmode_; }

  uint64_t CreateImmediate(int64_t imm_value) override { return imm_value; }

  std::string GetSecondaryRegisterName(const riscv64::XRegister& reg) override {
    CHECK(secondary_register_names_.find(reg) != secondary_register_names_.end());
    return secondary_register_names_[reg];
  }

  std::string GetRoundingModeName(const riscv64::RoundingMode& rm) override {
    CHECK(roundingmode_names_.find(rm) != roundingmode_names_.end());
    std::ostringstream srm;
    srm << roundingmode_names_[rm];
    return srm.str();
  }

  std::string RepeatInsn(size_t count, const std::string& insn) {
    std::string result;
    for (; count != 0u; --count) {
      result += insn;
    }
    return result;
  }

  void BranchHelper(void (riscv64::Riscv64Assembler::*f)(riscv64::Riscv64Label*),
                    const std::string& instr_name) {
    riscv64::Riscv64Label label1, label2;
    (Base::GetAssembler()->*f)(&label1);
    constexpr size_t kAddCount1 = 63;
    for (size_t i = 0; i != kAddCount1; ++i) {
      __ Add(riscv64::Zero, riscv64::Zero, riscv64::Zero);
    }
    __ Bind(&label1);
    (Base::GetAssembler()->*f)(&label2);
    constexpr size_t kAddCount2 = 64;
    for (size_t i = 0; i != kAddCount2; ++i) {
      __ Add(riscv64::Zero, riscv64::Zero, riscv64::Zero);
    }
    __ Bind(&label2);
    (Base::GetAssembler()->*f)(&label1);
    __ Add(riscv64::Zero, riscv64::Zero, riscv64::Zero);

    std::string expected = instr_name + " 1f\n" + RepeatInsn(kAddCount1, "Add zero, zero, zero\n") +
                           "1:\n" + instr_name + " 2f\n" +
                           RepeatInsn(kAddCount2, "Add zero, zero, zero\n") + "2:\n" + instr_name +
                           " 1b\n" + "add zero, zero, zero\n";
    DriverStr(expected, instr_name);
  }

  void BranchCondOneRegHelper(void (riscv64::Riscv64Assembler::*f)(riscv64::XRegister,
                                                                   riscv64::Riscv64Label*),
                              const std::string& instr_name) {
    riscv64::Riscv64Label label;
    (Base::GetAssembler()->*f)(riscv64::A0, &label);
    constexpr size_t kAddCount1 = 63;
    for (size_t i = 0; i != kAddCount1; ++i) {
      __ Add(riscv64::Zero, riscv64::Zero, riscv64::Zero);
    }
    __ Bind(&label);
    constexpr size_t kAddCount2 = 64;
    for (size_t i = 0; i != kAddCount2; ++i) {
      __ Add(riscv64::Zero, riscv64::Zero, riscv64::Zero);
    }
    (Base::GetAssembler()->*f)(riscv64::A1, &label);
    __ Add(riscv64::Zero, riscv64::Zero, riscv64::Zero);

    std::string expected = instr_name + " a0, 1f\n" +
                           RepeatInsn(kAddCount1, "add zero, zero, zero\n") + "1:\n" +
                           RepeatInsn(kAddCount2, "add zero, zero, zero\n") + instr_name +
                           " a1, 1b\n" + "add zero, zero, zero\n";
    DriverStr(expected, instr_name);
  }

  void BranchCondTwoRegsHelper(void (riscv64::Riscv64Assembler::*f)(riscv64::XRegister,
                                                                    riscv64::XRegister,
                                                                    riscv64::Riscv64Label*),
                               const std::string& instr_name) {
    riscv64::Riscv64Label label;
    (Base::GetAssembler()->*f)(riscv64::A0, riscv64::A1, &label);
    constexpr size_t kAddCount1 = 63;
    for (size_t i = 0; i != kAddCount1; ++i) {
      __ Add(riscv64::Zero, riscv64::Zero, riscv64::Zero);
    }
    __ Bind(&label);
    constexpr size_t kAddCount2 = 64;
    for (size_t i = 0; i != kAddCount2; ++i) {
      __ Add(riscv64::Zero, riscv64::Zero, riscv64::Zero);
    }
    (Base::GetAssembler()->*f)(riscv64::A2, riscv64::A3, &label);
    __ Add(riscv64::Zero, riscv64::Zero, riscv64::Zero);

    std::string expected = instr_name + " a0, a1, label\n" +
                           RepeatInsn(kAddCount1, "add zero, zero, zero\n") + "label:\n" +
                           RepeatInsn(kAddCount2, "add zero, zero, zero\n") + instr_name +
                           " a2, a3, label\n" + "add zero, zero, zero\n";
    DriverStr(expected, instr_name);
  }

 private:
  std::vector<riscv64::XRegister*> registers_;
  std::map<riscv64::XRegister, std::string, RISCV64CpuRegisterCompare> secondary_register_names_;

  std::vector<riscv64::FRegister*> fp_registers_;
  // std::vector<riscv64::VectorRegister*> vec_registers_;
  std::vector<riscv64::RoundingMode*> roundingmode_;
  std::map<riscv64::RoundingMode, std::string, RISCV64RoundingModeCompare> roundingmode_names_;

  std::unique_ptr<const Riscv64InstructionSetFeatures> instruction_set_features_;
};

TEST_F(AssemblerRiscv64Test, Toolchain) { EXPECT_TRUE(CheckTools()); }

////////////////
// Arithmetic //
///////////////

TEST_F(AssemblerRiscv64Test, Add) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Add, "add {reg1}, {reg2}, {reg3}"), "add");
}

TEST_F(AssemblerRiscv64Test, Addi) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Addi, -12, "addi {reg1}, {reg2}, {imm}"),
            "addi");
}

TEST_F(AssemblerRiscv64Test, Addw) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Addw, "addw {reg1}, {reg2}, {reg3}"), "addw");
}

TEST_F(AssemblerRiscv64Test, Addiw) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Addiw, -12, "addiw {reg1}, {reg2}, {imm}"),
            "addiw");
}

TEST_F(AssemblerRiscv64Test, Sub) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Sub, "sub {reg1}, {reg2}, {reg3}"), "sub");
}

TEST_F(AssemblerRiscv64Test, Subw) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Subw, "subw {reg1}, {reg2}, {reg3}"), "subw");
}

TEST_F(AssemblerRiscv64Test, Mul) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Mul, "mul {reg1}, {reg2}, {reg3}"), "mul");
}

TEST_F(AssemblerRiscv64Test, Mulh) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Mulh, "mulh {reg1}, {reg2}, {reg3}"), "mulh");
}

TEST_F(AssemblerRiscv64Test, Mulhsu) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Mulhsu, "mulhsu {reg1}, {reg2}, {reg3}"),
            "mulhsu");
}

TEST_F(AssemblerRiscv64Test, Mulhu) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Mulhu, "mulhu {reg1}, {reg2}, {reg3}"), "mulhu");
}

TEST_F(AssemblerRiscv64Test, Mulw) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Mulw, "mulw {reg1}, {reg2}, {reg3}"), "mulw");
}

TEST_F(AssemblerRiscv64Test, Div) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Div, "div {reg1}, {reg2}, {reg3}"), "div");
}

TEST_F(AssemblerRiscv64Test, Divu) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Divu, "divu {reg1}, {reg2}, {reg3}"), "divu");
}

TEST_F(AssemblerRiscv64Test, Divw) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Divw, "divw {reg1}, {reg2}, {reg3}"), "divw");
}

TEST_F(AssemblerRiscv64Test, Divuw) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Divuw, "divuw {reg1}, {reg2}, {reg3}"), "divuw");
}

TEST_F(AssemblerRiscv64Test, Rem) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Rem, "Rem {reg1}, {reg2}, {reg3}"), "Rem");
}

TEST_F(AssemblerRiscv64Test, Remw) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Remw, "Remw {reg1}, {reg2}, {reg3}"), "Remw");
}

//////////////
// Logic//////
//////////////

TEST_F(AssemblerRiscv64Test, And) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::And, "and {reg1}, {reg2}, {reg3}"), "and");
}

TEST_F(AssemblerRiscv64Test, Andi) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Andi, -12, "andi {reg1}, {reg2}, {imm}"),
            "andi");
}

TEST_F(AssemblerRiscv64Test, Neg) {
  DriverStr(RepeatRR(&riscv64::Riscv64Assembler::Neg, "neg {reg1}, {reg2}"), "neg");
}

TEST_F(AssemblerRiscv64Test, Negw) {
  DriverStr(RepeatRR(&riscv64::Riscv64Assembler::Negw, "negw {reg1}, {reg2}"), "negw");
}

TEST_F(AssemblerRiscv64Test, Or) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Or, "or {reg1}, {reg2}, {reg3}"), "or");
}

TEST_F(AssemblerRiscv64Test, Ori) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Ori, -12, "ori {reg1}, {reg2}, {imm}"), "ori");
}

TEST_F(AssemblerRiscv64Test, Xor) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Xor, "xor {reg1}, {reg2}, {reg3}"), "xor");
}

TEST_F(AssemblerRiscv64Test, Xori) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Xori, -12, "xori {reg1}, {reg2}, {imm}"),
            "xori");
}

TEST_F(AssemblerRiscv64Test, Seqz) {
  DriverStr(RepeatRR(&riscv64::Riscv64Assembler::Seqz, "Sltiu {reg1}, {reg2},1"), "seqz");
}
//////////////
// Shift//////
//////////////

TEST_F(AssemblerRiscv64Test, Sll) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Sll, "sll {reg1}, {reg2}, {reg3}"), "sll");
}

TEST_F(AssemblerRiscv64Test, Slli) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Slli, 6, "slli {reg1}, {reg2}, {imm}"), "slli");
}

TEST_F(AssemblerRiscv64Test, Srl) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Srl, "srl {reg1}, {reg2}, {reg3}"), "srl");
}

TEST_F(AssemblerRiscv64Test, Srli) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Srli, 6, "srli {reg1}, {reg2}, {imm}"), "srli");
}

TEST_F(AssemblerRiscv64Test, Sra) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Sra, "sra {reg1}, {reg2}, {reg3}"), "sra");
}

TEST_F(AssemblerRiscv64Test, Srai) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Srai, 6, "srai {reg1}, {reg2}, {imm}"), "srai");
}

TEST_F(AssemblerRiscv64Test, Sllw) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Sllw, "sllw {reg1}, {reg2}, {reg3}"), "sllw");
}

TEST_F(AssemblerRiscv64Test, Slliw) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Slliw, 5, "slliw {reg1}, {reg2}, {imm}"),
            "slliw");
}

TEST_F(AssemblerRiscv64Test, Srlw) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Srlw, "srlw {reg1}, {reg2}, {reg3}"), "srlw");
}

TEST_F(AssemblerRiscv64Test, Srliw) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Srliw, 5, "srliw {reg1}, {reg2}, {imm}"),
            "srliw");
}

TEST_F(AssemblerRiscv64Test, Sraw) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Sraw, "sraw {reg1}, {reg2}, {reg3}"), "sraw");
}

TEST_F(AssemblerRiscv64Test, Sraiw) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Sraiw, 5, "sraiw {reg1}, {reg2}, {imm}"),
            "sraiw");
}

////////////////
//// Loads//////
////////////////

TEST_F(AssemblerRiscv64Test, Lb) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Lb, -12, "lb {reg1}, {imm}({reg2})"), "lb");
}

TEST_F(AssemblerRiscv64Test, Lh) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Lh, -12, "lh {reg1}, {imm}({reg2})"), "lh");
}

TEST_F(AssemblerRiscv64Test, Lw) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Lw, -12, "lw {reg1}, {imm}({reg2})"), "lw");
}

TEST_F(AssemblerRiscv64Test, Ld) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Ld, -12, "ld {reg1}, {imm}({reg2})"), "ld");
}

TEST_F(AssemblerRiscv64Test, Lbu) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Lbu, -12, "lbu {reg1}, {imm}({reg2})"), "lbu");
}

TEST_F(AssemblerRiscv64Test, Lhu) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Lhu, -12, "lhu {reg1}, {imm}({reg2})"), "lhu");
}

TEST_F(AssemblerRiscv64Test, Lwu) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Lwu, -12, "lwu {reg1}, {imm}({reg2})"), "lwu");
}

TEST_F(AssemblerRiscv64Test, Lui) {
  DriverStr(RepeatRIb(&riscv64::Riscv64Assembler::Lui, 20, "lui {reg}, {imm}"), "lui");
}

TEST_F(AssemblerRiscv64Test, Auipc) {
  DriverStr(RepeatRIb(&riscv64::Riscv64Assembler::Auipc, 20, "auipc {reg}, {imm}"), "auipc");
}

TEST_F(AssemblerRiscv64Test, LrD) {
  DriverStr(RepeatRR(&riscv64::Riscv64Assembler::LrD, "lr.d {reg1}, ({reg2})"), "lr.d");
}

TEST_F(AssemblerRiscv64Test, LrDAq) {
  DriverStr(RepeatRR(&riscv64::Riscv64Assembler::LrDAq, "lr.d.aq {reg1}, ({reg2})"), "lr.d.aq");
}

TEST_F(AssemblerRiscv64Test, LrDRl) {
  DriverStr(RepeatRR(&riscv64::Riscv64Assembler::LrDRl, "lr.d.rl {reg1}, ({reg2})"), "lr.d.rl");
}

TEST_F(AssemblerRiscv64Test, LrDAqrl) {
  DriverStr(RepeatRR(&riscv64::Riscv64Assembler::LrDAqrl, "lr.d.aqrl {reg1}, ({reg2})"),
            "lr.d.aqrl");
}

TEST_F(AssemblerRiscv64Test, LrW) {
  DriverStr(RepeatRR(&riscv64::Riscv64Assembler::LrW, "lr.w {reg1}, ({reg2})"), "lr.w");
}

TEST_F(AssemblerRiscv64Test, LrWAq) {
  DriverStr(RepeatRR(&riscv64::Riscv64Assembler::LrWAq, "lr.w.aq {reg1}, ({reg2})"), "lr.w.aq");
}

TEST_F(AssemblerRiscv64Test, LrWRl) {
  DriverStr(RepeatRR(&riscv64::Riscv64Assembler::LrWRl, "lr.w.rl {reg1}, ({reg2})"), "lr.w.rl");
}

TEST_F(AssemblerRiscv64Test, LrWAqrl) {
  DriverStr(RepeatRR(&riscv64::Riscv64Assembler::LrWAqrl, "lr.w.aqrl {reg1}, ({reg2})"),
            "lr.w.aqrl");
}

//////////////
// Store//////
//////////////

TEST_F(AssemblerRiscv64Test, Sb) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Sb, -12, "sb {reg1}, {imm}({reg2})"), "sb");
}

TEST_F(AssemblerRiscv64Test, Sh) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Sh, -12, "sh {reg1}, {imm}({reg2})"), "sh");
}

TEST_F(AssemblerRiscv64Test, Sw) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Sw, -12, "sw {reg1}, {imm}({reg2})"), "sw");
}

TEST_F(AssemblerRiscv64Test, Sd) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Sd, -12, "sd {reg1}, {imm}({reg2})"), "sd");
}

TEST_F(AssemblerRiscv64Test, ScD) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::ScD, "sc.d {reg1}, {reg2}, ({reg3})"), "sc.d");
}

TEST_F(AssemblerRiscv64Test, ScDAq) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::ScDAq, "sc.d.aq {reg1}, {reg2}, ({reg3})"),
            "sc.d.aq");
}

TEST_F(AssemblerRiscv64Test, ScDRl) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::ScDRl, "sc.d.rl {reg1}, {reg2}, ({reg3})"),
            "sc.d.rl");
}

TEST_F(AssemblerRiscv64Test, ScDAqrl) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::ScDAqrl, "sc.d.aqrl {reg1}, {reg2}, ({reg3})"),
            "sc.d.aqrl");
}

TEST_F(AssemblerRiscv64Test, ScW) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::ScW, "sc.w {reg1}, {reg2}, ({reg3})"), "sc.w");
}

TEST_F(AssemblerRiscv64Test, ScWAq) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::ScWAq, "sc.w.aq {reg1}, {reg2}, ({reg3})"),
            "sc.w.aq");
}

TEST_F(AssemblerRiscv64Test, ScWRl) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::ScWRl, "sc.w.rl {reg1}, {reg2}, ({reg3})"),
            "sc.w.rl");
}

TEST_F(AssemblerRiscv64Test, ScWAqrl) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::ScWAqrl, "sc.w.aqrl {reg1}, {reg2}, ({reg3})"),
            "sc.w.aqrl");
}

//////////////
// Compare////
//////////////

TEST_F(AssemblerRiscv64Test, Slt) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Slt, "slt {reg1}, {reg2}, {reg3}"), "slt");
}

TEST_F(AssemblerRiscv64Test, Sltu) {
  DriverStr(RepeatRRR(&riscv64::Riscv64Assembler::Sltu, "sltu {reg1}, {reg2}, {reg3}"), "sltu");
}

TEST_F(AssemblerRiscv64Test, Slti) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Slti, -12, "slti {reg1}, {reg2}, {imm}"),
            "slti");
}

TEST_F(AssemblerRiscv64Test, Sltiu) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Sltiu, -12, "sltiu {reg1}, {reg2}, {imm}"),
            "sltiu");
}

///////////////
// Jump & Link/
///////////////

TEST_F(AssemblerRiscv64Test, Jalr) {
  DriverStr(RepeatRRIb(&riscv64::Riscv64Assembler::Jalr, -12, "jalr {reg1}, {reg2}, {imm}"),
            "jalr");
}

TEST_F(AssemblerRiscv64Test, JalrRs) {
  DriverStr(RepeatR(&riscv64::Riscv64Assembler::Jalr, "jalr {reg}"), "jalr");
}

TEST_F(AssemblerRiscv64Test, Jr) {
  DriverStr(RepeatR(&riscv64::Riscv64Assembler::Jr, "jr {reg}"), "jr");
}

/////////////
// Branches//
/////////////

TEST_F(AssemblerRiscv64Test, Beq) {
  BranchCondTwoRegsHelper(&riscv64::Riscv64Assembler::Beq, "beq");
}

TEST_F(AssemblerRiscv64Test, Bne) {
  BranchCondTwoRegsHelper(&riscv64::Riscv64Assembler::Bne, "bne");
}

TEST_F(AssemblerRiscv64Test, Beqz) {
  BranchCondOneRegHelper(&riscv64::Riscv64Assembler::Beqz, "beqz");
}

TEST_F(AssemblerRiscv64Test, Bnez) {
  BranchCondOneRegHelper(&riscv64::Riscv64Assembler::Bnez, "bnez");
}

TEST_F(AssemblerRiscv64Test, LongBeq) {
  riscv64::Riscv64Label label;
  __ Beq(riscv64::A0, riscv64::A1, &label);
  constexpr uint32_t kAdduCount1 = (1u << 12) + 1;
  for (uint32_t i = 0; i != kAdduCount1; ++i) {
    __ Add(riscv64::Zero, riscv64::Zero, riscv64::Zero);
  }
  __ Bind(&label);
  constexpr uint32_t kAdduCount2 = (1u << 12) + 1;
  for (uint32_t i = 0; i != kAdduCount2; ++i) {
    __ Add(riscv64::Zero, riscv64::Zero, riscv64::Zero);
  }
  __ Beq(riscv64::A2, riscv64::A3, &label);

  uint32_t offset_forward = 2 + kAdduCount1;  // 2: account for auipc and jalr.
  offset_forward <<= 2;
  offset_forward += (offset_forward & 0x800) << 1;  // Account for sign extension in jalr.

  uint32_t offset_back = -(kAdduCount2 + 1);  // 1: account for bne.
  offset_back <<= 2;
  offset_back += (offset_back & 0x800) << 1;  // Account for sign extension in jalr.

  /*reg AT =T2*/
  std::ostringstream oss;
  oss << "bne a0, a1, 1f\n"
         "auipc t2, 0x"
      << std::hex << High20Bits(offset_forward)
      << "\n"
         "Jalr zero, 0x"
      << std::hex << SignExtend64<riscv64::kIImm12Bits>(offset_forward)
      << "(t2)\n"
         "1:\n"
      << RepeatInsn(kAdduCount1, "add zero, zero, zero\n") << "2:\n"
      << RepeatInsn(kAdduCount2, "add zero, zero, zero\n")
      << "bne a2, a3, 3f\n"
         "auipc t2, 0x"
      << std::hex << High20Bits(offset_back)
      << "\n"
         "Jalr zero, 0x"
      << std::hex << SignExtend64<riscv64::kIImm12Bits>(offset_back)
      << "(t2)\n"
         "3:\n";
  std::string expected = oss.str();
  DriverStr(expected, "LongBeq");
}

TEST_F(AssemblerRiscv64Test, LongBne) {
  riscv64::Riscv64Label label;
  __ Bne(riscv64::A0, riscv64::A1, &label);
  constexpr uint32_t kAdduCount1 = (1u << 12) + 1;
  for (uint32_t i = 0; i != kAdduCount1; ++i) {
    __ Add(riscv64::Zero, riscv64::Zero, riscv64::Zero);
  }
  __ Bind(&label);
  constexpr uint32_t kAdduCount2 = (1u << 12) + 1;
  for (uint32_t i = 0; i != kAdduCount2; ++i) {
    __ Add(riscv64::Zero, riscv64::Zero, riscv64::Zero);
  }
  __ Bne(riscv64::A2, riscv64::A3, &label);

  uint32_t offset_forward = 2 + kAdduCount1;  // 2: account for auipc and jalr.
  offset_forward <<= 2;
  offset_forward += (offset_forward & 0x800) << 1;  // Account for sign extension in jalr.

  uint32_t offset_back = -(kAdduCount2 + 1);  // 1: account for bne.
  offset_back <<= 2;
  offset_back += (offset_back & 0x800) << 1;  // Account for sign extension in jalr.

  /*reg AT =T2*/
  std::ostringstream oss;
  oss << "beq a0, a1, 1f\n"
         "auipc t2, 0x"
      << std::hex << High20Bits(offset_forward)
      << "\n"
         "Jalr zero, 0x"
      << std::hex << SignExtend64<riscv64::kIImm12Bits>(offset_forward)
      << "(t2)\n"
         "1:\n"
      << RepeatInsn(kAdduCount1, "add zero, zero, zero\n") << "2:\n"
      << RepeatInsn(kAdduCount2, "add zero, zero, zero\n")
      << "beq a2, a3, 3f\n"
         "auipc t2, 0x"
      << std::hex << High20Bits(offset_back)
      << "\n"
         "Jalr zero, 0x"
      << std::hex << SignExtend64<riscv64::kIImm12Bits>(offset_back)
      << "(t2)\n"
         "3:\n";
  std::string expected = oss.str();
  DriverStr(expected, "LongBne");
}

TEST_F(AssemblerRiscv64Test, LongBeqz) {
  riscv64::Riscv64Label label;
  __ Beqz(riscv64::A0, &label);
  constexpr uint32_t kAdduCount1 = (1u << 12) + 1;
  for (uint32_t i = 0; i != kAdduCount1; ++i) {
    __ Add(riscv64::Zero, riscv64::Zero, riscv64::Zero);
  }
  __ Bind(&label);
  constexpr uint32_t kAdduCount2 = (1u << 12) + 1;
  for (uint32_t i = 0; i != kAdduCount2; ++i) {
    __ Add(riscv64::Zero, riscv64::Zero, riscv64::Zero);
  }
  __ Beqz(riscv64::A1, &label);

  uint32_t offset_forward = 2 + kAdduCount1;  // 2: account for auipc and jalr.
  offset_forward <<= 2;
  offset_forward += (offset_forward & 0x800) << 1;  // Account for sign extension in jalr.

  uint32_t offset_back = -(kAdduCount2 + 1);  // 1: account for bnez.
  offset_back <<= 2;
  offset_back += (offset_back & 0x800) << 1;  // Account for sign extension in jalr.

  /*reg AT =T2*/
  std::ostringstream oss;
  oss << "bnez a0, 1f\n"
         "auipc t2, 0x"
      << std::hex << High20Bits(offset_forward)
      << "\n"
         "jalr zero, 0x"
      << std::hex << SignExtend64<riscv64::kIImm12Bits>(offset_forward)
      << "(t2)\n"
         "1:\n"
      << RepeatInsn(kAdduCount1, "add zero, zero, zero\n") << "2:\n"
      << RepeatInsn(kAdduCount2, "add zero, zero, zero\n")
      << "bnez a1, 3f\n"
         "auipc t2, 0x"
      << std::hex << High20Bits(offset_back)
      << "\n"
         "Jalr zero, 0x"
      << std::hex << SignExtend64<riscv64::kIImm12Bits>(offset_back)
      << "(t2)\n"
         "3:\n";
  std::string expected = oss.str();
  DriverStr(expected, "LongBeqz");
}

TEST_F(AssemblerRiscv64Test, LongBnez) {
  riscv64::Riscv64Label label;
  __ Bnez(riscv64::A0, &label);
  constexpr uint32_t kAdduCount1 = (1u << 12) + 1;
  for (uint32_t i = 0; i != kAdduCount1; ++i) {
    __ Add(riscv64::Zero, riscv64::Zero, riscv64::Zero);
  }
  __ Bind(&label);
  constexpr uint32_t kAdduCount2 = (1u << 12) + 1;
  for (uint32_t i = 0; i != kAdduCount2; ++i) {
    __ Add(riscv64::Zero, riscv64::Zero, riscv64::Zero);
  }
  __ Bnez(riscv64::A1, &label);

  uint32_t offset_forward = 2 + kAdduCount1;  // 2: account for auipc and jalr.
  offset_forward <<= 2;
  offset_forward += (offset_forward & 0x800) << 1;  // Account for sign extension in jalr.

  uint32_t offset_back = -(kAdduCount2 + 1);  // 1: account for bnez.
  offset_back <<= 2;
  offset_back += (offset_back & 0x800) << 1;  // Account for sign extension in jalr.

  /*reg AT =T2*/
  std::ostringstream oss;
  oss << "beqz a0, 1f\n"
         "auipc t2, 0x"
      << std::hex << High20Bits(offset_forward)
      << "\n"
         "jalr zero, 0x"
      << std::hex << SignExtend64<riscv64::kIImm12Bits>(offset_forward)
      << "(t2)\n"
         "1:\n"
      << RepeatInsn(kAdduCount1, "add zero, zero, zero\n") << "2:\n"
      << RepeatInsn(kAdduCount2, "add zero, zero, zero\n")
      << "beqz a1, 3f\n"
         "auipc t2, 0x"
      << std::hex << High20Bits(offset_back)
      << "\n"
         "Jalr zero, 0x"
      << std::hex << SignExtend64<riscv64::kIImm12Bits>(offset_back)
      << "(t2)\n"
         "3:\n";
  std::string expected = oss.str();
  DriverStr(expected, "LongBnez");
}

TEST_F(AssemblerRiscv64Test, LongJ) {
  riscv64::Riscv64Label label1, label2;
  __ J(&label1);
  constexpr uint32_t kAdduCount1 = (1u << 20) + 1;
  for (uint32_t i = 0; i != kAdduCount1; ++i) {
    __ Add(riscv64::Zero, riscv64::Zero, riscv64::Zero);
  }
  __ Bind(&label1);
  __ J(&label2);
  constexpr uint32_t kAdduCount2 = (1u << 20) + 1;
  for (uint32_t i = 0; i != kAdduCount2; ++i) {
    __ Add(riscv64::Zero, riscv64::Zero, riscv64::Zero);
  }
  __ Bind(&label2);
  __ J(&label1);

  uint32_t offset_forward1 = 2 + kAdduCount1;  // 2: account for auipc and jalr.
  offset_forward1 <<= 2;
  offset_forward1 += (offset_forward1 & 0x800) << 1;  // Account for sign extension in jalr.

  uint32_t offset_forward2 = 2 + kAdduCount2;  // 2: account for auipc and jalr.
  offset_forward2 <<= 2;
  offset_forward2 += (offset_forward2 & 0x800) << 1;  // Account for sign extension in jalr.

  uint32_t offset_back = -(2 + kAdduCount2);  // 2: account for auipc and jalr.
  offset_back <<= 2;
  offset_back += (offset_back & 0x800) << 1;  // Account for sign extension in jalr.

  /*reg AT =T2*/
  std::ostringstream oss;
  oss << "auipc t2, 0x" << std::hex << High20Bits(offset_forward1)
      << "\n"
         "jalr zero, 0x"
      << std::hex << SignExtend64<riscv64::kIImm12Bits>(offset_forward1)
      << "(t2)\n"
         "1:\n"
      << RepeatInsn(kAdduCount1, "add zero, zero, zero\n") << "auipc t2, 0x" << std::hex
      << High20Bits(offset_forward2)
      << "\n"
         "jalr zero, 0x"
      << std::hex << SignExtend64<riscv64::kIImm12Bits>(offset_forward2)
      << "(t2)\n"
         "2:\n"
      << RepeatInsn(kAdduCount2, "add zero, zero, zero\n") << "auipc t2, 0x" << std::hex
      << High20Bits(offset_back)
      << "\n"
         "jalr zero, 0x"
      << std::hex << SignExtend64<riscv64::kIImm12Bits>(offset_back) << "(t2)\n";
  std::string expected = oss.str();
  DriverStr(expected, "LongJ");
}

TEST_F(AssemblerRiscv64Test, LongCall) {
  riscv64::Riscv64Label label1, label2;
  __ Call(riscv64::A1, &label1);
  constexpr uint32_t kAdduCount1 = (1u << 20) + 1;
  for (uint32_t i = 0; i != kAdduCount1; ++i) {
    __ Add(riscv64::Zero, riscv64::Zero, riscv64::Zero);
  }
  __ Bind(&label1);
  __ Call(riscv64::A1, &label2);
  constexpr uint32_t kAdduCount2 = (1u << 20) + 1;
  for (uint32_t i = 0; i != kAdduCount2; ++i) {
    __ Add(riscv64::Zero, riscv64::Zero, riscv64::Zero);
  }
  __ Bind(&label2);
  __ Call(riscv64::A1, &label1);

  uint32_t offset_forward1 = 2 + kAdduCount1;  // 2: account for auipc and jalr.
  offset_forward1 <<= 2;
  offset_forward1 += (offset_forward1 & 0x800) << 1;  // Account for sign extension in jalr.

  uint32_t offset_forward2 = 2 + kAdduCount2;  // 2: account for auipc and jalr.
  offset_forward2 <<= 2;
  offset_forward2 += (offset_forward2 & 0x800) << 1;  // Account for sign extension in jalr.

  uint32_t offset_back = -(2 + kAdduCount2);  // 2: account for auipc and jalr.
  offset_back <<= 2;
  offset_back += (offset_back & 0x800) << 1;  // Account for sign extension in jalr.

  /*reg AT =T2*/
  std::ostringstream oss;
  oss << "auipc t2, 0x" << std::hex << High20Bits(offset_forward1)
      << "\n"
         "jalr a1, 0x"
      << std::hex << SignExtend64<riscv64::kIImm12Bits>(offset_forward1)
      << "(t2)\n"
         "1:\n"
      << RepeatInsn(kAdduCount1, "add zero, zero, zero\n") << "auipc t2, 0x" << std::hex
      << High20Bits(offset_forward2)
      << "\n"
         "jalr a1, 0x"
      << std::hex << SignExtend64<riscv64::kIImm12Bits>(offset_forward2)
      << "(t2)\n"
         "2:\n"
      << RepeatInsn(kAdduCount2, "add zero, zero, zero\n") << "auipc t2, 0x" << std::hex
      << High20Bits(offset_back)
      << "\n"
         "jalr a1, 0x"
      << std::hex << SignExtend64<riscv64::kIImm12Bits>(offset_back) << "(t2)\n";
  std::string expected = oss.str();
  DriverStr(expected, "LongCall");
}

TEST_F(AssemblerRiscv64Test, J) { BranchHelper(&riscv64::Riscv64Assembler::J, "j"); }

TEST_F(AssemblerRiscv64Test, Call) { BranchHelper(&riscv64::Riscv64Assembler::Call, "jal"); }

TEST_F(AssemblerRiscv64Test, Blt) {
  BranchCondTwoRegsHelper(&riscv64::Riscv64Assembler::Blt, "blt");
}

TEST_F(AssemblerRiscv64Test, Bge) {
  BranchCondTwoRegsHelper(&riscv64::Riscv64Assembler::Bge, "bge");
}

TEST_F(AssemblerRiscv64Test, Bltu) {
  BranchCondTwoRegsHelper(&riscv64::Riscv64Assembler::Bltu, "bltu");
}

TEST_F(AssemblerRiscv64Test, Bgeu) {
  BranchCondTwoRegsHelper(&riscv64::Riscv64Assembler::Bgeu, "bgeu");
}

TEST_F(AssemblerRiscv64Test, Bgt) {
  BranchCondTwoRegsHelper(&riscv64::Riscv64Assembler::Bgt, "bgt");
}

TEST_F(AssemblerRiscv64Test, Ble) {
  BranchCondTwoRegsHelper(&riscv64::Riscv64Assembler::Ble, "ble");
}

TEST_F(AssemblerRiscv64Test, Bgtu) {
  BranchCondTwoRegsHelper(&riscv64::Riscv64Assembler::Bgtu, "bgtu");
}

TEST_F(AssemblerRiscv64Test, Bleu) {
  BranchCondTwoRegsHelper(&riscv64::Riscv64Assembler::Bleu, "bleu");
}

TEST_F(AssemblerRiscv64Test, Blez) {
  BranchCondOneRegHelper(&riscv64::Riscv64Assembler::Blez, "blez");
}

TEST_F(AssemblerRiscv64Test, Bgez) {
  BranchCondOneRegHelper(&riscv64::Riscv64Assembler::Bgez, "bgez");
}

TEST_F(AssemblerRiscv64Test, Bltz) {
  BranchCondOneRegHelper(&riscv64::Riscv64Assembler::Bltz, "bltz");
}

TEST_F(AssemblerRiscv64Test, Bgtz) {
  BranchCondOneRegHelper(&riscv64::Riscv64Assembler::Bgtz, "bgtz");
}

/**************************************/
/** Floating Single-Precision begins **/
/**************************************/

//////////////
// Arithmetic/
//////////////

TEST_F(AssemblerRiscv64Test, FaddS) {
  DriverStr(RepeatFFFRoundingMode(&riscv64::Riscv64Assembler::FaddS,
                                  "fadd.s {reg1}, {reg2}, {reg3}, {rm}"),
            "fadd.s");
}

TEST_F(AssemblerRiscv64Test, FsubS) {
  DriverStr(RepeatFFFRoundingMode(&riscv64::Riscv64Assembler::FsubS,
                                  "fsub.s {reg1}, {reg2}, {reg3}, {rm}"),
            "fsub.s");
}

TEST_F(AssemblerRiscv64Test, FmulS) {
  DriverStr(RepeatFFFRoundingMode(&riscv64::Riscv64Assembler::FmulS,
                                  "fmul.s {reg1}, {reg2}, {reg3}, {rm}"),
            "fmul.s");
}

TEST_F(AssemblerRiscv64Test, FmaddS) {
  DriverStr(RepeatFFFFRoundingMode(&riscv64::Riscv64Assembler::FmaddS,
                                   "fmadd.s {reg1}, {reg2}, {reg3}, {reg4}, {rm}"),
            "fmadd.s");
  /*
    // Debug Test Mode for single instruction Test
    __ FmaddS(riscv64::FT1, riscv64::FT2, riscv64::FT3, riscv64::FT4);
    DriverStr("fmadd.s ft1, ft2, ft3, ft4",
                  "fmadd.s");
  */
}

TEST_F(AssemblerRiscv64Test, FmsubS) {
  DriverStr(RepeatFFFFRoundingMode(&riscv64::Riscv64Assembler::FmsubS,
                                   "fmsub.s {reg1}, {reg2}, {reg3}, {reg4}, {rm}"),
            "fmsub.s");
}

TEST_F(AssemblerRiscv64Test, FnmaddS) {
  DriverStr(RepeatFFFFRoundingMode(&riscv64::Riscv64Assembler::FnmaddS,
                                   "fnmadd.s {reg1}, {reg2}, {reg3}, {reg4}, {rm}"),
            "fnmadd.s");
}

TEST_F(AssemblerRiscv64Test, FnmsubS) {
  DriverStr(RepeatFFFFRoundingMode(&riscv64::Riscv64Assembler::FnmsubS,
                                   "fnmsub.s {reg1}, {reg2}, {reg3}, {reg4}, {rm}"),
            "fnmsub.s");
}

TEST_F(AssemblerRiscv64Test, FdivS) {
  DriverStr(RepeatFFFRoundingMode(&riscv64::Riscv64Assembler::FdivS,
                                  "fdiv.s {reg1}, {reg2}, {reg3}, {rm}"),
            "fdiv.s");
}

TEST_F(AssemblerRiscv64Test, FsqrtS) {
  DriverStr(
      RepeatFFRoundingMode(&riscv64::Riscv64Assembler::FsqrtS, "fsqrt.s {reg1}, {reg2}, {rm}"),
      "fsqrt.s");
}

TEST_F(AssemblerRiscv64Test, FmvS) {
  DriverStr(RepeatFF(&riscv64::Riscv64Assembler::FmvS, "fmv.s {reg1}, {reg2}"), "fmv.s");
}

TEST_F(AssemblerRiscv64Test, FmvWX) {
  DriverStr(RepeatFR(&riscv64::Riscv64Assembler::FmvWX, "fmv.w.x {reg1}, {reg2}"), "fmv.w.x");
}

TEST_F(AssemblerRiscv64Test, FmvXW) {
  DriverStr(RepeatRF(&riscv64::Riscv64Assembler::FmvXW, "fmv.x.w {reg1}, {reg2}"), "fmv.x.w");
}

TEST_F(AssemblerRiscv64Test, FclassS) {
  DriverStr(RepeatRF(&riscv64::Riscv64Assembler::FclassS, "fclass.s {reg1}, {reg2}"), "fclass.s");
}

TEST_F(AssemblerRiscv64Test, FcvtLS) {
  DriverStr(
      RepeatRFRoundingMode(&riscv64::Riscv64Assembler::FcvtLS, "fcvt.l.s {reg1}, {reg2}, {rm}"),
      "fcvt.l.s");
}

TEST_F(AssemblerRiscv64Test, FcvtLuS) {
  DriverStr(
      RepeatRFRoundingMode(&riscv64::Riscv64Assembler::FcvtLuS, "fcvt.lu.s {reg1}, {reg2}, {rm}"),
      "fcvt.lu.s");
}

TEST_F(AssemblerRiscv64Test, FcvtWS) {
  DriverStr(
      RepeatRFRoundingMode(&riscv64::Riscv64Assembler::FcvtWS, "fcvt.w.s {reg1}, {reg2}, {rm}"),
      "fcvt.w.s");
}

TEST_F(AssemblerRiscv64Test, FcvtWuS) {
  DriverStr(
      RepeatRFRoundingMode(&riscv64::Riscv64Assembler::FcvtWuS, "fcvt.wu.s {reg1}, {reg2}, {rm}"),
      "fcvt.wu.s");
}

TEST_F(AssemblerRiscv64Test, FcvtSL) {
  DriverStr(
      RepeatFRRoundingMode(&riscv64::Riscv64Assembler::FcvtSL, "fcvt.s.l {reg1}, {reg2}, {rm}"),
      "fcvt.s.l");
}

TEST_F(AssemblerRiscv64Test, FcvtSLu) {
  DriverStr(
      RepeatFRRoundingMode(&riscv64::Riscv64Assembler::FcvtSLu, "fcvt.s.lu {reg1}, {reg2}, {rm}"),
      "fcvt.s.lu");
}

TEST_F(AssemblerRiscv64Test, FcvtSW) {
  DriverStr(
      RepeatFRRoundingMode(&riscv64::Riscv64Assembler::FcvtSW, "fcvt.s.w {reg1}, {reg2}, {rm}"),
      "fcvt.s.w");
}

TEST_F(AssemblerRiscv64Test, FcvtSWu) {
  DriverStr(
      RepeatFRRoundingMode(&riscv64::Riscv64Assembler::FcvtSWu, "fcvt.s.wu {reg1}, {reg2}, {rm}"),
      "fcvt.s.wu");
}

TEST_F(AssemblerRiscv64Test, FmaxS) {
  DriverStr(RepeatFFF(&riscv64::Riscv64Assembler::FmaxS, "fmax.s {reg1}, {reg2}, {reg3}"),
            "fmax.s");
}

TEST_F(AssemblerRiscv64Test, FminS) {
  DriverStr(RepeatFFF(&riscv64::Riscv64Assembler::FminS, "fmin.s {reg1}, {reg2}, {reg3}"),
            "fmin.s");
}

TEST_F(AssemblerRiscv64Test, FabsS) {
  DriverStr(RepeatFF(&riscv64::Riscv64Assembler::FabsS, "fsgnjx.s {reg1}, {reg2}, {reg2}"),
            "fsgnjx.s");
}

TEST_F(AssemblerRiscv64Test, FnegS) {
  DriverStr(RepeatFF(&riscv64::Riscv64Assembler::FnegS, "fsgnjn.s {reg1}, {reg2}, {reg2}"),
            "fsgnjn.s");
}

TEST_F(AssemblerRiscv64Test, FsgnjS) {
  DriverStr(RepeatFFF(&riscv64::Riscv64Assembler::FsgnjS, "fsgnj.s {reg1}, {reg2}, {reg3}"),
            "fsgnj.s");
}

TEST_F(AssemblerRiscv64Test, FsgnjnS) {
  DriverStr(RepeatFFF(&riscv64::Riscv64Assembler::FsgnjnS, "fsgnjn.s {reg1}, {reg2}, {reg3}"),
            "fsgnjn.s");
}

TEST_F(AssemblerRiscv64Test, FsgnjxS) {
  DriverStr(RepeatFFF(&riscv64::Riscv64Assembler::FsgnjxS, "fsgnjx.s {reg1}, {reg2}, {reg3}"),
            "fsgnjx.s");
}

/////////////
// Compare///
/////////////

TEST_F(AssemblerRiscv64Test, FeqS) {
  DriverStr(RepeatRFF(&riscv64::Riscv64Assembler::FeqS, "feq.s {reg1}, {reg2}, {reg3}"), "feq.s");
}

TEST_F(AssemblerRiscv64Test, FleS) {
  DriverStr(RepeatRFF(&riscv64::Riscv64Assembler::FleS, "fle.s {reg1}, {reg2}, {reg3}"), "fle.s");
}

TEST_F(AssemblerRiscv64Test, FltS) {
  DriverStr(RepeatRFF(&riscv64::Riscv64Assembler::FltS, "flt.s {reg1}, {reg2}, {reg3}"), "flt.s");
}

////////////
// Load/////
////////////

TEST_F(AssemblerRiscv64Test, Flw) {
  DriverStr(RepeatFRIb(&riscv64::Riscv64Assembler::Flw, -12, "flw {reg1}, {imm}({reg2})"), "flw");
}

////////////
// Store////
////////////

TEST_F(AssemblerRiscv64Test, Fsw) {
  DriverStr(RepeatFRIb(&riscv64::Riscv64Assembler::Fsw, -12, "fsw {reg1}, {imm}({reg2})"), "fsw");
}

/************************************/
/** Floating Single-Precision ends **/
/************************************/

/**************************************/
/** Floating Double-Precision begins **/
/**************************************/

//////////////
// Arithmetic/
//////////////

TEST_F(AssemblerRiscv64Test, FaddD) {
  DriverStr(RepeatFFFRoundingMode(&riscv64::Riscv64Assembler::FaddD,
                                  "fadd.d {reg1}, {reg2}, {reg3}, {rm}"),
            "fadd.d");
}

TEST_F(AssemblerRiscv64Test, FsubD) {
  DriverStr(RepeatFFFRoundingMode(&riscv64::Riscv64Assembler::FsubD,
                                  "fsub.d {reg1}, {reg2}, {reg3}, {rm}"),
            "fsub.d");
}

TEST_F(AssemblerRiscv64Test, FmulD) {
  DriverStr(RepeatFFFRoundingMode(&riscv64::Riscv64Assembler::FmulD,
                                  "fmul.d {reg1}, {reg2}, {reg3}, {rm}"),
            "fmul.d");
}

TEST_F(AssemblerRiscv64Test, FmaddD) {
  DriverStr(RepeatFFFFRoundingMode(&riscv64::Riscv64Assembler::FmaddD,
                                   "fmadd.d {reg1}, {reg2}, {reg3}, {reg4}, {rm}"),
            "fmadd.d");
}

TEST_F(AssemblerRiscv64Test, FmsubD) {
  DriverStr(RepeatFFFFRoundingMode(&riscv64::Riscv64Assembler::FmsubD,
                                   "fmsub.d {reg1}, {reg2}, {reg3}, {reg4}, {rm}"),
            "fmsub.d");
}

TEST_F(AssemblerRiscv64Test, FdivD) {
  DriverStr(RepeatFFFRoundingMode(&riscv64::Riscv64Assembler::FdivD,
                                  "fdiv.d {reg1}, {reg2}, {reg3}, {rm}"),
            "fdiv.d");
}

TEST_F(AssemblerRiscv64Test, FsqrtD) {
  DriverStr(
      RepeatFFRoundingMode(&riscv64::Riscv64Assembler::FsqrtD, "fsqrt.d {reg1}, {reg2}, {rm}"),
      "fsqrt.d");
}

TEST_F(AssemblerRiscv64Test, FmvDX) {
  DriverStr(RepeatFR(&riscv64::Riscv64Assembler::FmvDX, "fmv.d.x {reg1}, {reg2}"), "fmv.d.x");
}

TEST_F(AssemblerRiscv64Test, FmvXD) {
  DriverStr(RepeatRF(&riscv64::Riscv64Assembler::FmvXD, "fmv.x.d {reg1}, {reg2}"), "fmv.x.d");
}

TEST_F(AssemblerRiscv64Test, FmvD) {
  DriverStr(RepeatFF(&riscv64::Riscv64Assembler::FmvD, "fmv.d {reg1}, {reg2}"), "fmv.d");
}

TEST_F(AssemblerRiscv64Test, FclassD) {
  DriverStr(RepeatRF(&riscv64::Riscv64Assembler::FclassD, "fclass.d {reg1}, {reg2}"), "fclass.d");
}

TEST_F(AssemblerRiscv64Test, FcvtLD) {
  DriverStr(
      RepeatRFRoundingMode(&riscv64::Riscv64Assembler::FcvtLD, "fcvt.l.d {reg1}, {reg2}, {rm}"),
      "fcvt.l.d");
}

TEST_F(AssemblerRiscv64Test, FcvtLuD) {
  DriverStr(
      RepeatRFRoundingMode(&riscv64::Riscv64Assembler::FcvtLuD, "fcvt.lu.d {reg1}, {reg2}, {rm}"),
      "fcvt.lu.d");
}

TEST_F(AssemblerRiscv64Test, FcvtWD) {
  DriverStr(
      RepeatRFRoundingMode(&riscv64::Riscv64Assembler::FcvtWD, "fcvt.w.d {reg1}, {reg2}, {rm}"),
      "fcvt.w.d");
}

TEST_F(AssemblerRiscv64Test, FcvtWuD) {
  DriverStr(
      RepeatRFRoundingMode(&riscv64::Riscv64Assembler::FcvtWuD, "fcvt.wu.d {reg1}, {reg2}, {rm}"),
      "fcvt.wu.d");
}

TEST_F(AssemblerRiscv64Test, FcvtDL) {
  DriverStr(
      RepeatFRRoundingMode(&riscv64::Riscv64Assembler::FcvtDL, "fcvt.d.l {reg1}, {reg2}, {rm}"),
      "fcvt.d.l");
}

TEST_F(AssemblerRiscv64Test, FcvtDLu) {
  DriverStr(
      RepeatFRRoundingMode(&riscv64::Riscv64Assembler::FcvtDLu, "fcvt.d.lu {reg1}, {reg2}, {rm}"),
      "fcvt.d.lu");
}

TEST_F(AssemblerRiscv64Test, FcvtDW) {
  DriverStr(RepeatFR(&riscv64::Riscv64Assembler::FcvtDW, "fcvt.d.w {reg1}, {reg2}"), "fcvt.d.w");
}

TEST_F(AssemblerRiscv64Test, FcvtDWu) {
  DriverStr(RepeatFR(&riscv64::Riscv64Assembler::FcvtDWu, "fcvt.d.wu {reg1}, {reg2}"), "fcvt.d.wu");
}

TEST_F(AssemblerRiscv64Test, FcvtDS) {
  DriverStr(RepeatFF(&riscv64::Riscv64Assembler::FcvtDS, "fcvt.d.s {reg1}, {reg2}"), "fcvt.d.s");
}

TEST_F(AssemblerRiscv64Test, FcvtSD) {
  DriverStr(
      RepeatFFRoundingMode(&riscv64::Riscv64Assembler::FcvtSD, "fcvt.s.d {reg1}, {reg2}, {rm}"),
      "fcvt.s.d");
}

TEST_F(AssemblerRiscv64Test, FmaxD) {
  DriverStr(RepeatFFF(&riscv64::Riscv64Assembler::FmaxD, "fmax.d {reg1}, {reg2}, {reg3}"),
            "fmax.d");
}

TEST_F(AssemblerRiscv64Test, FminD) {
  DriverStr(RepeatFFF(&riscv64::Riscv64Assembler::FminD, "fmin.d {reg1}, {reg2}, {reg3}"),
            "fmin.d");
}

TEST_F(AssemblerRiscv64Test, FabsD) {
  DriverStr(RepeatFF(&riscv64::Riscv64Assembler::FabsD, "fsgnjx.d {reg1}, {reg2}, {reg2}"),
            "fsgnjx.d");
}

TEST_F(AssemblerRiscv64Test, FnegD) {
  DriverStr(RepeatFF(&riscv64::Riscv64Assembler::FnegD, "fsgnjn.d {reg1}, {reg2}, {reg2}"),
            "fsgnjn.d");
}

TEST_F(AssemblerRiscv64Test, FsgnjD) {
  DriverStr(RepeatFFF(&riscv64::Riscv64Assembler::FsgnjD, "fsgnj.d {reg1}, {reg2}, {reg3}"),
            "fsgnj.d");
}

TEST_F(AssemblerRiscv64Test, FsgnjnD) {
  DriverStr(RepeatFFF(&riscv64::Riscv64Assembler::FsgnjnD, "fsgnjn.d {reg1}, {reg2}, {reg3}"),
            "fsgnjn.d");
}

TEST_F(AssemblerRiscv64Test, FsgnjxD) {
  DriverStr(RepeatFFF(&riscv64::Riscv64Assembler::FsgnjxD, "fsgnjx.d {reg1}, {reg2}, {reg3}"),
            "fsgnjx.d");
}

/////////////
// Compare///
/////////////

TEST_F(AssemblerRiscv64Test, FeqD) {
  DriverStr(RepeatRFF(&riscv64::Riscv64Assembler::FeqD, "feq.d {reg1}, {reg2}, {reg3}"), "feq.d");
}

TEST_F(AssemblerRiscv64Test, FleD) {
  DriverStr(RepeatRFF(&riscv64::Riscv64Assembler::FleD, "fle.d {reg1}, {reg2}, {reg3}"), "fle.d");
}

TEST_F(AssemblerRiscv64Test, FltD) {
  DriverStr(RepeatRFF(&riscv64::Riscv64Assembler::FltD, "flt.d {reg1}, {reg2}, {reg3}"), "flt.d");
}

//////////
// Load///
//////////

TEST_F(AssemblerRiscv64Test, Fld) {
  DriverStr(RepeatFRIb(&riscv64::Riscv64Assembler::Fld, -12, "fld {reg1}, {imm}({reg2})"), "fld");
}

//////////
// Store//
//////////

TEST_F(AssemblerRiscv64Test, Fsd) {
  DriverStr(RepeatFRIb(&riscv64::Riscv64Assembler::Fsd, -12, "fsd {reg1}, {imm}({reg2})"), "fsd");
}

/************************************/
/** Floating Double-Precision ends **/
/************************************/

TEST_F(AssemblerRiscv64Test, Mv) {
  DriverStr(RepeatRR(&riscv64::Riscv64Assembler::Mv, "addi {reg1}, {reg2}, 0"), "mv");
}

TEST_F(AssemblerRiscv64Test, Ebreak) {
  __ Ebreak();
  const char* expected = "ebreak\n";
  DriverStr(expected, "ebreak");
}

TEST_F(AssemblerRiscv64Test, Nop) {
  __ Nop();
  const char* expected = "nop\n";
  DriverStr(expected, "nop");
}

TEST_F(AssemblerRiscv64Test, Ret) {
  __ Ret();
  const char* expected = "ret\n";
  DriverStr(expected, "ret");
}

TEST_F(AssemblerRiscv64Test, Fence) {
  __ Fence(riscv64::FenceParameters::fence_write, riscv64::FenceParameters::fence_write);
  __ Fence(riscv64::FenceParameters::fence_write,
           riscv64::FenceParameters::fence_read | riscv64::FenceParameters::fence_write);
  __ Fence(riscv64::FenceParameters::fence_write,
           riscv64::FenceParameters::fence_input | riscv64::FenceParameters::fence_output |
               riscv64::FenceParameters::fence_read | riscv64::FenceParameters::fence_write);
  __ Fence(riscv64::FenceParameters::fence_read,
           riscv64::FenceParameters::fence_read | riscv64::FenceParameters::fence_write);
  __ Fence(riscv64::FenceParameters::fence_input | riscv64::FenceParameters::fence_output |
               riscv64::FenceParameters::fence_read,
           riscv64::FenceParameters::fence_read | riscv64::FenceParameters::fence_write);
  __ Fence(riscv64::FenceParameters::fence_output | riscv64::FenceParameters::fence_read,
           riscv64::FenceParameters::fence_read | riscv64::FenceParameters::fence_write);
  __ Fence(riscv64::FenceParameters::fence_read | riscv64::FenceParameters::fence_write,
           riscv64::FenceParameters::fence_read | riscv64::FenceParameters::fence_write);
  __ Fence(riscv64::FenceParameters::fence_input | riscv64::FenceParameters::fence_output |
               riscv64::FenceParameters::fence_read,
           riscv64::FenceParameters::fence_input | riscv64::FenceParameters::fence_read |
               riscv64::FenceParameters::fence_write);
  __ Fence(riscv64::FenceParameters::fence_input | riscv64::FenceParameters::fence_output |
               riscv64::FenceParameters::fence_read | riscv64::FenceParameters::fence_write,
           riscv64::FenceParameters::fence_input | riscv64::FenceParameters::fence_read |
               riscv64::FenceParameters::fence_write);
  __ Fence();

  const char* expected =
      "fence w, w\n"
      "fence w, rw\n"
      "fence w, iorw\n"
      "fence r, rw\n"
      "fence ior, rw\n"
      "fence or, rw\n"
      "fence rw, rw\n"
      "fence ior, irw\n"
      "fence iorw, irw\n"
      "fence\n";
  DriverStr(expected, "fence");
}

// Execute "Li" test after adding instructions optimization
#if 0
TEST_F(AssemblerRiscv64Test, Li) {
  DriverStr(RepeatRIb(&riscv64::Riscv64Assembler::Li, -64, "li {reg}, {imm}"), "li");
/*
  // Debug Test Mode for single instruction Test
  __ Li(riscv64::T0, 0x8888);
  DriverStr("li t0, 0x8888",
                "li");
*/

/*
  // Debug Test mode for self controlled Exhaustive Test
#if __WORDSIZE == 64
  char asmStrBuf[50] = {0};
  int64_t i = 0x8000000000000000;
  int64_t prev_i = 0x8000000000000000;

  for(i = 0x8000000000000000; i <= 0x7FFFFFFFFFFFFFFF; i+=10000) {
    if(prev_i > i) {
      break;
    }

    SetUp();
    sprintf(asmStrBuf, "li t0, %ld\n", i);
    __ Li(riscv64::T0, i);
   std::string asmStr(asmStrBuf);

   DriverStr(asmStr,
                "li");
   TearDown();
   prev_i = i;
  }
#endif
*/
/*
  // Debug Test mode for self controlled passtern Test
  char asmStrBuf[100000] = {0};
  long i;

  memset(asmStrBuf, 0, sizeof(asmStrBuf));

  for(i = 0; i < 10; i ++) {

    sprintf(asmStrBuf + strlen(asmStrBuf), "li t0, %ld\n", i);
    __ Li(riscv64::T0, i);

  }

   std::string asmStr(asmStrBuf);
   DriverStr(asmStr,
                "li");
*/
}
#endif

TEST_F(AssemblerRiscv64Test, LoadFarthestNearLabelAddress) {
  riscv64::Riscv64Label label;
  __ LoadLabelAddress(riscv64::A1, &label);
  constexpr uint32_t kAdduCount = 0x3FF;
  for (uint32_t i = 0; i != kAdduCount; ++i) {
    __ Add(riscv64::Zero, riscv64::Zero, riscv64::Zero);
  }
  __ Bind(&label);

  uint32_t offset_forward = 2 + kAdduCount;  // 2: account for auipc and addi.
  offset_forward <<= 2;
  offset_forward += (offset_forward & 0x800) << 1;  // Account for sign extension in addi.

  /*reg AT =T2*/
  std::ostringstream oss;
  oss << "auipc t2, 0x" << std::hex << High20Bits(offset_forward)
      << "\n"
         "addi a1, t2, 0x"
      << std::hex << SignExtend64<riscv64::kIImm12Bits>(offset_forward) << "\n"
      << RepeatInsn(kAdduCount, "add zero, zero, zero\n");
  std::string expected = oss.str();
  DriverStr(expected, "LoadFarthestNearLabelAddress");
  EXPECT_EQ(__ GetLabelLocation(&label), (2 + kAdduCount) * 4);
}

TEST_F(AssemblerRiscv64Test, LoadNearestLiteral) {
  riscv64::Literal* literal = __ NewLiteral<uint32_t>(0x12345678);
  __ LoadLiteral(riscv64::A1, riscv64::kLoadWord, literal);
  constexpr uint32_t kAdduCount = 0xFFDFF;
  for (uint32_t i = 0; i != kAdduCount; ++i) {
    __ Add(riscv64::Zero, riscv64::Zero, riscv64::Zero);
  }

  uint32_t offset_forward = 2 + kAdduCount;  // 2: account for auipc and lw.
  offset_forward <<= 2;
  offset_forward += (offset_forward & 0x800) << 1;  // Account for sign extension in lw.

  /*reg AT =T2*/
  std::ostringstream oss;
  oss << "auipc t2, 0x" << std::hex << High20Bits(offset_forward)
      << "\n"
         "lw a1, 0x"
      << std::hex << SignExtend64<riscv64::kIImm12Bits>(offset_forward) << "(t2)\n"
      << RepeatInsn(kAdduCount, "add zero, zero, zero\n") << ".word 0x12345678\n";
  std::string expected = oss.str();
  DriverStr(expected, "LoadNearestLiteral");
  EXPECT_EQ(__ GetLabelLocation(literal->GetLabel()), (2 + kAdduCount) * 4);
}

TEST_F(AssemblerRiscv64Test, LoadNearestLiteralUnsigned) {
  riscv64::Literal* literal = __ NewLiteral<uint32_t>(0x12345678);
  __ LoadLiteral(riscv64::A1, riscv64::kLoadUnsignedWord, literal);
  constexpr uint32_t kAdduCount = 0x3FFDF;
  for (uint32_t i = 0; i != kAdduCount; ++i) {
    __ Add(riscv64::Zero, riscv64::Zero, riscv64::Zero);
  }

  uint32_t offset_forward = 2 + kAdduCount;  // 2: account for auipc and lwu.
  offset_forward <<= 2;
  offset_forward += (offset_forward & 0x800) << 1;  // Account for sign extension in lwu.

  /*reg AT =T2*/
  std::ostringstream oss;
  oss << "auipc t2, 0x" << std::hex << High20Bits(offset_forward)
      << "\n"
         "lwu a1, 0x"
      << std::hex << SignExtend64<riscv64::kIImm12Bits>(offset_forward) << "(t2)\n"
      << RepeatInsn(kAdduCount, "add zero, zero, zero\n") << ".word 0x12345678\n";
  std::string expected = oss.str();
  DriverStr(expected, "LoadNearestLiteralUnsigned");
  EXPECT_EQ(__ GetLabelLocation(literal->GetLabel()), (2 + kAdduCount) * 4);
}

TEST_F(AssemblerRiscv64Test, LoadNearestLiteralLongNoAlignment) {
  riscv64::Literal* literal = __ NewLiteral<uint64_t>(UINT64_C(0x0123456789ABCDEF));
  __ LoadLiteral(riscv64::A1, riscv64::kLoadDoubleword, literal);
  constexpr uint32_t kAdduCount = 0x3FFD;
  for (uint32_t i = 0; i != kAdduCount; ++i) {
    __ Add(riscv64::Zero, riscv64::Zero, riscv64::Zero);
  }

  uint32_t offset_forward = 2 + kAdduCount + 1;  // 2: account for auipc, andi and ld.
  offset_forward <<= 2;
  offset_forward += (offset_forward & 0x800) << 1;  // Account for sign extension in ld.

  /*reg AT =T2*/
  std::ostringstream oss;
  oss << "auipc t2, 0x" << std::hex << High20Bits(offset_forward)
      << "\n"
         "ld a1, 0x"
      << std::hex << SignExtend64<riscv64::kIImm12Bits>(offset_forward) << "(t2)\n"
      << RepeatInsn(kAdduCount, "add zero, zero, zero\n")
      << ".word 0x00000000\n"
         ".dword 0x0123456789ABCDEF\n";
  std::string expected = oss.str();
  DriverStr(expected, "LoadNearestLiteralLongNoAlignment");
  EXPECT_EQ(__ GetLabelLocation(literal->GetLabel()), (2 + kAdduCount + 1) * 4);
}

TEST_F(AssemblerRiscv64Test, LoadNearestLiteralLongAlignment) {
  riscv64::Literal* literal = __ NewLiteral<uint64_t>(UINT64_C(0x0123456789ABCDEF));
  __ LoadLiteral(riscv64::A1, riscv64::kLoadDoubleword, literal);
  constexpr uint32_t kAdduCount = 0x3FFC;
  for (uint32_t i = 0; i != kAdduCount; ++i) {
    __ Add(riscv64::Zero, riscv64::Zero, riscv64::Zero);
  }

  uint32_t offset_forward = 2 + kAdduCount;  // 2: account for auipc, andi and ld.
  offset_forward <<= 2;
  offset_forward += (offset_forward & 0x800) << 1;  // Account for sign extension in ld.

  /*reg AT =T2*/
  std::ostringstream oss;
  oss << "auipc t2, 0x" << std::hex << High20Bits(offset_forward)
      << "\n"
         "ld a1, 0x"
      << std::hex << SignExtend64<riscv64::kIImm12Bits>(offset_forward) << "(t2)\n"
      << RepeatInsn(kAdduCount, "add zero, zero, zero\n") << ".dword 0x0123456789ABCDEF\n";
  std::string expected = oss.str();
  DriverStr(expected, "LoadNearestLiteralLongAlignment");
  EXPECT_EQ(__ GetLabelLocation(literal->GetLabel()), (2 + kAdduCount) * 4);
}

TEST_F(AssemblerRiscv64Test, StoreConstToOffset) {
  __ StoreConstToOffset(riscv64::kStoreByte, 0xFF, riscv64::A1, +0, riscv64::T6);
  __ StoreConstToOffset(riscv64::kStoreHalfword, 0xFFFF, riscv64::A1, +0, riscv64::T6);
  __ StoreConstToOffset(riscv64::kStoreWord, 0x12345678, riscv64::A1, +0, riscv64::T6);
  __ StoreConstToOffset(
      riscv64::kStoreDoubleword, 0x123456789ABCDEF0, riscv64::A1, +0, riscv64::T6);

  __ StoreConstToOffset(riscv64::kStoreByte, 0, riscv64::A1, +0, riscv64::T6);
  __ StoreConstToOffset(riscv64::kStoreHalfword, 0, riscv64::A1, +0, riscv64::T6);
  __ StoreConstToOffset(riscv64::kStoreWord, 0, riscv64::A1, +0, riscv64::T6);
  __ StoreConstToOffset(riscv64::kStoreDoubleword, 0, riscv64::A1, +0, riscv64::T6);

  __ StoreConstToOffset(
      riscv64::kStoreDoubleword, 0x1234567812345678, riscv64::A1, +0, riscv64::T6);
  __ StoreConstToOffset(
      riscv64::kStoreDoubleword, 0x1234567800000000, riscv64::A1, +0, riscv64::T6);
  __ StoreConstToOffset(
      riscv64::kStoreDoubleword, 0x0000000012345678, riscv64::A1, +0, riscv64::T6);

  __ StoreConstToOffset(riscv64::kStoreWord, 0, riscv64::T6, +0, riscv64::T6);
  __ StoreConstToOffset(riscv64::kStoreWord, 0x12345678, riscv64::T6, +0, riscv64::T6);

  __ StoreConstToOffset(riscv64::kStoreWord, 0, riscv64::A1, -0xFFF0, riscv64::T6);
  __ StoreConstToOffset(riscv64::kStoreWord, 0x12345678, riscv64::A1, +0xF7F0, riscv64::T6);
  __ StoreConstToOffset(riscv64::kStoreWord, 0x12345678, riscv64::A1, +0xFFF0, riscv64::T6);

  __ StoreConstToOffset(riscv64::kStoreWord, 0, riscv64::T6, -0xFFF0, riscv64::T6);
  __ StoreConstToOffset(riscv64::kStoreWord, 0x12345678, riscv64::T6, +0xF7F0, riscv64::T6);
  __ StoreConstToOffset(riscv64::kStoreWord, 0x12345678, riscv64::T6, +0xFFF0, riscv64::T6);

  const char* expected =
      "li t6,  0xFF\n"
      "sb t6, 0(a1)\n"
      "li t6,  0xFFFF\n"
      "sh t6, 0(a1)\n"
      "li t6, 0x12345678\n"
      "sw t6, 0(a1)\n"
      "li t6, 0x123456789ABCDEF0\n"
      "sd t6, 0(a1)\n"
      "sb zero, 0(a1)\n"
      "sh zero, 0(a1)\n"
      "sw zero, 0(a1)\n"
      "sd zero, 0(a1)\n"
      "li t6, 0x1234567812345678\n"
      "sd t6, 0(a1)\n"
      "li t6, 0x1234567800000000\n"
      "sd t6, 0(a1)\n"
      "li t6, 0x12345678\n"
      "sd t6, 0(a1)\n"
      "sw zero, 0(t6)\n"
      "li t2,0x12345678\n"
      "sw t2, 0(t6)\n"

      "lui t2, 0xffff0\n"
      "add t2, a1, t2\n"
      "sw zero, 0x10(t2) # 0x7F0\n"
      // 0xf is the upper 20-bit for 0xf7F0
      "lui t2, 0xf\n"
      "add t2, a1, t2\n"
      "li t6, 0x12345678\n"
      "sw t6, 2032(t2) # 0x7F0\n"
      // 0x10 is the 1 plus upper 20-bit for 0xFFF0
      "lui t2, 0x10\n"
      "add t2, a1, t2\n"
      "li t6, 0x12345678\n"
      "sw t6, 0xfffffffffffffff0(t2) # 0x7F0\n"

      "lui t2, 0xffff0\n"
      "add t2, t6, t2\n"
      "sw zero, 0x10(t2) # 0x7F0\n"
      // 0xf is the upper 20-bit for 0xF7F0
      "lui t2, 0xf\n"
      "add t2, t6, t2\n"
      "li t6, 0x12345678\n"
      "sw t6, 2032(t2) # 0x7F0\n"
      // 0x10 is the 1 plus upper 20-bit for 0xFFF0
      "lui t2, 0x10\n"
      "add t2, t6, t2\n"
      "li t6, 0x12345678\n"
      "sw t6, 0xfffffffffffffff0(t2) # 0x7F0\n";
  DriverStr(expected, "StoreConstToOffset");
}

TEST_F(AssemblerRiscv64Test, LoadFromOffset) {
  __ LoadFromOffset(riscv64::kLoadSignedByte, riscv64::T0, riscv64::T0, 0);
  __ LoadFromOffset(riscv64::kLoadSignedByte, riscv64::T0, riscv64::A0, 0);
  __ LoadFromOffset(riscv64::kLoadSignedByte, riscv64::T0, riscv64::A0, 1);
  __ LoadFromOffset(riscv64::kLoadSignedByte, riscv64::T0, riscv64::A0, 256);
  __ LoadFromOffset(riscv64::kLoadSignedByte, riscv64::T0, riscv64::A0, 0x7F8);
  __ LoadFromOffset(riscv64::kLoadSignedByte, riscv64::T0, riscv64::A0, 0x7FF);
  __ LoadFromOffset(riscv64::kLoadSignedByte, riscv64::T0, riscv64::A0, 0x800);
  __ LoadFromOffset(riscv64::kLoadSignedByte, riscv64::T0, riscv64::A0, 0x801);
  __ LoadFromOffset(riscv64::kLoadSignedByte, riscv64::T0, riscv64::A0, 0x10000);
  __ LoadFromOffset(riscv64::kLoadSignedByte, riscv64::T0, riscv64::A0, 0x12345678);
  __ LoadFromOffset(riscv64::kLoadSignedByte, riscv64::T0, riscv64::A0, -256);
  __ LoadFromOffset(riscv64::kLoadSignedByte, riscv64::T0, riscv64::A0, -2040);
  __ LoadFromOffset(riscv64::kLoadSignedByte, riscv64::T0, riscv64::A0, 0xABCDEF00);
  __ LoadFromOffset(riscv64::kLoadSignedByte, riscv64::T0, riscv64::A0, 0x7FFFFFFE);
  __ LoadFromOffset(riscv64::kLoadSignedByte, riscv64::T0, riscv64::A0, 0x7FFFFFFF);
  __ LoadFromOffset(riscv64::kLoadSignedByte, riscv64::T0, riscv64::A0, 0x80000000);
  __ LoadFromOffset(riscv64::kLoadSignedByte, riscv64::T0, riscv64::A0, 0x80000001);

  __ LoadFromOffset(riscv64::kLoadUnsignedByte, riscv64::T0, riscv64::T0, 0);
  __ LoadFromOffset(riscv64::kLoadUnsignedByte, riscv64::T0, riscv64::A0, 0);
  __ LoadFromOffset(riscv64::kLoadUnsignedByte, riscv64::T0, riscv64::A0, 1);
  __ LoadFromOffset(riscv64::kLoadUnsignedByte, riscv64::T0, riscv64::A0, 256);
  __ LoadFromOffset(riscv64::kLoadUnsignedByte, riscv64::T0, riscv64::A0, 0x7F8);
  __ LoadFromOffset(riscv64::kLoadUnsignedByte, riscv64::T0, riscv64::A0, 0x7FF);
  __ LoadFromOffset(riscv64::kLoadUnsignedByte, riscv64::T0, riscv64::A0, 0x800);
  __ LoadFromOffset(riscv64::kLoadUnsignedByte, riscv64::T0, riscv64::A0, 0x801);
  __ LoadFromOffset(riscv64::kLoadUnsignedByte, riscv64::T0, riscv64::A0, 0x10000);
  __ LoadFromOffset(riscv64::kLoadUnsignedByte, riscv64::T0, riscv64::A0, 0x12345678);
  __ LoadFromOffset(riscv64::kLoadUnsignedByte, riscv64::T0, riscv64::A0, -256);
  __ LoadFromOffset(riscv64::kLoadUnsignedByte, riscv64::T0, riscv64::A0, -2040);
  __ LoadFromOffset(riscv64::kLoadUnsignedByte, riscv64::T0, riscv64::A0, 0xABCDEF00);
  __ LoadFromOffset(riscv64::kLoadUnsignedByte, riscv64::T0, riscv64::A0, 0x7FFFFFFE);
  __ LoadFromOffset(riscv64::kLoadUnsignedByte, riscv64::T0, riscv64::A0, 0x7FFFFFFF);
  __ LoadFromOffset(riscv64::kLoadUnsignedByte, riscv64::T0, riscv64::A0, 0x80000000);
  __ LoadFromOffset(riscv64::kLoadUnsignedByte, riscv64::T0, riscv64::A0, 0x80000001);

  __ LoadFromOffset(riscv64::kLoadSignedHalfword, riscv64::T0, riscv64::T0, 0);
  __ LoadFromOffset(riscv64::kLoadSignedHalfword, riscv64::T0, riscv64::A0, 0);
  __ LoadFromOffset(riscv64::kLoadSignedHalfword, riscv64::T0, riscv64::A0, 2);
  __ LoadFromOffset(riscv64::kLoadSignedHalfword, riscv64::T0, riscv64::A0, 256);
  __ LoadFromOffset(riscv64::kLoadSignedHalfword, riscv64::T0, riscv64::A0, 0x7F8);
  __ LoadFromOffset(riscv64::kLoadSignedHalfword, riscv64::T0, riscv64::A0, 0x7FE);
  __ LoadFromOffset(riscv64::kLoadSignedHalfword, riscv64::T0, riscv64::A0, 0x800);
  __ LoadFromOffset(riscv64::kLoadSignedHalfword, riscv64::T0, riscv64::A0, 0x802);
  __ LoadFromOffset(riscv64::kLoadSignedHalfword, riscv64::T0, riscv64::A0, 0x10000);
  __ LoadFromOffset(riscv64::kLoadSignedHalfword, riscv64::T0, riscv64::A0, 0x12345678);
  __ LoadFromOffset(riscv64::kLoadSignedHalfword, riscv64::T0, riscv64::A0, -256);
  __ LoadFromOffset(riscv64::kLoadSignedHalfword, riscv64::T0, riscv64::A0, -2040);
  __ LoadFromOffset(riscv64::kLoadSignedHalfword, riscv64::T0, riscv64::A0, 0xABCDEF00);
  __ LoadFromOffset(riscv64::kLoadSignedHalfword, riscv64::T0, riscv64::A0, 0x7FFFFFFC);
  __ LoadFromOffset(riscv64::kLoadSignedHalfword, riscv64::T0, riscv64::A0, 0x7FFFFFFE);
  __ LoadFromOffset(riscv64::kLoadSignedHalfword, riscv64::T0, riscv64::A0, 0x80000000);
  __ LoadFromOffset(riscv64::kLoadSignedHalfword, riscv64::T0, riscv64::A0, 0x80000002);

  __ LoadFromOffset(riscv64::kLoadUnsignedHalfword, riscv64::T0, riscv64::T0, 0);
  __ LoadFromOffset(riscv64::kLoadUnsignedHalfword, riscv64::T0, riscv64::A0, 0);
  __ LoadFromOffset(riscv64::kLoadUnsignedHalfword, riscv64::T0, riscv64::A0, 2);
  __ LoadFromOffset(riscv64::kLoadUnsignedHalfword, riscv64::T0, riscv64::A0, 256);
  __ LoadFromOffset(riscv64::kLoadUnsignedHalfword, riscv64::T0, riscv64::A0, 0x7F8);
  __ LoadFromOffset(riscv64::kLoadUnsignedHalfword, riscv64::T0, riscv64::A0, 0x7FE);
  __ LoadFromOffset(riscv64::kLoadUnsignedHalfword, riscv64::T0, riscv64::A0, 0x800);
  __ LoadFromOffset(riscv64::kLoadUnsignedHalfword, riscv64::T0, riscv64::A0, 0x802);
  __ LoadFromOffset(riscv64::kLoadUnsignedHalfword, riscv64::T0, riscv64::A0, 0x10000);
  __ LoadFromOffset(riscv64::kLoadUnsignedHalfword, riscv64::T0, riscv64::A0, 0x12345678);
  __ LoadFromOffset(riscv64::kLoadUnsignedHalfword, riscv64::T0, riscv64::A0, -256);
  __ LoadFromOffset(riscv64::kLoadUnsignedHalfword, riscv64::T0, riscv64::A0, -2040);
  __ LoadFromOffset(riscv64::kLoadUnsignedHalfword, riscv64::T0, riscv64::A0, 0xABCDEF00);
  __ LoadFromOffset(riscv64::kLoadUnsignedHalfword, riscv64::T0, riscv64::A0, 0x7FFFFFFC);
  __ LoadFromOffset(riscv64::kLoadUnsignedHalfword, riscv64::T0, riscv64::A0, 0x7FFFFFFE);
  __ LoadFromOffset(riscv64::kLoadUnsignedHalfword, riscv64::T0, riscv64::A0, 0x80000000);
  __ LoadFromOffset(riscv64::kLoadUnsignedHalfword, riscv64::T0, riscv64::A0, 0x80000002);

  __ LoadFromOffset(riscv64::kLoadWord, riscv64::T0, riscv64::T0, 0);
  __ LoadFromOffset(riscv64::kLoadWord, riscv64::T0, riscv64::A0, 0);
  __ LoadFromOffset(riscv64::kLoadWord, riscv64::T0, riscv64::A0, 4);
  __ LoadFromOffset(riscv64::kLoadWord, riscv64::T0, riscv64::A0, 256);
  __ LoadFromOffset(riscv64::kLoadWord, riscv64::T0, riscv64::A0, 0x7F8);
  __ LoadFromOffset(riscv64::kLoadWord, riscv64::T0, riscv64::A0, 0x7FC);
  __ LoadFromOffset(riscv64::kLoadWord, riscv64::T0, riscv64::A0, 0x800);
  __ LoadFromOffset(riscv64::kLoadWord, riscv64::T0, riscv64::A0, 0x804);
  __ LoadFromOffset(riscv64::kLoadWord, riscv64::T0, riscv64::A0, 0x10000);
  __ LoadFromOffset(riscv64::kLoadWord, riscv64::T0, riscv64::A0, 0x12345678);
  __ LoadFromOffset(riscv64::kLoadWord, riscv64::T0, riscv64::A0, -256);
  __ LoadFromOffset(riscv64::kLoadWord, riscv64::T0, riscv64::A0, -2040);
  __ LoadFromOffset(riscv64::kLoadWord, riscv64::T0, riscv64::A0, 0xABCDEF00);
  __ LoadFromOffset(riscv64::kLoadWord, riscv64::T0, riscv64::A0, 0x7FFFFFF8);
  __ LoadFromOffset(riscv64::kLoadWord, riscv64::T0, riscv64::A0, 0x7FFFFFFC);
  __ LoadFromOffset(riscv64::kLoadWord, riscv64::T0, riscv64::A0, 0x80000000);
  __ LoadFromOffset(riscv64::kLoadWord, riscv64::T0, riscv64::A0, 0x80000004);

  __ LoadFromOffset(riscv64::kLoadDoubleword, riscv64::T0, riscv64::T0, 0);
  __ LoadFromOffset(riscv64::kLoadDoubleword, riscv64::T0, riscv64::A0, 0);
  __ LoadFromOffset(riscv64::kLoadDoubleword, riscv64::T0, riscv64::A0, 4);
  __ LoadFromOffset(riscv64::kLoadDoubleword, riscv64::T0, riscv64::A0, 256);
  __ LoadFromOffset(riscv64::kLoadDoubleword, riscv64::T0, riscv64::A0, 2040);
  __ LoadFromOffset(riscv64::kLoadDoubleword, riscv64::T0, riscv64::A0, 2044);
  __ LoadFromOffset(riscv64::kLoadDoubleword, riscv64::T0, riscv64::A0, 2048);
  __ LoadFromOffset(riscv64::kLoadDoubleword, riscv64::T0, riscv64::A0, -256);
  __ LoadFromOffset(riscv64::kLoadDoubleword, riscv64::T0, riscv64::A0, -2044);
  __ LoadFromOffset(riscv64::kLoadDoubleword, riscv64::T0, riscv64::A0, -4080);
  __ LoadFromOffset(riscv64::kLoadDoubleword, riscv64::T0, riscv64::A0, 0x10004);
  __ LoadFromOffset(riscv64::kLoadDoubleword, riscv64::T0, riscv64::A0, 0x27FFC);
  __ LoadFromOffset(riscv64::kLoadDoubleword, riscv64::T0, riscv64::A0, 0x12345678);
  __ LoadFromOffset(riscv64::kLoadDoubleword, riscv64::T0, riscv64::A0, 0xABCDEF00);
  __ LoadFromOffset(riscv64::kLoadDoubleword, riscv64::T0, riscv64::A0, 0x7FFFFFF8);
  __ LoadFromOffset(riscv64::kLoadDoubleword, riscv64::T0, riscv64::A0, 0x7FFFFFFC);
  __ LoadFromOffset(riscv64::kLoadDoubleword, riscv64::T0, riscv64::A0, 0x80000000);
  __ LoadFromOffset(riscv64::kLoadDoubleword, riscv64::T0, riscv64::A0, 0x80000004);
  __ LoadFromOffset(riscv64::kLoadDoubleword, riscv64::T0, riscv64::A0, 0x800007FC);

  const char* expected =
      "lb  t0,0(t0)\n"
      "lb  t0,0(a0)\n"
      "lb  t0,1(a0)\n"
      "lb  t0,256(a0)\n"
      "lb  t0,2040(a0)\n"
      "lb  t0,2047(a0)\n"
      "addi  t2,a0,2040\n"
      "lb  t0,8(t2)\n"
      "addi  t2,a0,2040\n"
      "lb  t0,9(t2)\n"
      "lui  t2,0x10\n"
      "add  t2,a0,t2\n"
      "lb  t0,0(t2) # 0x10000\n"
      "lui  t2,0x12345\n"
      "add  t2,a0,t2\n"
      "lb  t0,1656(t2) # 0x12345678\n"
      "lb  t0,-256(a0)\n"
      "lb  t0,-2040(a0)\n"
      "lui  t2,0xabcdf\n"
      "add  t2,a0,t2\n"
      "lb  t0,-256(t2) # 0xffffffffabcdef00\n"
      "lui  t2,0x80000\n"
      "slli  t2,t2,0x20\n"
      "srli  t2,t2,0x20\n"
      "add  t2,a0,t2\n"
      "lb  t0,-2(t2) # 0xffffffff7ffffffe\n"
      "lui  t2,0x80000\n"
      "slli  t2,t2,0x20\n"
      "srli  t2,t2,0x20\n"
      "add  t2,a0,t2\n"
      "lb  t0,-1(t2) # 0xffffffff7fffffff\n"
      "lui  t2,0x80000\n"
      "add  t2,a0,t2\n"
      "lb  t0,0(t2) # 0xffffffff80000000\n"
      "lui  t2,0x80000\n"
      "add  t2,a0,t2\n"
      "lb  t0,1(t2) # 0xffffffff80000001\n"

      "lbu  t0,0(t0)\n"
      "lbu  t0,0(a0)\n"
      "lbu  t0,1(a0)\n"
      "lbu  t0,256(a0)\n"
      "lbu  t0,2040(a0)\n"
      "lbu  t0,2047(a0)\n"
      "addi  t2,a0,2040\n"
      "lbu  t0,8(t2)\n"
      "addi  t2,a0,2040\n"
      "lbu  t0,9(t2)\n"
      "lui  t2,0x10\n"
      "add  t2,a0,t2\n"
      "lbu  t0,0(t2) # 0x10000\n"
      "lui  t2,0x12345\n"
      "add  t2,a0,t2\n"
      "lbu  t0,1656(t2) # 0x12345678\n"
      "lbu  t0,-256(a0)\n"
      "lbu  t0,-2040(a0)\n"
      "lui  t2,0xabcdf\n"
      "add  t2,a0,t2\n"
      "lbu  t0,-256(t2) # 0xffffffffabcdef00\n"
      "lui  t2,0x80000\n"
      "slli  t2,t2,0x20\n"
      "srli  t2,t2,0x20\n"
      "add  t2,a0,t2\n"
      "lbu  t0,-2(t2) # 0xffffffff7ffffffe\n"
      "lui  t2,0x80000\n"
      "slli  t2,t2,0x20\n"
      "srli  t2,t2,0x20\n"
      "add  t2,a0,t2\n"
      "lbu  t0,-1(t2) # 0xffffffff7fffffff\n"
      "lui  t2,0x80000\n"
      "add  t2,a0,t2\n"
      "lbu  t0,0(t2) # 0xffffffff80000000\n"
      "lui  t2,0x80000\n"
      "add  t2,a0,t2\n"
      "lbu  t0,1(t2) # 0xffffffff80000001\n"

      "lh  t0,0(t0)\n"
      "lh  t0,0(a0)\n"
      "lh  t0,2(a0)\n"
      "lh  t0,256(a0)\n"
      "lh  t0,2040(a0)\n"
      "lh  t0,2046(a0)\n"
      "addi  t2,a0,2040\n"
      "lh  t0,8(t2)\n"
      "addi  t2,a0,2040\n"
      "lh  t0,10(t2)\n"
      "lui  t2,0x10\n"
      "add  t2,a0,t2\n"
      "lh  t0,0(t2) # 0x10000\n"
      "lui  t2,0x12345\n"
      "add  t2,a0,t2\n"
      "lh  t0,1656(t2) # 0x12345678\n"
      "lh  t0,-256(a0)\n"
      "lh  t0,-2040(a0)\n"
      "lui  t2,0xabcdf\n"
      "add  t2,a0,t2\n"
      "lh  t0,-256(t2) # 0xffffffffabcdef00\n"
      "lui  t2,0x80000\n"
      "slli  t2,t2,0x20\n"
      "srli  t2,t2,0x20\n"
      "add  t2,a0,t2\n"
      "lh  t0,-4(t2) # 0xffffffff7ffffffc\n"
      "lui  t2,0x80000\n"
      "slli  t2,t2,0x20\n"
      "srli  t2,t2,0x20\n"
      "add  t2,a0,t2\n"
      "lh  t0,-2(t2) # 0xffffffff7ffffffe\n"
      "lui  t2,0x80000\n"
      "add  t2,a0,t2\n"
      "lh  t0,0(t2) # 0xffffffff80000000\n"
      "lui  t2,0x80000\n"
      "add  t2,a0,t2\n"
      "lh  t0,2(t2) # 0xffffffff80000002\n"

      "lhu  t0,0(t0)\n"
      "lhu  t0,0(a0)\n"
      "lhu  t0,2(a0)\n"
      "lhu  t0,256(a0)\n"
      "lhu  t0,2040(a0)\n"
      "lhu  t0,2046(a0)\n"
      "addi  t2,a0,2040\n"
      "lhu  t0,8(t2)\n"
      "addi  t2,a0,2040\n"
      "lhu  t0,10(t2)\n"
      "lui  t2,0x10\n"
      "add  t2,a0,t2\n"
      "lhu  t0,0(t2) # 0x10000\n"
      "lui  t2,0x12345\n"
      "add  t2,a0,t2\n"
      "lhu  t0,1656(t2) # 0x12345678\n"
      "lhu  t0,-256(a0)\n"
      "lhu  t0,-2040(a0)\n"
      "lui  t2,0xabcdf\n"
      "add  t2,a0,t2\n"
      "lhu  t0,-256(t2) # 0xffffffffabcdef00\n"
      "lui  t2,0x80000\n"
      "slli  t2,t2,0x20\n"
      "srli  t2,t2,0x20\n"
      "add  t2,a0,t2\n"
      "lhu  t0,-4(t2) # 0xffffffff7ffffffc\n"
      "lui  t2,0x80000\n"
      "slli  t2,t2,0x20\n"
      "srli  t2,t2,0x20\n"
      "add  t2,a0,t2\n"
      "lhu  t0,-2(t2) # 0xffffffff7ffffffe\n"
      "lui  t2,0x80000\n"
      "add  t2,a0,t2\n"
      "lhu  t0,0(t2) # 0xffffffff80000000\n"
      "lui  t2,0x80000\n"
      "add  t2,a0,t2\n"
      "lhu  t0,2(t2) # 0xffffffff80000002\n"

      "lw  t0,0(t0)\n"
      "lw  t0,0(a0)\n"
      "lw  t0,4(a0)\n"
      "lw  t0,256(a0)\n"
      "lw  t0,2040(a0)\n"
      "lw  t0,2044(a0)\n"
      "addi  t2,a0,2040\n"
      "lw  t0,8(t2)\n"
      "addi  t2,a0,2040\n"
      "lw  t0,12(t2)\n"
      "lui  t2,0x10\n"
      "add  t2,a0,t2\n"
      "lw  t0,0(t2) # 0x10000\n"
      "lui  t2,0x12345\n"
      "add  t2,a0,t2\n"
      "lw  t0,1656(t2) # 0x12345678\n"
      "lw  t0,-256(a0)\n"
      "lw  t0,-2040(a0)\n"
      "lui  t2,0xabcdf\n"
      "add  t2,a0,t2\n"
      "lw  t0,-256(t2) # 0xffffffffabcdef00\n"
      "lui  t2,0x80000\n"
      "slli  t2,t2,0x20\n"
      "srli  t2,t2,0x20\n"
      "add  t2,a0,t2\n"
      "lw  t0,-8(t2) # 0xffffffff7ffffff8\n"
      "lui  t2,0x80000\n"
      "slli  t2,t2,0x20\n"
      "srli  t2,t2,0x20\n"
      "add  t2,a0,t2\n"
      "lw  t0,-4(t2) # 0xffffffff7ffffffc\n"
      "lui  t2,0x80000\n"
      "add  t2,a0,t2\n"
      "lw  t0,0(t2) # 0xffffffff80000000\n"
      "lui  t2,0x80000\n"
      "add  t2,a0,t2\n"
      "lw  t0,4(t2) # 0xffffffff80000004\n"

      // kLoadDoubleword
      "ld  t0,0(t0)\n"
      // 0
      "ld t0, 0(a0)\n"
      // 4
      "lwu t0, 4(a0)\n"
      "lwu t2, 8(a0)\n"
      "Slli t2, t2, 32\n"
      "Add t0, t0, t2\n"
      // 256
      "ld t0, 256(a0)\n"
      // 2040
      "ld t0, 2040(a0)\n"
      // 2044
      "addi t2,a0,2040\n"
      "lwu t0,4(t2)\n"
      "lwu t2,8(t2)\n"
      "slli t2,t2,0x20\n"
      "add t0,t0,t2\n"
      // 2048
      "Addi t2, a0, 2040\n"
      "ld t0, 8(t2)\n"
      // -256
      "ld t0, -256(a0)\n"
      // -2044
      "lwu  t0,-2044(a0)\n"
      "lwu  t2,-2040(a0)\n"
      "slli  t2,t2,0x20\n"
      "add  t0,t0,t2\n"
      // -4080
      "Addi t2, a0, -2040\n"
      "ld t0, -2040(t2)\n"
      // 0x10(16) is the upper 20-bit for 0x10004
      "lui t2, 16\n"
      "Add t2, a0, t2\n"
      "lwu t0, 4(t2)\n"
      "lwu t2, 8(t2)\n"
      "Slli t2, t2, 32\n"
      "Add t0, t0, t2\n"
      // 0x27FFC
      "lui t2, 40\n"
      "Add t2, a0, t2\n"
      "lwu t0, -4(t2)\n"
      "lwu t2, 0(t2)\n"
      "Slli t2, t2, 32\n"
      "Add t0, t0, t2\n"
      // 0x12345678
      "lui  t2,0x12345\n"
      "add  t2,a0,t2\n"
      "ld  t0,0x678(t2) # 0x12345678\n"
      // 0xABCDEF00
      "lui  t2,0xabcdf\n"
      "add  t2,a0,t2\n"
      "ld  t0,-256(t2) # 0xffffffffabcdef00\n"
      // 0x7FFFFFF8
      "lui  t2,0x80000\n"
      "slli  t2,t2,0x20\n"
      "srli  t2,t2,0x20\n"
      "add  t2,a0,t2\n"
      "ld  t0,-8(t2)\n"
      // 0x7FFFFFFC
      "lui  t2,0x80000\n"
      "slli  t2,t2,0x20\n"
      "srli  t2,t2,0x20\n"
      "add  t2,a0,t2\n"
      "lwu  t0,-4(t2)\n"
      "lwu  t2,0(t2)\n"
      "slli  t2,t2,0x20\n"
      "add  t0,t0,t2\n"
      // 0x80000000
      "lui t2, 524288\n"
      "Add t2, a0, t2\n"
      "ld t0, 0(t2)\n"
      // 0x80000(524288) is the upper 20-bit for 0x80000004
      "lui t2, 524288\n"
      "Add t2, a0, t2\n"
      "lwu t0, 4(t2) # 0xffffffff80000004\n"
      "lwu t2, 8(t2)\n"
      "Slli t2, t2, 32\n"
      "Add t0, t0, t2\n"
      // 0x80000 is the upper 20-bit for 0x800007FC
      "lui  t2,0x80000\n"
      "add  t2,a0,t2\n"
      "addi  t2,t2,8 # 0xffffffff80000008\n"
      "lwu  t0,2036(t2)\n"
      "lwu  t2,2040(t2)\n"
      "slli  t2,t2,0x20\n"
      "add  t0,t0,t2\n";
  DriverStr(expected, "LoadFromOffset");

  DriverStr(expected, "LoadFromOffset");
}

TEST_F(AssemblerRiscv64Test, LoadFpuFromOffset) {
  __ LoadFpuFromOffset(riscv64::kLoadWord, riscv64::F0, riscv64::A0, 0);
  __ LoadFpuFromOffset(riscv64::kLoadWord, riscv64::F0, riscv64::A0, 4);
  __ LoadFpuFromOffset(riscv64::kLoadWord, riscv64::F0, riscv64::A0, 256);
  __ LoadFpuFromOffset(riscv64::kLoadWord, riscv64::F0, riscv64::A0, 2044 /*0x7FC*/);
  __ LoadFpuFromOffset(riscv64::kLoadWord, riscv64::F0, riscv64::A0, 2048 /*0x800*/);
  __ LoadFpuFromOffset(riscv64::kLoadWord, riscv64::F0, riscv64::A0, 4080 /*0xFF0*/);
  __ LoadFpuFromOffset(riscv64::kLoadWord, riscv64::F0, riscv64::A0, 4084 /*0xFF4*/);
  __ LoadFpuFromOffset(riscv64::kLoadWord, riscv64::F0, riscv64::A0, 4096 /*0x1000*/);
  __ LoadFpuFromOffset(riscv64::kLoadWord, riscv64::F0, riscv64::A0, 0x12345678);
  __ LoadFpuFromOffset(riscv64::kLoadWord, riscv64::F0, riscv64::A0, -256);
  __ LoadFpuFromOffset(riscv64::kLoadWord, riscv64::F0, riscv64::A0, -2048);
  __ LoadFpuFromOffset(riscv64::kLoadWord, riscv64::F0, riscv64::A0, -2044);
  __ LoadFpuFromOffset(riscv64::kLoadWord, riscv64::F0, riscv64::A0, -4080);
  __ LoadFpuFromOffset(riscv64::kLoadWord, riscv64::F0, riscv64::A0, 0xABCDEF00);
  __ LoadFpuFromOffset(riscv64::kLoadWord, riscv64::F0, riscv64::A0, 0x7FFFFFFC);

  __ LoadFpuFromOffset(riscv64::kLoadDoubleword, riscv64::F0, riscv64::A0, 0);
  __ LoadFpuFromOffset(riscv64::kLoadDoubleword, riscv64::F0, riscv64::A0, 4);
  __ LoadFpuFromOffset(riscv64::kLoadDoubleword, riscv64::F0, riscv64::A0, 256);
  __ LoadFpuFromOffset(riscv64::kLoadDoubleword, riscv64::F0, riscv64::A0, 2044 /*0x7FC*/);
  __ LoadFpuFromOffset(riscv64::kLoadDoubleword, riscv64::F0, riscv64::A0, 2048 /*0x800*/);
  __ LoadFpuFromOffset(riscv64::kLoadDoubleword, riscv64::F0, riscv64::A0, 4080 /*0xFF0*/);
  __ LoadFpuFromOffset(riscv64::kLoadDoubleword, riscv64::F0, riscv64::A0, 4084 /*0xFF4*/);
  __ LoadFpuFromOffset(riscv64::kLoadDoubleword, riscv64::F0, riscv64::A0, 4096 /*0x1000*/);
  __ LoadFpuFromOffset(riscv64::kLoadDoubleword, riscv64::F0, riscv64::A0, 0x12345678);
  __ LoadFpuFromOffset(riscv64::kLoadDoubleword, riscv64::F0, riscv64::A0, -256);
  __ LoadFpuFromOffset(riscv64::kLoadDoubleword, riscv64::F0, riscv64::A0, -2048);
  __ LoadFpuFromOffset(riscv64::kLoadDoubleword, riscv64::F0, riscv64::A0, -2044);
  __ LoadFpuFromOffset(riscv64::kLoadDoubleword, riscv64::F0, riscv64::A0, -4080);
  __ LoadFpuFromOffset(riscv64::kLoadDoubleword, riscv64::F0, riscv64::A0, 0xABCDEF00);
  __ LoadFpuFromOffset(riscv64::kLoadDoubleword, riscv64::F0, riscv64::A0, 0x7FFFFFFC);
  __ LoadFpuFromOffset(riscv64::kLoadDoubleword, riscv64::F0, riscv64::A0, 0x80000004);
  __ LoadFpuFromOffset(riscv64::kLoadDoubleword, riscv64::F0, riscv64::A0, 0x800007FC);

  const char* expected =
      "flw f0, 0(a0)\n"
      "flw f0, 4(a0)\n"
      "flw f0, 256(a0)\n"
      // Load word from offset 2044(0x7FC)
      "flw f0, 2044(a0) # 0x7FC\n"
      // Load word from offset 2048(0x800)
      "Addi t2, a0, 2040 #0x7F8\n"
      "flw f0, 8(t2)\n"
      // Load word from offset 4080(0xFF0)
      "Addi t2, a0, 2040 #0x7F8\n"
      "flw f0, 2040(t2)\n"
      // Load word from offset 4084(0xFF4)
      "lui t2, 1 # 0x1\n"
      "Add t2, a0, t2\n"
      "flw f0, -12(t2) # 0xFFF4\n"
      // Load word from offset 4096(0x1000)
      "lui t2, 1 # 0x1\n"
      "add t2, a0, t2\n"
      "flw f0, 0(t2)\n"
      // Load word from offset 0x12345678
      "lui t2, 74565 # 0x12345\n"
      "add t2, a0, t2\n"
      "flw f0, 1656(t2) # 0x678\n"
      // Load word from offset -256=0xFFFF FF00
      "flw f0, -256(a0)\n"
      // Load word from offset -2048 = 0xFFFF F800
      "flw f0, -2048(a0)\n"
      // Load word from offset -2044 = 0xFFFF F804
      "flw f0, -2044(a0)\n"
      // Load word from offset -4080 = 0xFFFF F010
      "Addi t2, a0, -2040 #0x7F8\n"
      "flw f0, -2040(t2)\n"
      // Load word from offset 0xABCDEF00
      "lui t2, 703711 # 0xABCDF\n"
      "add t2, a0, t2\n"
      "flw f0, -256(t2) # 0xFF00\n"
      // Load word from offset 0x7FFF FFFC
      "lui t2, 524288 # 0x80000\n"
      "slli t2, t2, 32\n"
      "srli t2, t2, 32\n"
      "add t2, a0, t2\n"
      "flw f0, -4(t2) # 0xFFFC\n"

      // Load double word from offset 0
      "fld f0, 0(a0)\n"
      // Load double word from offset 4
      "lwu t3, 8(a0)\n"
      "Slli t3, t3, 32\n"
      "lwu t2, 4(a0)\n"
      "add t2, t2, t3\n"
      "fmv.d.x f0, t2\n"
      // Load double word from offset 256
      "fld f0, 256(a0)\n"
      // Load double word from offset 2044(0x7FC)
      "Addi t2, a0, 2040 #0x7F8\n"
      "lwu t3, 8(t2)\n"
      "Slli t3, t3, 32\n"
      "lwu t2, 4(t2)\n"
      "add t2, t2, t3\n"
      "fmv.d.x f0, t2\n"
      // Load double word from offset 2048(0x800)
      "Addi t2, a0, 2040 #0x7F8\n"
      "fld f0, 8(t2)\n"
      // Load double word from offset 4080(0xFF0)
      "Addi t2, a0, 2040 #0x7F8\n"
      "fld f0, 2040(t2)\n"
      // Load double word from offset 4084(0xFF4)
      "lui t2, 1 # 0x1\n"
      "Add t2, a0, t2\n"
      "lwu t3, -8(t2)# 0xFFF8\n"
      "Slli t3, t3, 32\n"
      "lwu t2, -12(t2) # 0xFFF4\n"
      "add t2, t2, t3\n"
      "fmv.d.x f0, t2\n"
      // Load double word from offset 4096(0x1000)
      "lui t2, 1 # 0x1\n"
      "add t2, a0, t2\n"
      "fld f0, 0(t2)\n"
      // Load double word from offset 0x12345678
      "lui t2, 74565 # 0x12345\n"
      "add t2, a0, t2\n"
      "fld f0, 1656(t2) # 0x678\n"
      // Load double word from offset -256
      "fld f0, -256(a0)\n"
      // Load double word from offset -2048 = 0xFFFF F800
      "fld f0, -2048(a0)\n"
      // Load double word from offset -2044 = 0xFFFF F804
      "lwu t3, -2040(a0) # 0xF800\n"
      "Slli t3, t3, 32\n"
      "lwu t2, -2044(a0) # 0xF804\n"
      "add t2, t2, t3\n"
      "fmv.d.x f0, t2\n"
      // Load double word from offset -4080 = 0xFFFF F010
      "Addi t2, a0, -2040 #0x7F8\n"
      "fld f0, -2040(t2)\n"
      // Load double word from offset 0xABCDEF00
      "lui t2, 703711 # 0xABCDF\n"
      "add t2, a0, t2\n"
      "fld f0, -256(t2) # 0xFF00\n"
      // Load double word from offset 0x7FFF FFFC
      "lui t2, 524288 # 0x80000\n"
      "slli t2, t2, 32\n"
      "srli t2, t2, 32\n"
      "add t2, a0, t2\n"
      "lwu t3, 0(t2)\n"
      "Slli t3, t3, 32\n"
      "lwu t2, -4(t2) # 0xFFFC\n"
      "add t2, t2, t3\n"
      "fmv.d.x f0, t2\n"
      // Load double word from offset  0x8000 0004
      "lui t2, 524288 # 0x80000\n"
      "Add t2, a0, t2\n"
      "lwu t3, 8(t2)\n"
      "Slli t3, t3, 32\n"
      "lwu t2, 4(t2)\n"
      "add t2, t2, t3\n"
      "fmv.d.x f0, t2\n"
      // Load double word from offset  0x8000 07FC
      "lui  t2, 524288 # 0x80000\n"
      "add  t2, a0, t2\n"
      "addi t2, t2 ,8\n"
      "lwu t3, 2040(t2) # 0x7F8=0x7F4+4 \n"
      "Slli t3, t3, 32\n"
      "lwu t2, 2036(t2) # 0x7F4=0x7FC - 8 \n"
      "add t2, t2, t3\n"
      "fmv.d.x f0, t2\n";

  DriverStr(expected, "LoadFpuFromOffset");
}

TEST_F(AssemblerRiscv64Test, StoreToOffset) {
  __ StoreToOffset(riscv64::kStoreByte, riscv64::T0, riscv64::A0, 0);
  __ StoreToOffset(riscv64::kStoreHalfword, riscv64::T0, riscv64::A0, 0);

  __ StoreToOffset(riscv64::kStoreWord, riscv64::T0, riscv64::A0, 0);
  __ StoreToOffset(riscv64::kStoreWord, riscv64::T0, riscv64::A0, 4);
  __ StoreToOffset(riscv64::kStoreWord, riscv64::T0, riscv64::A0, 256);
  __ StoreToOffset(riscv64::kStoreWord, riscv64::T0, riscv64::A0, 2044 /*0x7FC*/);
  __ StoreToOffset(riscv64::kStoreWord, riscv64::T0, riscv64::A0, 2048 /*0x800*/);
  __ StoreToOffset(riscv64::kStoreWord, riscv64::T0, riscv64::A0, 4080 /*0xFF0*/);
  __ StoreToOffset(riscv64::kStoreWord, riscv64::T0, riscv64::A0, 4084 /*0xFF4*/);
  __ StoreToOffset(riscv64::kStoreWord, riscv64::T0, riscv64::A0, 4096 /*0x1000*/);
  __ StoreToOffset(riscv64::kStoreWord, riscv64::T0, riscv64::A0, 0x12345678);
  __ StoreToOffset(riscv64::kStoreWord, riscv64::T0, riscv64::A0, -256);
  __ StoreToOffset(riscv64::kStoreWord, riscv64::T0, riscv64::A0, -2048);
  __ StoreToOffset(riscv64::kStoreWord, riscv64::T0, riscv64::A0, -2044);
  __ StoreToOffset(riscv64::kStoreWord, riscv64::T0, riscv64::A0, -4080);
  __ StoreToOffset(riscv64::kStoreWord, riscv64::T0, riscv64::A0, 0xABCDEF00);
  __ StoreToOffset(riscv64::kStoreWord, riscv64::T0, riscv64::A0, 0x7FFFFFFC);

  __ StoreToOffset(riscv64::kStoreDoubleword, riscv64::T0, riscv64::A0, 0);
  __ StoreToOffset(riscv64::kStoreDoubleword, riscv64::T0, riscv64::A0, 4);
  __ StoreToOffset(riscv64::kStoreDoubleword, riscv64::T0, riscv64::A0, 256);
  __ StoreToOffset(riscv64::kStoreDoubleword, riscv64::T0, riscv64::A0, 2044 /*0x7FC*/);
  __ StoreToOffset(riscv64::kStoreDoubleword, riscv64::T0, riscv64::A0, 2048 /*0x800*/);
  __ StoreToOffset(riscv64::kStoreDoubleword, riscv64::T0, riscv64::A0, 4080 /*0xFF0*/);
  __ StoreToOffset(riscv64::kStoreDoubleword, riscv64::T0, riscv64::A0, 4084 /*0xFF4*/);
  __ StoreToOffset(riscv64::kStoreDoubleword, riscv64::T0, riscv64::A0, 4096 /*0x1000*/);
  __ StoreToOffset(riscv64::kStoreDoubleword, riscv64::T0, riscv64::A0, 0x12345678);
  __ StoreToOffset(riscv64::kStoreDoubleword, riscv64::T0, riscv64::A0, -256);
  __ StoreToOffset(riscv64::kStoreDoubleword, riscv64::T0, riscv64::A0, -2048);
  __ StoreToOffset(riscv64::kStoreDoubleword, riscv64::T0, riscv64::A0, -2044);
  __ StoreToOffset(riscv64::kStoreDoubleword, riscv64::T0, riscv64::A0, -4080);
  __ StoreToOffset(riscv64::kStoreDoubleword, riscv64::T0, riscv64::A0, 0xABCDEF00);
  __ StoreToOffset(riscv64::kStoreDoubleword, riscv64::T0, riscv64::A0, 0x7FFFFFFC);
  __ StoreToOffset(riscv64::kStoreDoubleword, riscv64::T0, riscv64::A0, 0x80000004);
  __ StoreToOffset(riscv64::kStoreDoubleword, riscv64::T0, riscv64::A0, 0x800007FC);

  const char* expected =
      "sb t0, 0(a0)\n"
      "sh t0, 0(a0)\n"
      "sw t0, 0(a0)\n"
      "sw t0, 4(a0)\n"
      "sw t0, 256(a0)\n"
      // Store word from offset 2044(0x7FC)
      "sw t0, 2044(a0) # 0x7FC\n"
      // Store word from offset 2048(0x800)
      "Addi t2, a0, 2040 #0x7F8\n"
      "sw t0, 8(t2)\n"
      // Store word from offset 4080(0xFF0)
      "Addi t2, a0, 2040 #0x7F8\n"
      "sw t0, 2040(t2)\n"
      // Store word from offset 4084(0xFF4)
      "lui t2, 1 # 0x1\n"
      "Add t2, a0, t2\n"
      "sw t0, -12(t2) # 0xFFF4\n"
      // Store word from offset 4096(0x1000)
      "lui t2, 1 # 0x1\n"
      "add t2, a0, t2\n"
      "sw t0, 0(t2)\n"
      // Store word from offset 0x12345678
      "lui t2, 74565 # 0x12345\n"
      "add t2, a0, t2\n"
      "sw t0, 1656(t2) # 0x678\n"
      // Store word from offset -256=0xFFFF FF00
      "sw t0, -256(a0)\n"
      // Store word from offset -2048 = 0xFFFF F800
      "sw t0, -2048(a0)\n"
      // Store word from offset -2044 = 0xFFFF F804
      "sw t0, -2044(a0)\n"
      // Store word from offset -4080 = 0xFFFF F010
      "Addi t2, a0, -2040 #0x7F8\n"
      "sw t0, -2040(t2)\n"
      // Store word from offset 0xABCDEF00
      "lui t2, 703711 # 0xABCDF\n"
      "add t2, a0, t2\n"
      "sw t0, -256(t2) # 0xFF00\n"
      // Store word from offset 0x7FFF FFFC
      "lui t2, 524288 # 0x80000\n"
      "slli t2, t2, 32\n"
      "srli t2, t2, 32\n"
      "add t2, a0, t2\n"
      "sw t0, -4(t2) # 0xFFFC\n"

      // Store double word from offset 0
      "sd t0, 0(a0)\n"
      // Store double word from offset 4
      "sw t0, 4(a0)\n"
      "srli t3, t0, 32\n"
      "sw t3, 8(a0)\n"
      // Store double word from offset 256
      "sd t0, 256(a0)\n"
      // Store double word from offset 2044(0x7FC)
      "Addi t2, a0, 2040 #0x7F8\n"
      "sw t0, 4(t2)\n"
      "srli t3, t0, 32\n"
      "sw t3, 8(t2)\n"
      // Store double word from offset 2048(0x800)
      "Addi t2, a0, 2040 #0x7F8\n"
      "sd t0, 8(t2)\n"
      // Store double word from offset 4080(0xFF0)
      "Addi t2, a0, 2040 #0x7F8\n"
      "sd t0, 2040(t2)\n"
      // Store double word from offset 4084(0xFF4)
      "lui t2, 1 # 0x1\n"
      "Add t2, a0, t2\n"
      "sw t0, -12(t2) # 0xFFF4\n"
      "srli t3, t0, 32\n"
      "sw t3, -8(t2) # 0xFFF8\n"
      // Store double word from offset 4096(0x1000)
      "lui t2, 1 # 0x1\n"
      "add t2, a0, t2\n"
      "sd t0, 0(t2)\n"
      // Store double word from offset 0x12345678
      "lui t2, 74565 # 0x12345\n"
      "add t2, a0, t2\n"
      "sd t0, 1656(t2) # 0x678\n"
      // Store double word from offset -256
      "sd t0, -256(a0)\n"
      // Store double word from offset -2048 = 0xFFFF F800
      "sd t0, -2048(a0)\n"
      // Store double word from offset -2044 = 0xFFFF F804
      "sw t0, -2044(a0) # 0xF804\n"
      "srli t3, t0, 32\n"
      "sw t3, -2040(a0) # 0xF800\n"
      // Store double word from offset -4080 = 0xFFFF F010
      "Addi t2, a0, -2040 #0x7F8\n"
      "sd t0, -2040(t2)\n"
      // Store double word from offset 0xABCDEF00
      "lui t2, 703711 # 0xABCDF\n"
      "add t2, a0, t2\n"
      "sd t0, -256(t2) # 0xFF00\n"
      // Store double word from offset 0x7FFF FFFC
      "lui t2, 524288 # 0x80000\n"
      "slli t2, t2, 32\n"
      "srli t2, t2, 32\n"
      "add t2, a0, t2\n"
      "sw t0, -4(t2) # 0xFFFC\n"
      "srli t3, t0, 32\n"
      "sw t3, 0(t2)\n"
      // Store double word from offset  0x8000 0004
      "lui t2, 524288 # 0x80000\n"
      "add t2, a0, t2\n"
      "sw t0, 4(t2)\n"
      "srli t3, t0, 32\n"
      "sw t3, 8(t2)\n"
      // Store double word from offset 0x8000 07FC
      "lui t2, 524288 # 0x80000\n"
      "add t2, a0, t2\n"
      "addi t2, t2 ,8\n"
      "sw t0, 2036(t2) # 0x7F4=0x7FC-8\n"
      "srli t3, t0, 32\n"
      "sw t3, 2040(t2) # 0x7F4 + 4\n";

  DriverStr(expected, "StoreToOffset");
}

TEST_F(AssemblerRiscv64Test, StoreFpuToOffset) {
  __ StoreFpuToOffset(riscv64::kStoreWord, riscv64::F0, riscv64::A0, 0);
  __ StoreFpuToOffset(riscv64::kStoreWord, riscv64::F0, riscv64::A0, 4);
  __ StoreFpuToOffset(riscv64::kStoreWord, riscv64::F0, riscv64::A0, 256);
  __ StoreFpuToOffset(riscv64::kStoreWord, riscv64::F0, riscv64::A0, 2044 /*0x7FC*/);
  __ StoreFpuToOffset(riscv64::kStoreWord, riscv64::F0, riscv64::A0, 2048 /*0x800*/);
  __ StoreFpuToOffset(riscv64::kStoreWord, riscv64::F0, riscv64::A0, 4080 /*0xFF0*/);
  __ StoreFpuToOffset(riscv64::kStoreWord, riscv64::F0, riscv64::A0, 4084 /*0xFF4*/);
  __ StoreFpuToOffset(riscv64::kStoreWord, riscv64::F0, riscv64::A0, 4096 /*0x1000*/);
  __ StoreFpuToOffset(riscv64::kStoreWord, riscv64::F0, riscv64::A0, 0x12345678);
  __ StoreFpuToOffset(riscv64::kStoreWord, riscv64::F0, riscv64::A0, -256);
  __ StoreFpuToOffset(riscv64::kStoreWord, riscv64::F0, riscv64::A0, -2048);
  __ StoreFpuToOffset(riscv64::kStoreWord, riscv64::F0, riscv64::A0, -2044);
  __ StoreFpuToOffset(riscv64::kStoreWord, riscv64::F0, riscv64::A0, -4080);
  __ StoreFpuToOffset(riscv64::kStoreWord, riscv64::F0, riscv64::A0, 0xABCDEF00);
  __ StoreFpuToOffset(riscv64::kStoreWord, riscv64::F0, riscv64::A0, 0x7FFFFFFC);

  __ StoreFpuToOffset(riscv64::kStoreDoubleword, riscv64::F0, riscv64::A0, 0);
  __ StoreFpuToOffset(riscv64::kStoreDoubleword, riscv64::F0, riscv64::A0, 4);
  __ StoreFpuToOffset(riscv64::kStoreDoubleword, riscv64::F0, riscv64::A0, 256);
  __ StoreFpuToOffset(riscv64::kStoreDoubleword, riscv64::F0, riscv64::A0, 2044 /*0x7FC*/);
  __ StoreFpuToOffset(riscv64::kStoreDoubleword, riscv64::F0, riscv64::A0, 2048 /*0x800*/);
  __ StoreFpuToOffset(riscv64::kStoreDoubleword, riscv64::F0, riscv64::A0, 4080 /*0xFF0*/);
  __ StoreFpuToOffset(riscv64::kStoreDoubleword, riscv64::F0, riscv64::A0, 4084 /*0xFF4*/);
  __ StoreFpuToOffset(riscv64::kStoreDoubleword, riscv64::F0, riscv64::A0, 4096 /*0x1000*/);
  __ StoreFpuToOffset(riscv64::kStoreDoubleword, riscv64::F0, riscv64::A0, 0x12345678);
  __ StoreFpuToOffset(riscv64::kStoreDoubleword, riscv64::F0, riscv64::A0, -256);
  __ StoreFpuToOffset(riscv64::kStoreDoubleword, riscv64::F0, riscv64::A0, -2048);
  __ StoreFpuToOffset(riscv64::kStoreDoubleword, riscv64::F0, riscv64::A0, -2044);
  __ StoreFpuToOffset(riscv64::kStoreDoubleword, riscv64::F0, riscv64::A0, -4080);
  __ StoreFpuToOffset(riscv64::kStoreDoubleword, riscv64::F0, riscv64::A0, 0xABCDEF00);
  __ StoreFpuToOffset(riscv64::kStoreDoubleword, riscv64::F0, riscv64::A0, 0x7FFFFFFC);
  __ StoreFpuToOffset(riscv64::kStoreDoubleword, riscv64::F0, riscv64::A0, 0x80000004);
  __ StoreFpuToOffset(riscv64::kStoreDoubleword, riscv64::F0, riscv64::A0, 0x800007FC);

  const char* expected =
      "fsw f0, 0(a0)\n"
      "fsw f0, 4(a0)\n"
      "fsw f0, 256(a0)\n"
      // Store word from offset 2044(0x7FC)
      "fsw f0, 2044(a0) # 0x7FC\n"
      // Store word from offset 2048(0x800)
      "Addi t2, a0, 2040 #0x7F8\n"
      "fsw f0, 8(t2)\n"
      // Store word from offset 4080(0xFF0)
      "Addi t2, a0, 2040 #0x7F8\n"
      "fsw f0, 2040(t2)\n"
      // Store word from offset 4084(0xFF4)
      "lui t2, 1 # 0x1\n"
      "Add t2, a0, t2\n"
      "fsw f0, -12(t2) # 0xFFF4\n"
      // Store word from offset 4096(0x1000)
      "lui t2, 1 # 0x1\n"
      "add t2, a0, t2\n"
      "fsw f0, 0(t2)\n"
      // Store word from offset 0x12345678
      "lui t2, 74565 # 0x12345\n"
      "add t2, a0, t2\n"
      "fsw f0, 1656(t2) # 0x678\n"
      // Store word from offset -256=0xFFFF FF00
      "fsw f0, -256(a0)\n"
      // Store word from offset -2048 = 0xFFFF F800
      "fsw f0, -2048(a0)\n"
      // Store word from offset -2044 = 0xFFFF F804
      "fsw f0, -2044(a0)\n"
      // Store word from offset -4080 = 0xFFFF F010
      "Addi t2, a0, -2040 #0x7F8\n"
      "fsw f0, -2040(t2)\n"
      // Store word from offset 0xABCDEF00
      "lui t2, 703711 # 0xABCDF\n"
      "add t2, a0, t2\n"
      "fsw f0, -256(t2) # 0xFF00\n"
      // Store word from offset 0x7FFF FFFC
      "lui t2, 524288 # 0x80000\n"
      "slli t2, t2, 32\n"
      "srli t2, t2, 32\n"
      "add t2, a0, t2\n"
      "fsw f0, -4(t2) # 0xFFFC\n"

      // Store double word from offset 0
      "fsd f0, 0(a0)\n"
      // Store double word from offset 4
      "fmv.x.d t3, f0\n"
      "sw t3, 4(a0)\n"
      "srli t3, t3, 32\n"
      "sw t3, 8(a0)\n"
      // Store double word from offset 256
      "fsd f0, 256(a0)\n"
      // Store double word from offset 2044(0x7FC)
      "Addi t2, a0, 2040 #0x7F8\n"
      "fmv.x.d t3, f0\n"
      "sw t3, 4(t2)\n"
      "srli t3, t3, 32\n"
      "sw t3, 8(t2)\n"
      // Store double word from offset 2048(0x800)
      "Addi t2, a0, 2040 #0x7F8\n"
      "fsd f0, 8(t2)\n"
      // Store double word from offset 4080(0xFF0)
      "Addi t2, a0, 2040 #0x7F8\n"
      "fsd f0, 2040(t2)\n"
      // Store double word from offset 4084(0xFF4)
      "lui t2, 1 # 0x1\n"
      "Add t2, a0, t2\n"
      "fmv.x.d t3, f0\n"
      "sw t3, -12(t2) # 0xFFF4\n"
      "srli t3, t3, 32\n"
      "sw t3, -8(t2) # 0xFFF8\n"
      // Store double word from offset 4096(0x1000)
      "lui t2, 1 # 0x1\n"
      "add t2, a0, t2\n"
      "fsd f0, 0(t2)\n"
      // Store double word from offset 0x12345678
      "lui t2, 74565 # 0x12345\n"
      "add t2, a0, t2\n"
      "fsd f0, 1656(t2) # 0x678\n"
      // Store double word from offset -256
      "fsd f0, -256(a0)\n"
      // Store double word from offset -2048 = 0xFFFF F800
      "fsd f0, -2048(a0)\n"
      // Store double word from offset -2044 = 0xFFFF F804
      "fmv.x.d t3, f0\n"
      "sw t3, -2044(a0) # 0xF804\n"
      "srli t3, t3, 32\n"
      "sw t3, -2040(a0) # 0xF800\n"
      // Store double word from offset -4080 = 0xFFFF F010
      "Addi t2, a0, -2040 #0x7F8\n"
      "fsd f0, -2040(t2)\n"
      // Store double word from offset 0xABCDEF00
      "lui t2, 703711 # 0xABCDF\n"
      "add t2, a0, t2\n"
      "fsd f0, -256(t2) # 0xFF00\n"
      // Store double word from offset 0x7FFF FFFC
      "lui t2, 524288 # 0x80000\n"
      "slli t2, t2, 32\n"
      "srli t2, t2, 32\n"
      "add t2, a0, t2\n"
      "fmv.x.d t3, f0\n"
      "sw t3, -4(t2) # 0xFFFC\n"
      "srli t3, t3, 32\n"
      "sw t3,  0(t2)\n"
      // Store double word from offset  0x8000 0004
      "lui t2, 524288 # 0x80000\n"
      "add t2, a0, t2\n"
      "fmv.x.d t3, f0\n"
      "sw t3, 4(t2)\n"
      "srli t3, t3, 32\n"
      "sw t3, 8(t2)\n"
      // Store double word from offset 0x8000 07FC
      "lui t2, 524288 # 0x80000\n"
      "add t2, a0, t2\n"
      "addi t2, t2 ,8\n"
      "fmv.x.d t3, f0\n"
      "sw t3, 2036(t2) # 0x7F4=0x7FC-8\n"
      "srli t3, t3, 32\n"
      "sw t3, 2040(t2) # 0x7F4 + 4\n";

  DriverStr(expected, "StoreFpuToOffset");
}

#undef __

}  // namespace art
