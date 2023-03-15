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

namespace art {
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

/////////////////////////////// RV64 VARIANTS extension ///////////////////////////////

/////////////////////////////// RV64 "IM" Instructions ///////////////////////////////

// LUI/AUIPC (RV32I, with sign-extension on RV64I), opcode = 0x17, 0x37

void Riscv64Assembler::Lui(XRegister rd, uint32_t imm20) {
  EmitU(imm20, rd, 0x37);
}

void Riscv64Assembler::Auipc(XRegister rd, uint32_t imm20) {
  EmitU(imm20, rd, 0x17);
}

// Jump instructions (RV32I), opcode = 0x67, 0x6f

void Riscv64Assembler::Jal(XRegister rd, int32_t offset) {
  EmitJ(offset, rd, 0x6F);
}

void Riscv64Assembler::Jalr(XRegister rd, XRegister rs1, int32_t offset) {
  EmitI(offset, rs1, 0x0, rd, 0x67);
}

// Branch instructions, opcode = 0x63 (subfunc from 0x0 ~ 0x7), 0x67, 0x6f

void Riscv64Assembler::Beq(XRegister rs1, XRegister rs2, int32_t offset) {
  EmitB(offset, rs2, rs1, 0x0, 0x63);
}

void Riscv64Assembler::Bne(XRegister rs1, XRegister rs2, int32_t offset) {
  EmitB(offset, rs2, rs1, 0x1, 0x63);
}

void Riscv64Assembler::Blt(XRegister rs1, XRegister rs2, int32_t offset) {
  EmitB(offset, rs2, rs1, 0x4, 0x63);
}

void Riscv64Assembler::Bge(XRegister rs1, XRegister rs2, int32_t offset) {
  EmitB(offset, rs2, rs1, 0x5, 0x63);
}

void Riscv64Assembler::Bltu(XRegister rs1, XRegister rs2, int32_t offset) {
  EmitB(offset, rs2, rs1, 0x6, 0x63);
}

void Riscv64Assembler::Bgeu(XRegister rs1, XRegister rs2, int32_t offset) {
  EmitB(offset, rs2, rs1, 0x7, 0x63);
}

// Load instructions (RV32I+RV64I): opcode = 0x03, funct3 from 0x0 ~ 0x6

void Riscv64Assembler::Lb(XRegister rd, XRegister rs1, int32_t offset) {
  EmitI(offset, rs1, 0x0, rd, 0x03);
}

void Riscv64Assembler::Lh(XRegister rd, XRegister rs1, int32_t offset) {
  EmitI(offset, rs1, 0x1, rd, 0x03);
}

void Riscv64Assembler::Lw(XRegister rd, XRegister rs1, int32_t offset) {
  EmitI(offset, rs1, 0x2, rd, 0x03);
}

void Riscv64Assembler::Ld(XRegister rd, XRegister rs1, int32_t offset) {
  EmitI(offset, rs1, 0x3, rd, 0x03);
}

void Riscv64Assembler::Lbu(XRegister rd, XRegister rs1, int32_t offset) {
  EmitI(offset, rs1, 0x4, rd, 0x03);
}

void Riscv64Assembler::Lhu(XRegister rd, XRegister rs1, int32_t offset) {
  EmitI(offset, rs1, 0x5, rd, 0x03);
}

void Riscv64Assembler::Lwu(XRegister rd, XRegister rs1, int32_t offset) {
  EmitI(offset, rs1, 0x6, rd, 0x3);
}

// Store instructions (RV32I+RV64I): opcode = 0x23, funct3 from 0x0 ~ 0x3

void Riscv64Assembler::Sb(XRegister rs2, XRegister rs1, int32_t offset) {
  EmitS(offset, rs2, rs1, 0x0, 0x23);
}

void Riscv64Assembler::Sh(XRegister rs2, XRegister rs1, int32_t offset) {
  EmitS(offset, rs2, rs1, 0x1, 0x23);
}

void Riscv64Assembler::Sw(XRegister rs2, XRegister rs1, int32_t offset) {
  EmitS(offset, rs2, rs1, 0x2, 0x23);
}

void Riscv64Assembler::Sd(XRegister rs2, XRegister rs1, int32_t offset) {
  EmitS(offset, rs2, rs1, 0x3, 0x23);
}

// IMM ALU instructions (RV32I): opcode = 0x13, funct3 from 0x0 ~ 0x7

void Riscv64Assembler::Addi(XRegister rd, XRegister rs1, int32_t imm12) {
  EmitI(imm12, rs1, 0x0, rd, 0x13);
}

void Riscv64Assembler::Slti(XRegister rd, XRegister rs1, int32_t imm12) {
  EmitI(imm12, rs1, 0x2, rd, 0x13);
}

void Riscv64Assembler::Sltiu(XRegister rd, XRegister rs1, int32_t imm12) {
  EmitI(imm12, rs1, 0x3, rd, 0x13);
}

void Riscv64Assembler::Xori(XRegister rd, XRegister rs1, int32_t imm12) {
  EmitI(imm12, rs1, 0x4, rd, 0x13);
}

void Riscv64Assembler::Ori(XRegister rd, XRegister rs1, int32_t imm12) {
  EmitI(imm12, rs1, 0x6, rd, 0x13);
}

void Riscv64Assembler::Andi(XRegister rd, XRegister rs1, int32_t imm12) {
  EmitI(imm12, rs1, 0x7, rd, 0x13);
}

// 0x1 Split: 0x0(6b) + imm12(6b)
void Riscv64Assembler::Slli(XRegister rd, XRegister rs1, int32_t shamt) {
  CHECK(static_cast<uint32_t>(shamt) < 64) << shamt;
  EmitI6(0x0, shamt, rs1, 0x1, rd, 0x13);
}

// 0x5 Split: 0x0(6b) + imm12(6b)
void Riscv64Assembler::Srli(XRegister rd, XRegister rs1, int32_t shamt) {
  CHECK(static_cast<uint32_t>(shamt) < 64) << shamt;
  EmitI6(0x0, shamt, rs1, 0x5, rd, 0x13);
}

// 0x5 Split: 0x10(6b) + imm12(6b)
void Riscv64Assembler::Srai(XRegister rd, XRegister rs1, int32_t shamt) {
  CHECK(static_cast<uint32_t>(shamt) < 64) << shamt;
  EmitI6(0x10, shamt, rs1, 0x5, rd, 0x13);
}

// ALU instructions (RV32I): opcode = 0x33, funct3 from 0x0 ~ 0x7

void Riscv64Assembler::Add(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x0, rs2, rs1, 0x0, rd, 0x33);
}

void Riscv64Assembler::Sub(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x20, rs2, rs1, 0x0, rd, 0x33);
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

void Riscv64Assembler::Or(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x0, rs2, rs1, 0x06, rd, 0x33);
}

void Riscv64Assembler::And(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x0, rs2, rs1, 0x07, rd, 0x33);
}

void Riscv64Assembler::Sll(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x0, rs2, rs1, 0x01, rd, 0x33);
}

void Riscv64Assembler::Srl(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x0, rs2, rs1, 0x05, rd, 0x33);
}

void Riscv64Assembler::Sra(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x20, rs2, rs1, 0x05, rd, 0x33);
}

// 32bit Imm ALU instructions (RV64I): opcode = 0x1b, funct3 from 0x0, 0x1, 0x5

void Riscv64Assembler::Addiw(XRegister rd, XRegister rs1, int32_t imm12) {
  EmitI(imm12, rs1, 0x0, rd, 0x1b);
}

void Riscv64Assembler::Slliw(XRegister rd, XRegister rs1, int32_t shamt) {
  CHECK(static_cast<uint32_t>(shamt) < 32) << shamt;
  EmitR(0x0, shamt, rs1, 0x1, rd, 0x1b);
}

void Riscv64Assembler::Srliw(XRegister rd, XRegister rs1, int32_t shamt) {
  CHECK(static_cast<uint32_t>(shamt) < 32) << shamt;
  EmitR(0x0, shamt, rs1, 0x5, rd, 0x1b);
}

void Riscv64Assembler::Sraiw(XRegister rd, XRegister rs1, int32_t shamt) {
  CHECK(static_cast<uint32_t>(shamt) < 32) << shamt;
  EmitR(0x20, shamt, rs1, 0x5, rd, 0x1b);
}

// 32bit ALU instructions (RV64I): opcode = 0x3b, funct3 from 0x0 ~ 0x7

void Riscv64Assembler::Addw(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x0, rs2, rs1, 0x0, rd, 0x3b);
}

void Riscv64Assembler::Subw(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x20, rs2, rs1, 0x0, rd, 0x3b);
}

void Riscv64Assembler::Sllw(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x0, rs2, rs1, 0x1, rd, 0x3b);
}

void Riscv64Assembler::Srlw(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x0, rs2, rs1, 0x5, rd, 0x3b);
}

void Riscv64Assembler::Sraw(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x20, rs2, rs1, 0x5, rd, 0x3b);
}

// RV32M Standard Extension: opcode = 0x33, funct3 from 0x0 ~ 0x7

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

// RV64M Standard Extension: opcode = 0x3b, funct3 0x0 and from 0x4 ~ 0x7

void Riscv64Assembler::Mulw(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x1, rs2, rs1, 0x0, rd, 0x3b);
}

void Riscv64Assembler::Divw(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x1, rs2, rs1, 0x4, rd, 0x3b);
}

void Riscv64Assembler::Divuw(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x1, rs2, rs1, 0x5, rd, 0x3b);
}

void Riscv64Assembler::Remw(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x1, rs2, rs1, 0x6, rd, 0x3b);
}

void Riscv64Assembler::Remuw(XRegister rd, XRegister rs1, XRegister rs2) {
  EmitR(0x1, rs2, rs1, 0x7, rd, 0x3b);
}

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

// FP load/store instructions (RV32F+RV32D): opcode = 0x07, 0x27

void Riscv64Assembler::FLw(FRegister rd, XRegister rs1, int32_t offset) {
  EmitI(offset, rs1, 0x2, rd, 0x07);
}

void Riscv64Assembler::FLd(FRegister rd, XRegister rs1, int32_t offset) {
  EmitI(offset, rs1, 0x3, rd, 0x07);
}

void Riscv64Assembler::FSw(FRegister rs2, XRegister rs1, int32_t offset) {
  EmitS(offset, rs2, rs1, 0x2, 0x27);
}

void Riscv64Assembler::FSd(FRegister rs2, XRegister rs1, int32_t offset) {
  EmitS(offset, rs2, rs1, 0x3, 0x27);
}

// FP FMA instructions (RV32F+RV32D): opcode = 0x43, 0x47, 0x4b, 0x4f

void Riscv64Assembler::FMAddS(
    FRegister rd, FRegister rs1, FRegister rs2, FRegister rs3, FPRoundingMode frm) {
  EmitR4(rs3, 0x0, rs2, rs1, enum_cast<uint32_t>(frm), rd, 0x43);
}

void Riscv64Assembler::FMAddD(
    FRegister rd, FRegister rs1, FRegister rs2, FRegister rs3, FPRoundingMode frm) {
  EmitR4(rs3, 0x1, rs2, rs1, enum_cast<uint32_t>(frm), rd, 0x43);
}

void Riscv64Assembler::FMSubS(
    FRegister rd, FRegister rs1, FRegister rs2, FRegister rs3, FPRoundingMode frm) {
  EmitR4(rs3, 0x0, rs2, rs1, enum_cast<uint32_t>(frm), rd, 0x47);
}

void Riscv64Assembler::FMSubD(
    FRegister rd, FRegister rs1, FRegister rs2, FRegister rs3, FPRoundingMode frm) {
  EmitR4(rs3, 0x1, rs2, rs1, enum_cast<uint32_t>(frm), rd, 0x47);
}

void Riscv64Assembler::FNMSubS(
    FRegister rd, FRegister rs1, FRegister rs2, FRegister rs3, FPRoundingMode frm) {
  EmitR4(rs3, 0x0, rs2, rs1, enum_cast<uint32_t>(frm), rd, 0x4b);
}

void Riscv64Assembler::FNMSubD(
    FRegister rd, FRegister rs1, FRegister rs2, FRegister rs3, FPRoundingMode frm) {
  EmitR4(rs3, 0x1, rs2, rs1, enum_cast<uint32_t>(frm), rd, 0x4b);
}

void Riscv64Assembler::FNMAddS(
    FRegister rd, FRegister rs1, FRegister rs2, FRegister rs3, FPRoundingMode frm) {
  EmitR4(rs3, 0x0, rs2, rs1, enum_cast<uint32_t>(frm), rd, 0x4f);
}

void Riscv64Assembler::FNMAddD(
    FRegister rd, FRegister rs1, FRegister rs2, FRegister rs3, FPRoundingMode frm) {
  EmitR4(rs3, 0x1, rs2, rs1, enum_cast<uint32_t>(frm), rd, 0x4f);
}

// Simple FP instructions (RV32F+RV32D): opcode = 0x53, funct7 = 0b0XXXX0D

void Riscv64Assembler::FAddS(FRegister rd, FRegister rs1, FRegister rs2, FPRoundingMode frm) {
  EmitR(0x0, rs2, rs1, enum_cast<uint32_t>(frm), rd, 0x53);
}

void Riscv64Assembler::FAddD(FRegister rd, FRegister rs1, FRegister rs2, FPRoundingMode frm) {
  EmitR(0x1, rs2, rs1, enum_cast<uint32_t>(frm), rd, 0x53);
}

void Riscv64Assembler::FSubS(FRegister rd, FRegister rs1, FRegister rs2, FPRoundingMode frm) {
  EmitR(0x4, rs2, rs1, enum_cast<uint32_t>(frm), rd, 0x53);
}

void Riscv64Assembler::FSubD(FRegister rd, FRegister rs1, FRegister rs2, FPRoundingMode frm) {
  EmitR(0x5, rs2, rs1, enum_cast<uint32_t>(frm), rd, 0x53);
}

void Riscv64Assembler::FMulS(FRegister rd, FRegister rs1, FRegister rs2, FPRoundingMode frm) {
  EmitR(0x8, rs2, rs1, enum_cast<uint32_t>(frm), rd, 0x53);
}

void Riscv64Assembler::FMulD(FRegister rd, FRegister rs1, FRegister rs2, FPRoundingMode frm) {
  EmitR(0x9, rs2, rs1, enum_cast<uint32_t>(frm), rd, 0x53);
}

void Riscv64Assembler::FDivS(FRegister rd, FRegister rs1, FRegister rs2, FPRoundingMode frm) {
  EmitR(0xc, rs2, rs1, enum_cast<uint32_t>(frm), rd, 0x53);
}

void Riscv64Assembler::FDivD(FRegister rd, FRegister rs1, FRegister rs2, FPRoundingMode frm) {
  EmitR(0xd, rs2, rs1, enum_cast<uint32_t>(frm), rd, 0x53);
}

void Riscv64Assembler::FSqrtS(FRegister rd, FRegister rs1, FPRoundingMode frm) {
  EmitR(0x2c, 0x0, rs1, enum_cast<uint32_t>(frm), rd, 0x53);
}

void Riscv64Assembler::FSqrtD(FRegister rd, FRegister rs1, FPRoundingMode frm) {
  EmitR(0x2d, 0x0, rs1, enum_cast<uint32_t>(frm), rd, 0x53);
}

void Riscv64Assembler::FSgnjS(FRegister rd, FRegister rs1, FRegister rs2) {
  EmitR(0x10, rs2, rs1, 0x0, rd, 0x53);
}

void Riscv64Assembler::FSgnjD(FRegister rd, FRegister rs1, FRegister rs2) {
  EmitR(0x11, rs2, rs1, 0x0, rd, 0x53);
}

void Riscv64Assembler::FSgnjnS(FRegister rd, FRegister rs1, FRegister rs2) {
  EmitR(0x10, rs2, rs1, 0x1, rd, 0x53);
}

void Riscv64Assembler::FSgnjnD(FRegister rd, FRegister rs1, FRegister rs2) {
  EmitR(0x11, rs2, rs1, 0x1, rd, 0x53);
}

void Riscv64Assembler::FSgnjxS(FRegister rd, FRegister rs1, FRegister rs2) {
  EmitR(0x10, rs2, rs1, 0x2, rd, 0x53);
}

void Riscv64Assembler::FSgnjxD(FRegister rd, FRegister rs1, FRegister rs2) {
  EmitR(0x11, rs2, rs1, 0x2, rd, 0x53);
}

void Riscv64Assembler::FMinS(FRegister rd, FRegister rs1, FRegister rs2) {
  EmitR(0x14, rs2, rs1, 0x0, rd, 0x53);
}

void Riscv64Assembler::FMinD(FRegister rd, FRegister rs1, FRegister rs2) {
  EmitR(0x15, rs2, rs1, 0x0, rd, 0x53);
}

void Riscv64Assembler::FMaxS(FRegister rd, FRegister rs1, FRegister rs2) {
  EmitR(0x14, rs2, rs1, 0x1, rd, 0x53);
}

void Riscv64Assembler::FMaxD(FRegister rd, FRegister rs1, FRegister rs2) {
  EmitR(0x15, rs2, rs1, 0x1, rd, 0x53);
}

void Riscv64Assembler::FCvtSD(FRegister rd, FRegister rs1, FPRoundingMode frm) {
  EmitR(0x20, 0x1, rs1, enum_cast<uint32_t>(frm), rd, 0x53);
}

void Riscv64Assembler::FCvtDS(FRegister rd, FRegister rs1, FPRoundingMode frm) {
  // Note: The `frm` is useless, the result can represent every value of the source exactly.
  EmitR(0x21, 0x0, rs1, enum_cast<uint32_t>(frm), rd, 0x53);
}

// FP compare instructions (RV32F+RV32D): opcode = 0x53, funct7 = 0b101000D

void Riscv64Assembler::FEqS(XRegister rd, FRegister rs1, FRegister rs2) {
  EmitR(0x50, rs2, rs1, 0x2, rd, 0x53);
}

void Riscv64Assembler::FEqD(XRegister rd, FRegister rs1, FRegister rs2) {
  EmitR(0x51, rs2, rs1, 0x2, rd, 0x53);
}

void Riscv64Assembler::FLtS(XRegister rd, FRegister rs1, FRegister rs2) {
  EmitR(0x50, rs2, rs1, 0x1, rd, 0x53);
}

void Riscv64Assembler::FLtD(XRegister rd, FRegister rs1, FRegister rs2) {
  EmitR(0x51, rs2, rs1, 0x1, rd, 0x53);
}

void Riscv64Assembler::FLeS(XRegister rd, FRegister rs1, FRegister rs2) {
  EmitR(0x50, rs2, rs1, 0x0, rd, 0x53);
}

void Riscv64Assembler::FLeD(XRegister rd, FRegister rs1, FRegister rs2) {
  EmitR(0x51, rs2, rs1, 0x0, rd, 0x53);
}

// FP conversion instructions (RV32F+RV32D+RV64F+RV64D): opcode = 0x53, funct7 = 0b110X00D

void Riscv64Assembler::FCvtWS(XRegister rd, FRegister rs1, FPRoundingMode frm) {
  EmitR(0x60, 0x0, rs1, enum_cast<uint32_t>(frm), rd, 0x53);
}

void Riscv64Assembler::FCvtWD(XRegister rd, FRegister rs1, FPRoundingMode frm) {
  EmitR(0x61, 0x0, rs1, enum_cast<uint32_t>(frm), rd, 0x53);
}

void Riscv64Assembler::FCvtWuS(XRegister rd, FRegister rs1, FPRoundingMode frm) {
  EmitR(0x60, 0x1, rs1, enum_cast<uint32_t>(frm), rd, 0x53);
}

void Riscv64Assembler::FCvtWuD(XRegister rd, FRegister rs1, FPRoundingMode frm) {
  EmitR(0x61, 0x1, rs1, enum_cast<uint32_t>(frm), rd, 0x53);
}

void Riscv64Assembler::FCvtLS(XRegister rd, FRegister rs1, FPRoundingMode frm) {
  EmitR(0x60, 0x2, rs1, enum_cast<uint32_t>(frm), rd, 0x53);
}

void Riscv64Assembler::FCvtLD(XRegister rd, FRegister rs1, FPRoundingMode frm) {
  EmitR(0x61, 0x2, rs1, enum_cast<uint32_t>(frm), rd, 0x53);
}

void Riscv64Assembler::FCvtLuS(XRegister rd, FRegister rs1, FPRoundingMode frm) {
  EmitR(0x60, 0x3, rs1, enum_cast<uint32_t>(frm), rd, 0x53);
}

void Riscv64Assembler::FCvtLuD(XRegister rd, FRegister rs1, FPRoundingMode frm) {
  EmitR(0x61, 0x3, rs1, enum_cast<uint32_t>(frm), rd, 0x53);
}

void Riscv64Assembler::FCvtSW(FRegister rd, XRegister rs1, FPRoundingMode frm) {
  EmitR(0x68, 0x0, rs1, enum_cast<uint32_t>(frm), rd, 0x53);
}

void Riscv64Assembler::FCvtDW(FRegister rd, XRegister rs1, FPRoundingMode frm) {
  // Note: The `frm` is useless, the result can represent every value of the source exactly.
  EmitR(0x69, 0x0, rs1, enum_cast<uint32_t>(frm), rd, 0x53);
}

void Riscv64Assembler::FCvtSWu(FRegister rd, XRegister rs1, FPRoundingMode frm) {
  EmitR(0x68, 0x1, rs1, enum_cast<uint32_t>(frm), rd, 0x53);
}

void Riscv64Assembler::FCvtDWu(FRegister rd, XRegister rs1, FPRoundingMode frm) {
  // Note: The `frm` is useless, the result can represent every value of the source exactly.
  EmitR(0x69, 0x1, rs1, enum_cast<uint32_t>(frm), rd, 0x53);
}

void Riscv64Assembler::FCvtSL(FRegister rd, XRegister rs1, FPRoundingMode frm) {
  EmitR(0x68, 0x2, rs1, enum_cast<uint32_t>(frm), rd, 0x53);
}

void Riscv64Assembler::FCvtDL(FRegister rd, XRegister rs1, FPRoundingMode frm) {
  EmitR(0x69, 0x2, rs1, enum_cast<uint32_t>(frm), rd, 0x53);
}

void Riscv64Assembler::FCvtSLu(FRegister rd, XRegister rs1, FPRoundingMode frm) {
  EmitR(0x68, 0x3, rs1, enum_cast<uint32_t>(frm), rd, 0x53);
}

void Riscv64Assembler::FCvtDLu(FRegister rd, XRegister rs1, FPRoundingMode frm) {
  EmitR(0x69, 0x3, rs1, enum_cast<uint32_t>(frm), rd, 0x53);
}

// FP move instructions (RV32F+RV32D): opcode = 0x53, funct3 = 0x0, funct7 = 0b111X00D

void Riscv64Assembler::FMvXW(XRegister rd, FRegister rs1) {
  EmitR(0x70, 0x0, rs1, 0x0, rd, 0x53);
}

void Riscv64Assembler::FMvXD(XRegister rd, FRegister rs1) {
  EmitR(0x71, 0x0, rs1, 0x0, rd, 0x53);
}

void Riscv64Assembler::FMvWX(FRegister rd, XRegister rs1) {
  EmitR(0x78, 0x0, rs1, 0x0, rd, 0x53);
}

void Riscv64Assembler::FMvDX(FRegister rd, XRegister rs1) {
  EmitR(0x79, 0x0, rs1, 0x0, rd, 0x53);
}

// FP classify instructions (RV32F+RV32D): opcode = 0x53, funct3 = 0x1, funct7 = 0b111X00D

void Riscv64Assembler::FClassS(XRegister rd, FRegister rs1) {
  EmitR(0x70, 0x0, rs1, 0x1, rd, 0x53);
}

void Riscv64Assembler::FClassD(XRegister rd, FRegister rs1) {
  EmitR(0x71, 0x0, rs1, 0x1, rd, 0x53);
}

/////////////////////////////// RV64 "FD" Instructions  END ///////////////////////////////

////////////////////////////// RV64 MACRO Instructions  START ///////////////////////////////

// Pseudo instructions

void Riscv64Assembler::Nop() { Addi(Zero, Zero, 0); }

void Riscv64Assembler::Li(XRegister rd, int32_t imm) {
  if (IsInt<12>(imm))
    Addi(rd, Zero, imm);
  else if (Low12Bits(imm) == 0)
    Lui(rd, High20Bits(imm));
  else {
    Lui(rd, High20Bits(imm));
    Addi(rd, Zero, Low12Bits(imm));
  }
}

void Riscv64Assembler::Mv(XRegister rd, XRegister rs) { Addi(rd, rs, 0); }

void Riscv64Assembler::Not(XRegister rd, XRegister rs) { Xori(rd, rs, -1); }

void Riscv64Assembler::Neg(XRegister rd, XRegister rs) { Sub(rd, Zero, rs); }

void Riscv64Assembler::NegW(XRegister rd, XRegister rs) { Subw(rd, Zero, rs); }

void Riscv64Assembler::SextB(XRegister rd, XRegister rs) {
  Slli(rd, rs, kXlen - 8u);
  Srai(rd, rd, kXlen - 8u);
}

void Riscv64Assembler::SextH(XRegister rd, XRegister rs) {
  Slli(rd, rs, kXlen - 16u);
  Srai(rd, rd, kXlen - 16u);
}

void Riscv64Assembler::SextW(XRegister rd, XRegister rs) { Addiw(rd, rs, 0); }

void Riscv64Assembler::ZextB(XRegister rd, XRegister rs) { Andi(rd, rs, 0xff); }

void Riscv64Assembler::ZextH(XRegister rd, XRegister rs) {
  Slli(rd, rs, kXlen - 16u);
  Srli(rd, rd, kXlen - 16u);
}

void Riscv64Assembler::ZextW(XRegister rd, XRegister rs) {
  Slli(rd, rs, kXlen - 32u);
  Srli(rd, rd, kXlen - 32u);
}

void Riscv64Assembler::Seqz(XRegister rd, XRegister rs) { Sltiu(rd, rs, 1); }

void Riscv64Assembler::Snez(XRegister rd, XRegister rs) { Sltu(rd, Zero, rs); }

void Riscv64Assembler::Sltz(XRegister rd, XRegister rs) { Slt(rd, rs, Zero); }

void Riscv64Assembler::Sgtz(XRegister rd, XRegister rs) { Slt(rd, Zero, rs); }

void Riscv64Assembler::FMvS(FRegister rd, FRegister rs) { FSgnjS(rd, rs, rs); }

void Riscv64Assembler::FAbsS(FRegister rd, FRegister rs) { FSgnjxS(rd, rs, rs); }

void Riscv64Assembler::FNegS(FRegister rd, FRegister rs) { FSgnjnS(rd, rs, rs); }

void Riscv64Assembler::FMvD(FRegister rd, FRegister rs) { FSgnjS(rd, rs, rs); }

void Riscv64Assembler::FAbsD(FRegister rd, FRegister rs) { FSgnjxS(rd, rs, rs); }

void Riscv64Assembler::FNegD(FRegister rd, FRegister rs) { FSgnjnS(rd, rs, rs); }

void Riscv64Assembler::Beqz(XRegister rs, int32_t offset) {
  Beq(rs, Zero, offset);
}

void Riscv64Assembler::Bnez(XRegister rs, int32_t offset) {
  Bne(rs, Zero, offset);
}

void Riscv64Assembler::Blez(XRegister rt, int32_t offset) {
  Bge(Zero, rt, offset);
}

void Riscv64Assembler::Bgez(XRegister rt, int32_t offset) {
  Bge(rt, Zero, offset);
}

void Riscv64Assembler::Bltz(XRegister rt, int32_t offset) {
  Blt(rt, Zero, offset);
}

void Riscv64Assembler::Bgtz(XRegister rt, int32_t offset) {
  Blt(Zero, rt, offset);
}

void Riscv64Assembler::Bgt(XRegister rs, XRegister rt, int32_t offset) {
  Blt(rt, rs, offset);
}

void Riscv64Assembler::Ble(XRegister rs, XRegister rt, int32_t offset) {
  Bge(rt, rs, offset);
}

void Riscv64Assembler::Bgtu(XRegister rs, XRegister rt, int32_t offset) {
  Bltu(rt, rs, offset);
}

void Riscv64Assembler::Bleu(XRegister rs, XRegister rt, int32_t offset) {
  Bgeu(rt, rs, offset);
}

void Riscv64Assembler::J(int32_t offset) { Jal(Zero, offset); }

void Riscv64Assembler::Jal(int32_t offset) { Jal(RA, offset); }

void Riscv64Assembler::Jr(XRegister rs) { Jalr(Zero, rs, 0); }

void Riscv64Assembler::Jalr(XRegister rs) { Jalr(RA, rs, 0); }

void Riscv64Assembler::Jalr(XRegister rd, XRegister rs) { Jalr(rd, rs, 0); }

void Riscv64Assembler::Ret() { Jalr(Zero, RA, 0); }

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

void Riscv64Assembler::Call(int32_t offset) {
  if (IsInt<12>(offset)) {
    Jal(RA, offset);
  } else {
    // Round `offset` to nearest 4KiB offset for as the JALR has range [-0x800, 0x800).
    int32_t near_offset = (offset + 0x800) & ~0xfff;
    Auipc(RA, static_cast<uint32_t>(near_offset) >> 12);
    Jalr(RA, RA, offset - near_offset);
  }
}

void Riscv64Assembler::EmitBcond(BranchCondition cond,
                                 XRegister rs,
                                 XRegister rt,
                                 uint32_t imm16_21) {
  switch (cond) {
    case kCondLT:
      Blt(rs, rt, imm16_21);
      break;
    case kCondGE:
      Bge(rs, rt, imm16_21);
      break;
    case kCondLE:
      Bge(rt, rs, imm16_21);
      break;
    case kCondGT:
      Blt(rt, rs, imm16_21);
      break;
    case kCondLTZ:
      CHECK_EQ(rt, Zero);
      Bltz(rs, imm16_21);
      break;
    case kCondGEZ:
      CHECK_EQ(rt, Zero);
      Bgez(rs, imm16_21);
      break;
    case kCondLEZ:
      CHECK_EQ(rt, Zero);
      Blez(rs, imm16_21);
      break;
    case kCondGTZ:
      CHECK_EQ(rt, Zero);
      Bgtz(rs, imm16_21);
      break;
    case kCondEQ:
      Beq(rs, rt, imm16_21);
      break;
    case kCondNE:
      Bne(rs, rt, imm16_21);
      break;
    case kCondEQZ:
      CHECK_EQ(rt, Zero);
      Beqz(rs, imm16_21);
      break;
    case kCondNEZ:
      CHECK_EQ(rt, Zero);
      Bnez(rs, imm16_21);
      break;
    case kCondLTU:
      Bltu(rs, rt, imm16_21);
      break;
    case kCondGEU:
      Bgeu(rs, rt, imm16_21);
      break;
    case kUncond:
      // LOG(FATAL) << "Unexpected branch condition " << cond;
      LOG(FATAL) << "Unexpected branch condition ";
      UNREACHABLE();
  }
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

    // Fill the space with placeholder data as the data is not final
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
    // Mv the code residing between branch placeholders.
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
      J(offset);
      break;
    case Branch::kCondBranch:
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      EmitBcond(condition, lhs, rhs, offset);
      break;
    case Branch::kCall:
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Call(offset);
      break;
    case Branch::kBareUncondBranch:
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      J(offset);
      break;
    case Branch::kBareCondBranch:
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      EmitBcond(condition, lhs, rhs, offset);
      break;
    case Branch::kBareCall:
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Call(offset);
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
      Jalr(Zero, AT, Low12Bits(offset));
      break;
    case Branch::kLongCondBranch:
      // Skip (2 + itself) instructions and continue if the Cond isn't taken.
      EmitBcond(Branch::OppositeCondition(condition), lhs, rhs, 12);
      offset += (offset & 0x800) << 1;  // Account for sign extension in jic.
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Auipc(AT, High20Bits(offset));
      Jalr(Zero, AT, Low12Bits(offset));
      break;
    case Branch::kLongCall:
      offset += (offset & 0x800) << 1;  // Account for sign extension in jialc.
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Auipc(AT, High20Bits(offset));
      Jalr(RA, AT, Low12Bits(offset));
      break;
  }
  CHECK_EQ(overwrite_location_, branch->GetEndLocation());
  CHECK_LT(branch->GetSize(), static_cast<uint32_t>(Branch::kMaxBranchSize));
}

void Riscv64Assembler::Jal(Riscv64Label* label, bool is_bare) { Call(label, is_bare); }

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
  // for a load/store when the offset doesn't fit into int12_t.
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

/////////////////////////////// RV64 VARIANTS extension end ////////////

}  // namespace riscv64
}  // namespace art
