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

#ifndef ART_COMPILER_UTILS_RISCV64_ASSEMBLER_RISCV64_H_
#define ART_COMPILER_UTILS_RISCV64_ASSEMBLER_RISCV64_H_

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "arch/riscv64/instruction_set_features_riscv64.h"
#include "base/arena_containers.h"
#include "base/enums.h"
#include "base/globals.h"
#include "base/macros.h"
#include "base/stl_util_identity.h"
#include "constants_riscv64.h"
#include "heap_poisoning.h"
#include "managed_register_riscv64.h"
#include "utils/assembler.h"
#include "utils/label.h"

namespace art {
namespace riscv64 {

enum class FPRoundingMode : uint32_t {
  kRNE = 0x0,  // Round to Nearest, ties to Even
  kRTZ = 0x1,  // Round towards Zero
  kRDN = 0x2,  // Round Down (towards −Infinity)
  kRUP = 0x3,  // Round Up (towards +Infinity)
  kRMM = 0x4,  // Round to Nearest, ties to Max Magnitude
  kDYN = 0x7,  // Dynamic rounding mode
  kDefault = kDYN
};

enum LoadConst64Path {
  kLoadConst64PathZero = 0x0,
  kLoadConst64PathOri = 0x1,
  kLoadConst64PathDaddiu = 0x2,
  kLoadConst64PathLui = 0x4,
  kLoadConst64PathLuiOri = 0x8,
  kLoadConst64PathOriDahi = 0x10,
  kLoadConst64PathOriDati = 0x20,
  kLoadConst64PathLuiDahi = 0x40,
  kLoadConst64PathLuiDati = 0x80,
  kLoadConst64PathDaddiuDsrlX = 0x100,
  kLoadConst64PathOriDsllX = 0x200,
  kLoadConst64PathDaddiuDsllX = 0x400,
  kLoadConst64PathLuiOriDsllX = 0x800,
  kLoadConst64PathOriDsllXOri = 0x1000,
  kLoadConst64PathDaddiuDsllXOri = 0x2000,
  kLoadConst64PathDaddiuDahi = 0x4000,
  kLoadConst64PathDaddiuDati = 0x8000,
  kLoadConst64PathDinsu1 = 0x10000,
  kLoadConst64PathDinsu2 = 0x20000,
  kLoadConst64PathCatchAll = 0x40000,
  kLoadConst64PathAllPaths = 0x7ffff,
};

inline uint16_t Low12Bits(uint32_t value) { return static_cast<uint16_t>(value & 0xFFF); }

inline uint32_t High20Bits(uint32_t value) { return static_cast<uint32_t>(value >> 12); }

static constexpr size_t kRiscv64HalfwordSize = 2;
static constexpr size_t kRiscv64WordSize = 4;
static constexpr size_t kRiscv64DoublewordSize = 8;

enum LoadOperandType {
  kLoadSignedByte,
  kLoadUnsignedByte,
  kLoadSignedHalfword,
  kLoadUnsignedHalfword,
  kLoadWord,
  kLoadUnsignedWord,
  kLoadDoubleword,
  kLoadQuadword
};

// the type for fence
enum FenceType {
  kFenceNone = 0,
  kFenceWrite = 1,
  kFenceRead = 2,
  kFenceOutput = 4,
  kFenceInput = 8,
  kFenceDefault = 0xf,
};

enum StoreOperandType { kStoreByte, kStoreHalfword, kStoreWord, kStoreDoubleword, kStoreQuadword };

// Used to test the values returned by ClassS/ClassD.
enum FPClassMaskType {
  kNegativeInfinity = 0x001,
  kNegativeNormal = 0x002,
  kNegativeSubnormal = 0x004,
  kNegativeZero = 0x008,
  kPositiveZero = 0x010,
  kPositiveSubnormal = 0x020,
  kPositiveNormal = 0x040,
  kPositiveInfinity = 0x080,
  kSignalingNaN = 0x100,
  kQuietNaN = 0x200,
};

class Riscv64Label : public Label {
 public:
  Riscv64Label() : prev_branch_id_plus_one_(0) {}

  Riscv64Label(Riscv64Label&& src)
      : Label(std::move(src)), prev_branch_id_plus_one_(src.prev_branch_id_plus_one_) {}

 private:
  uint32_t prev_branch_id_plus_one_;  // To get distance from preceding branch, if any.

  friend class Riscv64Assembler;
  DISALLOW_COPY_AND_ASSIGN(Riscv64Label);
};

// Assembler literal is a value embedded in code, retrieved using a PC-relative load.
class Literal {
 public:
  static constexpr size_t kMaxSize = 8;

  Literal(uint32_t size, const uint8_t* data) : label_(), size_(size) {
    DCHECK_LE(size, Literal::kMaxSize);
    memcpy(data_, data, size);
  }

  template <typename T>
  T GetValue() const {
    DCHECK_EQ(size_, sizeof(T));
    T value;
    memcpy(&value, data_, sizeof(T));
    return value;
  }

  uint32_t GetSize() const { return size_; }

  const uint8_t* GetData() const { return data_; }

  Riscv64Label* GetLabel() { return &label_; }

  const Riscv64Label* GetLabel() const { return &label_; }

 private:
  Riscv64Label label_;
  const uint32_t size_;
  uint8_t data_[kMaxSize];

  DISALLOW_COPY_AND_ASSIGN(Literal);
};

// Jump table: table of labels emitted after the code and before the literals. Similar to literals.
class JumpTable {
 public:
  explicit JumpTable(std::vector<Riscv64Label*>&& labels) : label_(), labels_(std::move(labels)) {}

  size_t GetSize() const { return labels_.size() * sizeof(uint32_t); }

  const std::vector<Riscv64Label*>& GetData() const { return labels_; }

  Riscv64Label* GetLabel() { return &label_; }

  const Riscv64Label* GetLabel() const { return &label_; }

 private:
  Riscv64Label label_;
  std::vector<Riscv64Label*> labels_;

  DISALLOW_COPY_AND_ASSIGN(JumpTable);
};

class Riscv64Subset {
 private:
  // The subsets should be "I", "M", "A", "F", "D", "C", "V", "B", ...
  // which following the <The RISC-V Instruction Set Manual>.
  // document: https://github.com/riscv/riscv-isa-manual
  std::vector<std::string> subsets_;

 public:
  Riscv64Subset(std::vector<std::string> subsets) : subsets_(subsets) {
    // TODO: Check instruction set feature is legal or not.
    // RV64, I, M, A, F, D, C, V, B, ...
  }
  Riscv64Subset(const Riscv64InstructionSetFeatures* instruction_set_feature) {
    // TODO: Check instruction set feature is legal or not.
    // RV64, I, M, A, F, D, C, V, B, ...
    UNUSED(instruction_set_feature);
  }

  bool Riscv64SubsetIsSupported(std::string subset) {
    UNUSED(subset);
    return true;
  }

  // For Androind, rv64imafdcvb is expected.
  bool Riscv64SubsetCheck() {
    // Does subsets have imafafdcvb or not?
    return true;
  }
};

class Riscv64Assembler final : public Assembler {
 public:
  explicit Riscv64Assembler(ArenaAllocator* allocator,
                            const Riscv64InstructionSetFeatures* instruction_set_features = nullptr)
      : Assembler(allocator),
        overwriting_(false),
        overwrite_location_(0),
        literals_(allocator->Adapter(kArenaAllocAssembler)),
        long_literals_(allocator->Adapter(kArenaAllocAssembler)),
        jump_tables_(allocator->Adapter(kArenaAllocAssembler)),
        last_position_adjustment_(0),
        last_old_position_(0),
        last_branch_id_(0),
        subsets_(instruction_set_features) {
    // Check instruction set feature is suitable for android or not.
    if (!subsets_.Riscv64SubsetCheck())
      LOG(WARNING) << "unexpected Riscv64 subsets";
    cfi().DelayEmittingAdvancePCs();
  }

  virtual ~Riscv64Assembler() {
    for (auto& branch : branches_) {
      CHECK(branch.IsResolved());
    }
  }

  size_t CodeSize() const override { return Assembler::CodeSize(); }
  DebugFrameOpCodeWriterForAssembler& cfi() { return Assembler::cfi(); }

  // Note that PC-relative literal loads are handled as pseudo branches because they need very
  // similar relocation and may similarly expand in size to accommodate for larger offsets relative
  // to PC.
  enum BranchCondition {
    kCondLT,
    kCondGE,
    kCondLE,
    kCondGT,
    kCondLTZ,
    kCondGEZ,
    kCondLEZ,
    kCondGTZ,
    kCondEQ,
    kCondNE,
    kCondEQZ,
    kCondNEZ,
    kCondLTU,
    kCondGEU,
    kUncond,
  };
  friend std::ostream& operator<<(std::ostream& os, const BranchCondition& rhs);

  // According to "The RISC-V Instruction Set Manual"

  // LUI/AUIPC (RV32I, with sign-extension on RV64I), opcode = 0x17, 0x37
  // Note: These take a 20-bit unsigned value to align with the clang assembler for testing,
  // but the value stored in the register shall actually be sign-extended to 64 bits.
  void Lui(XRegister rd, uint32_t imm20);
  void Auipc(XRegister rd, uint32_t imm20);

  // Jump instructions (RV32I), opcode = 0x67, 0x6f
  void Jal(XRegister rd, int32_t offset);
  void Jalr(XRegister rd, XRegister rs1, int32_t offset);

  // Branch instructions (RV32I), opcode = 0x63, funct3 from 0x0 ~ 0x1 and 0x4 ~ 0x7
  void Beq(XRegister rs1, XRegister rs2, int32_t offset);
  void Bne(XRegister rs1, XRegister rs2, int32_t offset);
  void Blt(XRegister rs1, XRegister rs2, int32_t offset);
  void Bge(XRegister rs1, XRegister rs2, int32_t offset);
  void Bltu(XRegister rs1, XRegister rs2, int32_t offset);
  void Bgeu(XRegister rs1, XRegister rs2, int32_t offset);

  // Load instructions (RV32I+RV64I): opcode = 0x03, funct3 from 0x0 ~ 0x6
  void Lb(XRegister rd, XRegister rs1, int32_t offset);
  void Lh(XRegister rd, XRegister rs1, int32_t offset);
  void Lw(XRegister rd, XRegister rs1, int32_t offset);
  void Ld(XRegister rd, XRegister rs1, int32_t offset);
  void Lbu(XRegister rd, XRegister rs1, int32_t offset);
  void Lhu(XRegister rd, XRegister rs1, int32_t offset);
  void Lwu(XRegister rd, XRegister rs1, int32_t offset);

  // Store instructions (RV32I+RV64I): opcode = 0x23, funct3 from 0x0 ~ 0x3
  void Sb(XRegister rs2, XRegister rs1, int32_t offset);
  void Sh(XRegister rs2, XRegister rs1, int32_t offset);
  void Sw(XRegister rs2, XRegister rs1, int32_t offset);
  void Sd(XRegister rs2, XRegister rs1, int32_t offset);

  // IMM ALU instructions (RV32I): opcode = 0x13, funct3 from 0x0 ~ 0x7
  void Addi(XRegister rd, XRegister rs1, int32_t imm12);
  void Slti(XRegister rd, XRegister rs1, int32_t imm12);
  void Sltiu(XRegister rd, XRegister rs1, int32_t imm12);
  void Xori(XRegister rd, XRegister rs1, int32_t imm12);
  void Ori(XRegister rd, XRegister rs1, int32_t imm12);
  void Andi(XRegister rd, XRegister rs1, int32_t imm12);
  void Slli(XRegister rd, XRegister rs1, int32_t shamt);
  void Srli(XRegister rd, XRegister rs1, int32_t shamt);
  void Srai(XRegister rd, XRegister rs1, int32_t shamt);

  // ALU instructions (RV32I): opcode = 0x33, funct3 from 0x0 ~ 0x7
  void Add(XRegister rd, XRegister rs1, XRegister rs2);
  void Sub(XRegister rd, XRegister rs1, XRegister rs2);
  void Slt(XRegister rd, XRegister rs1, XRegister rs2);
  void Sltu(XRegister rd, XRegister rs1, XRegister rs2);
  void Xor(XRegister rd, XRegister rs1, XRegister rs2);
  void Or(XRegister rd, XRegister rs1, XRegister rs2);
  void And(XRegister rd, XRegister rs1, XRegister rs2);
  void Sll(XRegister rd, XRegister rs1, XRegister rs2);
  void Srl(XRegister rd, XRegister rs1, XRegister rs2);
  void Sra(XRegister rd, XRegister rs1, XRegister rs2);

  // opcode - 0xf 0xf and 0x73
  void Fence(uint8_t pred = kFenceDefault, uint8_t succ = kFenceDefault);
  void FenceTso();
  void Ecall();
  void Ebreak();

  // 32bit Imm ALU instructions (RV64I): opcode = 0x1b, funct3 from 0x0, 0x1, 0x5
  void Addiw(XRegister rd, XRegister rs1, int32_t imm12);
  void Slliw(XRegister rd, XRegister rs1, int32_t shamt);
  void Srliw(XRegister rd, XRegister rs1, int32_t shamt);
  void Sraiw(XRegister rd, XRegister rs1, int32_t shamt);

  // 32bit ALU instructions (RV64I): opcode = 0x3b, funct3 from 0x0 ~ 0x7
  void Addw(XRegister rd, XRegister rs1, XRegister rs2);
  void Subw(XRegister rd, XRegister rs1, XRegister rs2);
  void Sllw(XRegister rd, XRegister rs1, XRegister rs2);
  void Srlw(XRegister rd, XRegister rs1, XRegister rs2);
  void Sraw(XRegister rd, XRegister rs1, XRegister rs2);

  // RV32/RV64 Zifencei Standard Extension
  void FenceI();

  // Zicsr Standard Extension
  void Csrrw(XRegister rd, XRegister rs1, uint16_t csr);
  void Csrrs(XRegister rd, XRegister rs1, uint16_t csr);
  void Csrrc(XRegister rd, XRegister rs1, uint16_t csr);
  void Csrrwi(XRegister rd, uint16_t csr, uint8_t zimm /*imm5*/);
  void Csrrsi(XRegister rd, uint16_t csr, uint8_t zimm /*imm5*/);
  void Csrrci(XRegister rd, uint16_t csr, uint8_t zimm /*imm5*/);

  // RV32M Standard Extension: opcode = 0x33, funct3 from 0x0 ~ 0x7
  void Mul(XRegister rd, XRegister rs1, XRegister rs2);
  void Mulh(XRegister rd, XRegister rs1, XRegister rs2);
  void Mulhsu(XRegister rd, XRegister rs1, XRegister rs2);
  void Mulhu(XRegister rd, XRegister rs1, XRegister rs2);
  void Div(XRegister rd, XRegister rs1, XRegister rs2);
  void Divu(XRegister rd, XRegister rs1, XRegister rs2);
  void Rem(XRegister rd, XRegister rs1, XRegister rs2);
  void Remu(XRegister rd, XRegister rs1, XRegister rs2);

  // RV64M Standard Extension: opcode = 0x3b, funct3 0x0 and from 0x4 ~ 0x7
  void Mulw(XRegister rd, XRegister rs1, XRegister rs2);
  void Divw(XRegister rd, XRegister rs1, XRegister rs2);
  void Divuw(XRegister rd, XRegister rs1, XRegister rs2);
  void Remw(XRegister rd, XRegister rs1, XRegister rs2);
  void Remuw(XRegister rd, XRegister rs1, XRegister rs2);

  // RV32A Standard Extension
  void LrW(XRegister rd, XRegister rs1, uint8_t aqrl);
  void ScW(XRegister rd, XRegister rs2, XRegister rs1, uint8_t aqrl);
  void AmoSwapW(XRegister rd, XRegister rs2, XRegister rs1, uint8_t aqrl);
  void AmoAddW(XRegister rd, XRegister rs2, XRegister rs1, uint8_t aqrl);
  void AmoXorW(XRegister rd, XRegister rs2, XRegister rs1, uint8_t aqrl);
  void AmoAndW(XRegister rd, XRegister rs2, XRegister rs1, uint8_t aqrl);
  void AmoOrW(XRegister rd, XRegister rs2, XRegister rs1, uint8_t aqrl);
  void AmoMinW(XRegister rd, XRegister rs2, XRegister rs1, uint8_t aqrl);
  void AmoMaxW(XRegister rd, XRegister rs2, XRegister rs1, uint8_t aqrl);
  void AmoMinuW(XRegister rd, XRegister rs2, XRegister rs1, uint8_t aqrl);
  void AmoMaxuW(XRegister rd, XRegister rs2, XRegister rs1, uint8_t aqrl);

  // RV64A Standard Extension (in addition to RV32A)
  void LrD(XRegister rd, XRegister rs1, uint8_t aqrl);
  void ScD(XRegister rd, XRegister rs2, XRegister rs1, uint8_t aqrl);
  void AmoSwapD(XRegister rd, XRegister rs2, XRegister rs1, uint8_t aqrl);
  void AmoAddD(XRegister rd, XRegister rs2, XRegister rs1, uint8_t aqrl);
  void AmoXorD(XRegister rd, XRegister rs2, XRegister rs1, uint8_t aqrl);
  void AmoAndD(XRegister rd, XRegister rs2, XRegister rs1, uint8_t aqrl);
  void AmoOrD(XRegister rd, XRegister rs2, XRegister rs1, uint8_t aqrl);
  void AmoMinD(XRegister rd, XRegister rs2, XRegister rs1, uint8_t aqrl);
  void AmoMaxD(XRegister rd, XRegister rs2, XRegister rs1, uint8_t aqrl);
  void AmoMinuD(XRegister rd, XRegister rs2, XRegister rs1, uint8_t aqrl);
  void AmoMaxuD(XRegister rd, XRegister rs2, XRegister rs1, uint8_t aqrl);

  // FP load/store instructions (RV32F+RV32D): opcode = 0x07, 0x27
  void FLw(FRegister rd, XRegister rs1, int32_t offset);
  void FLd(FRegister rd, XRegister rs1, int32_t offset);
  void FSw(FRegister rs2, XRegister rs1, int32_t offset);
  void FSd(FRegister rs2, XRegister rs1, int32_t offset);

  // FP FMA instructions (RV32F+RV32D): opcode = 0x43, 0x47, 0x4b, 0x4f
  void FMAddS(FRegister rd, FRegister rs1, FRegister rs2, FRegister rs3, FPRoundingMode frm);
  void FMAddD(FRegister rd, FRegister rs1, FRegister rs2, FRegister rs3, FPRoundingMode frm);
  void FMSubS(FRegister rd, FRegister rs1, FRegister rs2, FRegister rs3, FPRoundingMode frm);
  void FMSubD(FRegister rd, FRegister rs1, FRegister rs2, FRegister rs3, FPRoundingMode frm);
  void FNMSubS(FRegister rd, FRegister rs1, FRegister rs2, FRegister rs3, FPRoundingMode frm);
  void FNMSubD(FRegister rd, FRegister rs1, FRegister rs2, FRegister rs3, FPRoundingMode frm);
  void FNMAddS(FRegister rd, FRegister rs1, FRegister rs2, FRegister rs3, FPRoundingMode frm);
  void FNMAddD(FRegister rd, FRegister rs1, FRegister rs2, FRegister rs3, FPRoundingMode frm);

  // FP FMA instruction helpers passing the default rounding mode.
  void FMAddS(FRegister rd, FRegister rs1, FRegister rs2, FRegister rs3) {
    FMAddS(rd, rs1, rs2, rs3, FPRoundingMode::kDefault);
  }
  void FMAddD(FRegister rd, FRegister rs1, FRegister rs2, FRegister rs3) {
    FMAddD(rd, rs1, rs2, rs3, FPRoundingMode::kDefault);
  }
  void FMSubS(FRegister rd, FRegister rs1, FRegister rs2, FRegister rs3) {
    FMSubS(rd, rs1, rs2, rs3, FPRoundingMode::kDefault);
  }
  void FMSubD(FRegister rd, FRegister rs1, FRegister rs2, FRegister rs3) {
    FMSubD(rd, rs1, rs2, rs3, FPRoundingMode::kDefault);
  }
  void FNMSubS(FRegister rd, FRegister rs1, FRegister rs2, FRegister rs3) {
    FNMSubS(rd, rs1, rs2, rs3, FPRoundingMode::kDefault);
  }
  void FNMSubD(FRegister rd, FRegister rs1, FRegister rs2, FRegister rs3) {
    FNMSubD(rd, rs1, rs2, rs3, FPRoundingMode::kDefault);
  }
  void FNMAddS(FRegister rd, FRegister rs1, FRegister rs2, FRegister rs3) {
    FNMAddS(rd, rs1, rs2, rs3, FPRoundingMode::kDefault);
  }
  void FNMAddD(FRegister rd, FRegister rs1, FRegister rs2, FRegister rs3) {
    FNMAddD(rd, rs1, rs2, rs3, FPRoundingMode::kDefault);
  }

  // Simple FP instructions (RV32F+RV32D): opcode = 0x53, funct7 = 0b0XXXX0D
  void FAddS(FRegister rd, FRegister rs1, FRegister rs2, FPRoundingMode frm);
  void FAddD(FRegister rd, FRegister rs1, FRegister rs2, FPRoundingMode frm);
  void FSubS(FRegister rd, FRegister rs1, FRegister rs2, FPRoundingMode frm);
  void FSubD(FRegister rd, FRegister rs1, FRegister rs2, FPRoundingMode frm);
  void FMulS(FRegister rd, FRegister rs1, FRegister rs2, FPRoundingMode frm);
  void FMulD(FRegister rd, FRegister rs1, FRegister rs2, FPRoundingMode frm);
  void FDivS(FRegister rd, FRegister rs1, FRegister rs2, FPRoundingMode frm);
  void FDivD(FRegister rd, FRegister rs1, FRegister rs2, FPRoundingMode frm);
  void FSqrtS(FRegister rd, FRegister rs1, FPRoundingMode frm);
  void FSqrtD(FRegister rd, FRegister rs1, FPRoundingMode frm);
  void FSgnjS(FRegister rd, FRegister rs1, FRegister rs2);
  void FSgnjD(FRegister rd, FRegister rs1, FRegister rs2);
  void FSgnjnS(FRegister rd, FRegister rs1, FRegister rs2);
  void FSgnjnD(FRegister rd, FRegister rs1, FRegister rs2);
  void FSgnjxS(FRegister rd, FRegister rs1, FRegister rs2);
  void FSgnjxD(FRegister rd, FRegister rs1, FRegister rs2);
  void FMinS(FRegister rd, FRegister rs1, FRegister rs2);
  void FMinD(FRegister rd, FRegister rs1, FRegister rs2);
  void FMaxS(FRegister rd, FRegister rs1, FRegister rs2);
  void FMaxD(FRegister rd, FRegister rs1, FRegister rs2);
  void FCvtSD(FRegister rd, FRegister rs1, FPRoundingMode frm);
  void FCvtDS(FRegister rd, FRegister rs1, FPRoundingMode frm);

  // Simple FP instruction helpers passing the default rounding mode.
  void FAddS(FRegister rd, FRegister rs1, FRegister rs2) {
    FAddS(rd, rs1, rs2, FPRoundingMode::kDefault);
  }
  void FAddD(FRegister rd, FRegister rs1, FRegister rs2) {
    FAddD(rd, rs1, rs2, FPRoundingMode::kDefault);
  }
  void FSubS(FRegister rd, FRegister rs1, FRegister rs2) {
    FSubS(rd, rs1, rs2, FPRoundingMode::kDefault);
  }
  void FSubD(FRegister rd, FRegister rs1, FRegister rs2) {
    FSubD(rd, rs1, rs2, FPRoundingMode::kDefault);
  }
  void FMulS(FRegister rd, FRegister rs1, FRegister rs2) {
    FMulS(rd, rs1, rs2, FPRoundingMode::kDefault);
  }
  void FMulD(FRegister rd, FRegister rs1, FRegister rs2) {
    FMulD(rd, rs1, rs2, FPRoundingMode::kDefault);
  }
  void FDivS(FRegister rd, FRegister rs1, FRegister rs2) {
    FDivS(rd, rs1, rs2, FPRoundingMode::kDefault);
  }
  void FDivD(FRegister rd, FRegister rs1, FRegister rs2) {
    FDivD(rd, rs1, rs2, FPRoundingMode::kDefault);
  }
  void FSqrtS(FRegister rd, FRegister rs1) {
    FSqrtS(rd, rs1, FPRoundingMode::kDefault);
  }
  void FSqrtD(FRegister rd, FRegister rs1) {
    FSqrtD(rd, rs1, FPRoundingMode::kDefault);
  }
  void FCvtSD(FRegister rd, FRegister rs1) {
    FCvtSD(rd, rs1, FPRoundingMode::kDefault);
  }
  void FCvtDS(FRegister rd, FRegister rs1) {
    FCvtDS(rd, rs1, FPRoundingMode::kDefault);
  }

  // FP compare instructions (RV32F+RV32D): opcode = 0x53, funct7 = 0b101000D
  void FEqS(XRegister rd, FRegister rs1, FRegister rs2);
  void FEqD(XRegister rd, FRegister rs1, FRegister rs2);
  void FLtS(XRegister rd, FRegister rs1, FRegister rs2);
  void FLtD(XRegister rd, FRegister rs1, FRegister rs2);
  void FLeS(XRegister rd, FRegister rs1, FRegister rs2);
  void FLeD(XRegister rd, FRegister rs1, FRegister rs2);

  // FP conversion instructions (RV32F+RV32D+RV64F+RV64D): opcode = 0x53, funct7 = 0b110X00D
  void FCvtWS(XRegister rd, FRegister rs1, FPRoundingMode frm);
  void FCvtWD(XRegister rd, FRegister rs1, FPRoundingMode frm);
  void FCvtWuS(XRegister rd, FRegister rs1, FPRoundingMode frm);
  void FCvtWuD(XRegister rd, FRegister rs1, FPRoundingMode frm);
  void FCvtLS(XRegister rd, FRegister rs1, FPRoundingMode frm);
  void FCvtLD(XRegister rd, FRegister rs1, FPRoundingMode frm);
  void FCvtLuS(XRegister rd, FRegister rs1, FPRoundingMode frm);
  void FCvtLuD(XRegister rd, FRegister rs1, FPRoundingMode frm);
  void FCvtSW(FRegister rd, XRegister rs1, FPRoundingMode frm);
  void FCvtDW(FRegister rd, XRegister rs1, FPRoundingMode frm);
  void FCvtSWu(FRegister rd, XRegister rs1, FPRoundingMode frm);
  void FCvtDWu(FRegister rd, XRegister rs1, FPRoundingMode frm);
  void FCvtSL(FRegister rd, XRegister rs1, FPRoundingMode frm);
  void FCvtDL(FRegister rd, XRegister rs1, FPRoundingMode frm);
  void FCvtSLu(FRegister rd, XRegister rs1, FPRoundingMode frm);
  void FCvtDLu(FRegister rd, XRegister rs1, FPRoundingMode frm);

  // FP conversion instruction helpers passing the default rounding mode.
  void FCvtWS(XRegister rd, FRegister rs1) { FCvtWS(rd, rs1, FPRoundingMode::kDefault); }
  void FCvtWD(XRegister rd, FRegister rs1) { FCvtWD(rd, rs1, FPRoundingMode::kDefault); }
  void FCvtWuS(XRegister rd, FRegister rs1) { FCvtWuS(rd, rs1, FPRoundingMode::kDefault); }
  void FCvtWuD(XRegister rd, FRegister rs1) { FCvtWuD(rd, rs1, FPRoundingMode::kDefault); }
  void FCvtLS(XRegister rd, FRegister rs1) { FCvtLS(rd, rs1, FPRoundingMode::kDefault); }
  void FCvtLD(XRegister rd, FRegister rs1) { FCvtLD(rd, rs1, FPRoundingMode::kDefault); }
  void FCvtLuS(XRegister rd, FRegister rs1) { FCvtLuS(rd, rs1, FPRoundingMode::kDefault); }
  void FCvtLuD(XRegister rd, FRegister rs1) { FCvtLuD(rd, rs1, FPRoundingMode::kDefault); }
  void FCvtSW(FRegister rd, XRegister rs1) { FCvtSW(rd, rs1, FPRoundingMode::kDefault); }
  void FCvtDW(FRegister rd, XRegister rs1) { FCvtDW(rd, rs1, FPRoundingMode::kDefault); }
  void FCvtSWu(FRegister rd, XRegister rs1) { FCvtSWu(rd, rs1, FPRoundingMode::kDefault); }
  void FCvtDWu(FRegister rd, XRegister rs1) { FCvtDWu(rd, rs1, FPRoundingMode::kDefault); }
  void FCvtSL(FRegister rd, XRegister rs1) { FCvtSL(rd, rs1, FPRoundingMode::kDefault); }
  void FCvtDL(FRegister rd, XRegister rs1) { FCvtDL(rd, rs1, FPRoundingMode::kDefault); }
  void FCvtSLu(FRegister rd, XRegister rs1) { FCvtSLu(rd, rs1, FPRoundingMode::kDefault); }
  void FCvtDLu(FRegister rd, XRegister rs1) { FCvtDLu(rd, rs1, FPRoundingMode::kDefault); }

  // FP move instructions (RV32F+RV32D): opcode = 0x53, funct3 = 0x0, funct7 = 0b111X00D
  void FMvXW(XRegister rd, FRegister rs1);
  void FMvXD(XRegister rd, FRegister rs1);
  void FMvWX(FRegister rd, XRegister rs1);
  void FMvDX(FRegister rd, XRegister rs1);

  // FP classify instructions (RV32F+RV32D): opcode = 0x53, funct3 = 0x1, funct7 = 0b111X00D
  void FClassS(XRegister rd, FRegister rs1);
  void FClassD(XRegister rd, FRegister rs1);

  ////////////////////////////// RV64 MACRO Instructions  START ///////////////////////////////
  // These pseudo instructions are from "RISC-V ASM manual".

  void Nop();
  void Li(XRegister rd, int32_t imm);
  void Mv(XRegister rd, XRegister rs);
  void Not(XRegister rd, XRegister rs);
  void Neg(XRegister rd, XRegister rs);
  void NegW(XRegister rd, XRegister rs);
  void SextB(XRegister rd, XRegister rs);
  void SextH(XRegister rd, XRegister rs);
  void SextW(XRegister rd, XRegister rs);
  void ZextB(XRegister rd, XRegister rs);
  void ZextH(XRegister rd, XRegister rs);
  void ZextW(XRegister rd, XRegister rs);
  void Seqz(XRegister rd, XRegister rs);
  void Snez(XRegister rd, XRegister rs);
  void Sltz(XRegister rd, XRegister rs);
  void Sgtz(XRegister rd, XRegister rs);
  void FMvS(FRegister rd, FRegister rs);
  void FAbsS(FRegister rd, FRegister rs);
  void FNegS(FRegister rd, FRegister rs);
  void FMvD(FRegister rd, FRegister rs);
  void FAbsD(FRegister rd, FRegister rs);
  void FNegD(FRegister rd, FRegister rs);

  // Branch pseudo instructions
  void Beqz(XRegister rs, int32_t offset);
  void Bnez(XRegister rs, int32_t offset);
  void Blez(XRegister rs, int32_t offset);
  void Bgez(XRegister rs, int32_t offset);
  void Bltz(XRegister rs, int32_t offset);
  void Bgtz(XRegister rs, int32_t offset);
  void Bgt(XRegister rs, XRegister rt, int32_t offset);
  void Ble(XRegister rs, XRegister rt, int32_t offset);
  void Bgtu(XRegister rs, XRegister rt, int32_t offset);
  void Bleu(XRegister rs, XRegister rt, int32_t offset);

  // Jump pseudo instructions
  void J(int32_t offset);
  void Jal(int32_t offset);
  void Jr(XRegister rs);
  void Jalr(XRegister rs);
  void Jalr(XRegister rd, XRegister rs);
  void Ret();
  void Call(int32_t offset);

  void Beqz(XRegister rs, Riscv64Label* label, bool is_bare = false);
  void Bnez(XRegister rs, Riscv64Label* label, bool is_bare = false);
  void Blez(XRegister rs, Riscv64Label* label, bool is_bare = false);
  void Bgez(XRegister rs, Riscv64Label* label, bool is_bare = false);
  void Bltz(XRegister rs, Riscv64Label* label, bool is_bare = false);
  void Bgtz(XRegister rs, Riscv64Label* label, bool is_bare = false);
  void Bgt(XRegister rs, XRegister rt, Riscv64Label* label, bool is_bare = false);
  void Ble(XRegister rs, XRegister rt, Riscv64Label* label, bool is_bare = false);
  void Bgtu(XRegister rs, XRegister rt, Riscv64Label* label, bool is_bare = false);
  void Bleu(XRegister rs, XRegister rt, Riscv64Label* label, bool is_bare = false);
  void J(Riscv64Label* label);
  void Jal(Riscv64Label* label);
  void Call(Riscv64Label* label);
  void Tail(Riscv64Label* label);

  // Jump and Branch to a label
  void Jal(XRegister rt, Riscv64Label* label, bool is_bare = false);
  void Beq(XRegister rs, XRegister rt, Riscv64Label* label, bool is_bare = false);
  void Bne(XRegister rs, XRegister rt, Riscv64Label* label, bool is_bare = false);
  void Blt(XRegister rs, XRegister rt, Riscv64Label* label, bool is_bare = false);
  void Bge(XRegister rs, XRegister rt, Riscv64Label* label, bool is_bare = false);
  void Bltu(XRegister rs, XRegister rt, Riscv64Label* label, bool is_bare = false);
  void Bgeu(XRegister rs, XRegister rt, Riscv64Label* label, bool is_bare = false);

  // Pseudo instructions for accessing control and status registers
  void RdInstret(XRegister rd);
  void RdCycle(XRegister rd);
  void RdTime(XRegister rd);
  void CsrR(XRegister rd, int32_t csr);
  void CsrW(int32_t csr, XRegister rd);
  void CsrS(int32_t csr, XRegister rd);
  void CsrWi(int32_t csr, int32_t imm);
  void CsrSi(int32_t csr, int32_t imm);
  void CsrCi(int32_t csr, int32_t imm);

  // Large const load
  void LoadConst32(XRegister rd, int32_t value);
  void LoadConst64(XRegister rd, int64_t value);

  // Branches
  void EmitBcond(BranchCondition cond, XRegister rs, XRegister rt, uint32_t imm16_21);

  void Jal(Riscv64Label* label, bool is_bare = false);

  /////////////////////////////// RV64 MACRO Instructions END ///////////////////////////////

  //
  // Heap poisoning.
  //

  // Poison a heap reference contained in `src` and store it in `dst`.
  void PoisonHeapReference(XRegister dst, XRegister src) {
    // dst = -src.
    // Negate the 32-bit ref.
    NegW(dst, src);
  }
  // Poison a heap reference contained in `reg`.
  void PoisonHeapReference(XRegister reg) {
    // reg = -reg.
    NegW(reg, reg);
  }
  // Unpoison a heap reference contained in `reg`.
  void UnpoisonHeapReference(XRegister reg) {
    // reg = -reg.
    // Negate the 32-bit ref.
    NegW(reg, reg);
  }
  // Poison a heap reference contained in `reg` if heap poisoning is enabled.
  void MaybePoisonHeapReference(XRegister reg) {
    if (kPoisonHeapReferences) {
      PoisonHeapReference(reg);
    }
  }
  // Unpoison a heap reference contained in `reg` if heap poisoning is enabled.
  void MaybeUnpoisonHeapReference(XRegister reg) {
    if (kPoisonHeapReferences) {
      UnpoisonHeapReference(reg);
    }
  }

  void Bind(Label* label) override { Bind(down_cast<Riscv64Label*>(label)); }
  void Jump(Label* label ATTRIBUTE_UNUSED) override {
    UNIMPLEMENTED(FATAL) << "Do not use Jump for RISCV64";
  }

  void Bind(Riscv64Label* label);

  // Create a new literal with a given value.
  // NOTE: Force the template parameter to be explicitly specified.
  template <typename T>
  Literal* NewLiteral(typename Identity<T>::type value) {
    static_assert(std::is_integral<T>::value, "T must be an integral type.");
    return NewLiteral(sizeof(value), reinterpret_cast<const uint8_t*>(&value));
  }

  // Load label address using PC-relative loads. To be used with data labels in the literal /
  // jump table area only and not with regular code labels.
  void LoadLabelAddress(XRegister dest_reg, Riscv64Label* label);

  // Create a new literal with the given data.
  Literal* NewLiteral(size_t size, const uint8_t* data);

  // Load literal using PC-relative loads.
  void LoadLiteral(XRegister dest_reg, LoadOperandType load_type, Literal* literal);

  // Create a jump table for the given labels that will be emitted when finalizing.
  // When the table is emitted, offsets will be relative to the location of the table.
  // The table location is determined by the location of its label (the label precedes
  // the table data) and should be loaded using LoadLabelAddress().
  JumpTable* CreateJumpTable(std::vector<Riscv64Label*>&& labels);

  void EmitLoad(ManagedRegister m_dst, XRegister src_register, int32_t src_offset, size_t size);
  void AdjustBaseAndOffset(XRegister& base, int32_t& offset, bool is_doubleword);

 private:
  // This will be used as an argument for loads/stores
  // when there is no need for implicit null checks.
  struct NoImplicitNullChecker {
    void operator()() const {}
  };

 public:
  template <typename ImplicitNullChecker = NoImplicitNullChecker>
  void StoreConstToOffset(StoreOperandType type,
                          int64_t value,
                          XRegister base,
                          int32_t offset,
                          XRegister temp,
                          ImplicitNullChecker null_checker = NoImplicitNullChecker()) {
    // We permit `base` and `temp` to coincide (however, we check that neither is AT),
    // in which case the `base` register may be overwritten in the process.
    CHECK_NE(temp, AT);  // Must not use AT as temp, so as not to overwrite the adjusted base.
    AdjustBaseAndOffset(base, offset, /* is_doubleword= */ (type == kStoreDoubleword));
    XRegister reg;
    // If the adjustment left `base` unchanged and equal to `temp`, we can't use `temp`
    // to load and hold the value but we can use AT instead as AT hasn't been used yet.
    // Otherwise, `temp` can be used for the value. And if `temp` is the same as the
    // original `base` (that is, `base` prior to the adjustment), the original `base`
    // register will be overwritten.
    if (base == temp) {
      temp = AT;
    }

    if (type == kStoreDoubleword && IsAligned<kRiscv64DoublewordSize>(offset)) {
      if (value == 0) {
        reg = Zero;
      } else {
        reg = temp;
        LoadConst64(reg, value);
      }
      Sd(reg, base, offset);
      null_checker();
    } else {
      uint32_t low = Low32Bits(value);
      uint32_t high = High32Bits(value);
      if (low == 0) {
        reg = Zero;
      } else {
        reg = temp;
        LoadConst32(reg, low);
      }
      switch (type) {
        case kStoreByte:
          Sb(reg, base, offset);
          break;
        case kStoreHalfword:
          Sh(reg, base, offset);
          break;
        case kStoreWord:
          Sw(reg, base, offset);
          break;
        case kStoreDoubleword:
          // not aligned to kRiscv64DoublewordSize
          CHECK_ALIGNED(offset, kRiscv64WordSize);
          Sw(reg, base, offset);
          null_checker();
          if (high == 0) {
            reg = Zero;
          } else {
            reg = temp;
            if (high != low) {
              LoadConst32(reg, high);
            }
          }
          Sw(reg, base, offset + kRiscv64WordSize);
          break;
        default:
          LOG(FATAL) << "UNREACHABLE";
      }
      if (type != kStoreDoubleword) {
        null_checker();
      }
    }
  }

  template <typename ImplicitNullChecker = NoImplicitNullChecker>
  void LoadFromOffset(LoadOperandType type,
                      XRegister reg,
                      XRegister base,
                      int32_t offset,
                      ImplicitNullChecker null_checker = NoImplicitNullChecker()) {
    AdjustBaseAndOffset(base, offset, /* is_doubleword= */ (type == kLoadDoubleword));

    switch (type) {
      case kLoadSignedByte:
        Lb(reg, base, offset);
        break;
      case kLoadUnsignedByte:
        Lbu(reg, base, offset);
        break;
      case kLoadSignedHalfword:
        Lh(reg, base, offset);
        break;
      case kLoadUnsignedHalfword:
        Lhu(reg, base, offset);
        break;
      case kLoadWord:
        CHECK_ALIGNED(offset, kRiscv64WordSize);
        Lw(reg, base, offset);
        break;
      case kLoadUnsignedWord:
        CHECK_ALIGNED(offset, kRiscv64WordSize);
        Lwu(reg, base, offset);
        break;
      case kLoadDoubleword:
        Ld(reg, base, offset);
        null_checker();
        break;
      default:
        LOG(FATAL) << "UNREACHABLE";
    }
    if (type != kLoadDoubleword) {
      null_checker();
    }
  }

  template <typename ImplicitNullChecker = NoImplicitNullChecker>
  void LoadFpuFromOffset(LoadOperandType type,
                         FRegister reg,
                         XRegister base,
                         int32_t offset,
                         ImplicitNullChecker null_checker = NoImplicitNullChecker()) {
    // int element_size_shift = -1;
    if (type != kLoadQuadword) {
      AdjustBaseAndOffset(base, offset, /* is_doubleword= */ (type == kLoadDoubleword));
    }
    switch (type) {
      case kLoadWord:
        CHECK_ALIGNED(offset, kRiscv64WordSize);
        FLw(reg, base, offset);
        null_checker();
        break;
      case kLoadDoubleword:
        FLd(reg, base, offset);
        null_checker();
        break;
      case kLoadQuadword:
        UNIMPLEMENTED(FATAL) << "store kStoreQuadword not implemented";
        break;
      default:
        LOG(FATAL) << "UNREACHABLE";
    }
  }

  template <typename ImplicitNullChecker = NoImplicitNullChecker>
  void StoreToOffset(StoreOperandType type,
                     XRegister reg,
                     XRegister base,
                     int32_t offset,
                     ImplicitNullChecker null_checker = NoImplicitNullChecker()) {
    // Must not use AT as `reg`, so as not to overwrite the value being stored
    // with the adjusted `base`.
    CHECK_NE(reg, AT);
    AdjustBaseAndOffset(base, offset, /* is_doubleword= */ (type == kStoreDoubleword));

    switch (type) {
      case kStoreByte:
        Sb(reg, base, offset);
        break;
      case kStoreHalfword:
        Sh(reg, base, offset);
        break;
      case kStoreWord:
        CHECK_ALIGNED(offset, kRiscv64WordSize);
        Sw(reg, base, offset);
        break;
      case kStoreDoubleword:
        Sd(reg, base, offset);
        null_checker();
        break;
      default:
        LOG(FATAL) << "UNREACHABLE";
    }
    if (type != kStoreDoubleword) {
      null_checker();
    }
  }

  template <typename ImplicitNullChecker = NoImplicitNullChecker>
  void StoreFpuToOffset(StoreOperandType type,
                        FRegister reg,
                        XRegister base,
                        int32_t offset,
                        ImplicitNullChecker null_checker = NoImplicitNullChecker()) {
    // int element_size_shift = -1;
    if (type != kStoreQuadword) {
      AdjustBaseAndOffset(base, offset, /* is_doubleword= */ (type == kStoreDoubleword));
    }

    switch (type) {
      case kStoreWord:
        CHECK_ALIGNED(offset, kRiscv64WordSize);
        FSw(reg, base, offset);
        null_checker();
        break;
      case kStoreDoubleword:
        FSd(reg, base, offset);
        null_checker();
        break;
      case kStoreQuadword:
        UNIMPLEMENTED(FATAL) << "store kStoreQuadword not implemented";
        null_checker();
        break;
      default:
        LOG(FATAL) << "UNREACHABLE";
    }
  }

  void LoadFromOffset(LoadOperandType type, XRegister reg, XRegister base, int32_t offset);
  void LoadFpuFromOffset(LoadOperandType type, FRegister reg, XRegister base, int32_t offset);
  void StoreToOffset(StoreOperandType type, XRegister reg, XRegister base, int32_t offset);
  void StoreFpuToOffset(StoreOperandType type, FRegister reg, XRegister base, int32_t offset);

  // Emit data (e.g. encoded instruction or immediate) to the instruction stream.
  void Emit(uint32_t value);

  // Emit slow paths queued during assembly and promote short branches to long if needed.
  void FinalizeCode() override;

  // Emit branches and finalize all instructions.
  void FinalizeInstructions(const MemoryRegion& region) override;

  // Returns the (always-)current location of a label (can be used in class CodeGeneratorRISCV64,
  // must be used instead of Riscv64Label::GetPosition()).
  uint32_t GetLabelLocation(const Riscv64Label* label) const;

  // Get the final position of a label after local fixup based on the old position
  // recorded before FinalizeCode().
  uint32_t GetAdjustedPosition(uint32_t old_position);

 private:
  class Branch {
   public:
    enum Type {
      // R6 short branches (can be promoted to long).
      kUncondBranch,
      kCondBranch,
      kCall,
      // R6 short branches (can't be promoted to long), forbidden/delay slots filled manually.
      kBareUncondBranch,
      kBareCondBranch,
      kBareCall,
      // label.
      kLabel,
      // literals.
      kLiteral,
      kLiteralUnsigned,
      kLiteralLong,
      // Long branches.
      kLongUncondBranch,
      kLongCondBranch,
      kLongCall,
    };

    // Bit sizes of offsets defined as enums to minimize chance of typos.
    enum OffsetBits {
      kOffset12 = 12,  // reserved for jalr
      kOffset13 = 13,
      kOffset21 = 21,
      kOffset32 = 32,
    };

    static constexpr uint32_t kUnresolved = 0xffffffff;  // Unresolved target_
    static constexpr int32_t kMaxBranchLength = 32;
    static constexpr int32_t kMaxBranchSize = kMaxBranchLength * sizeof(uint32_t);

    struct BranchInfo {
      // Branch length as a number of 4-byte-long instructions.
      uint32_t length;
      // Ordinal number (0-based) of the first (or the only) instruction that contains the branch's
      // PC-relative offset (or its most significant 16-bit half, which goes first).
      uint32_t instr_offset;
      // Different RISC-V instructions with PC-relative offsets apply said offsets to slightly
      // different origins, e.g. to PC or PC+4. Encode the origin distance (as a number of 4-byte
      // instructions) from the instruction containing the offset.
      uint32_t pc_org;
      // How large (in bits) a PC-relative offset can be for a given type of branch (kCondBranch
      // and kBareCondBranch are an exception: use kOffset23 for beqzc/bnezc).
      OffsetBits offset_size;
      // Some RISC-V instructions with PC-relative offsets shift the offset by 2. Encode the shift
      // count.
      int offset_shift;
    };
    static const BranchInfo branch_info_[/* Type */];

    // Unconditional branch or call.
    Branch(uint32_t location, uint32_t target, bool is_call, bool is_bare);
    // Conditional branch.
    Branch(uint32_t location,
           uint32_t target,
           BranchCondition condition,
           XRegister lhs_reg,
           XRegister rhs_reg,
           bool is_bare);
    // Label address (in literal area) or literal.
    Branch(uint32_t location, XRegister dest_reg, Type label_or_literal_type);

    // Some conditional branches with lhs = rhs are effectively NOPs, while some
    // others are effectively unconditional.
    static bool IsNop(BranchCondition condition, XRegister lhs, XRegister rhs);
    static bool IsUncond(BranchCondition condition, XRegister lhs, XRegister rhs);

    static BranchCondition OppositeCondition(BranchCondition cond);

    Type GetType() const;
    BranchCondition GetCondition() const;
    XRegister GetLeftRegister() const;
    XRegister GetRightRegister() const;
    uint32_t GetTarget() const;
    uint32_t GetLocation() const;
    uint32_t GetOldLocation() const;
    uint32_t GetLength() const;
    uint32_t GetOldLength() const;
    uint32_t GetSize() const;
    uint32_t GetOldSize() const;
    uint32_t GetEndLocation() const;
    uint32_t GetOldEndLocation() const;
    bool IsBare() const;
    bool IsLong() const;
    bool IsResolved() const;

    // Returns the bit size of the signed offset that the branch instruction can handle.
    OffsetBits GetOffsetSize() const;

    // Calculates the distance between two byte locations in the assembler buffer and
    // returns the number of bits needed to represent the distance as a signed integer.
    //
    // Branch instructions have signed offsets of 16, 19 (addiupc), 21 (beqzc/bnezc),
    // and 26 (bc) bits, which are additionally shifted left 2 positions at run time.
    //
    // Composite branches (made of several instructions) with longer reach have 32-bit
    // offsets encoded as 2 16-bit "halves" in two instructions (high half goes first).
    // The composite branches cover the range of PC + ~+/-2GB. The range is not end-to-end,
    // however. Consider the following implementation of a long unconditional branch, for
    // example:
    //
    //   auipc at, offset_31_16  // at = pc + sign_extend(offset_31_16) << 16
    //   jic   at, offset_15_0   // pc = at + sign_extend(offset_15_0)
    //
    // Both of the above instructions take 16-bit signed offsets as immediate operands.
    // When bit 15 of offset_15_0 is 1, it effectively causes subtraction of 0x10000
    // due to sign extension. This must be compensated for by incrementing offset_31_16
    // by 1. offset_31_16 can only be incremented by 1 if it's not 0x7FFF. If it is
    // 0x7FFF, adding 1 will overflow the positive offset into the negative range.
    // Therefore, the long branch range is something like from PC - 0x80000000 to
    // PC + 0x7FFF7FFF, IOW, shorter by 32KB on one side.
    //
    // The returned values are therefore: 18, 21, 23, 28 and 32. There's also a special
    // case with the addiu instruction and a 16 bit offset.
    static OffsetBits GetOffsetSizeNeeded(uint32_t location, uint32_t target);

    // Resolve a branch when the target is known.
    void Resolve(uint32_t target);

    // Relocate a branch by a given delta if needed due to expansion of this or another
    // branch at a given location by this delta (just changes location_ and target_).
    void Relocate(uint32_t expand_location, uint32_t delta);

    // If the branch is short, changes its type to long.
    void PromoteToLong();

    // If necessary, updates the type by promoting a short branch to a long branch
    // based on the branch location and target. Returns the amount (in bytes) by
    // which the branch size has increased.
    // max_short_distance caps the maximum distance between location_ and target_
    // that is allowed for short branches. This is for debugging/testing purposes.
    // max_short_distance = 0 forces all short branches to become long.
    // Use the implicit default argument when not debugging/testing.
    uint32_t PromoteIfNeeded(uint32_t max_short_distance = std::numeric_limits<uint32_t>::max());

    // Returns the location of the instruction(s) containing the offset.
    uint32_t GetOffsetLocation() const;

    // Calculates and returns the offset ready for encoding in the branch instruction(s).
    uint32_t GetOffset() const;

   private:
    // Completes branch construction by determining and recording its type.
    void InitializeType(Type initial_type);
    // Helper for the above.
    void InitShortOrLong(OffsetBits ofs_size, Type short_type, Type long_type);

    uint32_t old_location_;  // Offset into assembler buffer in bytes.
    uint32_t location_;      // Offset into assembler buffer in bytes.
    uint32_t target_;        // Offset into assembler buffer in bytes.

    XRegister lhs_reg_;          // Left-hand side register in conditional branches or
                                 // destination register in literals.
    XRegister rhs_reg_;          // Right-hand side register in conditional branches.
    BranchCondition condition_;  // Condition for conditional branches.

    Type type_;      // Current type of the branch.
    Type old_type_;  // Initial type of the branch.
  };
  friend std::ostream& operator<<(std::ostream& os, const Branch::Type& rhs);
  friend std::ostream& operator<<(std::ostream& os, const Branch::OffsetBits& rhs);

  template <typename Reg1, typename Reg2>
  void EmitI(int32_t imm12, Reg1 rs1, uint32_t funct3, Reg2 rd, uint32_t opcode) {
    DCHECK(IsInt<12>(imm12)) << imm12;
    DCHECK(IsUint<5>(static_cast<uint32_t>(rs1)));
    DCHECK(IsUint<3>(funct3));
    DCHECK(IsUint<5>(static_cast<uint32_t>(rd)));
    DCHECK(IsUint<7>(opcode));
    uint32_t encoding = static_cast<uint32_t>(imm12) << 20 | static_cast<uint32_t>(rs1) << 15 |
                        funct3 << 12 | static_cast<uint32_t>(rd) << 7 | opcode;
    Emit(encoding);
  }

  template <typename Reg1, typename Reg2, typename Reg3>
  void EmitR(uint32_t funct7, Reg1 rs2, Reg2 rs1, uint32_t funct3, Reg3 rd, uint32_t opcode) {
    DCHECK(IsUint<7>(funct7));
    DCHECK(IsUint<5>(static_cast<uint32_t>(rs2)));
    DCHECK(IsUint<5>(static_cast<uint32_t>(rs1)));
    DCHECK(IsUint<3>(funct3));
    DCHECK(IsUint<5>(static_cast<uint32_t>(rd)));
    DCHECK(IsUint<7>(opcode));
    uint32_t encoding = funct7 << 25 | static_cast<uint32_t>(rs2) << 20 |
                        static_cast<uint32_t>(rs1) << 15 | funct3 << 12 |
                        static_cast<uint32_t>(rd) << 7 | opcode;
    Emit(encoding);
  }

  template <typename Reg1, typename Reg2, typename Reg3, typename Reg4>
  void EmitR4(Reg1 rs3,
              uint32_t funct2,
              Reg2 rs2,
              Reg3 rs1,
              uint32_t funct3,
              Reg4 rd,
              uint32_t opcode) {
    DCHECK(IsUint<5>(static_cast<uint32_t>(rs3)));
    DCHECK(IsUint<2>(funct2));
    DCHECK(IsUint<5>(static_cast<uint32_t>(rs2)));
    DCHECK(IsUint<5>(static_cast<uint32_t>(rs1)));
    DCHECK(IsUint<3>(funct3));
    DCHECK(IsUint<5>(static_cast<uint32_t>(rd)));
    DCHECK(IsUint<7>(opcode));
    uint32_t encoding = static_cast<uint32_t>(rs3) << 27 | static_cast<uint32_t>(funct2) << 25 |
                        static_cast<uint32_t>(rs2) << 20 | static_cast<uint32_t>(rs1) << 15 |
                        static_cast<uint32_t>(funct3) << 12 | static_cast<uint32_t>(rd) << 7 |
                        opcode;
    Emit(encoding);
  }

  template <typename Reg1, typename Reg2>
  void EmitS(int32_t imm12, Reg1 rs2, Reg2 rs1, uint32_t funct3, uint32_t opcode) {
    DCHECK(IsInt<12>(imm12)) << imm12;
    DCHECK(IsUint<5>(static_cast<uint32_t>(rs2)));
    DCHECK(IsUint<5>(static_cast<uint32_t>(rs1)));
    DCHECK(IsUint<3>(funct3));
    DCHECK(IsUint<7>(opcode));
    uint32_t encoding = (static_cast<uint32_t>(imm12) & 0xFE0) << 20 |
                        static_cast<uint32_t>(rs2) << 20 | static_cast<uint32_t>(rs1) << 15 |
                        static_cast<uint32_t>(funct3) << 12 |
                        (static_cast<uint32_t>(imm12) & 0x1F) << 7 | opcode;
    Emit(encoding);
  }

  void EmitI6(uint32_t funct6,
              uint32_t imm6,
              XRegister rs1,
              uint32_t funct3,
              XRegister rd,
              uint32_t opcode) {
    DCHECK(IsUint<6>(funct6));
    DCHECK(IsUint<6>(imm6)) << imm6;
    DCHECK(IsUint<5>(static_cast<uint32_t>(rs1)));
    DCHECK(IsUint<3>(funct3));
    DCHECK(IsUint<5>(static_cast<uint32_t>(rd)));
    DCHECK(IsUint<7>(opcode));
    uint32_t encoding = funct6 << 26 | static_cast<uint32_t>(imm6) << 20 |
                        static_cast<uint32_t>(rs1) << 15 | funct3 << 12 |
                        static_cast<uint32_t>(rd) << 7 | opcode;
    Emit(encoding);
  }

  void EmitB(int32_t offset, XRegister rs2, XRegister rs1, uint32_t funct3, uint32_t opcode) {
    DCHECK_ALIGNED(offset, 2);
    DCHECK(IsInt<13>(offset)) << offset;
    DCHECK(IsUint<5>(static_cast<uint32_t>(rs2)));
    DCHECK(IsUint<5>(static_cast<uint32_t>(rs1)));
    DCHECK(IsUint<3>(funct3));
    uint32_t imm12 = (static_cast<uint32_t>(offset) >> 1) & 0xfffu;
    uint32_t encoding = (imm12 & 0x800u) << (31 - 11) | (imm12 & 0x03f0u) << (25 - 4) |
                        static_cast<uint32_t>(rs2) << 20 | static_cast<uint32_t>(rs1) << 15 |
                        static_cast<uint32_t>(funct3) << 12 |
                        (imm12 & 0xfu) << 8 | (imm12 & 0x400u) >> (10 - 7) | opcode;
    Emit(encoding);
  }

  void EmitU(uint32_t imm20, XRegister rd, uint32_t opcode) {
    CHECK(IsUint<20>(imm20)) << imm20;
    DCHECK(IsUint<5>(static_cast<uint32_t>(rd)));
    DCHECK(IsUint<7>(opcode));
    uint32_t encoding = imm20 << 12 | static_cast<uint32_t>(rd) << 7 | opcode;
    Emit(encoding);
  }

  void EmitJ(int32_t offset, XRegister rd, uint32_t opcode) {
    DCHECK_ALIGNED(offset, 2);
    CHECK(IsInt<21>(offset)) << offset;
    DCHECK(IsUint<5>(static_cast<uint32_t>(rd)));
    DCHECK(IsUint<7>(opcode));
    uint32_t imm20 = (static_cast<uint32_t>(offset) >> 1) & 0xfffffu;
    uint32_t encoding = (imm20 & 0x80000u) << (31 - 19) | (imm20 & 0x03ffu) << 21 |
                        (imm20 & 0x400u) << (20 - 10) | (imm20 & 0x7f800u) << (12 - 11) |
                        static_cast<uint32_t>(rd) << 7 | opcode;
    Emit(encoding);
  }

  /////////////////////////////// RV64 VARIANTS extension ////////////////

  void EmitLiterals();
  void EmitBranch(Branch* branch);
  void EmitBranches();
  void EmitJumpTables();

  void Buncond(Riscv64Label* label, bool is_bare);
  void Bcond(Riscv64Label* label,
             bool is_bare,
             BranchCondition condition,
             XRegister lhs,
             XRegister rhs = Zero);

  Branch* GetBranch(uint32_t branch_id);
  const Branch* GetBranch(uint32_t branch_id) const;
  void ReserveJumpTableSpace();
  void PromoteBranches();
  void PatchCFI();

  void Call(Riscv64Label* label, bool is_bare);
  void FinalizeLabeledBranch(Riscv64Label* label);

  std::vector<Branch> branches_;

  // Whether appending instructions at the end of the buffer or overwriting the existing ones.
  bool overwriting_;
  // The current overwrite location.
  uint32_t overwrite_location_;

  // Use std::deque<> for literal labels to allow insertions at the end
  // without invalidating pointers and references to existing elements.
  ArenaDeque<Literal> literals_;
  ArenaDeque<Literal> long_literals_;  // 64-bit literals separated for alignment reasons.

  // Jump table list.
  ArenaDeque<JumpTable> jump_tables_;

  // Data for AdjustedPosition(), see the description there.
  uint32_t last_position_adjustment_;
  uint32_t last_old_position_;
  uint32_t last_branch_id_;

  Riscv64Subset subsets_;

  static constexpr uint32_t kXlen = 64;

  DISALLOW_COPY_AND_ASSIGN(Riscv64Assembler);
};

}  // namespace riscv64
}  // namespace art

#endif  // ART_COMPILER_UTILS_RISCV64_ASSEMBLER_RISCV64_H_
