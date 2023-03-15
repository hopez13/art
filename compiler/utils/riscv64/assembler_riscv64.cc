/*
 * Copyright (C) 2014 The Android Open Source Project
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

namespace art {
namespace riscv64 {

static_assert(static_cast<size_t>(kRiscv64PointerSize) == kRiscv64DoublewordSize,
              "Unexpected Riscv64 pointer size.");
static_assert(kRiscv64PointerSize == PointerSize::k64, "Unexpected Riscv64 pointer size.");

void Riscv64Assembler::FinalizeCode() {
  for (auto& exception_block : exception_blocks_) {
    EmitExceptionPoll(&exception_block);
  }
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

void Riscv64Assembler::EmitI6(
    uint16_t funct6, uint16_t imm6, XRegister rs1, int funct3, XRegister rd, int opcode) {
  uint32_t encoding = static_cast<uint32_t>(funct6) << 26 |
                      (static_cast<uint32_t>(imm6) & 0x3F) << 20 |
                      static_cast<uint32_t>(rs1) << 15 | static_cast<uint32_t>(funct3) << 12 |
                      static_cast<uint32_t>(rd) << 7 | opcode;
  Emit(encoding);
}

void Riscv64Assembler::EmitB(uint16_t imm, XRegister rs2, XRegister rs1, int funct3, int opcode) {
  CHECK(IsUint<13>(imm)) << imm;
  uint32_t encoding = (static_cast<uint32_t>(imm) & 0x1000) >> 12 << 31 |
                      (static_cast<uint32_t>(imm) & 0x07E0) >> 5 << 25 |
                      static_cast<uint32_t>(rs2) << 20 | static_cast<uint32_t>(rs1) << 15 |
                      static_cast<uint32_t>(funct3) << 12 |
                      (static_cast<uint32_t>(imm) & 0x1E) >> 1 << 8 |
                      (static_cast<uint32_t>(imm) & 0x0800) >> 11 << 7 | opcode;
  Emit(encoding);
}

void Riscv64Assembler::EmitU(uint32_t imm, XRegister rd, int opcode) {
  uint32_t encoding = static_cast<uint32_t>(imm) << 12 | static_cast<uint32_t>(rd) << 7 | opcode;
  Emit(encoding);
}

void Riscv64Assembler::EmitJ(uint32_t imm20, XRegister rd, int opcode) {
  CHECK(IsUint<21>(imm20)) << imm20;
  // RV JAL: J-Imm = (offset x 2), encode (imm20>>1) into instruction.
  uint32_t encoding = (static_cast<uint32_t>(imm20) & 0x100000) >> 20 << 31 |
                      (static_cast<uint32_t>(imm20) & 0x07FE) >> 1 << 21 |
                      (static_cast<uint32_t>(imm20) & 0x800) >> 11 << 20 |
                      (static_cast<uint32_t>(imm20) & 0xFF000) >> 12 << 12 |
                      static_cast<uint32_t>(rd) << 7 | opcode;
  Emit(encoding);
}

/////////////////////////////// RV64 VARIANTS extension ///////////////////////////////

/////////////////////////////// RV64 "IM" Instructions ///////////////////////////////
// Load instructions: opcode = 0x03, subfunc(func3) from 0x0 ~ 0x6
void Riscv64Assembler::Lb(XRegister rd, XRegister rs1, uint16_t offset) {
  EmitI(offset, rs1, 0x0, rd, 0x03);
}

void Riscv64Assembler::Lh(XRegister rd, XRegister rs1, uint16_t offset) {
  EmitI(offset, rs1, 0x1, rd, 0x03);
}

void Riscv64Assembler::Lw(XRegister rd, XRegister rs1, uint16_t offset) {
  EmitI(offset, rs1, 0x2, rd, 0x03);
}

void Riscv64Assembler::Ld(XRegister rd, XRegister rs1, uint16_t offset) {
  EmitI(offset, rs1, 0x3, rd, 0x03);
}

void Riscv64Assembler::Lbu(XRegister rd, XRegister rs1, uint16_t offset) {
  EmitI(offset, rs1, 0x4, rd, 0x03);
}

void Riscv64Assembler::Lhu(XRegister rd, XRegister rs1, uint16_t offset) {
  EmitI(offset, rs1, 0x5, rd, 0x03);
}

void Riscv64Assembler::Lwu(XRegister rd, XRegister rs1, uint16_t offset) {
  EmitI(offset, rs1, 0x6, rd, 0x3);
}

// Store instructions: opcode = 0x23, subfunc(func3) from 0x0 ~ 0x3
void Riscv64Assembler::Sb(XRegister rs2, XRegister rs1, uint16_t offset) {
  EmitS(offset, rs2, rs1, 0x0, 0x23);
}

void Riscv64Assembler::Sh(XRegister rs2, XRegister rs1, uint16_t offset) {
  EmitS(offset, rs2, rs1, 0x1, 0x23);
}

void Riscv64Assembler::Sw(XRegister rs2, XRegister rs1, uint16_t offset) {
  EmitS(offset, rs2, rs1, 0x2, 0x23);
}

void Riscv64Assembler::Sd(XRegister rs2, XRegister rs1, uint16_t offset) {
  EmitS(offset, rs2, rs1, 0x3, 0x23);
}

// IMM ALU instructions: opcode = 0x13, subfunc(func3) from 0x0 ~ 0x7
void Riscv64Assembler::Addi(XRegister rd, XRegister rs1, uint16_t offset) {
  EmitI(offset, rs1, 0x0, rd, 0x13);
}

// 0x1 Split: 0x0(6b) + offset(6b)
void Riscv64Assembler::Slli(XRegister rd, XRegister rs1, uint16_t offset) {
#if defined(ART_TARGET_ANDROID)
  if ((rd != rs1) || (offset != 0))
#endif
    EmitI6(0x0, offset, rs1, 0x1, rd, 0x13);
}

void Riscv64Assembler::Slti(XRegister rd, XRegister rs1, uint16_t offset) {
  EmitI(offset, rs1, 0x2, rd, 0x13);
}

void Riscv64Assembler::Sltiu(XRegister rd, XRegister rs1, uint16_t offset) {
  EmitI(offset, rs1, 0x3, rd, 0x13);
}

void Riscv64Assembler::Xori(XRegister rd, XRegister rs1, uint16_t offset) {
  EmitI(offset, rs1, 0x4, rd, 0x13);
}

// 0x5 Split: 0x0(6b) + offset(6b)
void Riscv64Assembler::Srli(XRegister rd, XRegister rs1, uint16_t offset) {
#if defined(ART_TARGET_ANDROID)
  if ((rd != rs1) || (offset != 0))
#endif
    EmitI6(0x0, offset, rs1, 0x5, rd, 0x13);
}

void Riscv64Assembler::Srai(XRegister rd, XRegister rs1, uint16_t offset) {
  EmitI6(0x10, offset, rs1, 0x5, rd, 0x13);
}

void Riscv64Assembler::Ori(XRegister rd, XRegister rs1, uint16_t offset) {
  EmitI(offset, rs1, 0x6, rd, 0x13);
}

void Riscv64Assembler::Andi(XRegister rd, XRegister rs1, uint16_t offset) {
  EmitI(offset, rs1, 0x7, rd, 0x13);
}

// ALU instructions: opcode = 0x33, subfunc(func3) from 0x0 ~ 0x7
void Riscv64Assembler::Add(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x0, rs2, rs1, 0x0, rd, 0x33);
}

void Riscv64Assembler::Sll(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x0, rs2, rs1, 0x01, rd, 0x33);
}

void Riscv64Assembler::Slt(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x0, rs2, rs1, 0x02, rd, 0x33);
}

void Riscv64Assembler::Sltu(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x0, rs2, rs1, 0x03, rd, 0x33);
}

void Riscv64Assembler::Xor(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x0, rs2, rs1, 0x04, rd, 0x33);
}

void Riscv64Assembler::Srl(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x0, rs2, rs1, 0x05, rd, 0x33);
}

void Riscv64Assembler::Or(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x0, rs2, rs1, 0x06, rd, 0x33);
}

void Riscv64Assembler::And(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x0, rs2, rs1, 0x07, rd, 0x33);
}

void Riscv64Assembler::Mul(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x1, rs2, rs1, 0x0, rd, 0x33);
}

void Riscv64Assembler::Mulh(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x1, rs2, rs1, 0x1, rd, 0x33);
}

void Riscv64Assembler::Mulhsu(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x1, rs2, rs1, 0x2, rd, 0x33);
}

void Riscv64Assembler::Mulhu(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x1, rs2, rs1, 0x3, rd, 0x33);
}

void Riscv64Assembler::Div(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x1, rs2, rs1, 0x4, rd, 0x33);
}

void Riscv64Assembler::Divu(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x1, rs2, rs1, 0x5, rd, 0x33);
}

void Riscv64Assembler::Rem(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x1, rs2, rs1, 0x6, rd, 0x33);
}

void Riscv64Assembler::Remu(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x1, rs2, rs1, 0x7, rd, 0x33);
}

void Riscv64Assembler::Sub(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x20, rs2, rs1, 0x0, rd, 0x33);
}

void Riscv64Assembler::Sra(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x20, rs2, rs1, 0x05, rd, 0x33);
}

// 32bit Imm ALU instructions: opcode = 0x1b, subfunc(func3) - 0x0, 0x1, 0x5
void Riscv64Assembler::Addiw(XRegister rd, XRegister rs1, int16_t imm12) {
  EmitI(imm12, rs1, 0x0, rd, 0x1b);
}

void Riscv64Assembler::Slliw(XRegister rd, XRegister rs1, int16_t shamt) {
  CHECK(static_cast<uint16_t>(shamt) < 32) << shamt;
  EmitR(0x0, shamt, rs1, 0x1, rd, 0x1b);
}

void Riscv64Assembler::Srliw(XRegister rd, XRegister rs1, int16_t shamt) {
  CHECK(static_cast<uint16_t>(shamt) < 32) << shamt;
  EmitR(0x0, shamt, rs1, 0x5, rd, 0x1b);
}

void Riscv64Assembler::Sraiw(XRegister rd, XRegister rs1, int16_t shamt) {
  CHECK(static_cast<uint16_t>(shamt) < 32) << shamt;
  EmitR(0x20, shamt, rs1, 0x5, rd, 0x1b);
}

// 32bit ALU instructions: opcode = 0x3b, subfunc(func3) from 0x0 ~ 0x7
void Riscv64Assembler::Addw(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x0, rs2, rs1, 0x0, rd, 0x3b);
}

void Riscv64Assembler::Mulw(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x1, rs2, rs1, 0x0, rd, 0x3b);
}

void Riscv64Assembler::Subw(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x20, rs2, rs1, 0x0, rd, 0x3b);
}

void Riscv64Assembler::Sllw(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x0, rs2, rs1, 0x1, rd, 0x3b);
}

void Riscv64Assembler::Divw(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x1, rs2, rs1, 0x4, rd, 0x3b);
}

void Riscv64Assembler::Srlw(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x0, rs2, rs1, 0x5, rd, 0x3b);
}

void Riscv64Assembler::Divuw(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x1, rs2, rs1, 0x5, rd, 0x3b);
}

void Riscv64Assembler::Sraw(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x20, rs2, rs1, 0x5, rd, 0x3b);
}

void Riscv64Assembler::Remw(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x1, rs2, rs1, 0x6, rd, 0x3b);
}

void Riscv64Assembler::Remuw(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x1, rs2, rs1, 0x7, rd, 0x3b);
}

// opcode = 0x17 & 0x37
void Riscv64Assembler::Auipc(XRegister rd, uint32_t imm20) { EmitU(imm20, rd, 0x17); }

void Riscv64Assembler::Lui(XRegister rd, uint32_t imm20) { EmitU(imm20, rd, 0x37); }

// Branch and Jump instructions, opcode = 0x63 (subfunc from 0x0 ~ 0x7), 0x67, 0x6f
void Riscv64Assembler::Beq(XRegister rs1, XRegister rs2, uint16_t offset) {
  EmitB(offset, rs2, rs1, 0x0, 0x63);
}

void Riscv64Assembler::Bne(XRegister rs1, XRegister rs2, uint16_t offset) {
  EmitB(offset, rs2, rs1, 0x1, 0x63);
}

void Riscv64Assembler::Blt(XRegister rs1, XRegister rs2, uint16_t offset) {
  EmitB(offset, rs2, rs1, 0x4, 0x63);
}

void Riscv64Assembler::Bge(XRegister rs1, XRegister rs2, uint16_t offset) {
  EmitB(offset, rs2, rs1, 0x5, 0x63);
}

void Riscv64Assembler::Bltu(XRegister rs1, XRegister rs2, uint16_t offset) {
  EmitB(offset, rs2, rs1, 0x6, 0x63);
}

void Riscv64Assembler::Bgeu(XRegister rs1, XRegister rs2, uint16_t offset) {
  EmitB(offset, rs2, rs1, 0x7, 0x63);
}

void Riscv64Assembler::Jalr(XRegister rd, XRegister rs1, uint16_t offset) {
  EmitI(offset, rs1, 0x0, rd, 0x67);
}

void Riscv64Assembler::Jal(XRegister rd, uint32_t imm20) { EmitJ(imm20, rd, 0x6F); }

// opcode - 0xf 0xf and 0x73
void Riscv64Assembler::Fence(uint8_t pred, uint8_t succ) {
  EmitI(0x0 << 8 | pred << 4 | succ, 0x0, 0x0, 0x0, 0xf);
}

void Riscv64Assembler::FenceI() { EmitI(0x0 << 6 | 0x0 << 4 | 0x0, 0x0, 0x1, 0x0, 0xf); }

void Riscv64Assembler::Ecall() { EmitI(0x0, 0x0, 0x0, 0x0, 0x73); }

void Riscv64Assembler::Ebreak() { EmitI(0x1, 0x0, 0x0, 0x0, 0x73); }

void Riscv64Assembler::Csrrw(XRegister rd, XRegister rs1, uint16_t csr) {
  EmitI(csr, rs1, 0x1, rd, 0x73);
}

void Riscv64Assembler::Csrrs(XRegister rd, XRegister rs1, uint16_t csr) {
  EmitI(csr, rs1, 0x2, rd, 0x73);
}

void Riscv64Assembler::Csrrc(XRegister rd, XRegister rs1, uint16_t csr) {
  EmitI(csr, rs1, 0x3, rd, 0x73);
}

void Riscv64Assembler::Csrrwi(XRegister rd, uint16_t csr, uint8_t zimm) {
  EmitI(csr, zimm, 0x5, rd, 0x73);
}

void Riscv64Assembler::Csrrsi(XRegister rd, uint16_t csr, uint8_t zimm) {
  EmitI(csr, zimm, 0x6, rd, 0x73);
}

void Riscv64Assembler::Csrrci(XRegister rd, uint16_t csr, uint8_t zimm) {
  EmitI(csr, zimm, 0x7, rd, 0x73);
}
/////////////////////////////// RV64 "IM" Instructions  END ///////////////////////////////

/////////////////////////////// RV64 "A" Instructions  START ///////////////////////////////
void Riscv64Assembler::LrW(XRegister rd, XRegister rs1, uint8_t aqrl) {
  EmitR4(0x2, aqrl, 0x0, rs1, 0x2, rd, 0x2f);
}

void Riscv64Assembler::ScW(XRegister rd, XRegister rs2, XRegister rs1, uint8_t aqrl) {
  EmitR4(0x3, aqrl, rs2, rs1, 0x2, rd, 0x2f);
}

void Riscv64Assembler::AmoSwapW(XRegister rd, XRegister rs2, XRegister rs1, uint8_t aqrl) {
  EmitR4(0x1, aqrl, rs2, rs1, 0x2, rd, 0x2f);
}

void Riscv64Assembler::AmoAddW(XRegister rd, XRegister rs2, XRegister rs1, uint8_t aqrl) {
  EmitR4(0x0, aqrl, rs2, rs1, 0x2, rd, 0x2f);
}

void Riscv64Assembler::AmoXorW(XRegister rd, XRegister rs2, XRegister rs1, uint8_t aqrl) {
  EmitR4(0x4, aqrl, rs2, rs1, 0x2, rd, 0x2f);
}

void Riscv64Assembler::AmoAndW(XRegister rd, XRegister rs2, XRegister rs1, uint8_t aqrl) {
  EmitR4(0xc, aqrl, rs2, rs1, 0x2, rd, 0x2f);
}

void Riscv64Assembler::AmoOrW(XRegister rd, XRegister rs2, XRegister rs1, uint8_t aqrl) {
  EmitR4(0x8, aqrl, rs2, rs1, 0x2, rd, 0x2f);
}

void Riscv64Assembler::AmoMinW(XRegister rd, XRegister rs2, XRegister rs1, uint8_t aqrl) {
  EmitR4(0x10, aqrl, rs2, rs1, 0x2, rd, 0x2f);
}

void Riscv64Assembler::AmoMaxW(XRegister rd, XRegister rs2, XRegister rs1, uint8_t aqrl) {
  EmitR4(0x14, aqrl, rs2, rs1, 0x2, rd, 0x2f);
}

void Riscv64Assembler::AmoMinuW(XRegister rd, XRegister rs2, XRegister rs1, uint8_t aqrl) {
  EmitR4(0x18, aqrl, rs2, rs1, 0x2, rd, 0x2f);
}

void Riscv64Assembler::AmoMaxuW(XRegister rd, XRegister rs2, XRegister rs1, uint8_t aqrl) {
  EmitR4(0x1c, aqrl, rs2, rs1, 0x2, rd, 0x2f);
}

void Riscv64Assembler::LrD(XRegister rd, XRegister rs1, uint8_t aqrl) {
  EmitR4(0x2, aqrl, 0x0, rs1, 0x3, rd, 0x2f);
}

void Riscv64Assembler::ScD(XRegister rd, XRegister rs2, XRegister rs1, uint8_t aqrl) {
  EmitR4(0x3, aqrl, rs2, rs1, 0x3, rd, 0x2f);
}

void Riscv64Assembler::AmoSwapD(XRegister rd, XRegister rs2, XRegister rs1, uint8_t aqrl) {
  EmitR4(0x1, aqrl, rs2, rs1, 0x3, rd, 0x2f);
}

void Riscv64Assembler::AmoAddD(XRegister rd, XRegister rs2, XRegister rs1, uint8_t aqrl) {
  EmitR4(0x0, aqrl, rs2, rs1, 0x3, rd, 0x2f);
}

void Riscv64Assembler::AmoXorD(XRegister rd, XRegister rs2, XRegister rs1, uint8_t aqrl) {
  EmitR4(0x4, aqrl, rs2, rs1, 0x3, rd, 0x2f);
}

void Riscv64Assembler::AmoAndD(XRegister rd, XRegister rs2, XRegister rs1, uint8_t aqrl) {
  EmitR4(0xc, aqrl, rs2, rs1, 0x3, rd, 0x2f);
}

void Riscv64Assembler::AmoOrD(XRegister rd, XRegister rs2, XRegister rs1, uint8_t aqrl) {
  EmitR4(0x8, aqrl, rs2, rs1, 0x3, rd, 0x2f);
}

void Riscv64Assembler::AmoMinD(XRegister rd, XRegister rs2, XRegister rs1, uint8_t aqrl) {
  EmitR4(0x10, aqrl, rs2, rs1, 0x3, rd, 0x2f);
}

void Riscv64Assembler::AmoMaxD(XRegister rd, XRegister rs2, XRegister rs1, uint8_t aqrl) {
  EmitR4(0x14, aqrl, rs2, rs1, 0x3, rd, 0x2f);
}

void Riscv64Assembler::AmoMinuD(XRegister rd, XRegister rs2, XRegister rs1, uint8_t aqrl) {
  EmitR4(0x18, aqrl, rs2, rs1, 0x3, rd, 0x2f);
}

void Riscv64Assembler::AmoMaxuD(XRegister rd, XRegister rs2, XRegister rs1, uint8_t aqrl) {
  EmitR4(0x1c, aqrl, rs2, rs1, 0x3, rd, 0x2f);
}
/////////////////////////////// RV64 "A" Instructions  END ///////////////////////////////

/////////////////////////////// RV64 "FD" Instructions  START ///////////////////////////////
void Riscv64Assembler::FLw(FRegister rd, XRegister rs1, uint16_t offset) {
  EmitI(offset, rs1, 0x2, rd, 0x07);
}

void Riscv64Assembler::FLd(FRegister rd, XRegister rs1, uint16_t offset) {
  EmitI(offset, rs1, 0x3, rd, 0x07);
}

void Riscv64Assembler::FSw(FRegister rs2, XRegister rs1, uint16_t offset) {
  EmitS(offset, rs2, rs1, 0x2, 0x27);
}

void Riscv64Assembler::FSd(FRegister rs2, XRegister rs1, uint16_t offset) {
  EmitS(offset, rs2, rs1, 0x3, 0x27);
}

void Riscv64Assembler::FMAddS(FRegister rd, FRegister rs1, FRegister rs2, FRegister rs3) {
  EmitR4(rs3, 0x0, rs2, rs1, FRM, rd, 0x43);
}

void Riscv64Assembler::FMAddD(FRegister rd, FRegister rs1, FRegister rs2, FRegister rs3) {
  EmitR4(rs3, 0x1, rs2, rs1, FRM, rd, 0x43);
}

void Riscv64Assembler::FMSubS(FRegister rd, FRegister rs1, FRegister rs2, FRegister rs3) {
  EmitR4(rs3, 0x0, rs2, rs1, FRM, rd, 0x47);
}

void Riscv64Assembler::FMSubD(FRegister rd, FRegister rs1, FRegister rs2, FRegister rs3) {
  EmitR4(rs3, 0x1, rs2, rs1, FRM, rd, 0x47);
}

void Riscv64Assembler::FNMSubS(FRegister rd, FRegister rs1, FRegister rs2, FRegister rs3) {
  EmitR4(rs3, 0x0, rs2, rs1, FRM, rd, 0x4b);
}

void Riscv64Assembler::FNMSubD(FRegister rd, FRegister rs1, FRegister rs2, FRegister rs3) {
  EmitR4(rs3, 0x1, rs2, rs1, FRM, rd, 0x4b);
}

void Riscv64Assembler::FNMAddS(FRegister rd, FRegister rs1, FRegister rs2, FRegister rs3) {
  EmitR4(rs3, 0x0, rs2, rs1, FRM, rd, 0x4f);
}

void Riscv64Assembler::FNMAddD(FRegister rd, FRegister rs1, FRegister rs2, FRegister rs3) {
  EmitR4(rs3, 0x1, rs2, rs1, FRM, rd, 0x4f);
}

// opcode = 0x53, funct7 is even for float ops
void Riscv64Assembler::FAddS(FRegister rd, FRegister rs1, FRegister rs2) {
  EmitR(0x0, rs2, rs1, FRM, rd, 0x53);
}

void Riscv64Assembler::FSubS(FRegister rd, FRegister rs1, FRegister rs2) {
  EmitR(0x4, rs2, rs1, FRM, rd, 0x53);
}

void Riscv64Assembler::FMulS(FRegister rd, FRegister rs1, FRegister rs2) {
  EmitR(0x8, rs2, rs1, FRM, rd, 0x53);
}

void Riscv64Assembler::FDivS(FRegister rd, FRegister rs1, FRegister rs2) {
  EmitR(0xc, rs2, rs1, FRM, rd, 0x53);
}

void Riscv64Assembler::FSgnjS(FRegister rd, FRegister rs1, FRegister rs2) {
  EmitR(0x10, rs2, rs1, 0x0, rd, 0x53);
}

void Riscv64Assembler::FSgnjnS(FRegister rd, FRegister rs1, FRegister rs2) {
  EmitR(0x10, rs2, rs1, 0x1, rd, 0x53);
}

void Riscv64Assembler::FSgnjxS(FRegister rd, FRegister rs1, FRegister rs2) {
  EmitR(0x10, rs2, rs1, 0x2, rd, 0x53);
}

void Riscv64Assembler::FMinS(FRegister rd, FRegister rs1, FRegister rs2) {
  EmitR(0x14, rs2, rs1, 0x0, rd, 0x53);
}

void Riscv64Assembler::FMaxS(FRegister rd, FRegister rs1, FRegister rs2) {
  EmitR(0x14, rs2, rs1, 0x1, rd, 0x53);
}

void Riscv64Assembler::FCvtSD(FRegister rd, FRegister rs1) { EmitR(0x20, 0x1, rs1, FRM, rd, 0x53); }

void Riscv64Assembler::FSqrtS(FRegister rd, FRegister rs1) { EmitR(0x2c, 0x0, rs1, FRM, rd, 0x53); }

void Riscv64Assembler::FEqS(XRegister rd, FRegister rs1, FRegister rs2) {
  EmitR(0x50, rs2, rs1, 0x2, rd, 0x53);
}

void Riscv64Assembler::FLtS(XRegister rd, FRegister rs1, FRegister rs2) {
  EmitR(0x50, rs2, rs1, 0x1, rd, 0x53);
}

void Riscv64Assembler::FLeS(XRegister rd, FRegister rs1, FRegister rs2) {
  EmitR(0x50, rs2, rs1, 0x0, rd, 0x53);
}

void Riscv64Assembler::FCvtWS(XRegister rd, FRegister rs1, FPRoundingMode frm) {
  EmitR(0x60, 0x0, rs1, frm, rd, 0x53);
}

void Riscv64Assembler::FCvtWuS(XRegister rd, FRegister rs1) {
  EmitR(0x60, 0x1, rs1, FRM, rd, 0x53);
}

void Riscv64Assembler::FCvtLS(XRegister rd, FRegister rs1, FPRoundingMode frm) {
  EmitR(0x60, 0x2, rs1, frm, rd, 0x53);
}

void Riscv64Assembler::FCvtLuS(XRegister rd, FRegister rs1) {
  EmitR(0x60, 0x3, rs1, FRM, rd, 0x53);
}

void Riscv64Assembler::FCvtSW(FRegister rd, XRegister rs1) { EmitR(0x68, 0x0, rs1, FRM, rd, 0x53); }

void Riscv64Assembler::FCvtSWu(FRegister rd, XRegister rs1) {
  EmitR(0x68, 0x1, rs1, FRM, rd, 0x53);
}

void Riscv64Assembler::FCvtSL(FRegister rd, XRegister rs1) { EmitR(0x68, 0x2, rs1, FRM, rd, 0x53); }

void Riscv64Assembler::FCvtSLu(FRegister rd, XRegister rs1) {
  EmitR(0x68, 0x3, rs1, FRM, rd, 0x53);
}

void Riscv64Assembler::FMvXW(XRegister rd, FRegister rs1) { EmitR(0x70, 0x0, rs1, 0x0, rd, 0x53); }

void Riscv64Assembler::FClassS(XRegister rd, FRegister rs1) {
  EmitR(0x70, 0x0, rs1, 0x1, rd, 0x53);
}

void Riscv64Assembler::FMvWX(FRegister rd, XRegister rs1) { EmitR(0x78, 0x0, rs1, 0x0, rd, 0x53); }

// opcode = 0x53, funct7 is odd for float ops
void Riscv64Assembler::FAddD(FRegister rd, FRegister rs1, FRegister rs2) {
  EmitR(0x1, rs2, rs1, FRM, rd, 0x53);
}

void Riscv64Assembler::FSubD(FRegister rd, FRegister rs1, FRegister rs2) {
  EmitR(0x5, rs2, rs1, FRM, rd, 0x53);
}

void Riscv64Assembler::FMulD(FRegister rd, FRegister rs1, FRegister rs2) {
  EmitR(0x9, rs2, rs1, FRM, rd, 0x53);
}

void Riscv64Assembler::FDivD(FRegister rd, FRegister rs1, FRegister rs2) {
  EmitR(0xd, rs2, rs1, FRM, rd, 0x53);
}

void Riscv64Assembler::FSgnjD(FRegister rd, FRegister rs1, FRegister rs2) {
  EmitR(0x11, rs2, rs1, 0x0, rd, 0x53);
}

void Riscv64Assembler::FSgnjnD(FRegister rd, FRegister rs1, FRegister rs2) {
  EmitR(0x11, rs2, rs1, 0x1, rd, 0x53);
}

void Riscv64Assembler::FSgnjxD(FRegister rd, FRegister rs1, FRegister rs2) {
  EmitR(0x11, rs2, rs1, 0x2, rd, 0x53);
}

void Riscv64Assembler::FMinD(FRegister rd, FRegister rs1, FRegister rs2) {
  EmitR(0x15, rs2, rs1, 0x0, rd, 0x53);
}

void Riscv64Assembler::FMaxD(FRegister rd, FRegister rs1, FRegister rs2) {
  EmitR(0x15, rs2, rs1, 0x1, rd, 0x53);
}

void Riscv64Assembler::FCvtDS(FRegister rd, FRegister rs1) { EmitR(0x21, 0x0, rs1, 0x0, rd, 0x53); }

void Riscv64Assembler::FSqrtD(FRegister rd, FRegister rs1) { EmitR(0x2d, 0x0, rs1, FRM, rd, 0x53); }

void Riscv64Assembler::FLeD(XRegister rd, FRegister rs1, FRegister rs2) {
  EmitR(0x51, rs2, rs1, 0x0, rd, 0x53);
}

void Riscv64Assembler::FLtD(XRegister rd, FRegister rs1, FRegister rs2) {
  EmitR(0x51, rs2, rs1, 0x1, rd, 0x53);
}

void Riscv64Assembler::FEqD(XRegister rd, FRegister rs1, FRegister rs2) {
  EmitR(0x51, rs2, rs1, 0x2, rd, 0x53);
}

void Riscv64Assembler::FCvtWD(XRegister rd, FRegister rs1, FPRoundingMode frm) {
  EmitR(0x61, 0x0, rs1, frm, rd, 0x53);
}

void Riscv64Assembler::FCvtWuD(XRegister rd, FRegister rs1) {
  EmitR(0x61, 0x1, rs1, FRM, rd, 0x53);
}

void Riscv64Assembler::FCvtLD(XRegister rd, FRegister rs1, FPRoundingMode frm) {
  EmitR(0x61, 0x2, rs1, frm, rd, 0x53);
}

void Riscv64Assembler::FCvtLuD(XRegister rd, FRegister rs1) {
  EmitR(0x61, 0x3, rs1, FRM, rd, 0x53);
}

void Riscv64Assembler::FCvtDW(FRegister rd, XRegister rs1) { EmitR(0x69, 0x0, rs1, 0x0, rd, 0x53); }

void Riscv64Assembler::FCvtDWu(FRegister rd, XRegister rs1) {
  EmitR(0x69, 0x1, rs1, 0x0, rd, 0x53);
}

void Riscv64Assembler::FCvtDL(FRegister rd, XRegister rs1) { EmitR(0x69, 0x2, rs1, FRM, rd, 0x53); }

void Riscv64Assembler::FCvtDLu(FRegister rd, XRegister rs1) {
  EmitR(0x69, 0x3, rs1, FRM, rd, 0x53);
}

void Riscv64Assembler::FMvXD(XRegister rd, FRegister rs1) { EmitR(0x71, 0x0, rs1, 0x0, rd, 0x53); }

void Riscv64Assembler::FClassD(XRegister rd, FRegister rs1) {
  EmitR(0x71, 0x0, rs1, 0x1, rd, 0x53);
}

void Riscv64Assembler::FMvDX(FRegister rd, XRegister rs1) { EmitR(0x79, 0x0, rs1, 0x0, rd, 0x53); }

void Riscv64Assembler::MinS(FRegister fd, FRegister fs, FRegister ft) { FMinS(fd, fs, ft); }

void Riscv64Assembler::MinD(FRegister fd, FRegister fs, FRegister ft) { FMinD(fd, fs, ft); }

void Riscv64Assembler::MaxS(FRegister fd, FRegister fs, FRegister ft) { FMaxS(fd, fs, ft); }

void Riscv64Assembler::MaxD(FRegister fd, FRegister fs, FRegister ft) { FMaxD(fd, fs, ft); }
/////////////////////////////// RV64 "FD" Instructions  END ///////////////////////////////

////////////////////////////// RV64 MACRO Instructions  START ///////////////////////////////
void Riscv64Assembler::Nop() { Addi(Zero, Zero, 0); }

void Riscv64Assembler::Move(XRegister rd, XRegister rs) { Or(rd, rs, Zero); }

void Riscv64Assembler::Clear(XRegister rd) { Or(rd, Zero, Zero); }

void Riscv64Assembler::Not(XRegister rd, XRegister rs) { Xori(rd, rs, -1); }

void Riscv64Assembler::Break() { Ebreak(); }

void Riscv64Assembler::Sync(uint32_t stype) {
  UNUSED(stype);
  // XC-TODO: for performance, need set fence according to stype
  Fence(0xf, 0xf);
}

void Riscv64Assembler::Addiuw(XRegister rd, XRegister rs, int16_t imm16) {
  if (IsInt<12>(imm16)) {
    Addiw(rd, rs, imm16);
  } else {
    int32_t l = imm16 & 0xFFF;  // Higher 20b is Zero
    int32_t h = imm16 >> 12;
    if ((l & 0x800) != 0) {
      h += 1;
    }
    // rs and rd may be same or be TMP, use TMP2 here.
    Lui(TMP2, h);
    if (l)
      Addiw(TMP2, TMP2, l);
    Addw(rd, TMP2, rs);
  }
}

void Riscv64Assembler::Addiu(XRegister rd, XRegister rs, int16_t imm16) {
  if (IsInt<12>(imm16)) {
    Addi(rd, rs, imm16);
  } else {
    int32_t l = imm16 & 0xFFF;
    int32_t h = imm16 >> 12;
    if ((l & 0x800) != 0) {
      h += 1;
    }
    // rs and rd may be same or be TMP, use TMP2 here.
    Lui(TMP2, h);
    if (l)
      Addiw(TMP2, TMP2, l);
    Add(rd, TMP2, rs);
  }
}

void Riscv64Assembler::Addiuw32(XRegister rt, XRegister rs, int32_t value) {
  if (IsInt<12>(value)) {
    Addiw(rt, rs, value);
  } else {
    LoadConst32(TMP2, value);
    Addw(rt, rs, TMP2);
  }
}

void Riscv64Assembler::Addiu64(XRegister rt, XRegister rs, int64_t value, XRegister rtmp) {
  CHECK_NE(rs, rtmp);
  if (IsInt<12>(value)) {
    Addi(rt, rs, value);
  } else {
    LoadConst64(rtmp, value);
    Add(rt, rs, rtmp);
  }
}

void Riscv64Assembler::Srriw(XRegister rd, XRegister rs1, int imm5) {
  CHECK(0 <= imm5 < 32) << imm5;
  // Riscv64 codegen don't use the blocked registers for rd, rt, rs till now.
  // It's safe to use scratch registers here.
  Srliw(TMP, rs1, imm5);
  Slliw(rd, rs1, 32 - imm5);  // logical shift left (32 -shamt)
  Or(rd, rd, TMP);
}

void Riscv64Assembler::Srri(XRegister rd, XRegister rs1, int imm6) {
  CHECK(0 <= imm6 < 64) << imm6;
  // Riscv64 codegen don't use the blocked registers for rd, rt, rs till now.
  // It's safe to use scratch registers here.
  Srli(TMP, rs1, imm6);
  Slli(rd, rs1, (64 - imm6));
  Or(rd, rd, TMP);
}

void Riscv64Assembler::Srrw(XRegister rd, XRegister rt, XRegister rs) {
  // Riscv64 codegen don't use the blocked registers for rd, rt, rs till now.
  // It's safe to use TMP scratch registers here.
  Srlw(TMP, rt, rs);
  Subw(TMP2, Zero, rs);   // TMP2 = -rs
  Addiw(TMP2, TMP2, 32);  // TMP2 = 32 -rs
  Andi(TMP2, TMP2, 0x1F);
  Sllw(rd, rt, TMP2);
  Or(rd, rd, TMP);
}

void Riscv64Assembler::Srr(XRegister rd, XRegister rt, XRegister rs) {
  // Riscv64 codegen don't use the blocked registers for rd, rt, rs till now.
  // It's safe to use scratch registers here.
  Srl(TMP, rt, rs);
  Sub(TMP2, Zero, rs);   // TMP2 = -rs
  Addi(TMP2, TMP2, 64);  // TMP2 = 64 -rs
  Sll(rd, rt, TMP2);
  Or(rd, rd, TMP);
}

void Riscv64Assembler::Muhh(XRegister rd, XRegister rs, XRegister rt) {
  // There's no instruction in Riscv64 can get the high 32bit of 32-bit Multiplication.
  // Shift left 32 for both of source operands
  // Use TMP2 and T6 here
  Slli(TMP2, rs, 32);
  Slli(T6, rt, 32);
  Mul(rd, TMP2, T6);  // rd <-- (rs x rt)'s 64-bit result
  Srai(rd, rd, 32);   // get the high 32bit result and keep sign
}

void Riscv64Assembler::Aui(XRegister rt, XRegister rs, uint16_t imm16) {
  int32_t l = imm16 & 0xFFF;
  int32_t h = imm16 >> 12;
  if ((l & 0x800) != 0) {
    h += 1;
  }

  // rs and rd may be same or be TMP, use TMP2 here.
  Lui(TMP2, h);
  if (l)
    Addi(TMP2, TMP2, l);
  Slli(TMP2, TMP2, 16);
  Add(rt, rs, TMP2);
}

void Riscv64Assembler::Ahi(XRegister rs, uint16_t imm16) {
  int32_t l = imm16 & 0xFFF;
  int32_t h = imm16 >> 12;
  if ((l & 0x800) != 0) {
    h += 1;
  }

  // rs and rd may be same or be TMP, use TMP2 here.
  Lui(TMP2, h);
  if (l)
    Addi(TMP2, TMP2, l);
  Slli(TMP2, TMP2, 32);
  Add(rs, rs, TMP2);
}

void Riscv64Assembler::Ati(XRegister rs, uint16_t imm16) {
  int32_t l = imm16 & 0xFFF;
  int32_t h = imm16 >> 12;
  if ((l & 0x800) != 0) {
    h += 1;
  }

  // rs and rd may be same or be TMP, use TMP2 here.
  Lui(TMP2, h);
  if (l)
    Addi(TMP2, TMP2, l);
  Slli(TMP2, TMP2, 48);
  Add(rs, rs, TMP2);
}

void Riscv64Assembler::LoadConst32(XRegister rd, int32_t value) {
  if (IsInt<12>(value)) {
    Addi(rd, Zero, value);
  } else {
    int32_t l = value & 0xFFF;
    int32_t h = value >> 12;
    if ((l & 0x800) != 0) {
      h += 1;
    }
    Lui(rd, h);
    if (l)
      Addiw(rd, rd, l);
  }
}

void Riscv64Assembler::LoadConst64(XRegister rd, int64_t value) {
  if (IsInt<32>(value)) {
    LoadConst32(rd, value);
  } else {
    // Need to optimize in the future.
    int32_t hi = value >> 32;
    int32_t lo = value;

    XRegister scratch = TMP2;

    LoadConst32(scratch, lo);
    LoadConst32(rd, hi);
    Slli(rd, rd, 32);
    Slli(scratch, scratch, 32);
    Srli(scratch, scratch, 32);
    Or(rd, rd, scratch);
  }
}

// shift and add
void Riscv64Assembler::Addsl(XRegister rd, XRegister rs, XRegister rt, int saPlusOne) {
  CHECK(1 <= saPlusOne && saPlusOne < 4) << saPlusOne;
  Slli(TMP2, rs, saPlusOne);
  Add(rd, TMP2, rt);
}

void Riscv64Assembler::Extb(XRegister rt, XRegister rs, int pos, int size) {
  CHECK(IsUint<6>(pos)) << pos;
  CHECK(IsUint<6>(size - 1)) << size;
  Srli(rt, rs, pos);
  Slli(rt, rs, (64 - size));
  Srai(rt, rt, (64 - size));
}

void Riscv64Assembler::Extub(XRegister rt, XRegister rs, int pos, int size) {
  CHECK(IsUint<6>(pos)) << pos;
  CHECK(IsUint<6>(size - 1)) << size;
  Srli(rt, rs, pos);
  Slli(rt, rt, (64 - size));
  Srli(rt, rt, (64 - size));
}

// Branches
void Riscv64Assembler::Seleqz(XRegister rd, XRegister rs, XRegister rt) {
  if (rt == rd) {
    Move(TMP2, rt);
    Move(rd, rs);
    Beq(TMP2, Zero, 8);
    Move(rd, Zero);
  } else {
    Move(rd, rs);
    Beq(rt, Zero, 8);
    Move(rd, Zero);
  }
}

void Riscv64Assembler::Selnez(XRegister rd, XRegister rs, XRegister rt) {
  if (rt == rd) {
    Move(TMP2, rt);
    Move(rd, rs);
    Bne(TMP2, Zero, 8);
    Move(rd, Zero);
  } else {
    Move(rd, rs);
    Bne(rt, Zero, 8);
    Move(rd, Zero);
  }
}

void Riscv64Assembler::Bc(uint32_t imm20) { Jal(Zero, imm20); }

void Riscv64Assembler::Balc(uint32_t imm20) { Jal(RA, imm20); }

void Riscv64Assembler::Bltc(XRegister rs, XRegister rt, uint16_t imm12) {
  CHECK_NE(rs, Zero);
  CHECK_NE(rt, Zero);
  CHECK_NE(rs, rt);
  Blt(rs, rt, imm12);
}

void Riscv64Assembler::Bltzc(XRegister rt, uint16_t imm12) {
  CHECK_NE(rt, Zero);
  Blt(rt, Zero, imm12);
}

void Riscv64Assembler::Bgtzc(XRegister rt, uint16_t imm12) {
  CHECK_NE(rt, Zero);
  Blt(Zero, rt, imm12);
}

void Riscv64Assembler::Bgec(XRegister rs, XRegister rt, uint16_t imm12) {
  CHECK_NE(rs, Zero);
  CHECK_NE(rt, Zero);
  CHECK_NE(rs, rt);
  Bge(rs, rt, imm12);
}

void Riscv64Assembler::Bgezc(XRegister rt, uint16_t imm12) {
  CHECK_NE(rt, Zero);
  Bge(rt, Zero, imm12);
}

void Riscv64Assembler::Blezc(XRegister rt, uint16_t imm12) {
  CHECK_NE(rt, Zero);
  Bge(Zero, rt, imm12);
}

void Riscv64Assembler::Bltuc(XRegister rs, XRegister rt, uint16_t imm12) {
  CHECK_NE(rs, Zero);
  CHECK_NE(rt, Zero);
  CHECK_NE(rs, rt);
  Bltu(rs, rt, imm12);
}

void Riscv64Assembler::Bgeuc(XRegister rs, XRegister rt, uint16_t imm12) {
  CHECK_NE(rs, Zero);
  CHECK_NE(rt, Zero);
  CHECK_NE(rs, rt);
  Bgeu(rs, rt, imm12);
}

void Riscv64Assembler::Beqc(XRegister rs, XRegister rt, uint16_t imm12) {
  CHECK_NE(rs, Zero);
  CHECK_NE(rt, Zero);
  CHECK_NE(rs, rt);
  Beq(rs, rt, imm12);
}

void Riscv64Assembler::Bnec(XRegister rs, XRegister rt, uint16_t imm12) {
  CHECK_NE(rs, Zero);
  CHECK_NE(rt, Zero);
  CHECK_NE(rs, rt);
  Bne(rs, rt, imm12);
}

void Riscv64Assembler::Beqzc(XRegister rs, uint32_t imm12) {
  CHECK_NE(rs, Zero);
  Beq(rs, Zero, imm12);
}

void Riscv64Assembler::Bnezc(XRegister rs, uint32_t imm12) {
  CHECK_NE(rs, Zero);
  Bne(rs, Zero, imm12);
}

void Riscv64Assembler::EmitBcond(BranchCondition cond,
                                 XRegister rs,
                                 XRegister rt,
                                 uint32_t imm16_21) {
  switch (cond) {
    case kCondLT:
      Bltc(rs, rt, imm16_21);
      break;
    case kCondGE:
      Bgec(rs, rt, imm16_21);
      break;
    case kCondLE:
      Bgec(rt, rs, imm16_21);
      break;
    case kCondGT:
      Bltc(rt, rs, imm16_21);
      break;
    case kCondLTZ:
      CHECK_EQ(rt, Zero);
      Bltzc(rs, imm16_21);
      break;
    case kCondGEZ:
      CHECK_EQ(rt, Zero);
      Bgezc(rs, imm16_21);
      break;
    case kCondLEZ:
      CHECK_EQ(rt, Zero);
      Blezc(rs, imm16_21);
      break;
    case kCondGTZ:
      CHECK_EQ(rt, Zero);
      Bgtzc(rs, imm16_21);
      break;
    case kCondEQ:
      Beqc(rs, rt, imm16_21);
      break;
    case kCondNE:
      Bnec(rs, rt, imm16_21);
      break;
    case kCondEQZ:
      CHECK_EQ(rt, Zero);
      Beqzc(rs, imm16_21);
      break;
    case kCondNEZ:
      CHECK_EQ(rt, Zero);
      Bnezc(rs, imm16_21);
      break;
    case kCondLTU:
      Bltuc(rs, rt, imm16_21);
      break;
    case kCondGEU:
      Bgeuc(rs, rt, imm16_21);
      break;
    case kUncond:
      // LOG(FATAL) << "Unexpected branch condition " << cond;
      LOG(FATAL) << "Unexpected branch condition ";
      UNREACHABLE();
  }
}

// Jump
void Riscv64Assembler::Jalr(XRegister rd, XRegister rs) { Jalr(rd, rs, 0); }

void Riscv64Assembler::Jic(XRegister rt, uint16_t imm16) { Jalr(Zero, rt, imm16); }

void Riscv64Assembler::Jalr(XRegister rs) { Jalr(RA, rs, 0); }

void Riscv64Assembler::Jialc(XRegister rt, uint16_t imm16) { Jalr(RA, rt, imm16); }

void Riscv64Assembler::Jr(XRegister rs) { Jalr(Zero, rs, 0); }

// Atomic Ops
// MIPS: 0->fail
// RV  : 0-> sucess
void Riscv64Assembler::Sc(XRegister rt, XRegister base) {
  ScW(rt, rt, base, 0x0);
  Xori(rt, rt, 0x01);
}

void Riscv64Assembler::Scd(XRegister rt, XRegister base) {
  ScD(rt, rt, base, 0x0);
  Xori(rt, rt, 0x01);
}

void Riscv64Assembler::Ll(XRegister rt, XRegister base) {
  LrW(rt, base, 0x0);  // aq, rl
}

void Riscv64Assembler::Lld(XRegister rt, XRegister base) { LrD(rt, base, 0x0); }

// Float Ops
void Riscv64Assembler::AddS(FRegister fd, FRegister fs, FRegister ft) { FAddS(fd, fs, ft); }

void Riscv64Assembler::SubS(FRegister fd, FRegister fs, FRegister ft) { FSubS(fd, fs, ft); }

void Riscv64Assembler::MulS(FRegister fd, FRegister fs, FRegister ft) { FMulS(fd, fs, ft); }

void Riscv64Assembler::DivS(FRegister fd, FRegister fs, FRegister ft) { FDivS(fd, fs, ft); }

void Riscv64Assembler::AbsS(FRegister fd, FRegister fs) { FSgnjxS(fd, fs, fs); }

void Riscv64Assembler::MovS(FRegister fd, FRegister fs) { FSgnjS(fd, fs, fs); }

void Riscv64Assembler::NegS(FRegister fd, FRegister fs) { FSgnjnS(fd, fs, fs); }

void Riscv64Assembler::SqrtS(FRegister fd, FRegister fs) { FSqrtS(fd, fs); }

// Double Ops
void Riscv64Assembler::AddD(FRegister fd, FRegister fs, FRegister ft) { FAddD(fd, fs, ft); }

void Riscv64Assembler::SubD(FRegister fd, FRegister fs, FRegister ft) { FSubD(fd, fs, ft); }

void Riscv64Assembler::MulD(FRegister fd, FRegister fs, FRegister ft) { FMulD(fd, fs, ft); }

void Riscv64Assembler::DivD(FRegister fd, FRegister fs, FRegister ft) { FDivD(fd, fs, ft); }

void Riscv64Assembler::AbsD(FRegister fd, FRegister fs) { FSgnjxD(fd, fs, fs); }

void Riscv64Assembler::MovD(FRegister fd, FRegister fs) { FSgnjD(fd, fs, fs); }

void Riscv64Assembler::NegD(FRegister fd, FRegister fs) { FSgnjnD(fd, fs, fs); }

void Riscv64Assembler::SqrtD(FRegister fd, FRegister fs) { FSqrtD(fd, fs); }

// Float <-> double
void Riscv64Assembler::Cvtsd(FRegister fd, FRegister fs) { FCvtSD(fd, fs); }

void Riscv64Assembler::Cvtds(FRegister fd, FRegister fs) { FCvtDS(fd, fs); }

// According to VM spec, If the value' is NaN, the result of the conversion is a long 0.
// Acoording to IEEE-754, NaN should be convert to 2^63 -1
// NaN != NaN
void Riscv64Assembler::TruncLS(XRegister rd, FRegister fs) {
  Xor(rd, rd, rd);
  FEqS(TMP, fs, fs);
  riscv64::Riscv64Label label;
  Beqzc(TMP, &label);
  FCvtLS(rd, fs, kFPRoundingModeRTZ);
  Bind(&label);
}

void Riscv64Assembler::TruncLD(XRegister rd, FRegister fs) {
  Xor(rd, rd, rd);
  FEqD(TMP, fs, fs);
  riscv64::Riscv64Label label;
  Beqzc(TMP, &label);
  FCvtLD(rd, fs, kFPRoundingModeRTZ);
  Bind(&label);
}

void Riscv64Assembler::TruncWS(XRegister rd, FRegister fs) {
  Xor(rd, rd, rd);
  FEqS(TMP, fs, fs);
  riscv64::Riscv64Label label;
  Beqzc(TMP, &label);
  FCvtWS(rd, fs, kFPRoundingModeRTZ);
  Bind(&label);
}

void Riscv64Assembler::TruncWD(XRegister rd, FRegister fs) {
  Xor(rd, rd, rd);
  FEqD(TMP, fs, fs);
  riscv64::Riscv64Label label;
  Beqzc(TMP, &label);
  FCvtWD(rd, fs, kFPRoundingModeRTZ);
  Bind(&label);
}

// Java spec says: if one is NaN, return NaN, otherwise ...
void Riscv64Assembler::FJMaxMinS(FRegister fd, FRegister fs, FRegister ft, bool is_min) {
  riscv64::Riscv64Label labelFS, labelFT, labelDone;
  FEqS(TMP, fs, fs);
  Beqzc(TMP, &labelFS);  // fs is NaN
  FEqS(TMP, ft, ft);
  Beqzc(TMP, &labelFT);  // fs is NaN

  // All are not NaN
  if (is_min) {
    FMinS(fd, fs, ft);
  } else {
    FMaxS(fd, fs, ft);
  }
  Bc(&labelDone);

  Bind(&labelFS);  // fs is NaN
  MovS(fd, fs);
  Bc(&labelDone);

  Bind(&labelFT);  // ft is NaN
  MovS(fd, ft);

  Bind(&labelDone);
}

void Riscv64Assembler::FJMaxMinD(FRegister fd, FRegister fs, FRegister ft, bool is_min) {
  riscv64::Riscv64Label labelFS, labelFT, labelDone;
  FEqD(TMP, fs, fs);
  Beqzc(TMP, &labelFS);  // fs is NaN
  FEqD(TMP, ft, ft);
  Beqzc(TMP, &labelFT);  // fs is NaN

  // All are not NaN
  if (is_min) {
    FMinD(fd, fs, ft);
  } else {
    FMaxD(fd, fs, ft);
  }
  Bc(&labelDone);

  Bind(&labelFS);  // fs is NaN
  MovD(fd, fs);
  Bc(&labelDone);

  Bind(&labelFT);  // ft is NaN
  MovD(fd, ft);

  Bind(&labelDone);
}

// XC-TODO: there are no FSel instrctions in RVGC
void Riscv64Assembler::SelS(FRegister fd, FRegister fs, FRegister ft) {
  FMvXW(TMP, fd);
  Andi(TMP, TMP, 1);
  Beq(TMP, Zero, 12);
  FSgnjS(fd, ft, ft);
  Jal(Zero, 8);

  FSgnjS(fd, fs, fs);
}

void Riscv64Assembler::SelD(FRegister fd, FRegister fs, FRegister ft) {
  FMvXD(TMP, fd);
  Andi(TMP, TMP, 1);
  Beq(TMP, Zero, 12);
  FSgnjD(fd, ft, ft);
  Jal(Zero, 8);

  FSgnjD(fd, fs, fs);
}

void Riscv64Assembler::SeleqzS(FRegister fd, FRegister fs, FRegister ft) {
  FMvXW(TMP, ft);
  Andi(TMP, TMP, 1);
  Beq(TMP, Zero, 16);
  Addiw(TMP, Zero, 0);
  FCvtSW(fd, TMP);
  Jal(Zero, 8);

  FSgnjS(fd, fs, fs);
}

void Riscv64Assembler::SeleqzD(FRegister fd, FRegister fs, FRegister ft) {
  FMvXD(TMP, ft);
  Andi(TMP, TMP, 1);
  Beq(TMP, Zero, 16);
  Addi(TMP, Zero, 0);
  FCvtDL(fd, TMP);
  Jal(Zero, 8);

  FSgnjD(fd, fs, fs);
}

void Riscv64Assembler::SelnezS(FRegister fd, FRegister fs, FRegister ft) {
  FMvXW(TMP, ft);
  Andi(TMP, TMP, 1);
  Bne(TMP, Zero, 16);
  Addiw(TMP, Zero, 0);
  FCvtSW(fd, TMP);
  Jal(Zero, 8);

  FSgnjS(fd, fs, fs);
}

void Riscv64Assembler::SelnezD(FRegister fd, FRegister fs, FRegister ft) {
  FMvXD(TMP, ft);
  Andi(TMP, TMP, 1);
  Bne(TMP, Zero, 16);
  Addi(TMP, Zero, 0);
  FCvtDL(fd, TMP);
  Jal(Zero, 8);

  FSgnjD(fd, fs, fs);
}

// Java VM says. All values other than NaN are ordered, with negative infinity less than all finite
// values
//   and positive infinity greater than all finite values. Positive zero and negative zero are
//   considered equal.
// one of value1' or value2' is NaN. The fcmpg instruction pushes the int value 1 onto the operand
// stack and the fcmpl instruction pushes the int value -1 onto the operand stack
void Riscv64Assembler::CmpUltS(XRegister rd, FRegister fs, FRegister ft) {
  FClassS(TMP, fs);
  Srli(TMP, TMP, 8);
  Bne(TMP, Zero, 24);

  FClassS(TMP, ft);
  Srli(TMP, TMP, 8);
  Bne(TMP, Zero, 12);

  FLtS(rd, fs, ft);
  Jal(Zero, 8);

  Addi(rd, Zero, 1);  // unordered true;
}

void Riscv64Assembler::CmpLeS(XRegister rd, FRegister fs, FRegister ft) { FLeS(rd, fs, ft); }

void Riscv64Assembler::CmpUleS(XRegister rd, FRegister fs, FRegister ft) {
  FClassS(TMP, fs);
  Srli(TMP, TMP, 8);
  Bne(TMP, Zero, 24);

  FClassS(TMP, ft);
  Srli(TMP, TMP, 8);
  Bne(TMP, Zero, 12);

  FLeS(rd, fs, ft);
  Jal(Zero, 8);

  // NaN, RV will return 0 if we do not do this
  Addi(rd, Zero, 1);  // unordered true;
}

void Riscv64Assembler::CmpUneS(XRegister rd, FRegister fs, FRegister ft) {
  FClassS(TMP, fs);
  Srli(TMP, TMP, 8);
  Bne(TMP, Zero, 28);

  FClassS(TMP, ft);
  Srli(TMP, TMP, 8);
  Bne(TMP, Zero, 16);

  FEqS(TMP, fs, ft);
  Sltiu(rd, TMP, 1);
  Jal(Zero, 8);

  Addi(rd, Zero, 1);  // unordered true;
}

void Riscv64Assembler::CmpNeS(XRegister rd, FRegister fs, FRegister ft) {
  FEqS(rd, fs, ft);
  Sltiu(rd, rd, 1);
}

void Riscv64Assembler::CmpUnD(XRegister rd, FRegister fs, FRegister ft) {
  FClassD(TMP, fs);
  Srli(TMP, TMP, 8);
  Bne(TMP, Zero, 24);

  FClassD(TMP, ft);
  Srli(TMP, TMP, 8);
  Bne(TMP, Zero, 12);

  Addi(rd, Zero, 0);  // unordered false;
  Jal(Zero, 8);

  Addi(rd, Zero, 1);  // unordered true;
}

void Riscv64Assembler::CmpEqD(XRegister rd, FRegister fs, FRegister ft) { FEqD(rd, fs, ft); }

void Riscv64Assembler::CmpUeqD(XRegister rd, FRegister fs, FRegister ft) {
  FClassD(TMP, fs);
  Srli(TMP, TMP, 8);
  Bne(TMP, Zero, 24);

  FClassD(TMP, ft);
  Srli(TMP, TMP, 8);
  Bne(TMP, Zero, 12);

  FEqD(rd, fs, ft);
  Jal(Zero, 8);

  Addi(rd, Zero, 1);  // unordered true;
}

void Riscv64Assembler::CmpLtD(XRegister rd, FRegister fs, FRegister ft) { FLtD(rd, fs, ft); }

void Riscv64Assembler::CmpUltD(XRegister rd, FRegister fs, FRegister ft) {
  FClassD(TMP, fs);
  Srli(TMP, TMP, 8);
  Bne(TMP, Zero, 24);

  FClassD(TMP, ft);
  Srli(TMP, TMP, 8);
  Bne(TMP, Zero, 12);

  FLtD(rd, fs, ft);
  Jal(Zero, 8);

  Addi(rd, Zero, 1);  // unordered true;
}

void Riscv64Assembler::CmpLeD(XRegister rd, FRegister fs, FRegister ft) { FLeD(rd, fs, ft); }

void Riscv64Assembler::CmpUleD(XRegister rd, FRegister fs, FRegister ft) {
  FClassD(TMP, fs);
  Srli(TMP, TMP, 8);
  Bne(TMP, Zero, 24);

  FClassD(TMP, ft);
  Srli(TMP, TMP, 8);
  Bne(TMP, Zero, 12);

  FLeD(rd, fs, ft);
  Jal(Zero, 8);

  Addi(rd, Zero, 1);  // unordered true;
}

void Riscv64Assembler::CmpOrD(XRegister rd, FRegister fs, FRegister ft) {
  CmpUnD(rd, fs, ft);
  Sltiu(rd, rd, 1);
}

void Riscv64Assembler::CmpUneD(XRegister rd, FRegister fs, FRegister ft) {
  FClassD(TMP, fs);
  Srli(TMP, TMP, 8);
  Bne(TMP, Zero, 28);

  FClassD(TMP, ft);
  Srli(TMP, TMP, 8);
  Bne(TMP, Zero, 16);

  FEqD(TMP, fs, ft);
  Sltiu(rd, rd, 1);
  Jal(Zero, 8);

  Addi(rd, Zero, 1);  // unordered true;
}

void Riscv64Assembler::CmpNeD(XRegister rd, FRegister fs, FRegister ft) {
  FEqD(rd, fs, ft);
  Sltiu(rd, rd, 1);
}

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
    case kBareCall:
      type_ = kBareCall;
      CHECK_LE(offset_size_needed, GetOffsetSize());
      break;
    case kBareCondBranch:
      type_ = (condition_ == kUncond) ? kBareUncondBranch : kBareCondBranch;
      CHECK_LE(offset_size_needed, GetOffsetSize());
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
    case kCondGT:
    case kCondNE:
    case kCondLTU:
      return lhs == rhs;
    default:
      return false;
  }
}

bool Riscv64Assembler::Branch::IsUncond(BranchCondition condition, XRegister lhs, XRegister rhs) {
  switch (condition) {
    case kUncond:
      return true;
    case kCondGE:
    case kCondLE:
    case kCondEQ:
    case kCondGEU:
      return lhs == rhs;
    default:
      return false;
  }
}

Riscv64Assembler::Branch::Branch(uint32_t location, uint32_t target, bool is_call, bool is_bare)
    : old_location_(location),
      location_(location),
      target_(target),
      lhs_reg_(Zero),
      rhs_reg_(Zero),
      condition_(kUncond) {
  InitializeType(
      (is_call ? (is_bare ? kBareCall : kCall) : (is_bare ? kBareCondBranch : kCondBranch)));
}

Riscv64Assembler::Branch::Branch(uint32_t location,
                                 uint32_t target,
                                 Riscv64Assembler::BranchCondition condition,
                                 XRegister lhs_reg,
                                 XRegister rhs_reg,
                                 bool is_bare)
    : old_location_(location),
      location_(location),
      target_(target),
      lhs_reg_(lhs_reg),
      rhs_reg_(rhs_reg),
      condition_(condition) {
  // CHECK_NE(condition, kUncond);
  switch (condition) {
    case kCondEQ:
    case kCondNE:
    case kCondLT:
    case kCondGE:
    case kCondLE:
    case kCondGT:
    case kCondLTU:
    case kCondGEU:
      CHECK_NE(lhs_reg, Zero);
      CHECK_NE(rhs_reg, Zero);
      break;
    case kCondLTZ:
    case kCondGEZ:
    case kCondLEZ:
    case kCondGTZ:
    case kCondEQZ:
    case kCondNEZ:
      CHECK_NE(lhs_reg, Zero);
      CHECK_EQ(rhs_reg, Zero);
      break;
    case kUncond:
      UNREACHABLE();
  }
  CHECK(!IsNop(condition, lhs_reg, rhs_reg));
  if (IsUncond(condition, lhs_reg, rhs_reg)) {
    // Branch condition is always true, make the branch unconditional.
    condition_ = kUncond;
  }
  InitializeType((is_bare ? kBareCondBranch : kCondBranch));
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
    case kCondLT:
      return kCondGE;
    case kCondGE:
      return kCondLT;
    case kCondLE:
      return kCondGT;
    case kCondGT:
      return kCondLE;
    case kCondLTZ:
      return kCondGEZ;
    case kCondGEZ:
      return kCondLTZ;
    case kCondLEZ:
      return kCondGTZ;
    case kCondGTZ:
      return kCondLEZ;
    case kCondEQ:
      return kCondNE;
    case kCondNE:
      return kCondEQ;
    case kCondEQZ:
      return kCondNEZ;
    case kCondNEZ:
      return kCondEQZ;
    case kCondLTU:
      return kCondGEU;
    case kCondGEU:
      return kCondLTU;
    case kUncond:
      // LOG(FATAL) << "Unexpected branch condition " << cond;
      LOG(FATAL) << "Unexpected branch condition ";
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

bool Riscv64Assembler::Branch::IsBare() const {
  switch (type_) {
    // R6 short branches (can't be promoted to long), forbidden/delay slots filled manually.
    case kBareUncondBranch:
    case kBareCondBranch:
    case kBareCall:
      return true;
    default:
      return false;
  }
}

bool Riscv64Assembler::Branch::IsLong() const {
  switch (type_) {
    // R6 short branches (can be promoted to long).
    case kUncondBranch:
    case kCondBranch:
    case kCall:
    // R6 short branches (can't be promoted to long), forbidden/delay slots filled manually.
    case kBareUncondBranch:
    case kBareCondBranch:
    case kBareCall:
      return false;
    // Long branches.
    case kLongUncondBranch:
    case kLongCondBranch:
    case kLongCall:
    // label.
    case kLabel:
    // literals.
    case kLiteral:
    case kLiteralUnsigned:
    case kLiteralLong:
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
  CHECK(!IsBare());  // Bare branches do not promote.
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
  if (UNLIKELY(max_short_distance != std::numeric_limits<uint32_t>::max() && !IsBare())) {
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

void Riscv64Assembler::Buncond(Riscv64Label* label, bool is_bare) {
  uint32_t target = label->IsBound() ? GetLabelLocation(label) : Branch::kUnresolved;
  branches_.emplace_back(buffer_.Size(), target, /* is_call= */ false, is_bare);
  FinalizeLabeledBranch(label);
}

void Riscv64Assembler::Bcond(
    Riscv64Label* label, bool is_bare, BranchCondition condition, XRegister lhs, XRegister rhs) {
  // If lhs = rhs, this can be a NOP.
  if (Branch::IsNop(condition, lhs, rhs)) {
    return;
  }
  uint32_t target = label->IsBound() ? GetLabelLocation(label) : Branch::kUnresolved;
  branches_.emplace_back(buffer_.Size(), target, condition, lhs, rhs, is_bare);
  FinalizeLabeledBranch(label);
}

void Riscv64Assembler::Call(Riscv64Label* label, bool is_bare) {
  uint32_t target = label->IsBound() ? GetLabelLocation(label) : Branch::kUnresolved;
  branches_.emplace_back(buffer_.Size(), target, /* is_call= */ true, is_bare);
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
    Emit(0);  // NOP.
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

  // Align 64-bit literals by moving them down by 4 bytes if needed.
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

// Note: make sure branch_info_[] and EmitBranch() are kept synchronized.
const Riscv64Assembler::Branch::BranchInfo Riscv64Assembler::Branch::branch_info_[] = {
    // short branches (can be promoted to long).
    {1, 0, 0, Riscv64Assembler::Branch::kOffset21, 0},  // kUncondBranch
    {1, 0, 0, Riscv64Assembler::Branch::kOffset13, 0},  // kCondBranch
    {1, 0, 0, Riscv64Assembler::Branch::kOffset21, 0},  // kCall
    // short branches (can't be promoted to long), forbidden/delay slots filled manually.
    {1, 0, 0, Riscv64Assembler::Branch::kOffset21, 0},  // kBareUncondBranch
    {1, 0, 0, Riscv64Assembler::Branch::kOffset13, 0},  // kBareCondBranch
    {1, 0, 0, Riscv64Assembler::Branch::kOffset21, 0},  // kBareCall

    // label.
    {2, 0, 0, Riscv64Assembler::Branch::kOffset32, 0},  // kLabel
    // literals.
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
      Bc(offset);
      break;
    case Branch::kCondBranch:
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      EmitBcond(condition, lhs, rhs, offset);
      break;
    case Branch::kCall:
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Balc(offset);
      break;
    case Branch::kBareUncondBranch:
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Bc(offset);
      break;
    case Branch::kBareCondBranch:
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      EmitBcond(condition, lhs, rhs, offset);
      break;
    case Branch::kBareCall:
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Balc(offset);
      break;

    // label.
    case Branch::kLabel:
      offset += (offset & 0x800) << 1;  // Account for sign extension in daddiu.
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Auipc(AT, High20Bits(offset));
      Addi(lhs, AT, Low12Bits(offset));
      break;
    // literals.
    case Branch::kLiteral:
      offset += (offset & 0x800) << 1;  // Account for sign extension in lw.
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Auipc(AT, High20Bits(offset));
      Lw(lhs, AT, Low12Bits(offset));
      break;
    case Branch::kLiteralUnsigned:
      offset += (offset & 0x800) << 1;  // Account for sign extension in lwu.
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Auipc(AT, High20Bits(offset));
      Lwu(lhs, AT, Low12Bits(offset));
      break;
    case Branch::kLiteralLong:
      offset += (offset & 0x800) << 1;  // Account for sign extension in ld.
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Auipc(AT, High20Bits(offset));
      Ld(lhs, AT, Low12Bits(offset));
      break;

    // Long branches.
    case Branch::kLongUncondBranch:
      offset += (offset & 0x800) << 1;  // Account for sign extension in jic.
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Auipc(AT, High20Bits(offset));
      Jic(AT, Low12Bits(offset));
      break;
    case Branch::kLongCondBranch:
      // Skip (2 + itself) instructions and continue if the Cond isn't taken.
      EmitBcond(Branch::OppositeCondition(condition), lhs, rhs, 12);
      offset += (offset & 0x800) << 1;  // Account for sign extension in jic.
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Auipc(AT, High20Bits(offset));
      Jic(AT, Low12Bits(offset));
      break;
    case Branch::kLongCall:
      offset += (offset & 0x800) << 1;  // Account for sign extension in jialc.
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Auipc(AT, High20Bits(offset));
      Jialc(AT, Low12Bits(offset));
      break;
  }
  CHECK_EQ(overwrite_location_, branch->GetEndLocation());
  CHECK_LT(branch->GetSize(), static_cast<uint32_t>(Branch::kMaxBranchSize));
}

void Riscv64Assembler::Bc(Riscv64Label* label, bool is_bare) { Buncond(label, is_bare); }

void Riscv64Assembler::Balc(Riscv64Label* label, bool is_bare) { Call(label, is_bare); }

void Riscv64Assembler::Jal(Riscv64Label* label, bool is_bare) { Call(label, is_bare); }

void Riscv64Assembler::Bltc(XRegister rs, XRegister rt, Riscv64Label* label, bool is_bare) {
  Bcond(label, is_bare, kCondLT, rs, rt);
}

void Riscv64Assembler::Bltzc(XRegister rt, Riscv64Label* label, bool is_bare) {
  Bcond(label, is_bare, kCondLTZ, rt);
}

void Riscv64Assembler::Bgtzc(XRegister rt, Riscv64Label* label, bool is_bare) {
  Bcond(label, is_bare, kCondGTZ, rt);
}

void Riscv64Assembler::Bgec(XRegister rs, XRegister rt, Riscv64Label* label, bool is_bare) {
  Bcond(label, is_bare, kCondGE, rs, rt);
}

void Riscv64Assembler::Bgezc(XRegister rt, Riscv64Label* label, bool is_bare) {
  Bcond(label, is_bare, kCondGEZ, rt);
}

void Riscv64Assembler::Blezc(XRegister rt, Riscv64Label* label, bool is_bare) {
  Bcond(label, is_bare, kCondLEZ, rt);
}

void Riscv64Assembler::Bltuc(XRegister rs, XRegister rt, Riscv64Label* label, bool is_bare) {
  Bcond(label, is_bare, kCondLTU, rs, rt);
}

void Riscv64Assembler::Bgeuc(XRegister rs, XRegister rt, Riscv64Label* label, bool is_bare) {
  Bcond(label, is_bare, kCondGEU, rs, rt);
}

void Riscv64Assembler::Beqc(XRegister rs, XRegister rt, Riscv64Label* label, bool is_bare) {
  Bcond(label, is_bare, kCondEQ, rs, rt);
}

void Riscv64Assembler::Bnec(XRegister rs, XRegister rt, Riscv64Label* label, bool is_bare) {
  Bcond(label, is_bare, kCondNE, rs, rt);
}

void Riscv64Assembler::Beqzc(XRegister rs, Riscv64Label* label, bool is_bare) {
  Bcond(label, is_bare, kCondEQZ, rs);
}

void Riscv64Assembler::Bnezc(XRegister rs, Riscv64Label* label, bool is_bare) {
  Bcond(label, is_bare, kCondNEZ, rs);
}

void Riscv64Assembler::Bltz(XRegister rt, Riscv64Label* label, bool is_bare) {
  CHECK(is_bare);
  Bcond(label, is_bare, kCondLTZ, rt);
}

void Riscv64Assembler::Bgtz(XRegister rt, Riscv64Label* label, bool is_bare) {
  CHECK(is_bare);
  Bcond(label, is_bare, kCondGTZ, rt);
}

void Riscv64Assembler::Bgez(XRegister rt, Riscv64Label* label, bool is_bare) {
  CHECK(is_bare);
  Bcond(label, is_bare, kCondGEZ, rt);
}

void Riscv64Assembler::Blez(XRegister rt, Riscv64Label* label, bool is_bare) {
  CHECK(is_bare);
  Bcond(label, is_bare, kCondLEZ, rt);
}

void Riscv64Assembler::Beq(XRegister rs, XRegister rt, Riscv64Label* label, bool is_bare) {
  CHECK(is_bare);
  Bcond(label, is_bare, kCondEQ, rs, rt);
}

void Riscv64Assembler::Bne(XRegister rs, XRegister rt, Riscv64Label* label, bool is_bare) {
  CHECK(is_bare);
  Bcond(label, is_bare, kCondNE, rs, rt);
}

void Riscv64Assembler::Blt(XRegister rs, XRegister rt, Riscv64Label* label, bool is_bare) {
  CHECK(is_bare);
  Bcond(label, is_bare, kCondLT, rs, rt);
}

void Riscv64Assembler::Bge(XRegister rs, XRegister rt, Riscv64Label* label, bool is_bare) {
  CHECK(is_bare);
  Bcond(label, is_bare, kCondGE, rs, rt);
}

void Riscv64Assembler::Bltu(XRegister rs, XRegister rt, Riscv64Label* label, bool is_bare) {
  CHECK(is_bare);
  Bcond(label, is_bare, kCondLTU, rs, rt);
}

void Riscv64Assembler::Bgeu(XRegister rs, XRegister rt, Riscv64Label* label, bool is_bare) {
  CHECK(is_bare);
  Bcond(label, is_bare, kCondGEU, rs, rt);
}

void Riscv64Assembler::Beqz(XRegister rs, Riscv64Label* label, bool is_bare) {
  //  CHECK(is_bare);
  Bcond(label, is_bare, kCondEQZ, rs);
}

void Riscv64Assembler::Bnez(XRegister rs, Riscv64Label* label, bool is_bare) {
  //  CHECK(is_bare);
  Bcond(label, is_bare, kCondNEZ, rs);
}

void Riscv64Assembler::LoadFromOffset(LoadOperandType type,
                                      XRegister reg,
                                      XRegister base,
                                      int32_t offset) {
  LoadFromOffset<>(type, reg, base, offset);
}

void Riscv64Assembler::LoadFpuFromOffset(LoadOperandType type,
                                         FRegister reg,
                                         XRegister base,
                                         int32_t offset) {
  LoadFpuFromOffset<>(type, reg, base, offset);
}

/////////////////////////////// RV64 MACRO Instructions END ///////////////////////////////

void Riscv64Assembler::AdjustBaseAndOffset(XRegister& base, int32_t& offset, bool is_doubleword) {
  // This method is used to adjust the base register and offset pair
  // for a load/store when the offset doesn't fit into int16_t.
  // It is assumed that `base + offset` is sufficiently aligned for memory
  // operands that are machine word in size or smaller. For doubleword-sized
  // operands it's assumed that `base` is a multiple of 8, while `offset`
  // may be a multiple of 4 (e.g. 4-byte-aligned long and double arguments
  // and spilled variables on the stack accessed relative to the stack
  // pointer register).
  // We preserve the "alignment" of `offset` by adjusting it by a multiple of 8.
  CHECK_NE(base, AT);  // Must not overwrite the register `base` while loading `offset`.

  bool doubleword_aligned = IsAligned<kRiscv64DoublewordSize>(offset);
  bool two_accesses = is_doubleword && !doubleword_aligned;

  // IsInt<12> must be passed a signed value, hence the static cast below.
  if (IsInt<12>(offset) &&
      (!two_accesses || IsInt<12>(static_cast<int32_t>(offset + kRiscv64WordSize)))) {
    // Nothing to do: `offset` (and, if needed, `offset + 4`) fits into int12_t.
    return;
  }

  // Remember the "(mis)alignment" of `offset`, it will be checked at the end.
  uint32_t misalignment = offset & (kRiscv64DoublewordSize - 1);

  // First, see if `offset` can be represented as a sum of two 16-bit signed
  // offsets. This can save an instruction.
  // To simplify matters, only do this for a symmetric range of offsets from
  // about -64KB to about +64KB, allowing further addition of 4 when accessing
  // 64-bit variables with two 32-bit accesses.
  constexpr int32_t kMinOffsetForSimpleAdjustment = 0x7f8;  // Max int12_t that's a multiple of 8.
  constexpr int32_t kMaxOffsetForSimpleAdjustment = 2 * kMinOffsetForSimpleAdjustment;

  if (0 <= offset && offset <= kMaxOffsetForSimpleAdjustment) {
    Addi(AT, base, kMinOffsetForSimpleAdjustment);
    offset -= kMinOffsetForSimpleAdjustment;
  } else if (-kMaxOffsetForSimpleAdjustment <= offset && offset < 0) {
    Addi(AT, base, -kMinOffsetForSimpleAdjustment);
    offset += kMinOffsetForSimpleAdjustment;
  } else {
    // In more complex cases take advantage of the daui instruction, e.g.:
    //    daui   AT, base, offset_high
    //   [dahi   AT, 1]                       // When `offset` is close to +2GB.
    //    lw     reg_lo, offset_low(AT)
    //   [lw     reg_hi, (offset_low+4)(AT)]  // If misaligned 64-bit load.
    // or when offset_low+4 overflows int16_t:
    //    daui   AT, base, offset_high
    //    daddiu AT, AT, 8
    //    lw     reg_lo, (offset_low-8)(AT)
    //    lw     reg_hi, (offset_low-4)(AT)
    int32_t offset_low12 = 0xFFF & offset;
    int32_t offset_high20 = offset >> 12;

    if (offset_low12 & 0x800) {  // check int12_t sign bit
      offset_high20 += 1;
      offset_low12 |= 0xFFFFF000;  // sign extend offset_low12
    }

    Lui(AT, offset_high20);
    Add(AT, base, AT);

    if (two_accesses && !IsInt<12>(static_cast<int32_t>(offset_low12 + kRiscv64WordSize))) {
      // Avoid overflow in the 12-bit offset of the load/store instruction when adding 4.
      Addi(AT, AT, kRiscv64DoublewordSize);
      offset_low12 -= kRiscv64DoublewordSize;
    }

    offset = offset_low12;
  }
  base = AT;

  CHECK(IsInt<12>(offset));
  if (two_accesses) {
    CHECK(IsInt<12>(static_cast<int32_t>(offset + kRiscv64WordSize)));
  }
  CHECK_EQ(misalignment, offset & (kRiscv64DoublewordSize - 1));
}

void Riscv64Assembler::AdjustBaseOffsetAndElementSizeShift(XRegister& base,
                                                           int32_t& offset,
                                                           int& element_size_shift) {
  // This method is used to adjust the base register, offset and element_size_shift
  // for a vector load/store when the offset doesn't fit into allowed number of bits.
  // MSA ld.df and st.df instructions take signed offsets as arguments, but maximum
  // offset is dependant on the size of the data format df (10-bit offsets for ld.b,
  // 11-bit for ld.h, 12-bit for ld.w and 13-bit for ld.d).
  // If element_size_shift is non-negative at entry, it won't be changed, but offset
  // will be checked for appropriate alignment. If negative at entry, it will be
  // adjusted based on offset for maximum fit.
  // It's assumed that `base` is a multiple of 8.

  CHECK_NE(base, AT);  // Must not overwrite the register `base` while loading `offset`.

  if (element_size_shift >= 0) {
    CHECK_LE(element_size_shift, TIMES_8);
    CHECK_GE(JAVASTYLE_CTZ(offset), element_size_shift);
  } else if (IsAligned<kRiscv64DoublewordSize>(offset)) {
    element_size_shift = TIMES_8;
  } else if (IsAligned<kRiscv64WordSize>(offset)) {
    element_size_shift = TIMES_4;
  } else if (IsAligned<kRiscv64HalfwordSize>(offset)) {
    element_size_shift = TIMES_2;
  } else {
    element_size_shift = TIMES_1;
  }

  const int low_len = 10 + element_size_shift;  // How many low bits of `offset` ld.df/st.df
                                                // will take.
  int16_t low = offset & ((1 << low_len) - 1);  // Isolate these bits.
  low -= (low & (1 << (low_len - 1))) << 1;     // Sign-extend these bits.
  if (low == offset) {
    return;  // `offset` fits into ld.df/st.df.
  }

  // First, see if `offset` can be represented as a sum of two signed offsets.
  // This can save an instruction.

  // Max int16_t that's a multiple of element size.
  const int32_t kMaxDeltaForSimpleAdjustment = 0x7f8 - (1 << element_size_shift);
  // Max ld.df/st.df offset that's a multiple of element size.
  const int32_t kMaxLoadStoreOffset = 0x1ff << element_size_shift;
  const int32_t kMaxOffsetForSimpleAdjustment = kMaxDeltaForSimpleAdjustment + kMaxLoadStoreOffset;

  if (IsInt<12>(offset)) {
    Addiu(AT, base, offset);
    offset = 0;
  } else if (0 <= offset && offset <= kMaxOffsetForSimpleAdjustment) {
    Addiu(AT, base, kMaxDeltaForSimpleAdjustment);
    offset -= kMaxDeltaForSimpleAdjustment;
  } else if (-kMaxOffsetForSimpleAdjustment <= offset && offset < 0) {
    Addiu(AT, base, -kMaxDeltaForSimpleAdjustment);
    offset += kMaxDeltaForSimpleAdjustment;
  } else {
    // Let's treat `offset` as 64-bit to simplify handling of sign
    // extensions in the instructions that supply its smaller signed parts.
    //
    // 16-bit or smaller parts of `offset`:
    // |63  top  48|47  hi  32|31  upper  16|15  mid  13-10|12-9  low  0|
    //
    // Instructions that supply each part as a signed integer addend:
    // |dati       |dahi      |daui         |daddiu        |ld.df/st.df |
    //
    // `top` is always 0, so dati isn't used.
    // `hi` is 1 when `offset` is close to +2GB and 0 otherwise.
    uint64_t tmp = static_cast<uint64_t>(offset) - low;  // Exclude `low` from the rest of `offset`
                                                         // (accounts for sign of `low`).
    tmp += (tmp & (UINT64_C(1) << 15)) << 1;             // Account for sign extension in daddiu.
    tmp += (tmp & (UINT64_C(1) << 31)) << 1;             // Account for sign extension in daui.
    int16_t mid = Low16Bits(tmp);
    int16_t upper = High16Bits(tmp);
    int16_t hi = Low16Bits(High32Bits(tmp));
    Aui(AT, base, upper);
    if (hi != 0) {
      CHECK_EQ(hi, 1);
      Ahi(AT, hi);
    }
    if (mid != 0) {
      Addiu(AT, AT, mid);
    }
    offset = low;
  }
  base = AT;
  CHECK_GE(JAVASTYLE_CTZ(offset), element_size_shift);
  CHECK(IsInt<10>(offset >> element_size_shift));
}

void Riscv64Assembler::EmitLoad(ManagedRegister m_dst,
                                XRegister src_register,
                                int32_t src_offset,
                                size_t size) {
  Riscv64ManagedRegister dst = m_dst.AsRiscv64();
  if (dst.IsNoRegister()) {
    CHECK_EQ(0u, size) << dst;
  } else if (dst.IsXRegister()) {
    if (size == 4) {
      LoadFromOffset(kLoadWord, dst.AsXRegister(), src_register, src_offset);
    } else if (size == 8) {
      CHECK_EQ(8u, size) << dst;
      LoadFromOffset(kLoadDoubleword, dst.AsXRegister(), src_register, src_offset);
    } else {
      UNIMPLEMENTED(FATAL) << "We only support Load() of size 4 and 8";
    }
  } else if (dst.IsFRegister()) {
    if (size == 4) {
      CHECK_EQ(4u, size) << dst;
      LoadFpuFromOffset(kLoadWord, dst.AsFRegister(), src_register, src_offset);
    } else if (size == 8) {
      CHECK_EQ(8u, size) << dst;
      LoadFpuFromOffset(kLoadDoubleword, dst.AsFRegister(), src_register, src_offset);
    } else {
      UNIMPLEMENTED(FATAL) << "We only support Load() of size 4 and 8";
    }
  }
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

static dwarf::Reg DWARFReg(XRegister reg) { return dwarf::Reg::Riscv64Core(static_cast<int>(reg)); }

static dwarf::Reg DWARFFReg(FRegister reg) { return dwarf::Reg::Riscv64Fp(static_cast<int>(reg)); }

constexpr size_t kFramePointerSize = 8;

void Riscv64Assembler::BuildFrame(size_t frame_size,
                                  ManagedRegister method_reg,
                                  ArrayRef<const ManagedRegister> callee_save_regs) {
  CHECK_ALIGNED(frame_size, kStackAlignment);
  DCHECK(!overwriting_);

  // Increase frame to required size.
  IncreaseFrameSize(frame_size);

  // Skip spilling when there's no reg in callee_save_regs.
  if (callee_save_regs.size() > 0) {
    // Push callee saves and return address
    int stack_offset = frame_size - kFramePointerSize;
    StoreToOffset(kStoreDoubleword, RA, SP, stack_offset);
    cfi_.RelOffset(DWARFReg(RA), stack_offset);
    for (int i = callee_save_regs.size() - 1; i >= 0; --i) {
      stack_offset -= kFramePointerSize;
      if (callee_save_regs[i].AsRiscv64().IsXRegister()) {
        XRegister reg = callee_save_regs[i].AsRiscv64().AsXRegister();

        if (reg == RA)
          continue;  // RA is spilled already.

        StoreToOffset(kStoreDoubleword, reg, SP, stack_offset);
        cfi_.RelOffset(DWARFReg(reg), stack_offset);
      } else {
        FRegister reg = callee_save_regs[i].AsRiscv64().AsFRegister();
        StoreFpuToOffset(kStoreDoubleword, reg, SP, stack_offset);
        cfi_.RelOffset(DWARFFReg(reg), stack_offset);
      }
    }
  }

  if (method_reg.IsRegister()) {
    // Write ArtMethod*
    StoreToOffset(kStoreDoubleword, method_reg.AsRiscv64().AsXRegister(), SP, 0);
  }
}

void Riscv64Assembler::RemoveFrame(size_t frame_size,
                                   ArrayRef<const ManagedRegister> callee_save_regs,
                                   bool may_suspend ATTRIBUTE_UNUSED) {
  CHECK_ALIGNED(frame_size, kStackAlignment);
  DCHECK(!overwriting_);
  cfi_.RememberState();

  // Skip unspilling when there's no reg in callee_save_regs.
  if (callee_save_regs.size() > 0) {
    // Pop callee saves and return address
    int stack_offset = frame_size - kFramePointerSize;

    // Don't update the stack_offet for critical native.
    if (callee_save_regs.size() > 1)
      stack_offset -= (callee_save_regs.size() * kFramePointerSize);

    for (size_t i = 0; i < callee_save_regs.size(); ++i) {
      if (callee_save_regs[i].AsRiscv64().IsXRegister()) {
        XRegister reg = callee_save_regs[i].AsRiscv64().AsXRegister();

        if (reg == RA)
          continue;  // RA will be unspilled at last.

        LoadFromOffset(kLoadDoubleword, reg, SP, stack_offset);
        cfi_.Restore(DWARFReg(reg));
      } else {
        FRegister reg = callee_save_regs[i].AsRiscv64().AsFRegister();
        LoadFpuFromOffset(kLoadDoubleword, reg, SP, stack_offset);
        cfi_.Restore(DWARFFReg(reg));
      }
      stack_offset += kFramePointerSize;
    }
    LoadFromOffset(kLoadDoubleword, RA, SP, stack_offset);
    cfi_.Restore(DWARFReg(RA));
  }

  // Decrease frame to required size.
  DecreaseFrameSize(frame_size);

  // Then jump to the return address.
  Jr(RA);
  Nop();

  // The CFI should be restored for any code that follows the exit block.
  cfi_.RestoreState();
  cfi_.DefCFAOffset(frame_size);
}

void Riscv64Assembler::IncreaseFrameSize(size_t adjust) {
  CHECK_ALIGNED(adjust, kFramePointerSize);
  DCHECK(!overwriting_);
  Addiu64(SP, SP, static_cast<int32_t>(-adjust));
  cfi_.AdjustCFAOffset(adjust);
}

void Riscv64Assembler::DecreaseFrameSize(size_t adjust) {
  CHECK_ALIGNED(adjust, kFramePointerSize);
  DCHECK(!overwriting_);
  Addiu64(SP, SP, static_cast<int32_t>(adjust));
  cfi_.AdjustCFAOffset(-adjust);
}

void Riscv64Assembler::Store(FrameOffset dest, ManagedRegister msrc, size_t size) {
  Riscv64ManagedRegister src = msrc.AsRiscv64();
  if (src.IsNoRegister()) {
    CHECK_EQ(0u, size);
  } else if (src.IsXRegister()) {
    CHECK(size == 4 || size == 8) << size;
    if (size == 8) {
      StoreToOffset(kStoreDoubleword, src.AsXRegister(), SP, dest.Int32Value());
    } else if (size == 4) {
      StoreToOffset(kStoreWord, src.AsXRegister(), SP, dest.Int32Value());
    } else {
      UNIMPLEMENTED(FATAL) << "We only support Store() of size 4 and 8";
    }
  } else if (src.IsFRegister()) {
    CHECK(size == 4 || size == 8) << size;
    if (size == 8) {
      StoreFpuToOffset(kStoreDoubleword, src.AsFRegister(), SP, dest.Int32Value());
    } else if (size == 4) {
      StoreFpuToOffset(kStoreWord, src.AsFRegister(), SP, dest.Int32Value());
    } else {
      UNIMPLEMENTED(FATAL) << "We only support Store() of size 4 and 8";
    }
  }
}

void Riscv64Assembler::StoreRef(FrameOffset dest, ManagedRegister msrc) {
  Riscv64ManagedRegister src = msrc.AsRiscv64();
  CHECK(src.IsXRegister());
  StoreToOffset(kStoreWord, src.AsXRegister(), SP, dest.Int32Value());
}

void Riscv64Assembler::StoreRawPtr(FrameOffset dest, ManagedRegister msrc) {
  Riscv64ManagedRegister src = msrc.AsRiscv64();
  CHECK(src.IsXRegister());
  StoreToOffset(kStoreDoubleword, src.AsXRegister(), SP, dest.Int32Value());
}

void Riscv64Assembler::StoreImmediateToFrame(FrameOffset dest,
                                             uint32_t imm,
                                             ManagedRegister mscratch) {
  Riscv64ManagedRegister scratch = mscratch.AsRiscv64();
  CHECK(scratch.IsXRegister()) << scratch;
  LoadConst32(scratch.AsXRegister(), imm);
  StoreToOffset(kStoreWord, scratch.AsXRegister(), SP, dest.Int32Value());
}

void Riscv64Assembler::StoreStackOffsetToThread(ThreadOffset64 thr_offs,
                                                FrameOffset fr_offs,
                                                ManagedRegister mscratch) {
  Riscv64ManagedRegister scratch = mscratch.AsRiscv64();
  CHECK(scratch.IsXRegister()) << scratch;
  Addiu64(scratch.AsXRegister(), SP, fr_offs.Int32Value());
  StoreToOffset(kStoreDoubleword, scratch.AsXRegister(), S1, thr_offs.Int32Value());
}

void Riscv64Assembler::StoreStackPointerToThread(ThreadOffset64 thr_offs) {
  StoreToOffset(kStoreDoubleword, SP, S1, thr_offs.Int32Value());
}

void Riscv64Assembler::StoreSpanning(FrameOffset dest,
                                     ManagedRegister msrc,
                                     FrameOffset in_off,
                                     ManagedRegister mscratch) {
  Riscv64ManagedRegister src = msrc.AsRiscv64();
  Riscv64ManagedRegister scratch = mscratch.AsRiscv64();
  StoreToOffset(kStoreDoubleword, src.AsXRegister(), SP, dest.Int32Value());
  LoadFromOffset(kLoadDoubleword, scratch.AsXRegister(), SP, in_off.Int32Value());
  StoreToOffset(kStoreDoubleword, scratch.AsXRegister(), SP, dest.Int32Value() + 8);
}

void Riscv64Assembler::Load(ManagedRegister mdest, FrameOffset src, size_t size) {
  return EmitLoad(mdest, SP, src.Int32Value(), size);
}

void Riscv64Assembler::LoadFromThread(ManagedRegister mdest, ThreadOffset64 src, size_t size) {
  return EmitLoad(mdest, S1, src.Int32Value(), size);
}

void Riscv64Assembler::LoadRef(ManagedRegister mdest, FrameOffset src) {
  Riscv64ManagedRegister dest = mdest.AsRiscv64();
  CHECK(dest.IsXRegister());
  LoadFromOffset(kLoadUnsignedWord, dest.AsXRegister(), SP, src.Int32Value());
}

void Riscv64Assembler::LoadRef(ManagedRegister mdest,
                               ManagedRegister base,
                               MemberOffset offs,
                               bool unpoison_reference) {
  Riscv64ManagedRegister dest = mdest.AsRiscv64();
  CHECK(dest.IsXRegister() && base.AsRiscv64().IsXRegister());
  LoadFromOffset(
      kLoadUnsignedWord, dest.AsXRegister(), base.AsRiscv64().AsXRegister(), offs.Int32Value());
  if (unpoison_reference) {
    MaybeUnpoisonHeapReference(dest.AsXRegister());
  }
}

void Riscv64Assembler::LoadRawPtr(ManagedRegister mdest, ManagedRegister base, Offset offs) {
  Riscv64ManagedRegister dest = mdest.AsRiscv64();
  CHECK(dest.IsXRegister() && base.AsRiscv64().IsXRegister());
  LoadFromOffset(
      kLoadDoubleword, dest.AsXRegister(), base.AsRiscv64().AsXRegister(), offs.Int32Value());
}

void Riscv64Assembler::LoadRawPtrFromThread(ManagedRegister mdest, ThreadOffset64 offs) {
  Riscv64ManagedRegister dest = mdest.AsRiscv64();
  CHECK(dest.IsXRegister());
  LoadFromOffset(kLoadDoubleword, dest.AsXRegister(), S1, offs.Int32Value());
}

void Riscv64Assembler::SignExtend(ManagedRegister mreg ATTRIBUTE_UNUSED,
                                  size_t size ATTRIBUTE_UNUSED) {
  UNIMPLEMENTED(FATAL) << "No sign extension necessary for RISCV64";
}

void Riscv64Assembler::ZeroExtend(ManagedRegister mreg ATTRIBUTE_UNUSED,
                                  size_t size ATTRIBUTE_UNUSED) {
  UNIMPLEMENTED(FATAL) << "No zero extension necessary for RISCV64";
}

void Riscv64Assembler::Move(ManagedRegister mdest, ManagedRegister msrc, size_t size) {
  Riscv64ManagedRegister dest = mdest.AsRiscv64();
  Riscv64ManagedRegister src = msrc.AsRiscv64();
  if (!dest.Equals(src)) {
    if (dest.IsXRegister()) {
      CHECK(src.IsXRegister()) << src;
      Move(dest.AsXRegister(), src.AsXRegister());
    } else if (dest.IsFRegister()) {
      CHECK(src.IsFRegister()) << src;
      if (size == 4) {
        MovS(dest.AsFRegister(), src.AsFRegister());
      } else if (size == 8) {
        MovD(dest.AsFRegister(), src.AsFRegister());
      } else {
        UNIMPLEMENTED(FATAL) << "We only support Copy() of size 4 and 8";
      }
    }
  }
}

void Riscv64Assembler::CopyRef(FrameOffset dest, FrameOffset src, ManagedRegister mscratch) {
  Riscv64ManagedRegister scratch = mscratch.AsRiscv64();
  CHECK(scratch.IsXRegister()) << scratch;
  LoadFromOffset(kLoadWord, scratch.AsXRegister(), SP, src.Int32Value());
  StoreToOffset(kStoreWord, scratch.AsXRegister(), SP, dest.Int32Value());
}

void Riscv64Assembler::CopyRawPtrFromThread(FrameOffset fr_offs,
                                            ThreadOffset64 thr_offs,
                                            ManagedRegister mscratch) {
  Riscv64ManagedRegister scratch = mscratch.AsRiscv64();
  CHECK(scratch.IsXRegister()) << scratch;
  LoadFromOffset(kLoadDoubleword, scratch.AsXRegister(), S1, thr_offs.Int32Value());
  StoreToOffset(kStoreDoubleword, scratch.AsXRegister(), SP, fr_offs.Int32Value());
}

void Riscv64Assembler::CopyRawPtrToThread(ThreadOffset64 thr_offs,
                                          FrameOffset fr_offs,
                                          ManagedRegister mscratch) {
  Riscv64ManagedRegister scratch = mscratch.AsRiscv64();
  CHECK(scratch.IsXRegister()) << scratch;
  LoadFromOffset(kLoadDoubleword, scratch.AsXRegister(), SP, fr_offs.Int32Value());
  StoreToOffset(kStoreDoubleword, scratch.AsXRegister(), S1, thr_offs.Int32Value());
}

void Riscv64Assembler::Copy(FrameOffset dest,
                            FrameOffset src,
                            ManagedRegister mscratch,
                            size_t size) {
  Riscv64ManagedRegister scratch = mscratch.AsRiscv64();
  CHECK(scratch.IsXRegister()) << scratch;
  CHECK(size == 4 || size == 8) << size;
  if (size == 4) {
    LoadFromOffset(kLoadWord, scratch.AsXRegister(), SP, src.Int32Value());
    StoreToOffset(kStoreDoubleword, scratch.AsXRegister(), SP, dest.Int32Value());
  } else if (size == 8) {
    LoadFromOffset(kLoadDoubleword, scratch.AsXRegister(), SP, src.Int32Value());
    StoreToOffset(kStoreDoubleword, scratch.AsXRegister(), SP, dest.Int32Value());
  } else {
    UNIMPLEMENTED(FATAL) << "We only support Copy() of size 4 and 8";
  }
}

void Riscv64Assembler::Copy(FrameOffset dest,
                            ManagedRegister src_base,
                            Offset src_offset,
                            ManagedRegister mscratch,
                            size_t size) {
  XRegister scratch = mscratch.AsRiscv64().AsXRegister();
  CHECK(size == 4 || size == 8) << size;
  if (size == 4) {
    LoadFromOffset(kLoadWord, scratch, src_base.AsRiscv64().AsXRegister(), src_offset.Int32Value());
    StoreToOffset(kStoreDoubleword, scratch, SP, dest.Int32Value());
  } else if (size == 8) {
    LoadFromOffset(
        kLoadDoubleword, scratch, src_base.AsRiscv64().AsXRegister(), src_offset.Int32Value());
    StoreToOffset(kStoreDoubleword, scratch, SP, dest.Int32Value());
  } else {
    UNIMPLEMENTED(FATAL) << "We only support Copy() of size 4 and 8";
  }
}

void Riscv64Assembler::Copy(ManagedRegister dest_base,
                            Offset dest_offset,
                            FrameOffset src,
                            ManagedRegister mscratch,
                            size_t size) {
  XRegister scratch = mscratch.AsRiscv64().AsXRegister();
  CHECK(size == 4 || size == 8) << size;
  if (size == 4) {
    LoadFromOffset(kLoadWord, scratch, SP, src.Int32Value());
    StoreToOffset(
        kStoreDoubleword, scratch, dest_base.AsRiscv64().AsXRegister(), dest_offset.Int32Value());
  } else if (size == 8) {
    LoadFromOffset(kLoadDoubleword, scratch, SP, src.Int32Value());
    StoreToOffset(
        kStoreDoubleword, scratch, dest_base.AsRiscv64().AsXRegister(), dest_offset.Int32Value());
  } else {
    UNIMPLEMENTED(FATAL) << "We only support Copy() of size 4 and 8";
  }
}

void Riscv64Assembler::Copy(FrameOffset dest ATTRIBUTE_UNUSED,
                            FrameOffset src_base ATTRIBUTE_UNUSED,
                            Offset src_offset ATTRIBUTE_UNUSED,
                            ManagedRegister mscratch ATTRIBUTE_UNUSED,
                            size_t size ATTRIBUTE_UNUSED) {
  UNIMPLEMENTED(FATAL) << "No RISCV64 implementation";
}

void Riscv64Assembler::Copy(ManagedRegister dest,
                            Offset dest_offset,
                            ManagedRegister src,
                            Offset src_offset,
                            ManagedRegister mscratch,
                            size_t size) {
  XRegister scratch = mscratch.AsRiscv64().AsXRegister();
  CHECK(size == 4 || size == 8) << size;
  if (size == 4) {
    LoadFromOffset(kLoadWord, scratch, src.AsRiscv64().AsXRegister(), src_offset.Int32Value());
    StoreToOffset(
        kStoreDoubleword, scratch, dest.AsRiscv64().AsXRegister(), dest_offset.Int32Value());
  } else if (size == 8) {
    LoadFromOffset(
        kLoadDoubleword, scratch, src.AsRiscv64().AsXRegister(), src_offset.Int32Value());
    StoreToOffset(
        kStoreDoubleword, scratch, dest.AsRiscv64().AsXRegister(), dest_offset.Int32Value());
  } else {
    UNIMPLEMENTED(FATAL) << "We only support Copy() of size 4 and 8";
  }
}

void Riscv64Assembler::Copy(FrameOffset dest ATTRIBUTE_UNUSED,
                            Offset dest_offset ATTRIBUTE_UNUSED,
                            FrameOffset src ATTRIBUTE_UNUSED,
                            Offset src_offset ATTRIBUTE_UNUSED,
                            ManagedRegister mscratch ATTRIBUTE_UNUSED,
                            size_t size ATTRIBUTE_UNUSED) {
  UNIMPLEMENTED(FATAL) << "No RISCV64 implementation";
}

void Riscv64Assembler::MemoryBarrier(ManagedRegister mreg ATTRIBUTE_UNUSED) {
  // sync?
  UNIMPLEMENTED(FATAL) << "No RISCV64 implementation";
}

void Riscv64Assembler::CreateHandleScopeEntry(ManagedRegister mout_reg,
                                              FrameOffset handle_scope_offset,
                                              ManagedRegister min_reg,
                                              bool null_allowed) {
  Riscv64ManagedRegister out_reg = mout_reg.AsRiscv64();
  Riscv64ManagedRegister in_reg = min_reg.AsRiscv64();
  CHECK(in_reg.IsNoRegister() || in_reg.IsXRegister()) << in_reg;
  CHECK(out_reg.IsXRegister()) << out_reg;
  if (null_allowed) {
    Riscv64Label null_arg;
    // Null values get a handle scope entry value of 0.  Otherwise, the handle scope entry is
    // the address in the handle scope holding the reference.
    // e.g. out_reg = (handle == 0) ? 0 : (SP+handle_offset)
    if (in_reg.IsNoRegister()) {
      LoadFromOffset(
          kLoadUnsignedWord, out_reg.AsXRegister(), SP, handle_scope_offset.Int32Value());
      in_reg = out_reg;
    }
    if (!out_reg.Equals(in_reg)) {
      LoadConst32(out_reg.AsXRegister(), 0);
    }
    Beqzc(in_reg.AsXRegister(), &null_arg);
    Addiu64(out_reg.AsXRegister(), SP, handle_scope_offset.Int32Value());
    Bind(&null_arg);
  } else {
    Addiu64(out_reg.AsXRegister(), SP, handle_scope_offset.Int32Value());
  }
}

void Riscv64Assembler::CreateHandleScopeEntry(FrameOffset out_off,
                                              FrameOffset handle_scope_offset,
                                              ManagedRegister mscratch,
                                              bool null_allowed) {
  Riscv64ManagedRegister scratch = mscratch.AsRiscv64();
  CHECK(scratch.IsXRegister()) << scratch;
  if (null_allowed) {
    Riscv64Label null_arg;
    LoadFromOffset(kLoadUnsignedWord, scratch.AsXRegister(), SP, handle_scope_offset.Int32Value());
    // Null values get a handle scope entry value of 0.  Otherwise, the handle scope entry is
    // the address in the handle scope holding the reference.
    // e.g. scratch = (scratch == 0) ? 0 : (SP+handle_scope_offset)
    Beqzc(scratch.AsXRegister(), &null_arg);
    Addiu64(scratch.AsXRegister(), SP, handle_scope_offset.Int32Value());
    Bind(&null_arg);
  } else {
    Addiu64(scratch.AsXRegister(), SP, handle_scope_offset.Int32Value());
  }
  StoreToOffset(kStoreDoubleword, scratch.AsXRegister(), SP, out_off.Int32Value());
}

// Given a handle scope entry, load the associated reference.
void Riscv64Assembler::LoadReferenceFromHandleScope(ManagedRegister mout_reg,
                                                    ManagedRegister min_reg) {
  Riscv64ManagedRegister out_reg = mout_reg.AsRiscv64();
  Riscv64ManagedRegister in_reg = min_reg.AsRiscv64();
  CHECK(out_reg.IsXRegister()) << out_reg;
  CHECK(in_reg.IsXRegister()) << in_reg;
  Riscv64Label null_arg;
  if (!out_reg.Equals(in_reg)) {
    LoadConst32(out_reg.AsXRegister(), 0);
  }
  Beqzc(in_reg.AsXRegister(), &null_arg);
  LoadFromOffset(kLoadDoubleword, out_reg.AsXRegister(), in_reg.AsXRegister(), 0);
  Bind(&null_arg);
}

void Riscv64Assembler::VerifyObject(ManagedRegister src ATTRIBUTE_UNUSED,
                                    bool could_be_null ATTRIBUTE_UNUSED) {
  UNIMPLEMENTED(FATAL) << "No RISCV64 implementation";
}

void Riscv64Assembler::VerifyObject(FrameOffset src ATTRIBUTE_UNUSED,
                                    bool could_be_null ATTRIBUTE_UNUSED) {
  UNIMPLEMENTED(FATAL) << "No RISCV64 implementation";
}

void Riscv64Assembler::Call(ManagedRegister mbase, Offset offset, ManagedRegister mscratch) {
  Riscv64ManagedRegister base = mbase.AsRiscv64();
  Riscv64ManagedRegister scratch = mscratch.AsRiscv64();
  CHECK(base.IsXRegister()) << base;
  CHECK(scratch.IsXRegister()) << scratch;
  LoadFromOffset(kLoadDoubleword, scratch.AsXRegister(), base.AsXRegister(), offset.Int32Value());
  Jalr(scratch.AsXRegister());
  Nop();
}

void Riscv64Assembler::Call(FrameOffset base, Offset offset, ManagedRegister mscratch) {
  Riscv64ManagedRegister scratch = mscratch.AsRiscv64();
  CHECK(scratch.IsXRegister()) << scratch;
  // Call *(*(SP + base) + offset)
  LoadFromOffset(kLoadDoubleword, scratch.AsXRegister(), SP, base.Int32Value());
  LoadFromOffset(
      kLoadDoubleword, scratch.AsXRegister(), scratch.AsXRegister(), offset.Int32Value());
  Jalr(scratch.AsXRegister());
  Nop();
}

void Riscv64Assembler::CallFromThread(ThreadOffset64 offset ATTRIBUTE_UNUSED,
                                      ManagedRegister mscratch ATTRIBUTE_UNUSED) {
  UNIMPLEMENTED(FATAL) << "No RISCV64 implementation";
}

void Riscv64Assembler::GetCurrentThread(ManagedRegister tr) {
  Move(tr.AsRiscv64().AsXRegister(), S1);
}

void Riscv64Assembler::GetCurrentThread(FrameOffset offset,
                                        ManagedRegister mscratch ATTRIBUTE_UNUSED) {
  StoreToOffset(kStoreDoubleword, S1, SP, offset.Int32Value());
}

void Riscv64Assembler::ExceptionPoll(ManagedRegister mscratch, size_t stack_adjust) {
  Riscv64ManagedRegister scratch = mscratch.AsRiscv64();
  exception_blocks_.emplace_back(scratch, stack_adjust);
  LoadFromOffset(kLoadDoubleword,
                 scratch.AsXRegister(),
                 S1,
                 Thread::ExceptionOffset<kRiscv64PointerSize>().Int32Value());
  Bnezc(scratch.AsXRegister(), exception_blocks_.back().Entry());
}

void Riscv64Assembler::EmitExceptionPoll(Riscv64ExceptionSlowPath* exception) {
  Bind(exception->Entry());
  if (exception->stack_adjust_ != 0) {  // Fix up the frame.
    DecreaseFrameSize(exception->stack_adjust_);
  }
  // Pass exception object as argument.
  // Don't care about preserving A0 as this call won't return.
  CheckEntrypointTypes<kQuickDeliverException, void, mirror::Object*>();
  Move(A0, exception->scratch_.AsXRegister());
  // Set up call to Thread::Current()->pDeliverException
  LoadFromOffset(kLoadDoubleword,
                 T6,
                 S1,
                 QUICK_ENTRYPOINT_OFFSET(kRiscv64PointerSize, pDeliverException).Int32Value());
  Jr(T6);
  Nop();

  // Call never returns
  Break();
}

/////////////////////////////// RV64 VARIANTS extension end ////////////

}  // namespace riscv64
}  // namespace art
