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

#include "disassembler_riscv64.h"

#include "android-base/logging.h"
#include "android-base/stringprintf.h"

#include "base/bit_utils.h"
#include "base/casts.h"

using android::base::StringPrintf;

namespace art {
namespace riscv64 {

class DisassemblerRiscv64::Printer {
 public:
  Printer(DisassemblerRiscv64* disassembler, std::ostream& os)
      : disassembler_(disassembler), os_(os) {}

  void Dump32(const uint8_t* insn);
  void Dump16(const uint8_t* insn);
  void Dump2Byte(const uint8_t* data);
  void DumpByte(const uint8_t* data);

 private:
  // This enumeration should mirror the declarations in runtime/arch/riscv64/registers_riscv64.h.
  // We do not include that file to avoid a dependency on libart.
  enum {
    Zero = 0,
    RA = 1,
    FP  = 8,
    TR  = 9,
  };

  static const char* XRegName(uint32_t regno);
  static const char* FRegName(uint32_t regno);
  static const char* VRegName(uint32_t regno);
  static const char* RoundingModeName(uint32_t rm);

  static int32_t Decode32Imm12(uint32_t insn32) {
    uint32_t sign = (insn32 >> 31);
    uint32_t imm12 = (insn32 >> 20);
    return static_cast<int32_t>(imm12) - static_cast<int32_t>(sign << 12);  // Sign-extend.
  }

  static uint32_t Decode32UImm7(uint32_t insn32) { return (insn32 >> 25) & 0x7FU; }

  static uint32_t Decode32UImm12(uint32_t insn32) { return (insn32 >> 20) & 0x7FU; }

  static int32_t Decode32StoreOffset(uint32_t insn32) {
    uint32_t bit11 = insn32 >> 31;
    uint32_t bits5_11 = insn32 >> 25;
    uint32_t bits0_4 = (insn32 >> 7) & 0x1fu;
    uint32_t imm = (bits5_11 << 5) + bits0_4;
    return static_cast<int32_t>(imm) - static_cast<int32_t>(bit11 << 12);  // Sign-extend.
  }

  static uint32_t GetRd(uint32_t insn32) { return (insn32 >> 7) & 0x1fu; }
  static uint32_t GetRs1(uint32_t insn32) { return (insn32 >> 15) & 0x1fu; }
  static uint32_t GetRs2(uint32_t insn32) { return (insn32 >> 20) & 0x1fu; }
  static uint32_t GetRs3(uint32_t insn32) { return insn32 >> 27; }
  static uint32_t GetRoundingMode(uint32_t insn32) { return (insn32 >> 12) & 7u; }

  void PrintBranchOffset(int32_t offset);
  void PrintLoadStoreAddress(uint32_t rs1, int32_t offset);

  void Print32Lui(uint32_t insn32);
  void Print32Auipc(const uint8_t* insn, uint32_t insn32);
  void Print32Jal(const uint8_t* insn, uint32_t insn32);
  void Print32Jalr(const uint8_t* insn, uint32_t insn32);
  void Print32BCond(const uint8_t* insn, uint32_t insn32);
  void Print32Load(uint32_t insn32);
  void Print32Store(uint32_t insn32);
  void Print32FLoad(uint32_t insn32);
  void Print32FStore(uint32_t insn32);
  void Print32BinOpImm(uint32_t insn32);
  void Print32BinOp(uint32_t insn32);
  void Print32Atomic(uint32_t insn32);
  void Print32FpOp(uint32_t insn32);
  void Print32RVVOp(uint32_t insn32);
  void Print32FpFma(uint32_t insn32);
  void Print32Zicsr(uint32_t insn32);
  void Print32Fence(uint32_t insn32);

  static const char* decodeRVVMemInstr(const uint32_t insn32, const char*& rs2, bool isLoad);

  DisassemblerRiscv64* const disassembler_;
  std::ostream& os_;
};

const char* DisassemblerRiscv64::Printer::XRegName(uint32_t regno) {
  static const char* const kXRegisterNames[] = {
      "zero",
      "ra",
      "sp",
      "gp",
      "tp",
      "t0",
      "t1",
      "t2",
      "fp",  // s0/fp
      "tr",  // s1/tr - ART thread register
      "a0",
      "a1",
      "a2",
      "a3",
      "a4",
      "a5",
      "a6",
      "a7",
      "s2",
      "s3",
      "s4",
      "s5",
      "s6",
      "s7",
      "s8",
      "s9",
      "s10",
      "s11",
      "t3",
      "t4",
      "t5",
      "t6",
  };
  static_assert(std::size(kXRegisterNames) == 32);
  DCHECK_LT(regno, 32u);
  return kXRegisterNames[regno];
}

const char* DisassemblerRiscv64::Printer::FRegName(uint32_t regno) {
  static const char* const kFRegisterNames[] = {
      "ft0",
      "ft1",
      "ft2",
      "ft3",
      "ft4",
      "ft5",
      "ft6",
      "ft7",
      "fs0",
      "fs1",
      "fa0",
      "fa1",
      "fa2",
      "fa3",
      "fa4",
      "fa5",
      "fa6",
      "fa7",
      "fs2",
      "fs3",
      "fs4",
      "fs5",
      "fs6",
      "fs7",
      "fs8",
      "fs9",
      "fs10",
      "fs11",
      "ft8",
      "ft9",
      "ft10",
      "ft11",
  };
  static_assert(std::size(kFRegisterNames) == 32);
  DCHECK_LT(regno, 32u);
  return kFRegisterNames[regno];
}

const char* DisassemblerRiscv64::Printer::VRegName(uint32_t regno) {
  static const char* const kVRegisterNames[] = {
      "v0",  "v1",  "v2",  "v3",  "v4",  "v5",  "v6",  "v7",  "v8",  "v9",  "v10",
      "v11", "v12", "v13", "v14", "v15", "v16", "v17", "v18", "v19", "v20", "v21",
      "v22", "v23", "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31"};
  static_assert(std::size(kVRegisterNames) == 32);
  DCHECK_LT(regno, 32u);
  return kVRegisterNames[regno];
}

const char* DisassemblerRiscv64::Printer::RoundingModeName(uint32_t rm) {
  // Note: We do not print the rounding mode for DYN.
  static const char* const kRoundingModeNames[] = {
      ".rne", ".rtz", ".rdn", ".rup", ".rmm", ".<reserved-rm>", ".<reserved-rm>", /*DYN*/ ""
  };
  static_assert(std::size(kRoundingModeNames) == 8);
  DCHECK_LT(rm, 8u);
  return kRoundingModeNames[rm];
}

void DisassemblerRiscv64::Printer::PrintBranchOffset(int32_t offset) {
  os_ << (offset >= 0 ? "+" : "") << offset;
}

void DisassemblerRiscv64::Printer::PrintLoadStoreAddress(uint32_t rs1, int32_t offset) {
  if (offset != 0) {
    os_ << StringPrintf("%d", offset);
  }
  os_ << "(" << XRegName(rs1) << ")";

  if (rs1 == TR && offset >= 0) {
    // Add entrypoint name.
    os_ << " ; ";
    disassembler_->GetDisassemblerOptions()->thread_offset_name_function_(
        os_, dchecked_integral_cast<uint32_t>(offset));
  }
}

void DisassemblerRiscv64::Printer::Print32Lui(uint32_t insn32) {
  DCHECK_EQ(insn32 & 0x7fu, 0x37u);
  // TODO(riscv64): Should we also print the actual sign-extend value?
  os_ << StringPrintf("lui %s, %u", XRegName(GetRd(insn32)), insn32 >> 12);
}

void DisassemblerRiscv64::Printer::Print32Auipc([[maybe_unused]] const uint8_t* insn,
                                                uint32_t insn32) {
  DCHECK_EQ(insn32 & 0x7fu, 0x17u);
  // TODO(riscv64): Should we also print the calculated address?
  os_ << StringPrintf("auipc %s, %u", XRegName(GetRd(insn32)), insn32 >> 12);
}

void DisassemblerRiscv64::Printer::Print32Jal(const uint8_t* insn, uint32_t insn32) {
  DCHECK_EQ(insn32 & 0x7fu, 0x6fu);
  // Print an alias if available.
  uint32_t rd = GetRd(insn32);
  os_ << (rd == Zero ? "j " : "jal ");
  if (rd != Zero && rd != RA) {
    os_ << XRegName(rd) << ", ";
  }
  uint32_t bit20 = (insn32 >> 31);
  uint32_t bits1_10 = (insn32 >> 21) & 0x3ffu;
  uint32_t bit11 = (insn32 >> 20) & 1u;
  uint32_t bits12_19 = (insn32 >> 12) & 0xffu;
  uint32_t imm = (bits1_10 << 1) + (bit11 << 11) + (bits12_19 << 12) + (bit20 << 20);
  int32_t offset = static_cast<int32_t>(imm) - static_cast<int32_t>(bit20 << 21);  // Sign-extend.
  PrintBranchOffset(offset);
  os_ << " ; " << disassembler_->FormatInstructionPointer(insn + offset);

  // TODO(riscv64): When we implement shared thunks to reduce AOT slow-path code size,
  // check if this JAL lands at an entrypoint load from TR and, if so, print its name.
}

void DisassemblerRiscv64::Printer::Print32Jalr([[maybe_unused]] const uint8_t* insn,
                                               uint32_t insn32) {
  DCHECK_EQ(insn32 & 0x7fu, 0x67u);
  DCHECK_EQ((insn32 >> 12) & 7u, 0u);
  uint32_t rd = GetRd(insn32);
  uint32_t rs1 = GetRs1(insn32);
  int32_t imm12 = Decode32Imm12(insn32);
  // Print shorter macro instruction notation if available.
  if (rd == Zero && rs1 == RA && imm12 == 0) {
    os_ << "ret";
  } else if (rd == Zero && imm12 == 0) {
    os_ << "jr " << XRegName(rs1);
  } else if (rd == RA && imm12 == 0) {
    os_ << "jalr " << XRegName(rs1);
  } else {
    // TODO(riscv64): Should we also print the calculated address if the preceding
    // instruction is AUIPC? (We would need to record the previous instruction.)
    os_ << "jalr " << XRegName(rd) << ", ";
    // Use the same format as llvm-objdump: "rs1" if `imm12` is zero, otherwise "imm12(rs1)".
    if (imm12 == 0) {
      os_ << XRegName(rs1);
    } else {
      os_ << imm12 << "(" << XRegName(rs1) << ")";
    }
  }
}

void DisassemblerRiscv64::Printer::Print32BCond(const uint8_t* insn, uint32_t insn32) {
  DCHECK_EQ(insn32 & 0x7fu, 0x63u);
  static const char* const kOpcodes[] = {
      "beq", "bne", nullptr, nullptr, "blt", "bge", "bltu", "bgeu"
  };
  uint32_t funct3 = (insn32 >> 12) & 7u;
  const char* opcode = kOpcodes[funct3];
  if (opcode == nullptr) {
    os_ << "<unknown32>";
    return;
  }

  // Print shorter macro instruction notation if available.
  uint32_t rs1 = GetRs1(insn32);
  uint32_t rs2 = GetRs2(insn32);
  if (rs2 == Zero) {
    os_ << opcode << "z " << XRegName(rs1);
  } else if (rs1 == Zero && (funct3 == 4u || funct3 == 5u)) {
    // blt zero, rs2, offset ... bgtz rs2, offset
    // bge zero, rs2, offset ... blez rs2, offset
    os_ << (funct3 == 4u ? "bgtz " : "blez ") << XRegName(rs2);
  } else {
    os_ << opcode << " " << XRegName(rs1) << ", " << XRegName(rs2);
  }
  os_ << ", ";

  uint32_t bit12 = insn32 >> 31;
  uint32_t bits5_10 = (insn32 >> 25) & 0x3fu;
  uint32_t bits1_4 = (insn32 >> 8) & 0xfu;
  uint32_t bit11 = (insn32 >> 7) & 1u;
  uint32_t imm = (bit12 << 12) + (bit11 << 11) + (bits5_10 << 5) + (bits1_4 << 1);
  int32_t offset = static_cast<int32_t>(imm) - static_cast<int32_t>(bit12 << 13);  // Sign-extend.
  PrintBranchOffset(offset);
  os_ << " ; " << disassembler_->FormatInstructionPointer(insn + offset);
}

void DisassemblerRiscv64::Printer::Print32Load(uint32_t insn32) {
  DCHECK_EQ(insn32 & 0x7fu, 0x03u);
  static const char* const kOpcodes[] = {
      "lb", "lh", "lw", "ld", "lbu", "lhu", "lwu", nullptr
  };
  uint32_t funct3 = (insn32 >> 12) & 7u;
  const char* opcode = kOpcodes[funct3];
  if (opcode == nullptr) {
    os_ << "<unknown32>";
    return;
  }

  os_ << opcode << " " << XRegName(GetRd(insn32)) << ", ";
  PrintLoadStoreAddress(GetRs1(insn32), Decode32Imm12(insn32));

  // TODO(riscv64): If previous instruction is AUIPC for current `rs1` and we load
  // from the range specified by assembler options, print the loaded literal.
}

void DisassemblerRiscv64::Printer::Print32Store(uint32_t insn32) {
  DCHECK_EQ(insn32 & 0x7fu, 0x23u);
  static const char* const kOpcodes[] = {
      "sb", "sh", "sw", "sd", nullptr, nullptr, nullptr, nullptr
  };
  uint32_t funct3 = (insn32 >> 12) & 7u;
  const char* opcode = kOpcodes[funct3];
  if (opcode == nullptr) {
    os_ << "<unknown32>";
    return;
  }

  os_ << opcode << " " << XRegName(GetRs2(insn32)) << ", ";
  PrintLoadStoreAddress(GetRs1(insn32), Decode32StoreOffset(insn32));
}

enum class MemoryAddressMode : uint32_t {
  kMOP_UnitStride = 0b00,
  kMOP_IndexedUnordered = 0b01,
  kMOP_Strided = 0b10,
  kMOP_IndexedOrdered = 0b11,
};

enum class Nf : uint32_t {
  kNfg1 = 0b000,
  kNfg2 = 0b001,
  kNfg3 = 0b010,
  kNfg4 = 0b011,
  kNfg5 = 0b100,
  kNfg6 = 0b101,
  kNfg7 = 0b110,
  kNfg8 = 0b111,
};

const char* DisassemblerRiscv64::Printer::decodeRVVMemInstr(const uint32_t insn32,
                                                            const char*& rs2,
                                                            bool isLoad) {
  const uint32_t funct3 = (insn32 >> 12) & 7u;
  const uint32_t imm7 = Decode32UImm7(insn32);
  const enum Nf nf = static_cast<enum Nf>((imm7 >> 4) & 0x7U);
  const enum MemoryAddressMode mop = static_cast<enum MemoryAddressMode>((imm7 >> 1) & 0x3U);

  switch (mop) {
    case MemoryAddressMode::kMOP_UnitStride: {
      const uint32_t umop = GetRs2(insn32);
      switch (umop) {
        case 0b00000:  // Vector Unit-Stride Load
          static const char* const VUSL_Opcodes[8][8] = {
              {"vle8.v", nullptr, nullptr, nullptr, nullptr, "vle16.v", "vle32.v", "vle64.v"},
              {"vlseg2e8.v",
               nullptr,
               nullptr,
               nullptr,
               nullptr,
               "vlseg2e16.v",
               "vlseg2e32.v",
               "vlseg2e64.v"},
              {"vlseg3e8.v",
               nullptr,
               nullptr,
               nullptr,
               nullptr,
               "vlseg3e16.v",
               "vlseg3e32.v",
               "vlseg3e64.v"},
              {"vlseg4e8.v",
               nullptr,
               nullptr,
               nullptr,
               nullptr,
               "vlseg4e16.v",
               "vlseg4e32.v",
               "vlseg4e64.v"},
              {"vlseg5e8.v",
               nullptr,
               nullptr,
               nullptr,
               nullptr,
               "vlseg5e16.v",
               "vlseg5e32.v",
               "vlseg5e64.v"},
              {"vlseg6e8.v",
               nullptr,
               nullptr,
               nullptr,
               nullptr,
               "vlseg6e16.v",
               "vlseg6e32.v",
               "vlseg6e64.v"},
              {"vlseg7e8.v",
               nullptr,
               nullptr,
               nullptr,
               nullptr,
               "vlseg7e16.v",
               "vlseg7e32.v",
               "vlseg7e64.v"},
              {"vlseg8e8.v",
               nullptr,
               nullptr,
               nullptr,
               nullptr,
               "vlseg8e16.v",
               "vlseg8e32.v",
               "vlseg8e64.v"},
          };
          // Unit Stride Store mnemonics
          static const char* const VUSS_Opcodes[8][8] = {
              {"vse8.v", nullptr, nullptr, nullptr, nullptr, "vse16.v", "vse32.v", "vse64.v"},
              {"vsseg2e8.v",
               nullptr,
               nullptr,
               nullptr,
               nullptr,
               "vsseg2e16.v",
               "vsseg2e32.v",
               "vsseg2e64.v"},
              {"vsseg3e8.v",
               nullptr,
               nullptr,
               nullptr,
               nullptr,
               "vsseg3e16.v",
               "vsseg3e32.v",
               "vsseg3e64.v"},
              {"vsseg4e8.v",
               nullptr,
               nullptr,
               nullptr,
               nullptr,
               "vsseg4e16.v",
               "vsseg4e32.v",
               "vsseg4e64.v"},
              {"vsseg5e8.v",
               nullptr,
               nullptr,
               nullptr,
               nullptr,
               "vsseg5e16.v",
               "vsseg5e32.v",
               "vsseg5e64.v"},
              {"vsseg6e8.v",
               nullptr,
               nullptr,
               nullptr,
               nullptr,
               "vsseg6e16.v",
               "vsseg6e32.v",
               "vsseg6e64.v"},
              {"vsseg7e8.v",
               nullptr,
               nullptr,
               nullptr,
               nullptr,
               "vsseg7e16.v",
               "vsseg7e32.v",
               "vsseg7e64.v"},
              {"vsseg8e8.v",
               nullptr,
               nullptr,
               nullptr,
               nullptr,
               "vsseg8e16.v",
               "vsseg8e32.v",
               "vsseg8e64.v"},
          };
          return isLoad ? VUSL_Opcodes[enum_cast<uint32_t>(nf)][funct3] :
                          VUSS_Opcodes[enum_cast<uint32_t>(nf)][funct3];
        case 0b01000: {  // Vector Whole Register Load
          switch (nf) {
            case Nf::kNfg1:
              static const char* const VWHL1_Opcodes[8] = {"vl1re8.v",
                                                           nullptr,
                                                           nullptr,
                                                           nullptr,
                                                           nullptr,
                                                           "vl1re16.v",
                                                           "vl1re32.v",
                                                           "vl1re64.v"};
              static const char* const VWHS1_Opcodes[8] = {
                  "vs1r.v", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
              return isLoad ? VWHL1_Opcodes[funct3] : VWHS1_Opcodes[funct3];
            case Nf::kNfg2:
              static const char* const VWHL2_Opcodes[8] = {"vl2re8.v",
                                                           nullptr,
                                                           nullptr,
                                                           nullptr,
                                                           nullptr,
                                                           "vl2re16.v",
                                                           "vl2re32.v",
                                                           "vl2re64.v"};
              static const char* const VWHS2_Opcodes[8] = {
                  "vs2r.v", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
              return isLoad ? VWHL2_Opcodes[funct3] : VWHS2_Opcodes[funct3];
            case Nf::kNfg4:
              static const char* const VWHL4_Opcodes[8] = {"vl4re8.v",
                                                           nullptr,
                                                           nullptr,
                                                           nullptr,
                                                           nullptr,
                                                           "vl4re16.v",
                                                           "vl4re32.v",
                                                           "vl4re64.v"};
              static const char* const VWHS4_Opcodes[8] = {
                  "vs4r.v", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
              return isLoad ? VWHL4_Opcodes[funct3] : VWHS4_Opcodes[funct3];
            case Nf::kNfg8:
              static const char* const VWHL8_Opcodes[8] = {"vl8re8.v",
                                                           nullptr,
                                                           nullptr,
                                                           nullptr,
                                                           nullptr,
                                                           "vl8re16.v",
                                                           "vl8re32.v",
                                                           "vl8re64.v"};
              static const char* const VWHS8_Opcodes[8] = {
                  "vs8r.v", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
              return isLoad ? VWHL8_Opcodes[funct3] : VWHS8_Opcodes[funct3];
            default:
              return nullptr;
          }
        }
        case 0b01011:  // Vector Unit-Stride Mask Load
          return isLoad ? "vlm.v" : "vsm.v";
        case 0b10000:  // Vector Unit-Stride Fault-Only-First Load
          static const char* const VUSFFL_Opcodes[8][8] = {
              {"vle8ff.v",
               nullptr,
               nullptr,
               nullptr,
               nullptr,
               "vle16ff.v",
               "vle32ff.v",
               "vle64ff.v"},
              {"vlseg2e8ff.v",
               nullptr,
               nullptr,
               nullptr,
               nullptr,
               "vlseg2e16ff.v",
               "vlseg2e32ff.v",
               "vlseg2e64ff.v"},
              {"vlseg3e8ff.v",
               nullptr,
               nullptr,
               nullptr,
               nullptr,
               "vlseg3e16ff.v",
               "vlseg3e32ff.v",
               "vlseg3e64ff.v"},
              {"vlseg4e8ff.v",
               nullptr,
               nullptr,
               nullptr,
               nullptr,
               "vlseg4e16ff.v",
               "vlseg4e32ff.v",
               "vlseg4e64ff.v"},
              {"vlseg5e8ff.v",
               nullptr,
               nullptr,
               nullptr,
               nullptr,
               "vlseg5e16ff.v",
               "vlseg5e32ff.v",
               "vlseg5e64ff.v"},
              {"vlseg6e8ff.v",
               nullptr,
               nullptr,
               nullptr,
               nullptr,
               "vlseg6e16ff.v",
               "vlseg6e32ff.v",
               "vlseg6e64ff.v"},
              {"vlseg7e8ff.v",
               nullptr,
               nullptr,
               nullptr,
               nullptr,
               "vlseg7e16ff.v",
               "vlseg7e32ff.v",
               "vlseg7e64ff.v"},
              {"vlseg8e8ff.v",
               nullptr,
               nullptr,
               nullptr,
               nullptr,
               "vlseg8e16ff.v",
               "vlseg8e32ff.v",
               "vlseg8e64ff.v"},
          };
          return isLoad ? VUSFFL_Opcodes[enum_cast<uint32_t>(nf)][funct3] :
                          nullptr;  // only loads are possible
        default:                    // Unknown
          return nullptr;
      }
    }
    case MemoryAddressMode::kMOP_IndexedUnordered: {
      static const char* const VIUL_Opcodes[8][8] = {
          {"vluxei8.v",
           nullptr,
           nullptr,
           nullptr,
           nullptr,
           "vluxei16.v",
           "vluxei32.v",
           "vluxei64.v"},
          {"vluxseg2ei8.v",
           nullptr,
           nullptr,
           nullptr,
           nullptr,
           "vluxseg2ei16.v",
           "vluxseg2ei32.v",
           "vluxseg2ei64.v"},
          {"vluxseg3ei8.v",
           nullptr,
           nullptr,
           nullptr,
           nullptr,
           "vluxseg3ei16.v",
           "vluxseg3ei32.v",
           "vluxseg3ei64.v"},
          {"vluxseg4ei8.v",
           nullptr,
           nullptr,
           nullptr,
           nullptr,
           "vluxseg4ei16.v",
           "vluxseg4ei32.v",
           "vluxseg4ei64.v"},
          {"vluxseg5ei8.v",
           nullptr,
           nullptr,
           nullptr,
           nullptr,
           "vluxseg5ei16.v",
           "vluxseg5ei32.v",
           "vluxseg5ei64.v"},
          {"vluxseg6ei8.v",
           nullptr,
           nullptr,
           nullptr,
           nullptr,
           "vluxseg6ei16.v",
           "vluxseg6ei32.v",
           "vluxseg6ei64.v"},
          {"vluxseg7ei8.v",
           nullptr,
           nullptr,
           nullptr,
           nullptr,
           "vluxseg7ei16.v",
           "vluxseg7ei32.v",
           "vluxseg7ei64.v"},
          {"vluxseg8ei8.v",
           nullptr,
           nullptr,
           nullptr,
           nullptr,
           "vluxseg8ei16.v",
           "vluxseg8ei32.v",
           "vluxseg8ei64.v"},
      };
      // Indexed Unordered Store mnemonics
      static const char* const VIUS_Opcodes[8][8] = {
          {"vsuxei8.v",
           nullptr,
           nullptr,
           nullptr,
           nullptr,
           "vsuxei16.v",
           "vsuxei32.v",
           "vsuxei64.v"},
          {"vsuxseg2ei8.v",
           nullptr,
           nullptr,
           nullptr,
           nullptr,
           "vsuxseg2ei16.v",
           "vsuxseg2ei32.v",
           "vsuxseg2ei64.v"},
          {"vsuxseg3ei8.v",
           nullptr,
           nullptr,
           nullptr,
           nullptr,
           "vsuxseg3ei16.v",
           "vsuxseg3ei32.v",
           "vsuxseg3ei64.v"},
          {"vsuxseg4ei8.v",
           nullptr,
           nullptr,
           nullptr,
           nullptr,
           "vsuxseg4ei16.v",
           "vsuxseg4ei32.v",
           "vsuxseg4ei64.v"},
          {"vsuxseg5ei8.v",
           nullptr,
           nullptr,
           nullptr,
           nullptr,
           "vsuxseg5ei16.v",
           "vsuxseg5ei32.v",
           "vsuxseg5ei64.v"},
          {"vsuxseg6ei8.v",
           nullptr,
           nullptr,
           nullptr,
           nullptr,
           "vsuxseg6ei16.v",
           "vsuxseg6ei32.v",
           "vsuxseg6ei64.v"},
          {"vsuxseg7ei8.v",
           nullptr,
           nullptr,
           nullptr,
           nullptr,
           "vsuxseg7ei16.v",
           "vsuxseg7ei32.v",
           "vsuxseg7ei64.v"},
          {"vsuxseg8ei8.v",
           nullptr,
           nullptr,
           nullptr,
           nullptr,
           "vsuxseg8ei16.v",
           "vsuxseg8ei32.v",
           "vsuxseg8ei64.v"},
      };
      rs2 = VRegName(GetRs2(insn32));
      return isLoad ? VIUL_Opcodes[enum_cast<uint32_t>(nf)][funct3] :
                      VIUS_Opcodes[enum_cast<uint32_t>(nf)][funct3];
    }
    case MemoryAddressMode::kMOP_Strided: {
      static const char* const VSL_Opcodes[8][8] = {
          {"vlse8.v", nullptr, nullptr, nullptr, nullptr, "vlse16.v", "vlse32.v", "vlse64.v"},
          {"vlsseg2e8.v",
           nullptr,
           nullptr,
           nullptr,
           nullptr,
           "vlsseg2e16.v",
           "vlsseg2e32.v",
           "vlsseg2e64.v"},
          {"vlsseg3e8.v",
           nullptr,
           nullptr,
           nullptr,
           nullptr,
           "vlsseg3e16.v",
           "vlsseg3e32.v",
           "vlsseg3e64.v"},
          {"vlsseg4e8.v",
           nullptr,
           nullptr,
           nullptr,
           nullptr,
           "vlsseg4e16.v",
           "vlsseg4e32.v",
           "vlsseg4e64.v"},
          {"vlsseg5e8.v",
           nullptr,
           nullptr,
           nullptr,
           nullptr,
           "vlsseg5e16.v",
           "vlsseg5e32.v",
           "vlsseg5e64.v"},
          {"vlsseg6e8.v",
           nullptr,
           nullptr,
           nullptr,
           nullptr,
           "vlsseg6e16.v",
           "vlsseg6e32.v",
           "vlsseg6e64.v"},
          {"vlsseg7e8.v",
           nullptr,
           nullptr,
           nullptr,
           nullptr,
           "vlsseg7e16.v",
           "vlsseg7e32.v",
           "vlsseg7e64.v"},
          {"vlsseg8e8.v",
           nullptr,
           nullptr,
           nullptr,
           nullptr,
           "vlsseg8e16.v",
           "vlsseg8e32.v",
           "vlsseg8e64.v"},
      };
      // Stride Store mnemonics
      static const char* const VSS_Opcodes[8][8] = {
          {"vsse8.v", nullptr, nullptr, nullptr, nullptr, "vsse16.v", "vsse32.v", "vsse64.v"},
          {"vssseg2e8.v",
           nullptr,
           nullptr,
           nullptr,
           nullptr,
           "vssseg2e16.v",
           "vssseg2e32.v",
           "vssseg2e64.v"},
          {"vssseg3e8.v",
           nullptr,
           nullptr,
           nullptr,
           nullptr,
           "vssseg3e16.v",
           "vssseg3e32.v",
           "vssseg3e64.v"},
          {"vssseg4e8.v",
           nullptr,
           nullptr,
           nullptr,
           nullptr,
           "vssseg4e16.v",
           "vssseg4e32.v",
           "vssseg4e64.v"},
          {"vssseg5e8.v",
           nullptr,
           nullptr,
           nullptr,
           nullptr,
           "vssseg5e16.v",
           "vssseg5e32.v",
           "vssseg5e64.v"},
          {"vssseg6e8.v",
           nullptr,
           nullptr,
           nullptr,
           nullptr,
           "vssseg6e16.v",
           "vssseg6e32.v",
           "vssseg6e64.v"},
          {"vssseg7e8.v",
           nullptr,
           nullptr,
           nullptr,
           nullptr,
           "vssseg7e16.v",
           "vssseg7e32.v",
           "vssseg7e64.v"},
          {"vssseg8e8.v",
           nullptr,
           nullptr,
           nullptr,
           nullptr,
           "vssseg8e16.v",
           "vssseg8e32.v",
           "vssseg8e64.v"},
      };
      rs2 = XRegName(GetRs2(insn32));
      return isLoad ? VSL_Opcodes[enum_cast<uint32_t>(nf)][funct3] :
                      VSS_Opcodes[enum_cast<uint32_t>(nf)][funct3];
    }
    case MemoryAddressMode::kMOP_IndexedOrdered: {
      static const char* const VIOL_Opcodes[][8] = {
          {"vloxei8.v",
           nullptr,
           nullptr,
           nullptr,
           nullptr,
           "vloxei16.v",
           "vloxei32.v",
           "vloxei64.v"},
          {"vloxseg2ei8.v",
           nullptr,
           nullptr,
           nullptr,
           nullptr,
           "vloxseg2ei16.v",
           "vloxseg2ei32.v",
           "vloxseg2ei64.v"},
          {"vloxseg3ei8.v",
           nullptr,
           nullptr,
           nullptr,
           nullptr,
           "vloxseg3ei16.v",
           "vloxseg3ei32.v",
           "vloxseg3ei64.v"},
          {"vloxseg4ei8.v",
           nullptr,
           nullptr,
           nullptr,
           nullptr,
           "vloxseg4ei16.v",
           "vloxseg4ei32.v",
           "vloxseg4ei64.v"},
          {"vloxseg5ei8.v",
           nullptr,
           nullptr,
           nullptr,
           nullptr,
           "vloxseg5ei16.v",
           "vloxseg5ei32.v",
           "vloxseg5ei64.v"},
          {"vloxseg6ei8.v",
           nullptr,
           nullptr,
           nullptr,
           nullptr,
           "vloxseg6ei16.v",
           "vloxseg6ei32.v",
           "vloxseg6ei64.v"},
          {"vloxseg7ei8.v",
           nullptr,
           nullptr,
           nullptr,
           nullptr,
           "vloxseg7ei16.v",
           "vloxseg7ei32.v",
           "vloxseg7ei64.v"},
          {"vloxseg8ei8.v",
           nullptr,
           nullptr,
           nullptr,
           nullptr,
           "vloxseg8ei16.v",
           "vloxseg8ei32.v",
           "vloxseg8ei64.v"},
      };
      // Indexed Ordered Store mnemonics
      static const char* const VIOS_Opcodes[][8] = {
          {"vsoxei8.v",
           nullptr,
           nullptr,
           nullptr,
           nullptr,
           "vsoxei16.v",
           "vsoxei32.v",
           "vsoxei64.v"},
          {"vsoxseg2ei8.v",
           nullptr,
           nullptr,
           nullptr,
           nullptr,
           "vsoxseg2ei16.v",
           "vsoxseg2ei32.v",
           "vsoxseg2ei64.v"},
          {"vsoxseg3ei8.v",
           nullptr,
           nullptr,
           nullptr,
           nullptr,
           "vsoxseg3ei16.v",
           "vsoxseg3ei32.v",
           "vsoxseg3ei64.v"},
          {"vsoxseg4ei8.v",
           nullptr,
           nullptr,
           nullptr,
           nullptr,
           "vsoxseg4ei16.v",
           "vsoxseg4ei32.v",
           "vsoxseg4ei64.v"},
          {"vsoxseg5ei8.v",
           nullptr,
           nullptr,
           nullptr,
           nullptr,
           "vsoxseg5ei16.v",
           "vsoxseg5ei32.v",
           "vsoxseg5ei64.v"},
          {"vsoxseg6ei8.v",
           nullptr,
           nullptr,
           nullptr,
           nullptr,
           "vsoxseg6ei16.v",
           "vsoxseg6ei32.v",
           "vsoxseg6ei64.v"},
          {"vsoxseg7ei8.v",
           nullptr,
           nullptr,
           nullptr,
           nullptr,
           "vsoxseg7ei16.v",
           "vsoxseg7ei32.v",
           "vsoxseg7ei64.v"},
          {"vsoxseg8ei8.v",
           nullptr,
           nullptr,
           nullptr,
           nullptr,
           "vsoxseg8ei16.v",
           "vsoxseg8ei32.v",
           "vsoxseg8ei64.v"},
      };
      rs2 = VRegName(GetRs2(insn32));
      return isLoad ? VIOL_Opcodes[enum_cast<uint32_t>(nf)][funct3] :
                      VIOS_Opcodes[enum_cast<uint32_t>(nf)][funct3];
    }
  }
}  // NOLINT(readability/fn_size)

void DisassemblerRiscv64::Printer::Print32FLoad(uint32_t insn32) {
  DCHECK_EQ(insn32 & 0x7fu, 0x07u);
  static const char* const kOpcodes[] = {
      nullptr, "flh", "flw", "fld", "flq", nullptr, nullptr, nullptr};
  int32_t offset = 0;
  const char *rd = nullptr, *rs2 = nullptr, *vm = "";
  const uint32_t funct3 = (insn32 >> 12) & 7u;
  const char* opcode = kOpcodes[funct3];
  if (opcode == nullptr) {
    // Vector Loads
    opcode = decodeRVVMemInstr(insn32, rs2, true);
    rd = VRegName(GetRd(insn32));

    if ((Decode32UImm7(insn32) & 0x1U) == 0) {
      vm = ", vm";
    }
  } else {
    rd = FRegName(GetRd(insn32));
    offset = Decode32Imm12(insn32);
  }

  if (opcode == nullptr) {
    os_ << "<unknown32>";
    return;
  }

  os_ << opcode << " " << rd << ", ";
  PrintLoadStoreAddress(GetRs1(insn32), offset);

  if (rs2) {
    os_ << ", " << rs2;
  }

  os_ << vm;

  // TODO(riscv64): If previous instruction is AUIPC for current `rs1` and we load
  // from the range specified by assembler options, print the loaded literal.
}

void DisassemblerRiscv64::Printer::Print32FStore(uint32_t insn32) {
  DCHECK_EQ(insn32 & 0x7fu, 0x27u);
  static const char* const kOpcodes[] = {
      nullptr, "fsh", "fsw", "fsd", "fsq", nullptr, nullptr, nullptr};

  uint32_t funct3 = (insn32 >> 12) & 7u;
  const char* opcode = kOpcodes[funct3];

  if (opcode == nullptr) {
    // Vector Stores
    const char* rs2 = nullptr;
    opcode = decodeRVVMemInstr(insn32, rs2, false);

    if (opcode == nullptr) {
      os_ << "<unknown32>";
      return;
    }

    os_ << opcode << " " << VRegName(GetRd(insn32)) << ", ";
    PrintLoadStoreAddress(GetRs1(insn32), 0);

    if (rs2) {
      os_ << ", " << rs2;
    }

    if ((Decode32UImm7(insn32) & 0x1U) == 0) {
      os_ << ", vm";
    }
  } else {
    os_ << opcode << " " << FRegName(GetRs2(insn32)) << ", ";
    PrintLoadStoreAddress(GetRs1(insn32), Decode32StoreOffset(insn32));
  }
}

void DisassemblerRiscv64::Printer::Print32BinOpImm(uint32_t insn32) {
  DCHECK_EQ(insn32 & 0x77u, 0x13u);  // Note: Bit 0x8 selects narrow binop.
  bool narrow = (insn32 & 0x8u) != 0u;
  uint32_t funct3 = (insn32 >> 12) & 7u;
  uint32_t rd = GetRd(insn32);
  uint32_t rs1 = GetRs1(insn32);
  int32_t imm = Decode32Imm12(insn32);

  // Print shorter macro instruction notation if available.
  if (funct3 == /*ADDI*/ 0u && imm == 0u) {
    if (narrow) {
      os_ << "sextw " << XRegName(rd) << ", " << XRegName(rs1);
    } else if (rd == Zero && rs1 == Zero) {
      os_ << "nop";  // Only canonical nop. Non-Zero `rd == rs1` nops are printed as "mv".
    } else {
      os_ << "mv " << XRegName(rd) << ", " << XRegName(rs1);
    }
  } else if (!narrow && funct3 == /*XORI*/ 4u && imm == -1) {
    os_ << "not " << XRegName(rd) << ", " << XRegName(rs1);
  } else if (!narrow && funct3 == /*ANDI*/ 7u && imm == 0xff) {
    os_ << "zextb " << XRegName(rd) << ", " << XRegName(rs1);
  } else if (!narrow && funct3 == /*SLTIU*/ 3u && imm == 1) {
    os_ << "seqz " << XRegName(rd) << ", " << XRegName(rs1);
  } else if ((insn32 & 0xfc00707fu) == 0x0800101bu) {
    os_ << "slli.uw " << XRegName(rd) << ", " << XRegName(rs1) << ", " << (imm & 0x3fu);
  } else if ((imm ^ 0x600u) < 3u && funct3 == 1u) {
    static const char* const kBitOpcodes[] = { "clz", "ctz", "cpop" };
    os_ << kBitOpcodes[imm ^ 0x600u] << (narrow ? "w " : " ")
        << XRegName(rd) << ", " << XRegName(rs1);
  } else if ((imm ^ 0x600u) < (narrow ? 32 : 64) && funct3 == 5u) {
    os_ << "rori" << (narrow ? "w " : " ")
        << XRegName(rd) << ", " << XRegName(rs1) << ", " << (imm ^ 0x600u);
  } else if (imm == 0x287u && !narrow && funct3 == 5u) {
    os_ << "orc.b " << XRegName(rd) << ", " << XRegName(rs1);
  } else if (imm == 0x6b8u && !narrow && funct3 == 5u) {
    os_ << "rev8 " << XRegName(rd) << ", " << XRegName(rs1);
  } else {
    bool bad_high_bits = false;
    if (funct3 == /*SLLI*/ 1u || funct3 == /*SRLI/SRAI*/ 5u) {
      imm &= (narrow ? 0x1fu : 0x3fu);
      uint32_t high_bits = insn32 & (narrow ? 0xfe000000u : 0xfc000000u);
      if (high_bits == 0x40000000u && funct3 == /*SRAI*/ 5u) {
        os_ << "srai";
      } else {
        os_ << ((funct3 == /*SRLI*/ 5u) ? "srli" : "slli");
        bad_high_bits = (high_bits != 0u);
      }
    } else if (!narrow || funct3 == /*ADDI*/ 0u) {
      static const char* const kOpcodes[] = {
          "addi", nullptr, "slti", "sltiu", "xori", nullptr, "ori", "andi"
      };
      DCHECK(kOpcodes[funct3] != nullptr);
      os_ << kOpcodes[funct3];
    } else {
      os_ << "<unknown32>";  // There is no SLTIW/SLTIUW/XORIW/ORIW/ANDIW.
      return;
    }
    os_ << (narrow ? "w " : " ") << XRegName(rd) << ", " << XRegName(rs1) << ", " << imm;
    if (bad_high_bits) {
      os_ << " (invalid high bits)";
    }
  }
}

void DisassemblerRiscv64::Printer::Print32BinOp(uint32_t insn32) {
  DCHECK_EQ(insn32 & 0x77u, 0x33u);  // Note: Bit 0x8 selects narrow binop.
  bool narrow = (insn32 & 0x8u) != 0u;
  uint32_t funct3 = (insn32 >> 12) & 7u;
  uint32_t rd = GetRd(insn32);
  uint32_t rs1 = GetRs1(insn32);
  uint32_t rs2 = GetRs2(insn32);
  uint32_t high_bits = insn32 & 0xfe000000u;

  // Print shorter macro instruction notation if available.
  if (high_bits == 0x40000000u && funct3 == /*SUB*/ 0u && rs1 == Zero) {
    os_ << (narrow ? "negw " : "neg ") << XRegName(rd) << ", " << XRegName(rs2);
  } else if (!narrow && funct3 == /*SLT*/ 2u && rs2 == Zero) {
    os_ << "sltz " << XRegName(rd) << ", " << XRegName(rs1);
  } else if (!narrow && funct3 == /*SLT*/ 2u && rs1 == Zero) {
    os_ << "sgtz " << XRegName(rd) << ", " << XRegName(rs2);
  } else if (!narrow && funct3 == /*SLTU*/ 3u && rs1 == Zero) {
    os_ << "snez " << XRegName(rd) << ", " << XRegName(rs2);
  } else if (narrow && high_bits == 0x08000000u && funct3 == /*ADD.UW*/ 0u && rs2 == Zero) {
    os_ << "zext.w " << XRegName(rd) << ", " << XRegName(rs1);
  } else {
    bool bad_high_bits = false;
    if (high_bits == 0x40000000u && (funct3 == /*SUB*/ 0u || funct3 == /*SRA*/ 5u)) {
      os_ << ((funct3 == /*SUB*/ 0u) ? "sub" : "sra");
    } else if (high_bits == 0x02000000u &&
               (!narrow || (funct3 == /*MUL*/ 0u || funct3 >= /*DIV/DIVU/REM/REMU*/ 4u))) {
      static const char* const kOpcodes[] = {
          "mul", "mulh", "mulhsu", "mulhu", "div", "divu", "rem", "remu"
      };
      os_ << kOpcodes[funct3];
    } else if (high_bits == 0x08000000u && narrow && funct3 == /*ADD.UW*/ 0u) {
      os_ << "add.u";  // "w" is added below.
    } else if (high_bits == 0x20000000u && (funct3 & 1u) == 0u && funct3 != 0u) {
      static const char* const kZbaOpcodes[] = { nullptr, "sh1add", "sh2add", "sh3add" };
      DCHECK(kZbaOpcodes[funct3 >> 1] != nullptr);
      os_ << kZbaOpcodes[funct3 >> 1] << (narrow ? ".u" /* "w" is added below. */ : "");
    } else if (high_bits == 0x40000000u && !narrow && funct3 >= 4u && funct3 != 5u) {
      static const char* const kZbbNegOpcodes[] = { "xnor", nullptr, "orn", "andn" };
      DCHECK(kZbbNegOpcodes[funct3 - 4u] != nullptr);
      os_ << kZbbNegOpcodes[funct3 - 4u];
    } else if (high_bits == 0x0a000000u && !narrow && funct3 >= 4u) {
      static const char* const kZbbMinMaxOpcodes[] = { "min", "minu", "max", "maxu" };
      DCHECK(kZbbMinMaxOpcodes[funct3 - 4u] != nullptr);
      os_ << kZbbMinMaxOpcodes[funct3 - 4u];
    } else if (high_bits == 0x60000000u && (funct3 == /*ROL*/ 1u || funct3 == /*ROL*/ 5u)) {
      os_ << (funct3 == /*ROL*/ 1u ? "rol" : "ror");
    } else if (!narrow || (funct3 == /*ADD*/ 0u || funct3 == /*SLL*/ 1u || funct3 == /*SRL*/ 5u)) {
      static const char* const kOpcodes[] = {
          "add", "sll", "slt", "sltu", "xor", "srl", "or", "and"
      };
      os_ << kOpcodes[funct3];
      bad_high_bits = (high_bits != 0u);
    } else {
      DCHECK(narrow);
      os_ << "<unknown32>";  // Some of the above instructions do not have a narrow version.
      return;
    }
    os_ << (narrow ? "w " : " ") << XRegName(rd) << ", " << XRegName(rs1) << ", " << XRegName(rs2);
    if (bad_high_bits) {
      os_ << " (invalid high bits)";
    }
  }
}

void DisassemblerRiscv64::Printer::Print32Atomic(uint32_t insn32) {
  DCHECK_EQ(insn32 & 0x7fu, 0x2fu);
  uint32_t funct3 = (insn32 >> 12) & 7u;
  uint32_t funct5 = (insn32 >> 27);
  if ((funct3 != 2u && funct3 != 3u) ||  // There are only 32-bit and 64-bit LR/SC/AMO*.
      (((funct5 & 3u) != 0u) && funct5 >= 4u)) {  // Only multiples of 4, or 1-3.
    os_ << "<unknown32>";
    return;
  }
  static const char* const kMul4Opcodes[] = {
      "amoadd", "amoxor", "amoor", "amoand", "amomin", "amomax", "amominu", "amomaxu"
  };
  static const char* const kOtherOpcodes[] = {
      nullptr, "amoswap", "lr", "sc"
  };
  const char* opcode = ((funct5 & 3u) == 0u) ? kMul4Opcodes[funct5 >> 2] : kOtherOpcodes[funct5];
  DCHECK(opcode != nullptr);
  uint32_t rd = GetRd(insn32);
  uint32_t rs1 = GetRs1(insn32);
  uint32_t rs2 = GetRs2(insn32);
  const char* type = (funct3 == 2u) ? ".w" : ".d";
  const char* aq = (((insn32 >> 26) & 1u) != 0u) ? ".aq" : "";
  const char* rl = (((insn32 >> 25) & 1u) != 0u) ? ".rl" : "";
  os_ << opcode << type << aq << rl << " " << XRegName(rd) << ", " << XRegName(rs1);
  if (funct5 == /*LR*/ 2u) {
    if (rs2 != 0u) {
      os_ << " (bad rs2)";
    }
  } else {
    os_ << ", " << XRegName(rs2);
  }
}

void DisassemblerRiscv64::Printer::Print32FpOp(uint32_t insn32) {
  DCHECK_EQ(insn32 & 0x7fu, 0x53u);
  uint32_t rd = GetRd(insn32);
  uint32_t rs1 = GetRs1(insn32);
  uint32_t rs2 = GetRs2(insn32);  // Sometimes used to to differentiate opcodes.
  uint32_t rm = GetRoundingMode(insn32);  // Sometimes used to to differentiate opcodes.
  uint32_t funct7 = insn32 >> 25;
  const char* type = ((funct7 & 1u) != 0u) ? ".d" : ".s";
  if ((funct7 & 2u) != 0u) {
    os_ << "<unknown32>";  // Note: This includes the "H" and "Q" extensions.
    return;
  }
  switch (funct7 >> 2) {
    case 0u:
    case 1u:
    case 2u:
    case 3u: {
      static const char* const kOpcodes[] = { "fadd", "fsub", "fmul", "fdiv" };
      os_ << kOpcodes[funct7 >> 2] << type << RoundingModeName(rm) << " "
          << FRegName(rd) << ", " << FRegName(rs1) << ", " << FRegName(rs2);
      return;
    }
    case 4u: {  // FSGN*
      // Print shorter macro instruction notation if available.
      static const char* const kOpcodes[] = { "fsgnj", "fsgnjn", "fsgnjx" };
      if (rm < std::size(kOpcodes)) {
        if (rs1 == rs2) {
          static const char* const kAltOpcodes[] = { "fmv", "fneg", "fabs" };
          static_assert(std::size(kOpcodes) == std::size(kAltOpcodes));
          os_ << kAltOpcodes[rm] << type << " " << FRegName(rd) << ", " << FRegName(rs1);
        } else {
          os_ << kOpcodes[rm] << type << " "
              << FRegName(rd) << ", " << FRegName(rs1) << ", " << FRegName(rs2);
        }
        return;
      }
      break;
    }
    case 5u: {  // FMIN/FMAX
      static const char* const kOpcodes[] = { "fmin", "fmax" };
      if (rm < std::size(kOpcodes)) {
        os_ << kOpcodes[rm] << type << " "
            << FRegName(rd) << ", " << FRegName(rs1) << ", " << FRegName(rs2);
        return;
      }
      break;
    }
    case 0x8u:  // FCVT between FP numbers.
      if ((rs2 ^ 1u) == (funct7 & 1u)) {
        os_ << ((rs2 != 0u) ? "fcvt.s.d" : "fcvt.d.s") << RoundingModeName(rm) << " "
            << FRegName(rd) << ", " << FRegName(rs1);
      }
      break;
    case 0xbu:
      if (rs2 == 0u) {
        os_ << "fsqrt" << type << RoundingModeName(rm) << " "
            << FRegName(rd) << ", " << FRegName(rs1);
        return;
      }
      break;
    case 0x14u: {  // FLE/FLT/FEQ
      static const char* const kOpcodes[] = { "fle", "flt", "feq" };
      if (rm < std::size(kOpcodes)) {
        os_ << kOpcodes[rm] << type << " "
            << XRegName(rd) << ", " << FRegName(rs1) << ", " << FRegName(rs2);
        return;
      }
      break;
    }
    case 0x18u: {  // FCVT from floating point numbers to integers
      static const char* const kIntTypes[] = { "w", "wu", "l", "lu" };
      if (rs2 < std::size(kIntTypes)) {
        os_ << "fcvt." << kIntTypes[rs2] << type << RoundingModeName(rm) << " "
            << XRegName(rd) << ", " << FRegName(rs1);
        return;
      }
      break;
    }
    case 0x1au: {  // FCVT from integers to floating point numbers
      static const char* const kIntTypes[] = { "w", "wu", "l", "lu" };
      if (rs2 < std::size(kIntTypes)) {
        os_ << "fcvt" << type << "." << kIntTypes[rs2] << RoundingModeName(rm) << " "
            << FRegName(rd) << ", " << XRegName(rs1);
        return;
      }
      break;
    }
    case 0x1cu:  // FMV from FPR to GPR, or FCLASS
      if (rs2 == 0u && rm == 0u) {
        os_ << (((funct7 & 1u) != 0u) ? "fmv.x.d " : "fmv.x.w ")
            << XRegName(rd) << ", " << FRegName(rs1);
        return;
      } else if (rs2 == 0u && rm == 1u) {
        os_ << "fclass" << type << " " << XRegName(rd) << ", " << FRegName(rs1);
        return;
      }
      break;
    case 0x1eu:  // FMV from GPR to FPR
      if (rs2 == 0u && rm == 0u) {
        os_ << (((funct7 & 1u) != 0u) ? "fmv.d.x " : "fmv.w.x ")
            << FRegName(rd) << ", " << XRegName(rs1);
        return;
      }
      break;
    default:
      break;
  }
  os_ << "<unknown32>";
}

static bool decodeVType(const uint32_t vtype,
                        const char*& vma,
                        const char*& vta,
                        const char*& vsew,
                        const char*& lmul) {
  const uint32_t lmul_v = vtype & 0x7U;
  const uint32_t vsew_v = (vtype >> 3) & 0x7U;
  const uint32_t vta_v = (vtype >> 6) & 0x1U;
  const uint32_t vma_v = (vtype >> 7) & 0x1U;

  if (vsew_v & 0x4U)
    return false;

  if (lmul_v == 0b100)
    return false;

  vta = vta_v ? "ta" : "tu";
  vma = vma_v ? "ma" : "mu";

  static const char* const vsews[] = {"e8", "e16", "e32", "e64"};

  static const char* const lmuls[] = {"m1", "m2", "m4", "m8", nullptr, "mf8", "mf4", "mf2"};

  vsew = vsews[vsew_v & 0x3];
  lmul = lmuls[lmul_v];

  return true;
}

enum class VAIEncodings : uint32_t {
  // ----Operands---- | Type of Scalar                | instruction type
  kOpIVV = 0b000,  // vector-vector    | --                            | R-type
  kOpFVV = 0b001,  // vector-vector    | --                            | R-type
  kOpMVV = 0b010,  // vector-vector    | --                            | R-type
  kOpIVI = 0b011,  // vector-immediate | imm[4:0]                      | RVV-type
  kOpIVX = 0b100,  // vector-scalar    | GPR x register rs1            | R-type
  kOpFVF = 0b101,  // vector-scalar    | FP f register rs1             | R-type
  kOpMVX = 0b110,  // vector-scalar    | GPR x register rs1            | R-type
  kOpCFG = 0b111,  // scalars-imms     | GPR x register rs1 & rs2/imm  | R/I/U-type
};

static const uint32_t VWXUNARY0 = 0b010000;  // OPMVV 4
static const uint32_t VRXUNARY0 = 0b010000;  // OPMVX 5
static const uint32_t VXUNARY0 = 0b010010;   // OPMVV 4
static const uint32_t VMUNARY0 = 0b010100;   // OPMVV 4

static const uint32_t VWFUNARY0 = 0b010000;  // OPFVV 6
static const uint32_t VRFUNARY0 = 0b010000;  // OPFVF 7
static const uint32_t VFUNARY0 = 0b010010;   // OPFVV 6
static const uint32_t VFUNARY1 = 0b010011;   // OPFVV 6

void DisassemblerRiscv64::Printer::Print32RVVOp(uint32_t insn32) {
  DCHECK_EQ(insn32 & 0x7fu, 0x57u);
  const enum VAIEncodings via = static_cast<enum VAIEncodings>((insn32 >> 12) & 7u);
  const uint32_t funct7 = Decode32UImm7(insn32);
  const uint32_t funct6 = funct7 >> 1;
  const char* vm = funct7 & 1 ? "" : ", vm";
  const char *opcode = nullptr, *rd = nullptr, *rs1 = nullptr, *rs2 = nullptr;

  switch (via) {
    case VAIEncodings::kOpIVV: {  // 1, vv, R VVV
      static const char* const OPIVVOpcodes[64] = {
          "vadd.vv",  nullptr,      "vsub.vv",     nullptr,        "vminu.vv",
          "vmin.vv",  "vmaxu.vv",   "vmax.vv",     nullptr,        "vand.vv",
          "vor.vv",   "vxor.vv",    "vrgather.vv", nullptr,        "vrgatherei16.vv",
          nullptr,    "vadc.vvm",   "vmadc.vvm",   "vsbc.vvm",     "vmsbc.vvm",
          nullptr,    nullptr,      nullptr,       "<vmerge/vmv>", "vmseq.vv",
          "vmsne.vv", "vmsltu.vv",  "vmslt.vv",    "vmsleu.vv",    "vmsle.vv",
          nullptr,    nullptr,      "vsaddu.vv",   "vsadd.vv",     "vssubu.vv",
          "vssub.vv", nullptr,      "vsll.vv",     nullptr,        "vsmul.vv",
          "vsrl.vv",  "vsra.vv",    "vssrl.vv",    "vssra.vv",     "vnsrl.wv",
          "vnsra.wv", "vnclipu.wv", "vnclip.wv",   "vwredsumu.vs", "vwredsum.vs",
          nullptr,    nullptr,      nullptr,       nullptr,        nullptr,
          nullptr,    nullptr,      nullptr,       nullptr,        nullptr,
          nullptr,    nullptr,      nullptr,       nullptr};

      rs2 = VRegName(GetRs2(insn32));
      if (funct6 == 0b010111) {
        // vmerge/vmv
        if ((funct7 & 1) && GetRs2(insn32) == 0) {
          // Note: The vector integer move instructions share the encoding with the vector merge
          // instructions, but with vm=1 and vs2=v0
          opcode = "vmv.v.v";
          rs2 = nullptr;
        } else {
          opcode = "vmerge.vvm";
          vm = ", v0";
        }
        vm = "";
      } else {
        opcode = OPIVVOpcodes[funct6];
      }

      rd = VRegName(GetRd(insn32));
      rs1 = VRegName(GetRs1(insn32));
      break;
    }
    case VAIEncodings::kOpIVX: {  // 2, vx, R VXV
      static const char* const OPIVXOpcodes[64] = {
          "vadd.vx",     nullptr,     "vsub.vx",     "vrsub.vx",      "vminu.vx",   "vmin.vx",
          "vmaxu.vx",    "vmax.vx",   nullptr,       "vand.vx",       "vor.vx",     "vxor.vx",
          "vrgather.vx", nullptr,     "vslideup.vx", "vslidedown.vx", "vadc.vxm",   "vmadc.vxm",
          "vsbc.vxm",    "vmsbc.vxm", nullptr,       nullptr,         nullptr,      "<vmerge/vmv>",
          "vmseq.vx",    "vmsne.vx",  "vmsltu.vx",   "vmslt.vx",      "vmsleu.vx",  "vmsle.vx",
          "vmsgtu.vx",   "vmsgt.vx",  "vsaddu.vx",   "vsadd.vx",      "vssubu.vx",  "vssub.vx",
          nullptr,       "vsll.vx",   nullptr,       "vsmul.vx",      "vsrl.vx",    "vsra.vx",
          "vssrl.vx",    "vssra.vx",  "vnsrl.wx",    "vnsra.wx",      "vnclipu.wx", "vnclip.wx",
          nullptr,       nullptr,     nullptr,       nullptr,         nullptr,      nullptr,
          nullptr,       nullptr,     nullptr,       nullptr,         nullptr,      nullptr,
          nullptr,       nullptr,     nullptr,       nullptr};

      rs2 = VRegName(GetRs2(insn32));
      if (funct6 == 0b010111) {
        // vmerge/vmv
        if ((funct7 & 1) && GetRs2(insn32) == 0) {
          // Note: The vector integer move instructions share the encoding with the vector merge
          // instructions, but with vm=1 and vs2=v0
          opcode = "vmv.v.x";
          rs2 = nullptr;
        } else {
          opcode = "vmerge.vxm";
          vm = ", v0";
        }
        vm = "";
      } else {
        opcode = OPIVXOpcodes[funct6];
      }

      opcode = OPIVXOpcodes[funct6];
      rd = VRegName(GetRd(insn32));
      rs1 = XRegName(GetRs1(insn32));
      break;
    }
    case VAIEncodings::kOpIVI: {  // 3, vi, RVV VIV
      static const char* const OPIVIOpcodes[64] = {
          "vadd.vi",     nullptr,    nullptr,       "vrsub.vi",      nullptr,      nullptr,
          nullptr,       nullptr,    nullptr,       "vand.vi",       "vor.vi",     "vxor.vi",
          "vrgather.vi", nullptr,    "vslideup.vi", "vslidedown.vi", "vadc.vim",   "vmadc.vim",
          nullptr,       nullptr,    nullptr,       nullptr,         nullptr,      "<vmerge/vmv>",
          "vmseq.vi",    "vmsne.vi", nullptr,       nullptr,         "vmsleu.vi",  "vmsle.vi",
          "vmsgtu.vi",   "vmsgt.vi", "vsaddu.vi",   "vsadd.vi",      nullptr,      nullptr,
          nullptr,       "vsll.vi",  nullptr,       nullptr,         "vsrl.vi",    "vsra.vi",
          "vssrl.vi",    "vssra.vi", "vnsrl.wi",    "vnsra.wi",      "vnclipu.wi", "vnclip.wi",
          nullptr,       nullptr,    nullptr,       nullptr,         nullptr,      nullptr,
          nullptr,       nullptr,    nullptr,       nullptr,         nullptr,      nullptr,
          nullptr,       nullptr,    nullptr,       nullptr};

      rs2 = VRegName(GetRs2(insn32));

      if (funct6 == 0b010111) {
        // vmerge/vmv
        if ((funct7 & 1) && GetRs2(insn32) == 0) {
          // Note: The vector integer move instructions share the encoding with the vector merge
          // instructions, but with vm=1 and vs2=v0
          opcode = "vmv.v.i";
          rs2 = nullptr;
        } else {
          opcode = "vmerge.vim";
          vm = ", v0";
        }
        vm = "";
      } else {
        opcode = OPIVIOpcodes[funct6];
      }

      opcode = OPIVIOpcodes[funct6];
      rd = VRegName(GetRd(insn32));
      break;
    }
    case VAIEncodings::kOpMVV: {  // 4, vs, R
      switch (funct6) {
        case VWXUNARY0: {
          static const char* const VWXUNARY0Opcodes[32] = {
              "vmv.x.s", nullptr,    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
              nullptr,   nullptr,    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
              "vpopc.m", "vfirst.m", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
              nullptr,   nullptr,    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
          opcode = VWXUNARY0Opcodes[GetRs1(insn32)];
          rd = XRegName(GetRd(insn32));
          rs2 = VRegName(GetRs2(insn32));
          break;
        }
        case VXUNARY0: {
          static const char* const VXUNARY0Opcodes[32] = {
              nullptr,     nullptr, "vzext.vf8", "vsext.vf8", "vzext.vf4", "vsext.vf4", "vzext.vf2",
              "vsext.vf2", nullptr, nullptr,     nullptr,     nullptr,     nullptr,     nullptr,
              nullptr,     nullptr, nullptr,     nullptr,     nullptr,     nullptr,     nullptr,
              nullptr,     nullptr, nullptr,     nullptr,     nullptr,     nullptr,     nullptr,
              nullptr,     nullptr, nullptr,     nullptr};
          opcode = VXUNARY0Opcodes[GetRs1(insn32)];
          rd = VRegName(GetRd(insn32));
          rs2 = VRegName(GetRs2(insn32));
          break;
        }
        case VMUNARY0: {
          static const char* const VMUNARY0Opcodes[32] = {
              nullptr,   "vmsbf.m", "vmsof.m", "vmsif.m", nullptr, nullptr, nullptr, nullptr,
              nullptr,   nullptr,   nullptr,   nullptr,   nullptr, nullptr, nullptr, nullptr,
              "viota.m", "vid.v",   nullptr,   nullptr,   nullptr, nullptr, nullptr, nullptr,
              nullptr,   nullptr,   nullptr,   nullptr,   nullptr, nullptr, nullptr, nullptr};
          opcode = VMUNARY0Opcodes[GetRs1(insn32)];
          rd = VRegName(GetRd(insn32));
          rs2 = VRegName(GetRs2(insn32));
          break;
        }
        default: {
          static const char* const OPMVVOpcodes[64] = {
              "vredsum.vs", "vredand.vs",  "vredor.vs",  "vredxor.vs",   "vredminu.vs",
              "vredmin.vs", "vredmaxu.vs", "vredmax.vs", "vaaddu.vv",    "vaadd.vv",
              "vasubu.vv",  "vasub.vv",    nullptr,      nullptr,        nullptr,
              nullptr,      nullptr,       nullptr,      nullptr,        nullptr,
              nullptr,      nullptr,       nullptr,      "vcompress.vm", "vmandn.mm",
              "vmand.mm",   "vmor.mm",     "vmxor.mm",   "vmorn.mm",     "vmnand.mm",
              "vmnor.mm",   "vmxnor.mm",   "vdivu.vv",   "vdiv.vv",      "vremu.vv",
              "vrem.vv",    "vmulhu.vv",   "vmul.vv",    "vmulhsu.vv",   "vmulh.vv",
              nullptr,      "vmadd.vv",    nullptr,      "vnmsub.vv",    nullptr,
              "vmacc.vv",   nullptr,       "vnmsac.vv",  "vwaddu.vv",    "vwadd.vv",
              "vwsubu.vv",  "vwsub.vv",    "vwaddu.wv",  "vwadd.wv",     "vwsubu.wv",
              "vwsub.wv",   "vwmulu.vv",   nullptr,      "vwmulsu.vv",   "vwmul.vv",
              "vwmaccu.vv", "vwmacc.vv",   nullptr,      "vwmaccsu.vv"};
          opcode = OPMVVOpcodes[funct6];
          rd = VRegName(GetRd(insn32));
          rs1 = VRegName(GetRs1(insn32));
          rs2 = VRegName(GetRs2(insn32));
          break;
        }
      }
      break;
    }
    case VAIEncodings::kOpMVX: {  // 5, vx, R
      switch (funct6) {
        case VRXUNARY0: {
          static const char* const VRXUNARY0Opcodes[32] = {
              "vmv.s.x", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
              nullptr,   nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
              nullptr,   nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
              nullptr,   nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
          opcode = VRXUNARY0Opcodes[GetRs2(insn32)];
          rd = VRegName(GetRd(insn32));
          rs1 = XRegName(GetRs1(insn32));
          break;
        }
        default: {
          static const char* const OPMVXOpcodes[64] = {
              nullptr,          nullptr,     nullptr,       nullptr,      nullptr,
              nullptr,          nullptr,     nullptr,       "vaaddu.vx",  "vaadd.vx",
              "vasubu.vx",      "vasub.vx",  nullptr,       nullptr,      "vslide1up.vx",
              "vslide1down.vx", nullptr,     nullptr,       nullptr,      nullptr,
              nullptr,          nullptr,     nullptr,       nullptr,      nullptr,
              nullptr,          nullptr,     nullptr,       nullptr,      nullptr,
              nullptr,          nullptr,     "vdivu.vx",    "vdiv.vx",    "vremu.vx",
              "vrem.vx",        "vmulhu.vx", "vmul.vx",     "vmulhsu.vx", "vmulh.vx",
              nullptr,          "vmadd.vx",  nullptr,       "vnmsub.vx",  nullptr,
              "vmacc.vx",       nullptr,     "vnmsac.vx",   "vwaddu.vx",  "vwadd.vx",
              "vwsubu.vx",      "vwsub.vx",  "vwaddu.wv",   "vwadd.wv",   "vwsubu.wv",
              "vwsub.wv",       "vwmulu.vx", nullptr,       "vwmulsu.vx", "vwmul.vx",
              "vwmaccu.vx",     "vwmacc.vx", "vwmaccus.vx", "vwmaccsu.vx"};
          opcode = OPMVXOpcodes[funct6];
          rd = VRegName(GetRd(insn32));
          rs1 = XRegName(GetRs1(insn32));
          rs2 = VRegName(GetRs2(insn32));
          break;
        }
      }
      break;
    }
    case VAIEncodings::kOpFVV: {  // 6, vv, R
      switch (funct6) {
        case VWFUNARY0: {
          static const char* const VWFUNARY0Opcodes[32] = {
              "vfmv.f.s", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
              nullptr,    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
              nullptr,    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
              nullptr,    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
          opcode = VWFUNARY0Opcodes[GetRs1(insn32)];
          rd = XRegName(GetRd(insn32));
          rs2 = VRegName(GetRs2(insn32));
          break;
        }
        case VFUNARY0: {
          static const char* const VFUNARY0Opcodes[32] = {"vfcvt.xu.f.v",
                                                          "vfcvt.x.f.v",
                                                          "vfcvt.f.xu.v",
                                                          "vfcvt.f.x.v",
                                                          nullptr,
                                                          nullptr,
                                                          "vfcvt.rtz.xu.f.v",
                                                          "vfcvt.rtz.x.f.v",
                                                          "vfwcvt.xu.f.v",
                                                          "vfwcvt.x.f.v",
                                                          "vfwcvt.f.xu.v",
                                                          "vfwcvt.f.x.v",
                                                          "vfwcvt.f.f.v",
                                                          nullptr,
                                                          "vfwcvt.rtz.xu.f.v",
                                                          "vfwcvt.rtz.x.f.v",
                                                          "vfncvt.xu.f.w",
                                                          "vfncvt.x.f.w",
                                                          "vfncvt.f.xu.w",
                                                          "vfncvt.f.x.w",
                                                          "vfncvt.f.f.w",
                                                          "vfncvt.rod.f.f.w",
                                                          "vfncvt.rtz.xu.f.w",
                                                          "vfncvt.rtz.x.f.w",
                                                          nullptr,
                                                          nullptr,
                                                          nullptr,
                                                          nullptr,
                                                          nullptr,
                                                          nullptr,
                                                          nullptr,
                                                          nullptr};
          opcode = VFUNARY0Opcodes[GetRs1(insn32)];
          rd = VRegName(GetRd(insn32));
          rs2 = VRegName(GetRs2(insn32));
          break;
        }
        case VFUNARY1: {
          static const char* const VFUNARY1Opcodes[32] = {
              "vfsqrt.v",  nullptr, nullptr, nullptr, "vfrsqrt7.v", "vfrec7.v", nullptr, nullptr,
              nullptr,     nullptr, nullptr, nullptr, nullptr,      nullptr,    nullptr, nullptr,
              "vfclass.v", nullptr, nullptr, nullptr, nullptr,      nullptr,    nullptr, nullptr,
              nullptr,     nullptr, nullptr, nullptr, nullptr,      nullptr,    nullptr, nullptr};
          opcode = VFUNARY1Opcodes[GetRs1(insn32)];
          rd = VRegName(GetRd(insn32));
          rs2 = VRegName(GetRs2(insn32));
          break;
        }
        default: {
          static const char* const OPFVVOpcodes[64] = {
              "vfadd.vv",    "vfredusum.vs",  "vfsub.vv",    "vfredosum.vs", "vfmin.vv",
              "vfredmin.vs", "vfmax.vv",      "vfredmax.vs", "vfsgnj.vv",    "vfsgnjn.vv",
              "vfsgnjx.vv",  nullptr,         nullptr,       nullptr,        nullptr,
              nullptr,       nullptr,         nullptr,       nullptr,        nullptr,
              nullptr,       nullptr,         nullptr,       nullptr,        "vmfeq.vv",
              "vmfle.vv",    nullptr,         "vmflt.vv",    "vmfne.vv",     nullptr,
              nullptr,       nullptr,         "vfdiv.vv",    nullptr,        nullptr,
              nullptr,       "vfmul.vv",      nullptr,       nullptr,        nullptr,
              "vfmadd.vv",   "vfnmadd.vv",    "vfmsub.vv",   "vfnmsub.vv",   "vfmacc.vv",
              "vfnmacc.vv",  "vfmsac.vv",     "vfnmsac.vv",  "vfwadd.vv",    "vfwredusum.vs",
              "vfwsub.vv",   "vfwredosum.vs", "vfwadd.wv",   nullptr,        "vfwsub.wv",
              nullptr,       "vfwmul.vv",     nullptr,       nullptr,        nullptr,
              "vfwmacc.vv",  "vfwnmacc.vv",   "vfwmsac.vv",  "vfwnmsac.vv"};
          opcode = OPFVVOpcodes[funct6];
          rd = VRegName(GetRd(insn32));
          rs1 = VRegName(GetRs1(insn32));
          rs2 = VRegName(GetRs2(insn32));
          break;
        }
      }
      break;
    }
    case VAIEncodings::kOpFVF: {  // 7, vf, R
      switch (funct6) {
        case VRFUNARY0: {
          static const char* const VRFUNARY0Opcodes[32] = {
              "vfmv.s.f", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
              nullptr,    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
              nullptr,    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
              nullptr,    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
          opcode = VRFUNARY0Opcodes[GetRs2(insn32)];
          rd = VRegName(GetRd(insn32));
          rs1 = FRegName(GetRs1(insn32));
          break;
        }
        default: {
          static const char* const OPFVFOpcodes[64] = {
              "vfadd.vf",        nullptr,       "vfsub.vf",   nullptr,        "vfmin.vf",
              nullptr,           "vfmax.vf",    nullptr,      "vfsgnj.vf",    "vfsgnjn.vf",
              "vfsgnjx.vf",      nullptr,       nullptr,      nullptr,        "vfslide1up.vf",
              "vfslide1down.vf", nullptr,       nullptr,      nullptr,        nullptr,
              nullptr,           nullptr,       nullptr,      "vfmerge.vfmv", "vmfeq.vf",
              "vmfle.vf",        nullptr,       "vmflt.vf",   "vmfne.vf",     "vmfgt.vf",
              nullptr,           "vmfge.vf",    "vfdiv.vf",   "vfrdiv.vf",    nullptr,
              nullptr,           "vfmul.vf",    nullptr,      nullptr,        "vfrsub.vf",
              "vfmadd.vf",       "vfnmadd.vf",  "vfmsub.vf",  "vfnmsub.vf",   "vfmacc.vf",
              "vfnmacc.vf",      "vfmsac.vf",   "vfnmsac.vf", "vfwadd.vf",    nullptr,
              "vfwsub.vf",       nullptr,       "vfwadd.wf",  nullptr,        "vfwsub.wf",
              nullptr,           "vfwmul.vf",   nullptr,      nullptr,        nullptr,
              "vfwmacc.vf",      "vfwnmacc.vf", "vfwmsac.vf", "vfwnmsac.vf"};

          if (funct6 == 0b010111) {
            // vfmerge.vfmv
            vm = ", v0";
          }

          opcode = OPFVFOpcodes[funct6];
          rd = VRegName(GetRd(insn32));
          rs1 = FRegName(GetRs1(insn32));
          rs2 = VRegName(GetRs2(insn32));
          break;
        }
      }
      break;
    }
    case VAIEncodings::kOpCFG: {  // vector ALU control instructions
      const char *vma = nullptr, *vta = nullptr, *vsew = nullptr, *lmul = nullptr;
      if (insn32 >> 31) {
        if ((insn32 >> 30) & 0x1U) {  // vsetivli
          const uint32_t zimm = Decode32UImm12(insn32) & ~0xC00U;
          const uint32_t imm5 = GetRs1(insn32);
          os_ << "vsetivli " << XRegName(GetRd(insn32)) << ", " << StringPrintf("0x%08x", imm5)
              << ", ";
          if (decodeVType(zimm, vma, vta, vsew, lmul)) {
            os_ << vsew << ", " << lmul << ", " << vta << ", " << vma;
          } else {
            os_ << StringPrintf("0x%08x", zimm) << "\t# incorrect VType literal";
          }
        } else {  // vsetvl
          os_ << "vsetvl " << XRegName(GetRd(insn32)) << ", " << XRegName(GetRs1(insn32)) << ", "
              << XRegName(GetRs2(insn32));
          if ((Decode32UImm7(insn32) & 0x40)) {  // should be zeros
            os_ << "\t# incorrect funct7 literal : "
                << StringPrintf("0x%08x", Decode32UImm7(insn32));
          }
        }
      } else {  // vsetvli
        const uint32_t zimm = Decode32UImm12(insn32) & ~0x800U;
        os_ << "vsetvli " << XRegName(GetRd(insn32)) << ", " << XRegName(GetRs1(insn32)) << ", ";
        if (decodeVType(zimm, vma, vta, vsew, lmul)) {
          os_ << vsew << ", " << lmul << ", " << vta << ", " << vma;
        } else {
          os_ << StringPrintf("0x%08x", zimm) << "\t# incorrect VType literal";
        }
      }
      return;
    }
  }

  if (opcode == nullptr) {
    os_ << "<unknown32>";
    return;
  }

  os_ << opcode << " " << rd;

  if (rs1) {
    os_ << ", " << rs1;
  } else if (via == VAIEncodings::kOpIVI) {
    os_ << StringPrintf(", 0x%08x", GetRs1(insn32));
  }

  if (rs2) {
    os_ << ", " << rs2;
  }

  os_ << vm;
}

void DisassemblerRiscv64::Printer::Print32FpFma(uint32_t insn32) {
  DCHECK_EQ(insn32 & 0x73u, 0x43u);  // Note: Bits 0xc select the FMA opcode.
  uint32_t funct2 = (insn32 >> 25) & 3u;
  if (funct2 >= 2u) {
    os_ << "<unknown32>";  // Note: This includes the "H" and "Q" extensions.
    return;
  }
  static const char* const kOpcodes[] = { "fmadd", "fmsub", "fnmsub", "fnmadd" };
  os_ << kOpcodes[(insn32 >> 2) & 3u] << ((funct2 != 0u) ? ".d" : ".s")
      << RoundingModeName(GetRoundingMode(insn32)) << " "
      << FRegName(GetRd(insn32)) << ", " << FRegName(GetRs1(insn32)) << ", "
      << FRegName(GetRs2(insn32)) << ", " << FRegName(GetRs3(insn32));
}

void DisassemblerRiscv64::Printer::Print32Zicsr(uint32_t insn32) {
  DCHECK_EQ(insn32 & 0x7fu, 0x73u);
  uint32_t funct3 = (insn32 >> 12) & 7u;
  static const char* const kOpcodes[] = {
      nullptr, "csrrw", "csrrs", "csrrc", nullptr, "csrrwi", "csrrsi", "csrrci"
  };
  const char* opcode = kOpcodes[funct3];
  if (opcode == nullptr) {
    os_ << "<unknown32>";
    return;
  }
  uint32_t rd = GetRd(insn32);
  uint32_t rs1_or_uimm = GetRs1(insn32);
  uint32_t csr = insn32 >> 20;
  // Print shorter macro instruction notation if available.
  if (funct3 == /*CSRRW*/ 1u && rd == 0u && rs1_or_uimm == 0u && csr == 0xc00u) {
    os_ << "unimp";
    return;
  } else if (funct3 == /*CSRRS*/ 2u && rs1_or_uimm == 0u) {
    if (csr == 0xc00u) {
      os_ << "rdcycle " << XRegName(rd);
    } else if (csr == 0xc01u) {
      os_ << "rdtime " << XRegName(rd);
    } else if (csr == 0xc02u) {
      os_ << "rdinstret " << XRegName(rd);
    } else {
      os_ << "csrr " << XRegName(rd) << ", " << csr;
    }
    return;
  }

  if (rd == 0u) {
    static const char* const kAltOpcodes[] = {
        nullptr, "csrw", "csrs", "csrc", nullptr, "csrwi", "csrsi", "csrci"
    };
    DCHECK(kAltOpcodes[funct3] != nullptr);
    os_ << kAltOpcodes[funct3] << " " << csr << ", ";
  } else {
    os_ << opcode << " " << XRegName(rd) << ", " << csr << ", ";
  }
  if (funct3 >= /*CSRRWI/CSRRSI/CSRRCI*/ 4u) {
    os_ << rs1_or_uimm;
  } else {
    os_ << XRegName(rs1_or_uimm);
  }
}

void DisassemblerRiscv64::Printer::Print32Fence(uint32_t insn32) {
  DCHECK_EQ(insn32 & 0x7fu, 0x0fu);
  if ((insn32 & 0xf00fffffu) == 0x0000000fu) {
    auto print_flags = [&](uint32_t flags) {
      if (flags == 0u) {
        os_ << "0";
      } else {
        DCHECK_LT(flags, 0x10u);
        static const char kFlagNames[] = "wroi";
        for (size_t bit : { 3u, 2u, 1u, 0u }) {  // Print in the "iorw" order.
          if ((flags & (1u << bit)) != 0u) {
            os_ << kFlagNames[bit];
          }
        }
      }
    };
    os_ << "fence.";
    print_flags((insn32 >> 24) & 0xfu);
    os_ << ".";
    print_flags((insn32 >> 20) & 0xfu);
  } else if (insn32 == 0x8330000fu) {
    os_ << "fence.tso";
  } else if (insn32 == 0x0000100fu) {
    os_ << "fence.i";
  } else {
    os_ << "<unknown32>";
  }
}

void DisassemblerRiscv64::Printer::Dump32(const uint8_t* insn) {
  uint32_t insn32 = static_cast<uint32_t>(insn[0]) +
                    (static_cast<uint32_t>(insn[1]) << 8) +
                    (static_cast<uint32_t>(insn[2]) << 16) +
                    (static_cast<uint32_t>(insn[3]) << 24);
  CHECK_EQ(insn32 & 3u, 3u);
  os_ << disassembler_->FormatInstructionPointer(insn) << StringPrintf(": %08x\t", insn32);
  switch (insn32 & 0x7fu) {
    case 0x37u:
      Print32Lui(insn32);
      break;
    case 0x17u:
      Print32Auipc(insn, insn32);
      break;
    case 0x6fu:
      Print32Jal(insn, insn32);
      break;
    case 0x67u:
      switch ((insn32 >> 12) & 7u) {  // funct3
        case 0:
          Print32Jalr(insn, insn32);
          break;
        default:
          os_ << "<unknown32>";
          break;
      }
      break;
    case 0x63u:
      Print32BCond(insn, insn32);
      break;
    case 0x03u:
      Print32Load(insn32);
      break;
    case 0x23u:
      Print32Store(insn32);
      break;
    case 0x07u:
      Print32FLoad(insn32);
      break;
    case 0x27u:
      Print32FStore(insn32);
      break;
    case 0x13u:
    case 0x1bu:
      Print32BinOpImm(insn32);
      break;
    case 0x33u:
    case 0x3bu:
      Print32BinOp(insn32);
      break;
    case 0x2fu:
      Print32Atomic(insn32);
      break;
    case 0x53u:
      Print32FpOp(insn32);
      break;
    case 0x57u:
      Print32RVVOp(insn32);
      break;
    case 0x43u:
    case 0x47u:
    case 0x4bu:
    case 0x4fu:
      Print32FpFma(insn32);
      break;
    case 0x73u:
      if ((insn32 & 0xffefffffu) == 0x00000073u) {
        os_ << ((insn32 == 0x00000073u) ? "ecall" : "ebreak");
      } else {
        Print32Zicsr(insn32);
      }
      break;
    case 0x0fu:
      Print32Fence(insn32);
      break;
    default:
      // TODO(riscv64): Disassemble more instructions.
      os_ << "<unknown32>";
      break;
  }
  os_ << "\n";
}

void DisassemblerRiscv64::Printer::Dump16(const uint8_t* insn) {
  uint32_t insn16 = static_cast<uint32_t>(insn[0]) + (static_cast<uint32_t>(insn[1]) << 8);
  CHECK_NE(insn16 & 3u, 3u);
  // TODO(riscv64): Disassemble instructions from the "C" extension.
  os_ << disassembler_->FormatInstructionPointer(insn)
      << StringPrintf(": %04x    \t<unknown16>\n", insn16);
}

void DisassemblerRiscv64::Printer::Dump2Byte(const uint8_t* data) {
  uint32_t value = data[0] + (data[1] << 8);
  os_ << disassembler_->FormatInstructionPointer(data)
      << StringPrintf(": %04x    \t.2byte %u\n", value, value);
}

void DisassemblerRiscv64::Printer::DumpByte(const uint8_t* data) {
  uint32_t value = *data;
  os_ << disassembler_->FormatInstructionPointer(data)
      << StringPrintf(": %02x      \t.byte %u\n", value, value);
}

size_t DisassemblerRiscv64::Dump(std::ostream& os, const uint8_t* begin) {
  if (begin < GetDisassemblerOptions()->base_address_ ||
      begin >= GetDisassemblerOptions()->end_address_) {
    return 0u;  // Outside the range.
  }
  Printer printer(this, os);
  if (!IsAligned<2u>(begin) || GetDisassemblerOptions()->end_address_ - begin == 1) {
    printer.DumpByte(begin);
    return 1u;
  }
  if ((*begin & 3u) == 3u) {
    if (GetDisassemblerOptions()->end_address_ - begin >= 4) {
      printer.Dump32(begin);
      return 4u;
    } else {
      printer.Dump2Byte(begin);
      return 2u;
    }
  } else {
    printer.Dump16(begin);
    return 2u;
  }
}

void DisassemblerRiscv64::Dump(std::ostream& os, const uint8_t* begin, const uint8_t* end) {
  Printer printer(this, os);
  const uint8_t* cur = begin;
  if (cur < end && !IsAligned<2u>(cur)) {
    // Unaligned, dump as a `.byte` to get to an aligned address.
    printer.DumpByte(cur);
    cur += 1;
  }
  if (cur >= end) {
    return;
  }
  while (end - cur >= 4) {
    if ((*cur & 3u) == 3u) {
      printer.Dump32(cur);
      cur += 4;
    } else {
      printer.Dump16(cur);
      cur += 2;
    }
  }
  if (end - cur >= 2) {
    if ((*cur & 3u) == 3u) {
      // Not enough data for a 32-bit instruction. Dump as `.2byte`.
      printer.Dump2Byte(cur);
    } else {
      printer.Dump16(cur);
    }
    cur += 2;
  }
  if (end != cur) {
    CHECK_EQ(end - cur, 1);
    printer.DumpByte(cur);
  }
}

}  // namespace riscv64
}  // namespace art
