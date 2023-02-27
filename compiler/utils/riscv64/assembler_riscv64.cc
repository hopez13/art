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

#include "base/bit_utils.h"
#include "base/casts.h"
#include "base/memory_region.h"
#include "entrypoints/quick/quick_entrypoints.h"
#include "entrypoints/quick/quick_entrypoints_enum.h"
#include "thread.h"

namespace art HIDDEN {
namespace riscv64 {

static_assert(static_cast<size_t>(kRiscv64PointerSize) == kRiscv64DoublewordSize,
              "Unexpected Riscv64 pointer size.");
static_assert(kRiscv64PointerSize == PointerSize::k64, "Unexpected Riscv64 pointer size.");

void Riscv64Assembler::FinalizeCode() {
  ReserveJumpTableSpace();
  EmitLiterals();
  PromoteBranches();
}

void Riscv64Assembler::FinalizeInstructions(const MemoryRegion& region) {
  EmitBranches();
  EmitJumpTables();
  Assembler::FinalizeInstructions(region);
  PatchCFI();
}

void Riscv64Assembler::PatchCFI() {
  if (cfi().NumberOfDelayedAdvancePCs() == 0u) {
    return;
  }

  using DelayedAdvancePC = DebugFrameOpCodeWriterForAssembler::DelayedAdvancePC;
  const auto data = cfi().ReleaseStreamAndPrepareForDelayedAdvancePC();
  const std::vector<uint8_t>& old_stream = data.first;
  const std::vector<DelayedAdvancePC>& advances = data.second;

  // Refill our data buffer with patched opcodes.
  cfi().ReserveCFIStream(old_stream.size() + advances.size() + 16);
  size_t stream_pos = 0;
  for (const DelayedAdvancePC& advance : advances) {
    DCHECK_GE(advance.stream_pos, stream_pos);
    // Copy old data up to the point where advance was issued.
    cfi().AppendRawData(old_stream, stream_pos, advance.stream_pos);
    stream_pos = advance.stream_pos;
    // Insert the advance command with its final offset.
    size_t final_pc = GetAdjustedPosition(advance.pc);
    cfi().AdvancePC(final_pc);
  }
  // Copy the final segment if any.
  cfi().AppendRawData(old_stream, stream_pos, old_stream.size());
}

void Riscv64Assembler::EmitBranches() {
  CHECK(!overwriting_);
  // Switch from appending instructions at the end of the buffer to overwriting
  // existing instructions (branch placeholders) in the buffer.
  overwriting_ = true;
  for (auto& branch : branches_) {
    EmitBranch(&branch);
  }
  overwriting_ = false;
}

void Riscv64Assembler::Emit(uint32_t value) {
  if (overwriting_) {
    // Branches to labels are emitted into their placeholders here.
    buffer_.Store<uint32_t>(overwrite_location_, value);
    overwrite_location_ += sizeof(uint32_t);
  } else {
    // Other instructions are simply appended at the end here.
    AssemblerBuffer::EnsureCapacity ensured(&buffer_);
    buffer_.Emit<uint32_t>(value);
  }
}

void Riscv64Assembler::EmitR(
    int32_t funct7, Register rs2, Register rs1, int32_t funct3, Register rd, int32_t opcode) {
  uint32_t encoding =
      static_cast<uint32_t>(funct7) << kFunct7Shift | static_cast<uint32_t>(rs2) << kRs2Shift |
      static_cast<uint32_t>(rs1) << kRs1Shift | static_cast<uint32_t>(funct3) << kFunct3Shift |
      static_cast<uint32_t>(rd) << kRdShift | opcode;
  Emit(encoding);
}

void Riscv64Assembler::EmitR4(Register rs3,
                              int32_t funct2,
                              Register rs2,
                              Register rs1,
                              int32_t funct3,
                              Register rd,
                              int32_t opcode) {
  uint32_t encoding =
      static_cast<uint32_t>(rs3) << kRs3Shift | static_cast<uint32_t>(funct2) << kFunct2Shift |
      static_cast<uint32_t>(rs2) << kRs2Shift | static_cast<uint32_t>(rs1) << kRs1Shift |
      static_cast<uint32_t>(funct3) << kFunct3Shift | static_cast<uint32_t>(rd) << kRdShift |
      opcode;
  Emit(encoding);
}

void Riscv64Assembler::EmitI(
    uint16_t imm12, XRegister rs1, int32_t funct3, Register rd, int32_t opcode) {
  CHECK_NE(rs1, kNoXRegister);
  // NOTE : since the imm12 is at the MSB 12 bits position, so that we can save performance with
  // following alignment :
  //        1. The caller of EmitI do not need to do imm12 bit length cut
  //        2. The caller of EmitI need to implent the imm12 range check
  // CHECK(IsUint<12>(imm12)) << imm12;
  uint32_t encoding = static_cast<uint32_t>(imm12) << kIImm12Shift |
                      static_cast<uint32_t>(rs1) << kRs1Shift |
                      static_cast<uint32_t>(funct3) << kFunct3Shift |
                      static_cast<uint32_t>(rd) << kRdShift | opcode;
  Emit(encoding);
}

void Riscv64Assembler::EmitS(
    uint16_t imm7, Register rs2, XRegister rs1, int32_t funct3, uint16_t imm5, int32_t opcode) {
  CHECK_NE(rs1, kNoXRegister);
  uint32_t encoding =
      static_cast<uint32_t>(imm7) << kSImm7Shift | static_cast<uint32_t>(rs2) << kRs2Shift |
      static_cast<uint32_t>(rs1) << kRs1Shift | static_cast<uint32_t>(funct3) << kFunct3Shift |
      static_cast<uint32_t>(imm5) << kSImm5Shift | opcode;
  Emit(encoding);
}

// refer to constants_riscv64.h for detail description of every bit
void Riscv64Assembler::EmitB(
    uint16_t imm12, XRegister rs2, XRegister rs1, int32_t funct3, int32_t opcode) {
  // imm12(only bit0 ~ bit11 is valid) has already been shifted in function GetOffset() by
  // offset_shift
  CHECK_NE(rs1, kNoXRegister);
  CHECK_NE(rs2, kNoXRegister);
  uint16_t imm7;
  uint16_t imm5;
  CHECK(IsUint<12>(imm12)) << imm12;
  imm7 = static_cast<uint16_t>((imm12 & kGetBit12Mask) >> kBBit12Shift) |
         ((imm12 & kGetBit10_5Mask) >> kBBit10_5Shift);
  imm5 = static_cast<uint16_t>((imm12 & kGetBit4_1Mask) << kBBit4_1Shift) |
         ((imm12 & kGetBit11Mask) >> kBBit11Shift);
  uint32_t encoding =
      static_cast<uint32_t>(imm7) << kBImm7Shift | static_cast<uint32_t>(rs2) << kRs2Shift |
      static_cast<uint32_t>(rs1) << kRs1Shift | static_cast<uint32_t>(funct3) << kFunct3Shift |
      static_cast<uint32_t>(imm5) << kBImm5Shift | opcode;
  Emit(encoding);
}

void Riscv64Assembler::EmitU(uint32_t imm20, XRegister rd, int32_t opcode) {
  CHECK_NE(rd, kNoXRegister);
  /* check the imm value at caller */
  uint32_t encoding =
      static_cast<uint32_t>(imm20) << kUImm20Shift | static_cast<uint32_t>(rd) << kRdShift | opcode;
  Emit(encoding);
}

void Riscv64Assembler::EmitJ(uint32_t imm20, XRegister rd, int opcode) {
  CHECK_NE(rd, kNoXRegister);
  uint32_t imm_20;
  imm_20 =
      ((imm20 & kGetBit20Mask) >> kJBit20Shift) | ((imm20 & kGetBit10_1Mask) << kJBit10_1Shift) |
      ((imm20 & kGetBit11Mask) >> kJBit11Shift) | ((imm20 & kGetBit19_12Mask) >> kJBit19_12Shift);

  uint32_t encoding = static_cast<uint32_t>(imm_20) << KJImm20shift |
                      static_cast<uint32_t>(rd) << kRdShift | opcode;
  Emit(encoding);
}

void Riscv64Assembler::Add(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0, rs2, rs1, 0, rd, 0x33);
}

void Riscv64Assembler::Addi(XRegister rd, XRegister rs1, int16_t imm12) {
  CHECK(IsInt<12>(imm12)) << imm12;
  // NOTE : since the imm12 is at the MSB 12bits position, we can save cycles for not cutting imm12
  // to 12 bits
  EmitI(imm12, rs1, 0, rd, 0x13);
}

void Riscv64Assembler::Addw(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0, rs2, rs1, 0, rd, 0x3b);
}

void Riscv64Assembler::Addiw(XRegister rd, XRegister rs1, int16_t imm12) {
  CHECK(IsInt<12>(imm12)) << imm12;
  // NOTE : since the imm12 is at the MSB 12bits position, we can save cycles for not cutting imm12
  // to 12 bits
  EmitI(imm12, rs1, 0, rd, 0x1b);
}

void Riscv64Assembler::Sub(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x20, rs2, rs1, 0, rd, 0x33);
}

void Riscv64Assembler::Subw(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x20, rs2, rs1, 0, rd, 0x3B);
}

void Riscv64Assembler::Mul(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(1, rs2, rs1, 0, rd, 0x33);
}

void Riscv64Assembler::Mulh(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(1, rs2, rs1, 1, rd, 0x33);
}

void Riscv64Assembler::Mulhsu(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(1, rs2, rs1, 2, rd, 0x33);
}

void Riscv64Assembler::Mulhu(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(1, rs2, rs1, 3, rd, 0x33);
}

void Riscv64Assembler::Mulw(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(1, rs2, rs1, 0, rd, 0x3B);
}

void Riscv64Assembler::Div(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(1, rs2, rs1, 4, rd, 0x33);
}

void Riscv64Assembler::Divu(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(1, rs2, rs1, 5, rd, 0x33);
}

void Riscv64Assembler::Divw(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(1, rs2, rs1, 4, rd, 0x3B);
}

void Riscv64Assembler::Divuw(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(1, rs2, rs1, 5, rd, 0x3B);
}

void Riscv64Assembler::Rem(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x1, rs2, rs1, 6, rd, 0x33);
}

void Riscv64Assembler::Remw(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x1, rs2, rs1, 6, rd, 0x3B);
}

// Logic
void Riscv64Assembler::And(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0, rs2, rs1, 7, rd, 0x33);
}

void Riscv64Assembler::Andi(XRegister rd, XRegister rs1, int64_t imm12) {
  CHECK(IsInt<12>(imm12)) << imm12;
  EmitI(imm12, rs1, 7, rd, 0x13);
}

void Riscv64Assembler::Neg(XRegister rd, XRegister rs2) { Sub(rd, Zero, rs2); }

void Riscv64Assembler::Negw(XRegister rd, XRegister rs2) { Subw(rd, Zero, rs2); }

void Riscv64Assembler::Or(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0, rs2, rs1, 6, rd, 0x33);
}

void Riscv64Assembler::Ori(XRegister rd, XRegister rs1, int64_t imm12) {
  CHECK(IsInt<12>(imm12)) << imm12;
  EmitI(imm12, rs1, 6, rd, 0x13);
}

void Riscv64Assembler::Xor(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0, rs2, rs1, 4, rd, 0x33);
}

void Riscv64Assembler::Xori(XRegister rd, XRegister rs1, int64_t imm12) {
  CHECK(IsInt<12>(imm12)) << imm12;
  EmitI(imm12, rs1, 4, rd, 0x13);
}

// Shift
void Riscv64Assembler::Sll(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0, rs2, rs1, 1, rd, 0x33);
}

void Riscv64Assembler::Slli(XRegister rd, XRegister rs1, uint16_t shamt6) {
  CHECK(IsUint<6>(shamt6)) << shamt6;
  EmitI(shamt6, rs1, 1, rd, 0x13);
}

void Riscv64Assembler::Srl(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0, rs2, rs1, 5, rd, 0x33);
}

void Riscv64Assembler::Srli(XRegister rd, XRegister rs1, uint16_t shamt6) {
  CHECK(IsUint<6>(shamt6)) << shamt6;
  EmitI(shamt6, rs1, 5, rd, 0x13);
}

void Riscv64Assembler::Sra(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x20, rs2, rs1, 5, rd, 0x33);
}

void Riscv64Assembler::Srai(XRegister rd, XRegister rs1, uint16_t shamt6) {
  CHECK(IsUint<6>(shamt6)) << shamt6;
  EmitI((0x400 | shamt6), rs1, 5, rd, 0x13);
}

void Riscv64Assembler::Sllw(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0, rs2, rs1, 1, rd, 0x3B);
}

void Riscv64Assembler::Slliw(XRegister rd, XRegister rs1, uint16_t shamt5) {
  CHECK(IsUint<5>(shamt5)) << shamt5;
  EmitI(shamt5, rs1, 1, rd, 0x1B);
}

void Riscv64Assembler::Srlw(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0, rs2, rs1, 5, rd, 0x3B);
}

void Riscv64Assembler::Srliw(XRegister rd, XRegister rs1, uint16_t shamt5) {
  CHECK(IsUint<5>(shamt5)) << shamt5;
  EmitI(shamt5, rs1, 5, rd, 0x1B);
}

void Riscv64Assembler::Sraw(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x20, rs2, rs1, 5, rd, 0x3B);
}

void Riscv64Assembler::Sraiw(XRegister rd, XRegister rs1, uint16_t shamt5) {
  CHECK(IsUint<5>(shamt5)) << shamt5;
  EmitI((0x400 | shamt5), rs1, 5, rd, 0x1B);
}

void Riscv64Assembler::Lb(XRegister rd, XRegister rs1, int16_t imm12) {
  CHECK(IsInt<12>(imm12)) << imm12;
  // NOTE : since the imm12 is at the MSB 12bits position, we can save cycles for not cutting imm12
  // to 12 bits
  EmitI(imm12, rs1, 0, rd, 0x03);
}

void Riscv64Assembler::Lh(XRegister rd, XRegister rs1, int16_t imm12) {
  CHECK(IsInt<12>(imm12)) << imm12;
  // NOTE : since the imm12 is at the MSB 12bits position, we can save cycles for not cutting imm12
  // to 12 bits
  EmitI(imm12, rs1, 1, rd, 0x03);
}

void Riscv64Assembler::Lw(XRegister rd, XRegister rs1, int16_t imm12) {
  CHECK(IsInt<12>(imm12)) << imm12;
  // NOTE : since the imm12 is at the MSB 12bits position, we can save cycles for not cutting imm12
  // to 12 bits
  EmitI(imm12, rs1, 2, rd, 0x03);
}

void Riscv64Assembler::Ld(XRegister rd, XRegister rs1, int16_t imm12) {
  CHECK(IsInt<12>(imm12)) << imm12;
  // NOTE : since the imm12 is at the MSB 12bits position, we can save cycles for not cutting imm12
  // to 12 bits
  EmitI(imm12, rs1, 3, rd, 0x03);
}

void Riscv64Assembler::Lbu(XRegister rd, XRegister rs1, int16_t imm12) {
  CHECK(IsInt<12>(imm12)) << imm12;
  // NOTE : since the imm12 is at the MSB 12bits position, we can save cycles for not cutting imm12
  // to 12 bits
  EmitI(imm12, rs1, 4, rd, 0x03);
}

void Riscv64Assembler::Lhu(XRegister rd, XRegister rs1, int16_t imm12) {
  CHECK(IsInt<12>(imm12)) << imm12;
  // NOTE : since the imm12 is at the MSB 12bits position, we can save cycles for not cutting imm12
  // to 12 bits
  EmitI(imm12, rs1, 5, rd, 0x03);
}

void Riscv64Assembler::Lwu(XRegister rd, XRegister rs1, int16_t imm12) {
  CHECK(IsInt<12>(imm12)) << imm12;
  // NOTE : since the imm12 is at the MSB 12bits position, we can save cycles for not cutting imm12
  // to 12 bits
  EmitI(imm12, rs1, 6, rd, 0x03);
}

void Riscv64Assembler::Lui(XRegister rd, uint32_t imm20) {
  CHECK(IsUint<20>(imm20)) << imm20;
  EmitU(imm20, rd, 0x37);
}

void Riscv64Assembler::LrD(XRegister rd, XRegister rs1) {
  // INFO : lr.d will set the aq and rl bits to 0
  // if we want to set the aq or rl bits, we can use lr.d.aq / lr.d.rl / lr.d.aqrl
  EmitR(0x8, Zero, rs1, 3, rd, 0x2F);
}

void Riscv64Assembler::LrDAq(XRegister rd, XRegister rs1) { EmitR(0xA, Zero, rs1, 3, rd, 0x2F); }

void Riscv64Assembler::LrDRl(XRegister rd, XRegister rs1) { EmitR(0x9, Zero, rs1, 3, rd, 0x2F); }

void Riscv64Assembler::LrDAqrl(XRegister rd, XRegister rs1) { EmitR(0xB, Zero, rs1, 3, rd, 0x2F); }

void Riscv64Assembler::LrW(XRegister rd, XRegister rs1) {
  // INFO : lr.w will set the aq and rl bits to 0
  // if we want to set the aq or rl bits, we can use lr.w.aq / lr.w.rl / lr.w.aqrl
  EmitR(0x8, Zero, rs1, 2, rd, 0x2F);
}

void Riscv64Assembler::LrWAq(XRegister rd, XRegister rs1) { EmitR(0xA, Zero, rs1, 2, rd, 0x2F); }

void Riscv64Assembler::LrWRl(XRegister rd, XRegister rs1) { EmitR(0x9, Zero, rs1, 2, rd, 0x2F); }

void Riscv64Assembler::LrWAqrl(XRegister rd, XRegister rs1) { EmitR(0xB, Zero, rs1, 2, rd, 0x2F); }

void Riscv64Assembler::Li(XRegister rd, int64_t imm64) {
  if (IsInt<12>(imm64)) {
    Addi(rd, Zero, static_cast<int16_t>(imm64));
  } else {
    LiInternal(rd, imm64);
    // TODO: add instructions optimization for special cases, refers to LLVM.
  }
}

void Riscv64Assembler::LiInternal(XRegister rd, int64_t imm64) {
  if (IsInt<32>(imm64)) {
    // Get Hi20 bits with Bit 11 add 1
    int64_t Hi20 = ((imm64 + 0x800) >> kIImm12Bits) & 0xFFFFF;
    // Get the signed extention low 12 bits
    int64_t Lo12 = SignExtend64<kIImm12Bits>(imm64);

    // Emit Lui
    if (Hi20)
      Lui(rd, Hi20);

    // Emit Addiw base on previously Emit Lui or not
    if (Lo12 || Hi20 == 0) {
      Hi20 ? Addiw(rd, rd, Lo12) : Addi(rd, Zero, Lo12);
    }
    return;
  }

  // Get the signed extention low 12 bits
  int64_t Lo12 = SignExtend64<kIImm12Bits>(imm64);
  // Get High 52 bits with bit 11 add 1
  int64_t Hi52 = ((uint64_t)imm64 + 0x800ull) >> 12;
  // Find the 1st set bit from LSB of Hi52
  int ShiftAmount = 0;
  while ((((uint64_t)Hi52 >> ShiftAmount) & 1) == 0) {
    ShiftAmount++;
  }

  // Add back the number 12 which right shift out before
  ShiftAmount += kIImm12Bits;

  // Right shift out the LSB 0 bits and signed Extention it
  Hi52 = SignExtend64(Hi52 >> (ShiftAmount - kIImm12Bits), 64 - ShiftAmount);
  // Call Li itself with Hi52
  LiInternal(rd, Hi52);

  // Emit slli instruction with ShiftAmount and rd
  Slli(rd, rd, ShiftAmount);

  if (Lo12) {
    // Emit the Addi for Lo12 bits
    Addi(rd, rd, Lo12);
  }
}

// Store
void Riscv64Assembler::Sb(XRegister rs2, XRegister rs1, int16_t imm12) {
  CHECK(IsInt<12>(imm12)) << imm12;
  EmitS(((imm12 >> 5) & 0x7f), rs2, rs1, 0, (imm12 & 0x1f), 0x23);
}

void Riscv64Assembler::Sh(XRegister rs2, XRegister rs1, int16_t imm12) {
  CHECK(IsInt<12>(imm12)) << imm12;
  EmitS(((imm12 >> 5) & 0x7f), rs2, rs1, 1, (imm12 & 0x1f), 0x23);
}

void Riscv64Assembler::Sw(XRegister rs2, XRegister rs1, int16_t imm12) {
  CHECK(IsInt<12>(imm12)) << imm12;
  EmitS(((imm12 >> 5) & 0x7f), rs2, rs1, 2, (imm12 & 0x1f), 0x23);
}

void Riscv64Assembler::Sd(XRegister rs2, XRegister rs1, int16_t imm12) {
  CHECK(IsInt<12>(imm12)) << imm12;
  EmitS(((imm12 >> 5) & 0x7f), rs2, rs1, 3, (imm12 & 0x1f), 0x23);
}

void Riscv64Assembler::ScD(XRegister rd, XRegister rs2, XRegister rs1) {
  // INFO : sc.d will set the aq and rl bits to 0
  // if we want to set the aq or rl bits, we can use sc.d.aq / sc.d.rl / sc.d.aqrl
  EmitR(0xC, rs2, rs1, 3, rd, 0x2F);
}

void Riscv64Assembler::ScDAq(XRegister rd, XRegister rs2, XRegister rs1) {
  EmitR(0xE, rs2, rs1, 3, rd, 0x2F);
}

void Riscv64Assembler::ScDRl(XRegister rd, XRegister rs2, XRegister rs1) {
  EmitR(0xD, rs2, rs1, 3, rd, 0x2F);
}

void Riscv64Assembler::ScDAqrl(XRegister rd, XRegister rs2, XRegister rs1) {
  EmitR(0xF, rs2, rs1, 3, rd, 0x2F);
}

void Riscv64Assembler::ScW(XRegister rd, XRegister rs2, XRegister rs1) {
  // INFO : sc.w will set the aq and rl bits to 0
  // if we want to set the aq or rl bits, we can use sc.w.aq / sc.w.rl / sc.w.aqrl
  EmitR(0xC, rs2, rs1, 2, rd, 0x2F);
}

void Riscv64Assembler::ScWAq(XRegister rd, XRegister rs2, XRegister rs1) {
  EmitR(0xE, rs2, rs1, 2, rd, 0x2F);
}

void Riscv64Assembler::ScWRl(XRegister rd, XRegister rs2, XRegister rs1) {
  EmitR(0xD, rs2, rs1, 2, rd, 0x2F);
}

void Riscv64Assembler::ScWAqrl(XRegister rd, XRegister rs2, XRegister rs1) {
  EmitR(0xF, rs2, rs1, 2, rd, 0x2F);
}

// Compare
void Riscv64Assembler::Slt(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0, rs2, rs1, 2, rd, 0x33);
}

void Riscv64Assembler::Sltu(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0, rs2, rs1, 3, rd, 0x33);
}

void Riscv64Assembler::Slti(XRegister rd, XRegister rs1, int16_t imm12) {
  CHECK(IsInt<12>(imm12)) << imm12;
  EmitI(imm12, rs1, 2, rd, 0x13);
}

void Riscv64Assembler::Sltiu(XRegister rd, XRegister rs1, int16_t imm12) {
  CHECK(IsInt<12>(imm12)) << imm12;
  // INFO : the imm12 input can be signed value, but when core do the compare will treat it as
  // unsigned
  EmitI(imm12, rs1, 3, rd, 0x13);
}

// Jump & Link
void Riscv64Assembler::Jalr(XRegister rd, XRegister rs1, int16_t imm12) {
  CHECK(IsInt<12>(imm12)) << imm12;
  EmitI(imm12, rs1, 0, rd, 0x67);
}

void Riscv64Assembler::Jalr(XRegister rs1) { Jalr(RA, rs1, 0); }

void Riscv64Assembler::Jr(XRegister rs1) { Jalr(Zero, rs1, 0); }

void Riscv64Assembler::Jal(XRegister rd, uint32_t imm20) {
  CHECK(IsUint<20>(imm20)) << imm20;
  EmitJ(imm20, rd, 0x6f);
}

void Riscv64Assembler::Jal(uint32_t imm20) { Jal(RA, imm20); }

void Riscv64Assembler::J(uint32_t imm20) { Jal(Zero, imm20); }
void Riscv64Assembler::Ret(void) { Jalr(Zero, RA, 0); }

void Riscv64Assembler::Auipc(XRegister rd, uint32_t imm20) {
  CHECK(IsUint<20>(imm20)) << imm20;
  EmitU(imm20, rd, 0x17);
}
/*
void Riscv64Assembler::Call(XRegister rd, uint32_t imm32) {
  if (IsUint<20>(imm32)) {
    Jal(rd, imm32);
  } else {
    Auipc(AT, High20Bits(imm32));
    Jalr(rd, AT, SignExtend64<kIImm12Bits>(imm32));
  }
}

void Riscv64Assembler::Call(uint32_t imm32) {
  Call(RA, imm32);
}*/

void Riscv64Assembler::Fence(uint16_t prec, uint16_t succ) {
  uint16_t imm12 = static_cast<uint16_t>((prec & 0xf) << 4) | (succ & 0xf);
  EmitI(imm12, Zero, 0, Zero, 0xf);
}

// Branches
void Riscv64Assembler::Beq(XRegister rs1, XRegister rs2, uint16_t imm12) {
  EmitB(imm12, rs2, rs1, 0, 0x63);
}

void Riscv64Assembler::Beqz(XRegister rs1, uint16_t imm12) { Beq(rs1, Zero, imm12); }

void Riscv64Assembler::Bne(XRegister rs1, XRegister rs2, uint16_t imm12) {
  EmitB(imm12, rs2, rs1, 1, 0x63);
}

void Riscv64Assembler::Bnez(XRegister rs1, uint16_t imm12) { Bne(rs1, Zero, imm12); }

void Riscv64Assembler::Blt(XRegister rs1, XRegister rs2, uint16_t imm12) {
  EmitB(imm12, rs2, rs1, 4, 0x63);
}

void Riscv64Assembler::Bge(XRegister rs1, XRegister rs2, uint16_t imm12) {
  EmitB(imm12, rs2, rs1, 5, 0x63);
}

void Riscv64Assembler::Bltu(XRegister rs1, XRegister rs2, uint16_t imm12) {
  EmitB(imm12, rs2, rs1, 6, 0x63);
}

void Riscv64Assembler::Bgeu(XRegister rs1, XRegister rs2, uint16_t imm12) {
  EmitB(imm12, rs2, rs1, 7, 0x63);
}

void Riscv64Assembler::Bgt(XRegister rs1, XRegister rs2, uint16_t imm12) { Blt(rs2, rs1, imm12); }

void Riscv64Assembler::Ble(XRegister rs1, XRegister rs2, uint16_t imm12) { Bge(rs2, rs1, imm12); }

void Riscv64Assembler::Bgtu(XRegister rs1, XRegister rs2, uint16_t imm12) { Bltu(rs2, rs1, imm12); }

void Riscv64Assembler::Bleu(XRegister rs1, XRegister rs2, uint16_t imm12) { Bgeu(rs2, rs1, imm12); }

void Riscv64Assembler::Blez(XRegister rs1, uint16_t imm12) { Bge(Zero, rs1, imm12); }

void Riscv64Assembler::Bgez(XRegister rs1, uint16_t imm12) { Bge(rs1, Zero, imm12); }

void Riscv64Assembler::Bltz(XRegister rs1, uint16_t imm12) { Blt(rs1, Zero, imm12); }

void Riscv64Assembler::Bgtz(XRegister rs1, uint16_t imm12) { Blt(Zero, rs1, imm12); }

void Riscv64Assembler::EmitBcond(BranchCondition cond,
                                 XRegister rs1,
                                 XRegister rs2,
                                 uint32_t imm12) {
  switch (cond) {
    case kCondEQ:
      Beq(rs1, rs2, imm12);
      break;
    case kCondNE:
      Bne(rs1, rs2, imm12);
      break;
    case kCondLT:
      Blt(rs1, rs2, imm12);
      break;
    case kCondGE:
      Bge(rs1, rs2, imm12);
      break;
    case kCondLTU:
      Bltu(rs1, rs2, imm12);
      break;
    case kCondGEU:
      Bgeu(rs1, rs2, imm12);
      break;
    case kCondGT:
      Bgt(rs1, rs2, imm12);
      break;
    case kCondLE:
      Ble(rs1, rs2, imm12);
      break;
    case kCondGTU:
      Bgtu(rs1, rs2, imm12);
      break;
    case kCondLEU:
      Bleu(rs1, rs2, imm12);
      break;
    case kCondEQZ:
      CHECK_EQ(rs2, Zero);
      Beqz(rs1, imm12);
      break;
    case kCondNEZ:
      CHECK_EQ(rs2, Zero);
      Bnez(rs1, imm12);
      break;
    case kCondLEZ:
      CHECK_EQ(rs2, Zero);
      Blez(rs1, imm12);
      break;
    case kCondGEZ:
      CHECK_EQ(rs2, Zero);
      Bgez(rs1, imm12);
      break;
    case kCondLTZ:
      CHECK_EQ(rs2, Zero);
      Bltz(rs1, imm12);
      break;
    case kCondGTZ:
      CHECK_EQ(rs2, Zero);
      Bgtz(rs1, imm12);
      break;
    case kUncond:
      LOG(FATAL) << "Unexpected branch condition " << cond;
      UNREACHABLE();
  }
}

/** Floating Single-Precision begins **/
// Arithmetic

void Riscv64Assembler::FaddS(FRegister frd, FRegister frs1, FRegister frs2, RoundingMode rm) {
  EmitR(0, frs2, frs1, rm, frd, 0x53);
}

void Riscv64Assembler::FsubS(FRegister frd, FRegister frs1, FRegister frs2, RoundingMode rm) {
  EmitR(0x4, frs2, frs1, rm, frd, 0x53);
}

void Riscv64Assembler::FmulS(FRegister frd, FRegister frs1, FRegister frs2, RoundingMode rm) {
  EmitR(0x8, frs2, frs1, rm, frd, 0x53);
}

void Riscv64Assembler::FmaddS(
    FRegister frd, FRegister frs1, FRegister frs2, FRegister frs3, RoundingMode rm) {
  EmitR4(frs3, 0, frs2, frs1, rm, frd, 0x43);
}

void Riscv64Assembler::FmsubS(
    FRegister frd, FRegister frs1, FRegister frs2, FRegister frs3, RoundingMode rm) {
  EmitR4(frs3, 0, frs2, frs1, rm, frd, 0x47);
}

void Riscv64Assembler::FnmaddS(
    FRegister frd, FRegister frs1, FRegister frs2, FRegister frs3, RoundingMode rm) {
  EmitR4(frs3, 0, frs2, frs1, rm, frd, 0x4F);
}

void Riscv64Assembler::FnmsubS(
    FRegister frd, FRegister frs1, FRegister frs2, FRegister frs3, RoundingMode rm) {
  EmitR4(frs3, 0, frs2, frs1, rm, frd, 0x4B);
}

void Riscv64Assembler::FdivS(FRegister frd, FRegister frs1, FRegister frs2, RoundingMode rm) {
  EmitR(0xC, frs2, frs1, rm, frd, 0x53);
}

void Riscv64Assembler::FsqrtS(FRegister frd, FRegister frs1, RoundingMode rm) {
  EmitR(0x2C, static_cast<FRegister>(0), frs1, rm, frd, 0x53);
}

void Riscv64Assembler::FmvS(FRegister frd, FRegister frs1) { FsgnjS(frd, frs1, frs1); }

void Riscv64Assembler::FmvWX(FRegister frd, XRegister rs1) {
  EmitR(0x78, static_cast<FRegister>(0), rs1, 0, frd, 0x53);
}

void Riscv64Assembler::FmvXW(XRegister rd, FRegister frs1) {
  EmitR(0x70, static_cast<FRegister>(0), frs1, 0, rd, 0x53);
}

void Riscv64Assembler::FclassS(XRegister rd, FRegister frs1) {
  EmitR(0x70, static_cast<FRegister>(0), frs1, 1, rd, 0x53);
}

void Riscv64Assembler::FcvtLS(XRegister rd, FRegister frs1, RoundingMode rm) {
  EmitR(0x60, static_cast<FRegister>(2), frs1, rm, rd, 0x53);
}

void Riscv64Assembler::FcvtLuS(XRegister rd, FRegister frs1, RoundingMode rm) {
  EmitR(0x60, static_cast<FRegister>(3), frs1, rm, rd, 0x53);
}

void Riscv64Assembler::FcvtWS(XRegister rd, FRegister frs1, RoundingMode rm) {
  EmitR(0x60, static_cast<FRegister>(0), frs1, rm, rd, 0x53);
}

void Riscv64Assembler::FcvtWuS(XRegister rd, FRegister frs1, RoundingMode rm) {
  EmitR(0x60, static_cast<FRegister>(1), frs1, rm, rd, 0x53);
}

void Riscv64Assembler::FcvtSL(FRegister frd, XRegister rs1, RoundingMode rm) {
  EmitR(0x68, static_cast<FRegister>(2), rs1, rm, frd, 0x53);
}

void Riscv64Assembler::FcvtSLu(FRegister frd, XRegister rs1, RoundingMode rm) {
  EmitR(0x68, static_cast<FRegister>(3), rs1, rm, frd, 0x53);
}

void Riscv64Assembler::FcvtSW(FRegister frd, XRegister rs1, RoundingMode rm) {
  EmitR(0x68, static_cast<FRegister>(0), rs1, rm, frd, 0x53);
}

void Riscv64Assembler::FcvtSWu(FRegister frd, XRegister rs1, RoundingMode rm) {
  EmitR(0x68, static_cast<FRegister>(1), rs1, rm, frd, 0x53);
}

void Riscv64Assembler::FmaxS(FRegister frd, FRegister frs1, FRegister frs2) {
  EmitR(0x14, frs2, frs1, 1, frd, 0x53);
}

void Riscv64Assembler::FminS(FRegister frd, FRegister frs1, FRegister frs2) {
  EmitR(0x14, frs2, frs1, 0, frd, 0x53);
}

void Riscv64Assembler::FabsS(FRegister frd, FRegister frs1) { FsgnjxS(frd, frs1, frs1); }

void Riscv64Assembler::FnegS(FRegister frd, FRegister frs1) { FsgnjnS(frd, frs1, frs1); }

void Riscv64Assembler::FsgnjS(FRegister frd, FRegister frs1, FRegister frs2) {
  EmitR(0x10, frs2, frs1, 0, frd, 0x53);
}

void Riscv64Assembler::FsgnjnS(FRegister frd, FRegister frs1, FRegister frs2) {
  EmitR(0x10, frs2, frs1, 1, frd, 0x53);
}

void Riscv64Assembler::FsgnjxS(FRegister frd, FRegister frs1, FRegister frs2) {
  EmitR(0x10, frs2, frs1, 2, frd, 0x53);
}

// Compare
void Riscv64Assembler::FeqS(XRegister rd, FRegister frs1, FRegister frs2) {
  EmitR(0x50, frs2, frs1, 2, rd, 0x53);
}

void Riscv64Assembler::FleS(XRegister rd, FRegister frs1, FRegister frs2) {
  EmitR(0x50, frs2, frs1, 0, rd, 0x53);
}

void Riscv64Assembler::FltS(XRegister rd, FRegister frs1, FRegister frs2) {
  EmitR(0x50, frs2, frs1, 1, rd, 0x53);
}

// Load
void Riscv64Assembler::Flw(FRegister frd, XRegister rs1, int16_t imm12) {
  CHECK(IsInt<12>(imm12)) << imm12;
  EmitI(imm12, rs1, 2, frd, 0x7);
}

// Store
void Riscv64Assembler::Fsw(FRegister frs2, XRegister rs1, int16_t imm12) {
  CHECK(IsInt<12>(imm12)) << imm12;
  EmitS(((imm12 >> 5) & 0x7f), frs2, rs1, 2, (imm12 & 0x1f), 0x27);
}

/** Floating Single-Precision ends **/

/** Floating Double-Precision begins **/
// Arithmetic
void Riscv64Assembler::FaddD(FRegister frd, FRegister frs1, FRegister frs2, RoundingMode rm) {
  EmitR(1, frs2, frs1, rm, frd, 0x53);
}

void Riscv64Assembler::FsubD(FRegister frd, FRegister frs1, FRegister frs2, RoundingMode rm) {
  EmitR(0x5, frs2, frs1, rm, frd, 0x53);
}

void Riscv64Assembler::FmulD(FRegister frd, FRegister frs1, FRegister frs2, RoundingMode rm) {
  EmitR(0x9, frs2, frs1, rm, frd, 0x53);
}

void Riscv64Assembler::FmaddD(
    FRegister frd, FRegister frs1, FRegister frs2, FRegister frs3, RoundingMode rm) {
  EmitR4(frs3, 1, frs2, frs1, rm, frd, 0x43);
}

void Riscv64Assembler::FmsubD(
    FRegister frd, FRegister frs1, FRegister frs2, FRegister frs3, RoundingMode rm) {
  EmitR4(frs3, 1, frs2, frs1, rm, frd, 0x47);
}

void Riscv64Assembler::FnmaddD(
    FRegister frd, FRegister frs1, FRegister frs2, FRegister frs3, RoundingMode rm) {
  EmitR4(frs3, 1, frs2, frs1, rm, frd, 0x4F);
}

void Riscv64Assembler::FnmsubD(
    FRegister frd, FRegister frs1, FRegister frs2, FRegister frs3, RoundingMode rm) {
  EmitR4(frs3, 1, frs2, frs1, rm, frd, 0x4B);
}

void Riscv64Assembler::FdivD(FRegister frd, FRegister frs1, FRegister frs2, RoundingMode rm) {
  EmitR(0xD, frs2, frs1, rm, frd, 0x53);
}

void Riscv64Assembler::FsqrtD(FRegister frd, FRegister frs1, RoundingMode rm) {
  EmitR(0x2D, static_cast<FRegister>(0), frs1, rm, frd, 0x53);
}

void Riscv64Assembler::FmvDX(FRegister frd, XRegister rs1) {
  EmitR(0x79, static_cast<FRegister>(0), rs1, 0, frd, 0x53);
}

void Riscv64Assembler::FmvXD(XRegister rd, FRegister frs1) {
  EmitR(0x71, static_cast<FRegister>(0), frs1, 0, rd, 0x53);
}

void Riscv64Assembler::FmvD(FRegister frd, FRegister frs1) { FsgnjD(frd, frs1, frs1); }

void Riscv64Assembler::FclassD(XRegister rd, FRegister frs1) {
  EmitR(0x71, static_cast<FRegister>(0), frs1, 1, rd, 0x53);
}

void Riscv64Assembler::FcvtLD(XRegister rd, FRegister frs1, RoundingMode rm) {
  EmitR(0x61, static_cast<FRegister>(2), frs1, rm, rd, 0x53);
}

void Riscv64Assembler::FcvtLuD(XRegister rd, FRegister frs1, RoundingMode rm) {
  EmitR(0x61, static_cast<FRegister>(3), frs1, rm, rd, 0x53);
}

void Riscv64Assembler::FcvtWD(XRegister rd, FRegister frs1, RoundingMode rm) {
  EmitR(0x61, static_cast<FRegister>(0), frs1, rm, rd, 0x53);
}

void Riscv64Assembler::FcvtWuD(XRegister rd, FRegister frs1, RoundingMode rm) {
  EmitR(0x61, static_cast<FRegister>(1), frs1, rm, rd, 0x53);
}

void Riscv64Assembler::FcvtDL(FRegister frd, XRegister rs1, RoundingMode rm) {
  EmitR(0x69, static_cast<FRegister>(2), rs1, rm, frd, 0x53);
}

void Riscv64Assembler::FcvtDLu(FRegister frd, XRegister rs1, RoundingMode rm) {
  EmitR(0x69, static_cast<FRegister>(3), rs1, rm, frd, 0x53);
}

void Riscv64Assembler::FcvtDW(FRegister frd, XRegister rs1) {
  EmitR(0x69, static_cast<FRegister>(0), rs1, 0, frd, 0x53);
}

void Riscv64Assembler::FcvtDWu(FRegister frd, XRegister rs1) {
  EmitR(0x69, static_cast<FRegister>(1), rs1, 0, frd, 0x53);
}

void Riscv64Assembler::FcvtDS(FRegister frd, FRegister frs1) {
  EmitR(0x21, static_cast<FRegister>(0), frs1, 0, frd, 0x53);
}

void Riscv64Assembler::FcvtSD(FRegister frd, FRegister frs1, RoundingMode rm) {
  EmitR(0x20, static_cast<FRegister>(1), frs1, rm, frd, 0x53);
}

void Riscv64Assembler::FmaxD(FRegister frd, FRegister frs1, FRegister frs2) {
  EmitR(0x15, frs2, frs1, 1, frd, 0x53);
}

void Riscv64Assembler::FminD(FRegister frd, FRegister frs1, FRegister frs2) {
  EmitR(0x15, frs2, frs1, 0, frd, 0x53);
}

void Riscv64Assembler::FabsD(FRegister frd, FRegister frs1) { FsgnjxD(frd, frs1, frs1); }

void Riscv64Assembler::FnegD(FRegister frd, FRegister frs1) { FsgnjnD(frd, frs1, frs1); }

void Riscv64Assembler::FsgnjD(FRegister frd, FRegister frs1, FRegister frs2) {
  EmitR(0x11, frs2, frs1, 0, frd, 0x53);
}

void Riscv64Assembler::FsgnjnD(FRegister frd, FRegister frs1, FRegister frs2) {
  EmitR(0x11, frs2, frs1, 1, frd, 0x53);
}

void Riscv64Assembler::FsgnjxD(FRegister frd, FRegister frs1, FRegister frs2) {
  EmitR(0x11, frs2, frs1, 2, frd, 0x53);
}

// Compare
void Riscv64Assembler::FeqD(XRegister rd, FRegister frs1, FRegister frs2) {
  EmitR(0x51, frs2, frs1, 2, rd, 0x53);
}

void Riscv64Assembler::FleD(XRegister rd, FRegister frs1, FRegister frs2) {
  EmitR(0x51, frs2, frs1, 0, rd, 0x53);
}

void Riscv64Assembler::FltD(XRegister rd, FRegister frs1, FRegister frs2) {
  EmitR(0x51, frs2, frs1, 1, rd, 0x53);
}

// Load
void Riscv64Assembler::Fld(FRegister frd, XRegister rs1, int16_t imm12) {
  CHECK(IsInt<12>(imm12)) << imm12;
  EmitI(imm12, rs1, 3, frd, 0x7);
}

// Store
void Riscv64Assembler::Fsd(FRegister frs2, XRegister rs1, int16_t imm12) {
  CHECK(IsInt<12>(imm12)) << imm12;
  EmitS(((imm12 >> 5) & 0x7f), frs2, rs1, 3, (imm12 & 0x1f), 0x27);
}

/** Floating Double-Precision ends **/

void Riscv64Assembler::Ebreak(void) {
  EmitI(1, static_cast<XRegister>(0), 0, static_cast<XRegister>(0), 0x73);
}

void Riscv64Assembler::Nop(void) {
  /* by use addi*/
  Addi(Zero, Zero, 0);
}

void Riscv64Assembler::Mv(XRegister rd, XRegister rs1) { Addi(rd, rs1, 0); }

void Riscv64Assembler::Seqz(XRegister rd, XRegister rs1) { Sltiu(rd, rs1, 1); }

void Riscv64Assembler::Branch::InitShortOrLong(Riscv64Assembler::Branch::OffsetBits offset_size,
                                               Riscv64Assembler::Branch::Type short_type,
                                               Riscv64Assembler::Branch::Type long_type) {
  type_ = (offset_size <= branch_info_[short_type].offset_size) ? short_type : long_type;
}

void Riscv64Assembler::Branch::InitializeType(Type initial_type) {
  OffsetBits offset_size_needed = GetOffsetSizeNeeded(location_, target_);

  switch (initial_type) {
    case kLabel:
    case kLiteral:
    case kLiteralUnsigned:
    case kLiteralLong:
      CHECK(!IsResolved());
      type_ = initial_type;
      break;
    case kCall:
      InitShortOrLong(offset_size_needed, kCall, kLongCall);
      break;
    case kCondBranch:
      switch (condition_) {
        case kUncond:
          InitShortOrLong(offset_size_needed, kUncondBranch, kLongUncondBranch);
          break;
        default:
          InitShortOrLong(offset_size_needed, kCondBranch, kLongCondBranch);
          break;
      }
      break;
    default:
      LOG(FATAL) << "Unexpected branch type " << initial_type;
      UNREACHABLE();
  }
  old_type_ = type_;
}

bool Riscv64Assembler::Branch::IsNop(BranchCondition condition, XRegister lhs, XRegister rhs) {
  switch (condition) {
    case kCondLT:
    case kCondLTU:
    case kCondNE:
    case kCondGT:
    case kCondGTU:
      return lhs == rhs;
    default:
      return false;
  }
}

bool Riscv64Assembler::Branch::IsUncond(BranchCondition condition, XRegister lhs, XRegister rhs) {
  switch (condition) {
    case kUncond:
      return true;
    case kCondEQ:
    case kCondGE:
    case kCondGEU:
    case kCondLE:
    case kCondLEU:
      return lhs == rhs;
    default:
      return false;
  }
}

Riscv64Assembler::Branch::Branch(uint32_t location,
                                 uint32_t target,
                                 bool is_call,
                                 XRegister lhs_reg)
    : old_location_(location),
      location_(location),
      target_(target),
      lhs_reg_(lhs_reg),
      rhs_reg_(Zero),
      condition_(kUncond) {
  InitializeType(is_call ? kCall : kCondBranch);
}

Riscv64Assembler::Branch::Branch(uint32_t location,
                                 uint32_t target,
                                 Riscv64Assembler::BranchCondition condition,
                                 XRegister lhs_reg,
                                 XRegister rhs_reg)
    : old_location_(location),
      location_(location),
      target_(target),
      lhs_reg_(lhs_reg),
      rhs_reg_(rhs_reg),
      condition_(condition) {
  CHECK_NE(condition, kUncond);
  switch (condition) {
    case kCondEQ:
    case kCondNE:
    case kCondLT:
    case kCondGE:
    case kCondLTU:
    case kCondGEU:
    case kCondGT:
    case kCondLE:
    case kCondGTU:
    case kCondLEU:
      CHECK_NE(lhs_reg, Zero);
      CHECK_NE(rhs_reg, Zero);
      break;
    case kCondEQZ:
    case kCondNEZ:
    case kCondLEZ:
    case kCondGEZ:
    case kCondLTZ:
    case kCondGTZ:
      CHECK_NE(lhs_reg, Zero);
      CHECK_EQ(rhs_reg, Zero);
      break;
    /*
    // floating point condition branch is not supported
    case kCondF:
    case kCondT:
      CHECK_EQ(rhs_reg, Zero);
      break;
    */
    case kUncond:
      UNREACHABLE();
  }
  CHECK(!IsNop(condition, lhs_reg, rhs_reg));
  if (IsUncond(condition, lhs_reg, rhs_reg)) {
    // Branch condition is always true, make the branch unconditional.
    condition_ = kUncond;
  }
  InitializeType(kCondBranch);
}

Riscv64Assembler::Branch::Branch(uint32_t location, XRegister dest_reg, Type label_or_literal_type)
    : old_location_(location),
      location_(location),
      target_(kUnresolved),
      lhs_reg_(dest_reg),
      rhs_reg_(Zero),
      condition_(kUncond) {
  CHECK_NE(dest_reg, Zero);
  InitializeType(label_or_literal_type);
}

Riscv64Assembler::BranchCondition Riscv64Assembler::Branch::OppositeCondition(
    Riscv64Assembler::BranchCondition cond) {
  switch (cond) {
    case kCondEQ:
      return kCondNE;
    case kCondNE:
      return kCondEQ;
    case kCondLT:
      return kCondGE;
    case kCondGE:
      return kCondLT;
    case kCondLTU:
      return kCondGEU;
    case kCondGEU:
      return kCondLTU;
    case kCondGT:
      return kCondLE;
    case kCondLE:
      return kCondGT;
    case kCondGTU:
      return kCondLEU;
    case kCondLEU:
      return kCondGTU;
    case kCondEQZ:
      return kCondNEZ;
    case kCondNEZ:
      return kCondEQZ;
    case kCondLEZ:
      return kCondGTZ;
    case kCondGEZ:
      return kCondLTZ;
    case kCondLTZ:
      return kCondGEZ;
    case kCondGTZ:
      return kCondLEZ;
    case kUncond:
      LOG(FATAL) << "Unexpected branch condition " << cond;
  }
  UNREACHABLE();
}

Riscv64Assembler::Branch::Type Riscv64Assembler::Branch::GetType() const { return type_; }

Riscv64Assembler::BranchCondition Riscv64Assembler::Branch::GetCondition() const {
  return condition_;
}

XRegister Riscv64Assembler::Branch::GetLeftRegister() const { return lhs_reg_; }

XRegister Riscv64Assembler::Branch::GetRightRegister() const { return rhs_reg_; }

uint32_t Riscv64Assembler::Branch::GetTarget() const { return target_; }

uint32_t Riscv64Assembler::Branch::GetLocation() const { return location_; }

uint32_t Riscv64Assembler::Branch::GetOldLocation() const { return old_location_; }

uint32_t Riscv64Assembler::Branch::GetLength() const { return branch_info_[type_].length; }

uint32_t Riscv64Assembler::Branch::GetOldLength() const { return branch_info_[old_type_].length; }

uint32_t Riscv64Assembler::Branch::GetSize() const { return GetLength() * sizeof(uint32_t); }

uint32_t Riscv64Assembler::Branch::GetOldSize() const { return GetOldLength() * sizeof(uint32_t); }

uint32_t Riscv64Assembler::Branch::GetEndLocation() const { return GetLocation() + GetSize(); }

uint32_t Riscv64Assembler::Branch::GetOldEndLocation() const {
  return GetOldLocation() + GetOldSize();
}

bool Riscv64Assembler::Branch::IsLong() const {
  switch (type_) {
    case kUncondBranch:
    case kCondBranch:
    case kCall:
    // Near label.
    case kLabel:
    // Near literals.
    case kLiteral:
    case kLiteralUnsigned:
    case kLiteralLong:
      return false;
    // Long branches.
    case kLongUncondBranch:
    case kLongCondBranch:
    case kLongCall:
      return true;
  }
  UNREACHABLE();
}

bool Riscv64Assembler::Branch::IsResolved() const { return target_ != kUnresolved; }

Riscv64Assembler::Branch::OffsetBits Riscv64Assembler::Branch::GetOffsetSize() const {
  return branch_info_[type_].offset_size;
}

Riscv64Assembler::Branch::OffsetBits Riscv64Assembler::Branch::GetOffsetSizeNeeded(
    uint32_t location, uint32_t target) {
  // For unresolved targets assume the shortest encoding
  // (later it will be made longer if needed).
  if (target == kUnresolved)
    return kOffset13;
  int64_t distance = static_cast<int64_t>(target) - location;
  // To simplify calculations in composite branches consisting of multiple instructions
  // bump up the distance by a value larger than the max byte size of a composite branch.
  distance += (distance >= 0) ? kMaxBranchSize : -kMaxBranchSize;
  if (IsInt<kOffset13>(distance))
    return kOffset13;
  else if (IsInt<kOffset21>(distance))
    return kOffset21;
  return kOffset32;
}

void Riscv64Assembler::Branch::Resolve(uint32_t target) { target_ = target; }

void Riscv64Assembler::Branch::Relocate(uint32_t expand_location, uint32_t delta) {
  if (location_ > expand_location) {
    location_ += delta;
  }
  if (!IsResolved()) {
    return;  // Don't know the target yet.
  }
  if (target_ > expand_location) {
    target_ += delta;
  }
}

void Riscv64Assembler::Branch::PromoteToLong() {
  switch (type_) {
    // R6 short branches (can be promoted to long).
    case kUncondBranch:
      type_ = kLongUncondBranch;
      break;
    case kCondBranch:
      type_ = kLongCondBranch;
      break;
    case kCall:
      type_ = kLongCall;
      break;
    default:
      // Note: 'type_' is already long.
      break;
  }
  CHECK(IsLong());
}

uint32_t Riscv64Assembler::Branch::PromoteIfNeeded(uint32_t max_short_distance) {
  // If the branch is still unresolved or already long, nothing to do.
  if (IsLong() || !IsResolved()) {
    return 0;
  }
  // Promote the short branch to long if the offset size is too small
  // to hold the distance between location_ and target_.
  if (GetOffsetSizeNeeded(location_, target_) > GetOffsetSize()) {
    PromoteToLong();
    uint32_t old_size = GetOldSize();
    uint32_t new_size = GetSize();
    CHECK_GT(new_size, old_size);
    return new_size - old_size;
  }
  // The following logic is for debugging/testing purposes.
  // Promote some short branches to long when it's not really required.
  if (UNLIKELY(max_short_distance != std::numeric_limits<uint32_t>::max())) {
    int64_t distance = static_cast<int64_t>(target_) - location_;
    distance = (distance >= 0) ? distance : -distance;
    if (distance >= max_short_distance) {
      PromoteToLong();
      uint32_t old_size = GetOldSize();
      uint32_t new_size = GetSize();
      CHECK_GT(new_size, old_size);
      return new_size - old_size;
    }
  }
  return 0;
}

uint32_t Riscv64Assembler::Branch::GetOffsetLocation() const {
  return location_ + branch_info_[type_].instr_offset * sizeof(uint32_t);
}

uint32_t Riscv64Assembler::Branch::GetOffset() const {
  CHECK(IsResolved());
  uint32_t ofs_mask = 0xFFFFFFFF >> (32 - GetOffsetSize());
  // Calculate the byte distance between instructions and also account for
  // different PC-relative origins.
  uint32_t offset_location = GetOffsetLocation();
  /*
    // NOT NEEDED for RISCV
    if (type_ == kLiteralLong) {
      // Special case for the ldpc instruction, whose address (PC) is rounded down to
      // a multiple of 8 before adding the offset.
      // Note, branch promotion has already taken care of aligning `target_` to an
      // address that's a multiple of 8.
      offset_location = RoundDown(offset_location, sizeof(uint64_t));
    }
  */
  uint32_t offset = target_ - offset_location - branch_info_[type_].pc_org * sizeof(uint32_t);
  // Prepare the offset for encoding into the instruction(s).
  offset = (offset & ofs_mask) >> branch_info_[type_].offset_shift;
  return offset;
}

Riscv64Assembler::Branch* Riscv64Assembler::GetBranch(uint32_t branch_id) {
  CHECK_LT(branch_id, branches_.size());
  return &branches_[branch_id];
}

const Riscv64Assembler::Branch* Riscv64Assembler::GetBranch(uint32_t branch_id) const {
  CHECK_LT(branch_id, branches_.size());
  return &branches_[branch_id];
}

void Riscv64Assembler::Bind(Riscv64Label* label) {
  CHECK(!label->IsBound());
  uint32_t bound_pc = buffer_.Size();

  // Walk the list of branches referring to and preceding this label.
  // Store the previously unknown target addresses in them.
  while (label->IsLinked()) {
    uint32_t branch_id = label->Position();
    Branch* branch = GetBranch(branch_id);
    branch->Resolve(bound_pc);

    uint32_t branch_location = branch->GetLocation();
    // Extract the location of the previous branch in the list (walking the list backwards;
    // the previous branch ID was stored in the space reserved for this branch).
    uint32_t prev = buffer_.Load<uint32_t>(branch_location);

    // On to the previous branch in the list...
    label->position_ = prev;
  }

  // Now make the label object contain its own location (relative to the end of the preceding
  // branch, if any; it will be used by the branches referring to and following this label).
  label->prev_branch_id_plus_one_ = branches_.size();
  if (label->prev_branch_id_plus_one_) {
    uint32_t branch_id = label->prev_branch_id_plus_one_ - 1;
    const Branch* branch = GetBranch(branch_id);
    bound_pc -= branch->GetEndLocation();
  }
  label->BindTo(bound_pc);
}

uint32_t Riscv64Assembler::GetLabelLocation(const Riscv64Label* label) const {
  CHECK(label->IsBound());
  uint32_t target = label->Position();
  if (label->prev_branch_id_plus_one_) {
    // Get label location based on the branch preceding it.
    uint32_t branch_id = label->prev_branch_id_plus_one_ - 1;
    const Branch* branch = GetBranch(branch_id);
    target += branch->GetEndLocation();
  }
  return target;
}

uint32_t Riscv64Assembler::GetAdjustedPosition(uint32_t old_position) {
  // We can reconstruct the adjustment by going through all the branches from the beginning
  // up to the old_position. Since we expect AdjustedPosition() to be called in a loop
  // with increasing old_position, we can use the data from last AdjustedPosition() to
  // continue where we left off and the whole loop should be O(m+n) where m is the number
  // of positions to adjust and n is the number of branches.
  if (old_position < last_old_position_) {
    last_position_adjustment_ = 0;
    last_old_position_ = 0;
    last_branch_id_ = 0;
  }
  while (last_branch_id_ != branches_.size()) {
    const Branch* branch = GetBranch(last_branch_id_);
    if (branch->GetLocation() >= old_position + last_position_adjustment_) {
      break;
    }
    last_position_adjustment_ += branch->GetSize() - branch->GetOldSize();
    ++last_branch_id_;
  }
  last_old_position_ = old_position;
  return old_position + last_position_adjustment_;
}

void Riscv64Assembler::FinalizeLabeledBranch(Riscv64Label* label) {
  uint32_t length = branches_.back().GetLength();
  if (!label->IsBound()) {
    // Branch forward (to a following label), distance is unknown.
    // The first branch forward will contain 0, serving as the terminator of
    // the list of forward-reaching branches.
    Emit(label->position_);
    length--;
    // Now make the label object point to this branch
    // (this forms a linked list of branches preceding this label).
    uint32_t branch_id = branches_.size() - 1;
    label->LinkTo(branch_id);
  }
  // Reserve space for the branch.
  for (; length != 0u; --length) {
    Nop();
  }
}

void Riscv64Assembler::Buncond(Riscv64Label* label) {
  uint32_t target = label->IsBound() ? GetLabelLocation(label) : Branch::kUnresolved;
  branches_.emplace_back(buffer_.Size(), target, /* is_call= */ false);
  FinalizeLabeledBranch(label);
}

void Riscv64Assembler::Bcond(Riscv64Label* label,
                             BranchCondition condition,
                             XRegister lhs,
                             XRegister rhs) {
  // If lhs = rhs, this can be a NOP.
  if (Branch::IsNop(condition, lhs, rhs)) {
    return;
  }
  uint32_t target = label->IsBound() ? GetLabelLocation(label) : Branch::kUnresolved;
  branches_.emplace_back(buffer_.Size(), target, condition, lhs, rhs);
  FinalizeLabeledBranch(label);
}

void Riscv64Assembler::BuncondCall(XRegister rd, Riscv64Label* label) {
  uint32_t target = label->IsBound() ? GetLabelLocation(label) : Branch::kUnresolved;
  branches_.emplace_back(buffer_.Size(), target, /* is_call= */ true, rd);
  FinalizeLabeledBranch(label);
}

void Riscv64Assembler::LoadLabelAddress(XRegister dest_reg, Riscv64Label* label) {
  // Label address loads are treated as pseudo branches since they require very similar handling.
  DCHECK(!label->IsBound());
  branches_.emplace_back(buffer_.Size(), dest_reg, Branch::kLabel);
  FinalizeLabeledBranch(label);
}

Literal* Riscv64Assembler::NewLiteral(size_t size, const uint8_t* data) {
  // We don't support byte and half-word literals.
  if (size == 4u) {
    literals_.emplace_back(size, data);
    return &literals_.back();
  } else {
    DCHECK_EQ(size, 8u);
    long_literals_.emplace_back(size, data);
    return &long_literals_.back();
  }
}

void Riscv64Assembler::LoadLiteral(XRegister dest_reg,
                                   LoadOperandType load_type,
                                   Literal* literal) {
  // Literal loads are treated as pseudo branches since they require very similar handling.
  Branch::Type literal_type;
  switch (load_type) {
    case kLoadWord:
      DCHECK_EQ(literal->GetSize(), 4u);
      literal_type = Branch::kLiteral;
      break;
    case kLoadUnsignedWord:
      DCHECK_EQ(literal->GetSize(), 4u);
      literal_type = Branch::kLiteralUnsigned;
      break;
    case kLoadDoubleword:
      DCHECK_EQ(literal->GetSize(), 8u);
      literal_type = Branch::kLiteralLong;
      break;
    default:
      LOG(FATAL) << "Unexpected literal load type " << load_type;
      UNREACHABLE();
  }
  Riscv64Label* label = literal->GetLabel();
  DCHECK(!label->IsBound());
  branches_.emplace_back(buffer_.Size(), dest_reg, literal_type);
  FinalizeLabeledBranch(label);
}

JumpTable* Riscv64Assembler::CreateJumpTable(std::vector<Riscv64Label*>&& labels) {
  jump_tables_.emplace_back(std::move(labels));
  JumpTable* table = &jump_tables_.back();
  DCHECK(!table->GetLabel()->IsBound());
  return table;
}

void Riscv64Assembler::ReserveJumpTableSpace() {
  if (!jump_tables_.empty()) {
    for (JumpTable& table : jump_tables_) {
      Riscv64Label* label = table.GetLabel();
      Bind(label);

      // Bulk ensure capacity, as this may be large.
      size_t orig_size = buffer_.Size();
      size_t required_capacity = orig_size + table.GetSize();
      if (required_capacity > buffer_.Capacity()) {
        buffer_.ExtendCapacity(required_capacity);
      }
#ifndef NDEBUG
      buffer_.has_ensured_capacity_ = true;
#endif

      // Fill the space with dummy data as the data is not final
      // until the branches have been promoted. And we shouldn't
      // be moving uninitialized data during branch promotion.
      for (size_t cnt = table.GetData().size(), i = 0; i < cnt; i++) {
        buffer_.Emit<uint32_t>(0x1abe1234u);
      }

#ifndef NDEBUG
      buffer_.has_ensured_capacity_ = false;
#endif
    }
  }
}

void Riscv64Assembler::EmitJumpTables() {
  if (!jump_tables_.empty()) {
    CHECK(!overwriting_);
    // Switch from appending instructions at the end of the buffer to overwriting
    // existing instructions (here, jump tables) in the buffer.
    overwriting_ = true;

    for (JumpTable& table : jump_tables_) {
      Riscv64Label* table_label = table.GetLabel();
      uint32_t start = GetLabelLocation(table_label);
      overwrite_location_ = start;

      for (Riscv64Label* target : table.GetData()) {
        CHECK_EQ(buffer_.Load<uint32_t>(overwrite_location_), 0x1abe1234u);
        // The table will contain target addresses relative to the table start.
        uint32_t offset = GetLabelLocation(target) - start;
        Emit(offset);
      }
    }

    overwriting_ = false;
  }
}

void Riscv64Assembler::EmitLiterals() {
  if (!literals_.empty()) {
    for (Literal& literal : literals_) {
      Riscv64Label* label = literal.GetLabel();
      Bind(label);
      AssemblerBuffer::EnsureCapacity ensured(&buffer_);
      DCHECK_EQ(literal.GetSize(), 4u);
      for (size_t i = 0, size = literal.GetSize(); i != size; ++i) {
        buffer_.Emit<uint8_t>(literal.GetData()[i]);
      }
    }
  }
  if (!long_literals_.empty()) {
    // Reserve 4 bytes for potential alignment. If after the branch promotion the 64-bit
    // literals don't end up 8-byte-aligned, they will be moved down 4 bytes.
    Emit(0);  // 0x00000000
    for (Literal& literal : long_literals_) {
      Riscv64Label* label = literal.GetLabel();
      Bind(label);
      AssemblerBuffer::EnsureCapacity ensured(&buffer_);
      DCHECK_EQ(literal.GetSize(), 8u);
      for (size_t i = 0, size = literal.GetSize(); i != size; ++i) {
        buffer_.Emit<uint8_t>(literal.GetData()[i]);
      }
    }
  }
}

void Riscv64Assembler::PromoteBranches() {
  // Promote short branches to long as necessary.
  bool changed;
  do {
    changed = false;
    for (auto& branch : branches_) {
      CHECK(branch.IsResolved());
      uint32_t delta = branch.PromoteIfNeeded();
      // If this branch has been promoted and needs to expand in size,
      // relocate all branches by the expansion size.
      if (delta) {
        changed = true;
        uint32_t expand_location = branch.GetLocation();
        for (auto& branch2 : branches_) {
          branch2.Relocate(expand_location, delta);
        }
      }
    }
  } while (changed);

  // Account for branch expansion by resizing the code buffer
  // and moving the code in it to its final location.
  size_t branch_count = branches_.size();
  if (branch_count > 0) {
    // Resize.
    Branch& last_branch = branches_[branch_count - 1];
    uint32_t size_delta = last_branch.GetEndLocation() - last_branch.GetOldEndLocation();
    uint32_t old_size = buffer_.Size();
    buffer_.Resize(old_size + size_delta);
    // Move the code residing between branch placeholders.
    uint32_t end = old_size;
    for (size_t i = branch_count; i > 0;) {
      Branch& branch = branches_[--i];
      uint32_t size = end - branch.GetOldEndLocation();
      buffer_.Move(branch.GetEndLocation(), branch.GetOldEndLocation(), size);
      end = branch.GetOldLocation();
    }
  }

  // Align 64-bit literals by moving them down by 4 bytes.
  // This will reduce the PC-relative distance, which should be safe for both near and far literals.
  if (!long_literals_.empty()) {
    uint32_t first_literal_location = GetLabelLocation(long_literals_.front().GetLabel());
    size_t lit_size = long_literals_.size() * sizeof(uint64_t);
    size_t buf_size = buffer_.Size();
    // 64-bit literals must be at the very end of the buffer.
    CHECK_EQ(first_literal_location + lit_size, buf_size);
    if (!IsAligned<sizeof(uint64_t)>(first_literal_location)) {
      buffer_.Move(first_literal_location - sizeof(uint32_t), first_literal_location, lit_size);
      // The 4 reserved bytes proved useless, reduce the buffer size.
      buffer_.Resize(buf_size - sizeof(uint32_t));
      // Reduce target addresses in literal and address loads by 4 bytes in order for correct
      // offsets from PC to be generated.
      for (auto& branch : branches_) {
        uint32_t target = branch.GetTarget();
        if (target >= first_literal_location) {
          branch.Resolve(target - sizeof(uint32_t));
        }
      }
      // If after this we ever call GetLabelLocation() to get the location of a 64-bit literal,
      // we need to adjust the location of the literal's label as well.
      for (Literal& literal : long_literals_) {
        // Bound label's position is negative, hence incrementing it instead of decrementing.
        literal.GetLabel()->position_ += sizeof(uint32_t);
      }
    }
  }
}

// Note: all kind of BranchInfo which has kOffsetNeedtoCheck type need to be checked later.
// Note: make sure branch_info_[] and EmitBranch() are kept synchronized.
const Riscv64Assembler::Branch::BranchInfo Riscv64Assembler::Branch::branch_info_[] = {
    // short branches (can be promoted to long).
    {1, 0, 0, Riscv64Assembler::Branch::kOffset21, 1},  // kUncondBranch
    {1, 0, 0, Riscv64Assembler::Branch::kOffset13, 1},  // kCondBranch
    {1, 0, 0, Riscv64Assembler::Branch::kOffset21, 1},  // kCall

    // Near label.
    {2, 0, 0, Riscv64Assembler::Branch::kOffset32, 0},  // kLabel
    // Near literals.
    {2, 0, 0, Riscv64Assembler::Branch::kOffset32, 0},  // kLiteral
    {2, 0, 0, Riscv64Assembler::Branch::kOffset32, 0},  // kLiteralUnsigned
    {2, 0, 0, Riscv64Assembler::Branch::kOffset32, 0},  // kLiteralLong
    // Long branches.
    {2, 0, 0, Riscv64Assembler::Branch::kOffset32, 0},  // kLongUncondBranch
    {3, 1, 0, Riscv64Assembler::Branch::kOffset32, 0},  // kLongCondBranch
    {2, 0, 0, Riscv64Assembler::Branch::kOffset32, 0},  // kLongCall
};

// Note: make sure branch_info_[] and EmitBranch() are kept synchronized.
void Riscv64Assembler::EmitBranch(Riscv64Assembler::Branch* branch) {
  CHECK(overwriting_);
  overwrite_location_ = branch->GetLocation();
  uint32_t offset = branch->GetOffset();
  BranchCondition condition = branch->GetCondition();
  XRegister lhs = branch->GetLeftRegister();
  XRegister rhs = branch->GetRightRegister();
  switch (branch->GetType()) {
    // Short branches.
    case Branch::kUncondBranch:
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      // using pseudo instruction j
      J(offset);
      break;
    case Branch::kCondBranch:
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      EmitBcond(condition, lhs, rhs, offset);
      break;
    case Branch::kCall:
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      // using pseudo JAL jump to offset with saving LR
      Jal(offset);
      break;
    // Near label.
    case Branch::kLabel:
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      offset += (offset & 0x800) << 1;  // Account for sign extension in addi.
      Auipc(AT, High20Bits(offset));
      Addi(lhs, AT, SignExtend64<kIImm12Bits>(offset));
      break;
    // Near literals.
    case Branch::kLiteral:
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      offset += (offset & 0x800) << 1;  // Account for sign extension in lw.
      Auipc(AT, High20Bits(offset));
      Lw(lhs, AT, SignExtend64<kIImm12Bits>(offset));
      break;
    case Branch::kLiteralUnsigned:
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      offset += (offset & 0x800) << 1;  // Account for sign extension in lwu.
      Auipc(AT, High20Bits(offset));
      Lwu(lhs, AT, SignExtend64<kIImm12Bits>(offset));
      break;
    case Branch::kLiteralLong:
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      offset += (offset & 0x800) << 1;  // Account for sign extension in ld.
      Auipc(AT, High20Bits(offset));
      Ld(lhs, AT, SignExtend64<kIImm12Bits>(offset));
      break;
    // Long branches.
    case Branch::kLongUncondBranch:
      offset += (offset & 0x800) << 1;  // Account for sign extension in jalr.
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Auipc(AT, High20Bits(offset));
      Jalr(Zero, AT, SignExtend64<kIImm12Bits>(offset));
      break;
    case Branch::kLongCondBranch:
      // input parameter 4 in EmitBcond instruction is to skip the following auipc and jalr
      // instruction if the condition is not fulfill
      offset += (offset & 0x800) << 1;  // Account for sign extension in jalr.
      EmitBcond(Branch::OppositeCondition(condition), lhs, rhs, 6);
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Auipc(AT, High20Bits(offset));
      Jalr(Zero, AT, SignExtend64<kIImm12Bits>(offset));
      break;
    case Branch::kLongCall:
      // using JALR jump to offset with saving LR
      offset += (offset & 0x800) << 1;  // Account for sign extension in jalr.
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Auipc(AT, High20Bits(offset));
      Jalr(lhs, AT, SignExtend64<kIImm12Bits>(offset));
      break;
  }
  CHECK_EQ(overwrite_location_, branch->GetEndLocation());
  CHECK_LT(branch->GetSize(), static_cast<uint32_t>(Branch::kMaxBranchSize));
}

void Riscv64Assembler::J(Riscv64Label* label) { Buncond(label); }

void Riscv64Assembler::Call(XRegister rd, Riscv64Label* label) { BuncondCall(rd, label); }

void Riscv64Assembler::Call(Riscv64Label* label) { BuncondCall(RA, label); }

void Riscv64Assembler::Beq(XRegister rs1, XRegister rs2, Riscv64Label* label) {
  Bcond(label, kCondEQ, rs1, rs2);
}

void Riscv64Assembler::Bne(XRegister rs1, XRegister rs2, Riscv64Label* label) {
  Bcond(label, kCondNE, rs1, rs2);
}

void Riscv64Assembler::Blt(XRegister rs1, XRegister rs2, Riscv64Label* label) {
  Bcond(label, kCondLT, rs1, rs2);
}

void Riscv64Assembler::Bge(XRegister rs1, XRegister rs2, Riscv64Label* label) {
  Bcond(label, kCondGE, rs1, rs2);
}

void Riscv64Assembler::Bltu(XRegister rs1, XRegister rs2, Riscv64Label* label) {
  Bcond(label, kCondLTU, rs1, rs2);
}

void Riscv64Assembler::Bgeu(XRegister rs1, XRegister rs2, Riscv64Label* label) {
  Bcond(label, kCondGEU, rs1, rs2);
}

void Riscv64Assembler::Bgt(XRegister rs1, XRegister rs2, Riscv64Label* label) {
  Bcond(label, kCondGT, rs1, rs2);
}

void Riscv64Assembler::Ble(XRegister rs1, XRegister rs2, Riscv64Label* label) {
  Bcond(label, kCondLE, rs1, rs2);
}

void Riscv64Assembler::Bgtu(XRegister rs1, XRegister rs2, Riscv64Label* label) {
  Bcond(label, kCondGTU, rs1, rs2);
}

void Riscv64Assembler::Bleu(XRegister rs1, XRegister rs2, Riscv64Label* label) {
  Bcond(label, kCondLEU, rs1, rs2);
}

void Riscv64Assembler::Blez(XRegister rs1, Riscv64Label* label) { Bcond(label, kCondLEZ, rs1); }

void Riscv64Assembler::Bgez(XRegister rs1, Riscv64Label* label) { Bcond(label, kCondGEZ, rs1); }

void Riscv64Assembler::Bltz(XRegister rs1, Riscv64Label* label) { Bcond(label, kCondLTZ, rs1); }

void Riscv64Assembler::Bgtz(XRegister rs1, Riscv64Label* label) { Bcond(label, kCondGTZ, rs1); }

void Riscv64Assembler::Beqz(XRegister rs1, Riscv64Label* label) { Bcond(label, kCondEQZ, rs1); }

void Riscv64Assembler::Bnez(XRegister rs1, Riscv64Label* label) { Bcond(label, kCondNEZ, rs1); }

void Riscv64Assembler::Addiw32(XRegister rd, XRegister rs1, int32_t value) {
  CHECK_NE(rs1, AT);
  if (IsInt<12>(value)) {
    Addiw(rd, rs1, value);
  } else {
    Li(AT, value);
    Addw(rd, rs1, AT);
  }
}

void Riscv64Assembler::Addi64(XRegister rd, XRegister rs1, int64_t value) {
  CHECK_NE(rs1, AT);
  if (IsInt<12>(value)) {
    Addi(rd, rs1, value);
  } else {
    Li(AT, value);
    Add(rd, rs1, AT);
  }
}

void Riscv64Assembler::AdjustBaseAndOffset(XRegister& base, int32_t& offset, bool is_doubleword) {
  // This method is used to adjust the base register and offset pair
  // for a load/store when the offset doesn't fit into int12_t.
  // It is assumed that `base + offset` is sufficiently aligned for memory
  // operands that are machine word in size or smaller. For doubleword-sized
  // operands it's assumed that `base` is a multiple of 8, while `offset`
  // may be a multiple of 4 (e.g. 4-byte-aligned long and double arguments
  // and spilled variables on the stack accessed relative to the stack
  // pointer register).
  // We preserve the "alignment" of `offset` by adjusting it by a multiple of 8.
  CHECK_NE(base, AT);  // Must not overwrite the register `base` while loading `offset`.

  // Must not overwrite the register `base` while loading `offset`.
  bool doubleword_aligned = IsAligned<kRiscv64DoublewordSize>(offset);
  bool two_accesses = is_doubleword && !doubleword_aligned;

  // IsInt<12> must be passed a signed value, hence the static cast below.
  if (IsInt<12>(offset) &&
      (!two_accesses || IsInt<12>(static_cast<int32_t>(offset + kRiscv64WordSize)))) {
    // Nothing to do: `offset` (and, if needed, `offset + 4`) fits into 12-bit int.
    return;
  }

  // Remember the "(mis)alignment" of `offset`, it will be checked at the end.
  uint32_t misalignment = offset & (kRiscv64DoublewordSize - 1);

  // if the Offset is less than a 2 * 12-bit value, the "base" will be adjust by
  // kMinOffsetForSimpleAdjustment in other cases, the "base" will be adjust by the upper 20-bit
  // value
  constexpr int32_t kMinOffsetForSimpleAdjustment =
      0x7f8;  // Max 12-bit value that's a multiple of 8.
  constexpr int32_t kMaxOffsetForSimpleAdjustment = 2 * kMinOffsetForSimpleAdjustment;  // 0xFF0

  if (0 <= offset && offset <= kMaxOffsetForSimpleAdjustment) {
    Addi(AT, base, kMinOffsetForSimpleAdjustment);
    offset -= kMinOffsetForSimpleAdjustment;
  } else if (-kMaxOffsetForSimpleAdjustment <= offset && offset < 0) {
    Addi(AT, base, -kMinOffsetForSimpleAdjustment);
    offset += kMinOffsetForSimpleAdjustment;
  } else {
    // In more complex cases using Lui and Add...
    int32_t offLow12Ext32 = SignExtend32<12>(offset);
    uint32_t offHigh20 = High20Bits(offset);
    bool incrementHigh20 = offLow12Ext32 < 0;
    bool overflowHigh20 = false;

    if (incrementHigh20) {
      offHigh20++;
      overflowHigh20 = (offHigh20 == 0x80000);
    }

    Lui(AT, offHigh20);
    if (overflowHigh20) {
      // using logic shift to resolve the overflow issue (When `offset` is close to +2GB.)
      Slli(AT, AT, 32);
      Srli(AT, AT, 32);
    }

    Add(AT, base, AT);
    if (two_accesses && !IsInt<12>(static_cast<int32_t>(offLow12Ext32 + kRiscv64WordSize))) {
      // Avoid overflow in the 12-bit offset of the load/store instruction when adding 4.
      Addi(AT, AT, kRiscv64DoublewordSize);
      offLow12Ext32 -= kRiscv64DoublewordSize;
    }
    offset = offLow12Ext32;
  }
  base = AT;

  CHECK(IsInt<12>(offset));
  if (two_accesses) {
    CHECK(IsInt<12>(static_cast<int32_t>(offset + kRiscv64WordSize)));
  }
  CHECK_EQ(misalignment, offset & (kRiscv64DoublewordSize - 1));
}

void Riscv64Assembler::LoadFromOffset(LoadOperandType type,
                                      XRegister reg,
                                      XRegister base,
                                      int32_t offset) {
  LoadFromOffset<>(type, reg, base, offset);
}

constexpr size_t kFramePointerSize = 8;

void Riscv64Assembler::IncreaseFrameSize(size_t adjust) {
  CHECK_ALIGNED(adjust, kFramePointerSize);
  DCHECK(!overwriting_);
  Addi64(SP, SP, static_cast<int32_t>(-adjust));
  cfi_.AdjustCFAOffset(adjust);
}

void Riscv64Assembler::DecreaseFrameSize(size_t adjust) {
  CHECK_ALIGNED(adjust, kFramePointerSize);
  DCHECK(!overwriting_);
  Addi64(SP, SP, static_cast<int32_t>(adjust));
  cfi_.AdjustCFAOffset(-adjust);
}

void Riscv64Assembler::LoadFpuFromOffset(LoadOperandType type,
                                         FRegister reg,
                                         XRegister base,
                                         int32_t offset) {
  LoadFpuFromOffset<>(type, reg, base, offset);
}

void Riscv64Assembler::StoreToOffset(StoreOperandType type,
                                     XRegister reg,
                                     XRegister base,
                                     int32_t offset) {
  StoreToOffset<>(type, reg, base, offset);
}

void Riscv64Assembler::StoreFpuToOffset(StoreOperandType type,
                                        FRegister reg,
                                        XRegister base,
                                        int32_t offset) {
  StoreFpuToOffset<>(type, reg, base, offset);
}

#if 0
void Riscv64Assembler::CallFromThread(ThreadOffset64 offset ATTRIBUTE_UNUSED,
                                     ManagedRegister mscratch ATTRIBUTE_UNUSED) {
  UNIMPLEMENTED(FATAL) << "No RISCV64 implementation";
}

void Riscv64Assembler::GetCurrentThread(ManagedRegister tr) {
  NIMPLEMENTED(FATAL) << "No RISCV64 implementation";
}

void Riscv64Assembler::GetCurrentThread(FrameOffset offset,
                                       ManagedRegister mscratch ATTRIBUTE_UNUSED) {
  NIMPLEMENTED(FATAL) << "No RISCV64 implementation";
  // StoreToOffset(kStoreDoubleword, S1, SP, offset.Int32Value());
}

#endif

}  // namespace riscv64
}  // namespace art
