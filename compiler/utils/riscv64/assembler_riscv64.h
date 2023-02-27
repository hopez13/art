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

#include <deque>
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
#include "offsets.h"
#include "utils/assembler.h"
#include "utils/jni_macro_assembler.h"
#include "utils/label.h"

namespace art {
namespace riscv64 {

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

enum RoundingMode { RNE = 0, RTZ = 1, RDN = 2, RUP = 3, RMM = 4, DYN = 7, Invalid };

// the parameters of fence instruction
enum FenceParameters {
  fence_none = 0,
  fence_write = 1,
  fence_read = 2,
  fence_output = 4,
  fence_input = 8,
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

class Riscv64Assembler final : public Assembler {
 public:
  // using JNIBase = JNIMacroAssembler<PointerSize::k64>;

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
        last_branch_id_(0) {
    // cfi().DelayEmittingAdvancePCs();
    UNUSED(instruction_set_features);
  }

  virtual ~Riscv64Assembler() {}

  // size_t CodeSize() const override { return Assembler::CodeSize(); }
  // DebugFrameOpCodeWriterForAssembler& cfi() override { return Assembler::cfi(); }

  // Emit Machine Instructions.
  // Arithmetic
  void Add(XRegister rd, XRegister rs1, XRegister rs2);    // Add
  void Addi(XRegister rd, XRegister rs1, int16_t imm12);   // Add Immediate
  void Addw(XRegister rd, XRegister rs1, XRegister rs2);   // Add Word (RV64I-only)
  void Addiw(XRegister rd, XRegister rs1, int16_t imm12);  // Add Immediate Word(RV64I-only)
  void Sub(XRegister rd, XRegister rs1, XRegister rs2);    // Sub
  void Subw(XRegister rd, XRegister rs1, XRegister rs2);   // Sub Word (RV64I-only)

  void Mul(XRegister rd, XRegister rs1, XRegister rs2);     // Multiplication
  void Mulh(XRegister rd, XRegister rs1, XRegister rs2);    // Multiply High
  void Mulhsu(XRegister rd, XRegister rs1, XRegister rs2);  // Multiply High Signed-Unsigned
  void Mulhu(XRegister rd, XRegister rs1, XRegister rs2);   // Multiply High Unsigned
  void Mulw(XRegister rd, XRegister rs1, XRegister rs2);    // Multiply Word
  void Div(XRegister rd, XRegister rs1, XRegister rs2);     // Divide
  void Divu(XRegister rd, XRegister rs1, XRegister rs2);    // Divide Unsigned
  void Divw(XRegister rd, XRegister rs1, XRegister rs2);    // Divide Word
  void Divuw(XRegister rd, XRegister rs1, XRegister rs2);   // Divide Word, Unsigned
  void Rem(XRegister rd, XRegister rs1, XRegister rs2);     // Remainder
  void Remw(XRegister rd, XRegister rs1, XRegister rs2);    // Remainder Word

  // Logic
  void And(XRegister rd, XRegister rs1, XRegister rs2);  // AND
  void Andi(XRegister rd,
            XRegister rs1,
            int64_t imm12);  // AND Immediate, define the imm12 as int64_t type to detect some over
                             // range scenario
  void Neg(XRegister rd, XRegister rs2);                // Negate
  void Negw(XRegister rd, XRegister rs2);               // Negate Word
  void Or(XRegister rd, XRegister rs1, XRegister rs2);  // OR
  void Ori(XRegister rd,
           XRegister rs1,
           int64_t imm12);  // OR Immediate, define the imm12 as int64_t type to detect some over
                            // range scenario
  void Xor(XRegister rd, XRegister rs1, XRegister rs2);  // XOR
  void Xori(XRegister rd,
            XRegister rs1,
            int64_t imm12);  // Exclusive-OR Immediate, define the imm12 as int64_t type to detect
                             // some over range scenario

  // Shift
  void Sll(XRegister rd, XRegister rs1, XRegister rs2);     // Shift Left Logical
  void Slli(XRegister rd, XRegister rs1, uint16_t shamt6);  // Shift Left Logical Immediate
  void Srl(XRegister rd, XRegister rs1, XRegister rs2);     // Shift right Logical
  void Srli(XRegister rd, XRegister rs1, uint16_t shamt6);  // Shift right Logical Immediate
  void Sra(XRegister rd, XRegister rs1, XRegister rs2);     // Shift right Arithmetic
  void Srai(XRegister rd, XRegister rs1, uint16_t shamt6);  // Shift right Arithmetic Immediate
  void Sllw(XRegister rd, XRegister rs1, XRegister rs2);    // Shift Left Logical Word(RV64I-only)
  void Slliw(XRegister rd,
             XRegister rs1,
             uint16_t shamt5);  // Shift Left Logical Word Immediate (RV64I-only)
  void Srlw(XRegister rd, XRegister rs1, XRegister rs2);  // Shift right Logical Word(RV64I-only)
  void Srliw(XRegister rd,
             XRegister rs1,
             uint16_t shamt5);  // Shift right Logical Word Immediate (RV64I-only)
  void Sraw(XRegister rd,
            XRegister rs1,
            XRegister rs2);  // Shift Right Arithmetic Word (RV64I-only)
  void Sraiw(XRegister rd,
             XRegister rs1,
             uint16_t shamt5);  // Shift Right Arithmetic Word Immediate (RV64I-only)

  // Loads
  void Lb(XRegister rd, XRegister rs1, int16_t imm12);   // Load Byte
  void Lh(XRegister rd, XRegister rs1, int16_t imm12);   // Load Halfword
  void Lw(XRegister rd, XRegister rs1, int16_t imm12);   // Load Word
  void Ld(XRegister rd, XRegister rs1, int16_t imm12);   // Load Doubleword
  void Lbu(XRegister rd, XRegister rs1, int16_t imm12);  // Load Byte Unsigned
  void Lhu(XRegister rd, XRegister rs1, int16_t imm12);  // Load Halfword Unsigned
  void Lwu(XRegister rd, XRegister rs1, int16_t imm12);  // Load Word, Unsigned

  void Lui(XRegister rd, uint32_t imm20);  // Load Upper Immediate

  void LrD(XRegister rd, XRegister rs1);      // Load-Reserved Doubleword
  void LrDAq(XRegister rd, XRegister rs1);    // Load-Reserved Doubleword with aq bit set
  void LrDRl(XRegister rd, XRegister rs1);    // Load-Reserved Doubleword with rl bit set
  void LrDAqrl(XRegister rd, XRegister rs1);  // Load-Reserved Doubleword with aq and rl bit set
  void LrW(XRegister rd, XRegister rs1);      // Load-Reserved Word
  void LrWAq(XRegister rd, XRegister rs1);    // Load-Reserved Word with aq bit set
  void LrWRl(XRegister rd, XRegister rs1);    // Load-Reserved Word with rl bit set
  void LrWAqrl(XRegister rd, XRegister rs1);  // Load-Reserved Word with aq and rl bit set

  void Li(XRegister rd, int64_t imm64);          // Load Immediate Pesudo Instruction
  void LiInternal(XRegister rd, int64_t imm64);  // Li internal implementation function

  // Store
  void Sb(XRegister rs2, XRegister rs1, int16_t imm12);  // Store Byte
  void Sh(XRegister rs2, XRegister rs1, int16_t imm12);  // Store Halfword
  void Sw(XRegister rs2, XRegister rs1, int16_t imm12);  // Store Word
  void Sd(XRegister rs2, XRegister rs1, int16_t imm12);  // Store Doubleword
  void ScD(XRegister rd, XRegister rs2, XRegister rs1);  // Store-Conditional Doubleword
  void ScDAq(XRegister rd,
             XRegister rs2,
             XRegister rs1);  // Store-Conditional Doubleword with aq bit set
  void ScDRl(XRegister rd,
             XRegister rs2,
             XRegister rs1);  // Store-Conditional Doubleword with rl bit set
  void ScDAqrl(XRegister rd,
               XRegister rs2,
               XRegister rs1);  // Store-Conditional Doubleword with aq and rl bit set
  void ScW(XRegister rd, XRegister rs2, XRegister rs1);      // Store-Conditional Word
  void ScWAq(XRegister rd, XRegister rs2, XRegister rs1);    // Store-Conditional Word
  void ScWRl(XRegister rd, XRegister rs2, XRegister rs1);    // Store-Conditional Word
  void ScWAqrl(XRegister rd, XRegister rs2, XRegister rs1);  // Store-Conditional Word

  // Compare
  void Slt(XRegister rd, XRegister rs1, XRegister rs2);    // Set If Less Then
  void Sltu(XRegister rd, XRegister rs1, XRegister rs2);   // Set If Less Then, Unsigned
  void Slti(XRegister rd, XRegister rs1, int16_t imm12);   // Set If Less Then Immediate
  void Sltiu(XRegister rd, XRegister rs1, int16_t imm12);  // Set If Less Then Immediate, Unsigned

  // Jump & Link
  void Jalr(XRegister rd, XRegister rs1, int16_t imm12);  // Jump and Link Register
  void Jalr(XRegister rs1);                // Pseudo instruction for Jump and Link Register
  void Jr(XRegister rs1);                  // Pseudo instruction,Jump to rs1 without saving LR
  void Jal(XRegister rd, uint32_t imm20);  // Jump and link Immediate
  void Jal(uint32_t imm20);                // Pseudo instruction for jal Jump and Link
  void J(uint32_t imm20);                  // Pseudo instruction,Jump to rs1 without saving LR

  void Auipc(XRegister rd, uint32_t imm20);  // Add Upper Immediate to PC
  void Ret(void);                            // Return

  // void Call(XRegister rd, uint32_t imm32);                      // Pseudo instruction for call
  // void Call(uint32_t imm32);                                      // Pseudo instruction for call

  void Fence(uint16_t prec = riscv64::FenceParameters::fence_input |
                             riscv64::FenceParameters::fence_output |
                             riscv64::FenceParameters::fence_read |
                             riscv64::FenceParameters::fence_write,
             uint16_t succ = riscv64::FenceParameters::fence_input |
                             riscv64::FenceParameters::fence_output |
                             riscv64::FenceParameters::fence_read |
                             riscv64::FenceParameters::fence_write);  // Fence Memory and I/O

  // Branches
  void Beq(XRegister rs1, XRegister rs2, uint16_t imm12);   // Branch if Equal
  void Beqz(XRegister rs1, uint16_t imm12);                 // Pseudo instruction for Beq
  void Bne(XRegister rs1, XRegister rs2, uint16_t imm12);   // Branch if Not Equal
  void Bnez(XRegister rs1, uint16_t imm12);                 // Pseudo instruction for Bne
  void Blt(XRegister rs1, XRegister rs2, uint16_t imm12);   // Branch if Less Than
  void Bge(XRegister rs1, XRegister rs2, uint16_t imm12);   // Branch if Greater Than or Equal
  void Bltu(XRegister rs1, XRegister rs2, uint16_t imm12);  // Branch if Less Than, Unsigned
  void Bgeu(XRegister rs1,
            XRegister rs2,
            uint16_t imm12);  // Branch if Greater Than or Equal, Unsigned
  void Bgt(XRegister rs1,
           XRegister rs2,
           uint16_t imm12);  // Pseudo instruction Branch if Greater Than
  void Ble(XRegister rs1,
           XRegister rs2,
           uint16_t imm12);  // Pseudo instruction Branch if Less Than or Equal
  void Bgtu(XRegister rs1,
            XRegister rs2,
            uint16_t imm12);  // Pseudo instruction Branch if Greater Than, Unsigned
  void Bleu(XRegister rs1,
            XRegister rs2,
            uint16_t imm12);  // Pseudo instruction Branch if Less Than or Equal, Unsigned
  void Blez(XRegister rs1, uint16_t imm12);  // Pseudo instruction for Ble
  void Bgez(XRegister rs1, uint16_t imm12);  // Pseudo instruction for Bge
  void Bltz(XRegister rs1, uint16_t imm12);  // Pseudo instruction for Bge
  void Bgtz(XRegister rs1, uint16_t imm12);  // Pseudo instruction for Bge

  /** Floating Single-Precision begins **/

  // Arithmetic
  void FaddS(FRegister frd,
             FRegister frs1,
             FRegister frs2,
             RoundingMode rm = RoundingMode::DYN);  // Floating-point Add, Single-Precision
  void FsubS(FRegister frd,
             FRegister frs1,
             FRegister frs2,
             RoundingMode rm = RoundingMode::DYN);  // Floating-point Subtract, Single-Precision
  void FmulS(FRegister frd,
             FRegister frs1,
             FRegister frs2,
             RoundingMode rm = RoundingMode::DYN);  // Floating-point Multiply, Single-Precision
  void FmaddS(
      FRegister frd,
      FRegister frs1,
      FRegister frs2,
      FRegister frs3,
      RoundingMode rm = RoundingMode::DYN);  // Floating-point Fused Multiply-Add, Single-Precision
  void FmsubS(FRegister frd,
              FRegister frs1,
              FRegister frs2,
              FRegister frs3,
              RoundingMode rm =
                  RoundingMode::DYN);  // Floating-point Fused Multiply- Subtarct, Single-Precision
  void FnmaddS(FRegister frd,
               FRegister frs1,
               FRegister frs2,
               FRegister frs3,
               RoundingMode rm = RoundingMode::DYN);  // Floating-point Fused Negative Multiply-Add,
                                                      // Single-Precision
  void FnmsubS(FRegister frd,
               FRegister frs1,
               FRegister frs2,
               FRegister frs3,
               RoundingMode rm = RoundingMode::DYN);  // Floating-point Fused Negative
                                                      // Multiply-Subtract, Single-Precision
  void FdivS(FRegister frd,
             FRegister frs1,
             FRegister frs2,
             RoundingMode rm = RoundingMode::DYN);  // Floating-point Divide, Single-Precision
  void FsqrtS(FRegister frd,
              FRegister frs1,
              RoundingMode rm = RoundingMode::DYN);  // Floating-point Square Root, Single-Precision

  // Move
  void FmvS(
      FRegister frd,
      FRegister frs1);  // Pseudo Instruction Floating-point Move Floating-point, Single-Precision
  void FmvWX(FRegister frd,
             XRegister rs1);  // Floating-point Move Word from Integer, Single-Precision
  void FmvXW(XRegister rd,
             FRegister frs1);  // Floating-point Move Word to Integer, Single-Precision

  void FclassS(XRegister rd, FRegister frs1);  // Floating-point Classify, Single-Precision

  void FcvtLS(XRegister rd,
              FRegister frs1,
              RoundingMode rm = RoundingMode::DYN);  // Floating-point Convert to Long from Single
  void FcvtLuS(
      XRegister rd,
      FRegister frs1,
      RoundingMode rm = RoundingMode::DYN);  // Floating-point Convert to Unsigned Long from Single
  void FcvtWS(XRegister rd,
              FRegister frs1,
              RoundingMode rm = RoundingMode::DYN);  // Floating-point Convert to Word from Single
  void FcvtWuS(
      XRegister rd,
      FRegister frs1,
      RoundingMode rm = RoundingMode::DYN);  // Floating-point Convert toUnsigned Word from Single
  void FcvtSL(FRegister frd,
              XRegister rs1,
              RoundingMode rm = RoundingMode::DYN);  // Floating-point Convert to Single From Long
  void FcvtSLu(
      FRegister frd,
      XRegister rs1,
      RoundingMode rm = RoundingMode::DYN);  // Floating-point Convert to Single from Unsigned Long
  void FcvtSW(FRegister frd,
              XRegister rs1,
              RoundingMode rm = RoundingMode::DYN);  // Floating-point Convert to Single From Word
  void FcvtSWu(
      FRegister frd,
      XRegister rs1,
      RoundingMode rm = RoundingMode::DYN);  // Floating-point Convert to Single From Unsigned Word

  void FmaxS(FRegister frd,
             FRegister frs1,
             FRegister frs2);  // Floating-point Maximum, Single-Precision
  void FminS(FRegister frd,
             FRegister frs1,
             FRegister frs2);                 // Floating-point MiNImum, Single-Precision
  void FabsS(FRegister frd, FRegister frs1);  // Floating-point abs, Single-Precision
  void FnegS(FRegister frd, FRegister frs1);  // Floating-point neg, Single-Precision

  void FsgnjS(FRegister frd,
              FRegister frs1,
              FRegister frs2);  // Floating-point Sign Inject, Single-Precision
  void FsgnjnS(FRegister frd,
               FRegister frs1,
               FRegister frs2);  // Floating-point Sign Inject-Negate, Single-Precision
  void FsgnjxS(FRegister frd,
               FRegister frs1,
               FRegister frs2);  // Floating-point Sign Inject-Xor, Single-Precision

  // Compare
  void FeqS(XRegister rd,
            FRegister frs1,
            FRegister frs2);  // Floating-point Equals, Single-Precision
  void FleS(XRegister rd,
            FRegister frs1,
            FRegister frs2);  // Floating-point Less Than or Equals, Single-Precision
  void FltS(XRegister rd,
            FRegister frs1,
            FRegister frs2);  // Floating-point Less Than, Single-Precision

  // Load
  void Flw(FRegister frd, XRegister rs1, int16_t imm12);  // Floating-point Load Word

  // Store
  void Fsw(FRegister frs2, XRegister rs1, int16_t imm12);  // Floating-point Store Word

  /** Floating Single-Precision ends **/

  /** Floating Double-Precision begins **/

  // Arithmetic
  void FaddD(FRegister frd,
             FRegister frs1,
             FRegister frs2,
             RoundingMode rm = RoundingMode::DYN);  // Floating-point Add, Double-Precision
  void FsubD(FRegister frd,
             FRegister frs1,
             FRegister frs2,
             RoundingMode rm = RoundingMode::DYN);  // Floating-point Subtract, Double-Precision
  void FmulD(FRegister frd,
             FRegister frs1,
             FRegister frs2,
             RoundingMode rm = RoundingMode::DYN);  // Floating-point Multiply, Double-Precision
  void FmaddD(
      FRegister frd,
      FRegister frs1,
      FRegister frs2,
      FRegister frs3,
      RoundingMode rm = RoundingMode::DYN);  // Floating-point Fused Multiply-Add, Double-Precision
  void FmsubD(FRegister frd,
              FRegister frs1,
              FRegister frs2,
              FRegister frs3,
              RoundingMode rm =
                  RoundingMode::DYN);  // Floating-point Fused Multiply- Subtract, Double-Precision
  void FnmaddD(FRegister frd,
               FRegister frs1,
               FRegister frs2,
               FRegister frs3,
               RoundingMode rm = RoundingMode::DYN);  // Floating-point Fused Negative Multiply-Add,
                                                      // Double-Precision
  void FnmsubD(FRegister frd,
               FRegister frs1,
               FRegister frs2,
               FRegister frs3,
               RoundingMode rm = RoundingMode::DYN);  // Floating-point Fused Negative
                                                      // Multiply-Subtract, Double-Precision
  void FdivD(FRegister frd,
             FRegister frs1,
             FRegister frs2,
             RoundingMode rm = RoundingMode::DYN);  // Floating-point Divide, Double-Precision
  void FsqrtD(FRegister frd,
              FRegister frs1,
              RoundingMode rm = RoundingMode::DYN);  // Floating-point Square Root, Double-Precision

  void FmvDX(FRegister frd,
             XRegister rs1);  // Floating-point Move Word from Integer, Double-Precision
  void FmvXD(XRegister rd,
             FRegister frs1);  // Floating-point Move Word to Integer, Double-Precision
  void FmvD(FRegister frd,
            FRegister frs1);  // Pseudo Instruction Floating-point Move Double to Double

  void FclassD(XRegister rd, FRegister frs1);  // Floating-point Classify, Double-Precision

  void FcvtLD(XRegister rd,
              FRegister frs1,
              RoundingMode rm = RoundingMode::DYN);  // Floating-point Convert to Long from Double
  void FcvtLuD(
      XRegister rd,
      FRegister frs1,
      RoundingMode rm = RoundingMode::DYN);  // Floating-point Convert to Unsigned Long from Double
  void FcvtWD(
      XRegister rd,
      FRegister frs1,
      RoundingMode rm = RoundingMode::DYN);  // Floating-point Convert to Word from Double-Precision
  void FcvtWuD(XRegister rd,
               FRegister frs1,
               RoundingMode rm = RoundingMode::DYN);  // Floating-point Convert to Unsigned Word
                                                      // from Double-Precision
  void FcvtDL(FRegister frd,
              XRegister rs1,
              RoundingMode rm = RoundingMode::DYN);  // Floating-point Convert to Double From Long
  void FcvtDLu(
      FRegister frd,
      XRegister rs1,
      RoundingMode rm = RoundingMode::DYN);   // Floating-point Convert to Double from Unsigned Long
  void FcvtDW(FRegister frd, XRegister rs1);  // Floating-point Convert to Double From Word,
                                              // widening convertion no RM field
  void FcvtDWu(FRegister frd, XRegister rs1);  // Floating-point Convert to Double From Unsigned
                                               // Word, widening convertion no RM field
  void FcvtDS(FRegister frd, FRegister frs1);  // Floating-point Convert to Double from Single,
                                               // widening convertion no RM field
  void FcvtSD(FRegister frd,
              FRegister frs1,
              RoundingMode rm = RoundingMode::DYN);  // Floating-point Convert to Single from Double

  void FmaxD(FRegister frd,
             FRegister frs1,
             FRegister frs2);  // Floating-point Maximum, Double-Precision
  void FminD(FRegister frd,
             FRegister frs1,
             FRegister frs2);                 // Floating-point Minimum, Double-Precision
  void FabsD(FRegister frd, FRegister frs1);  // Floating-point abs, Double-Precision
  void FnegD(FRegister frd, FRegister frs1);  // Floating-point neg, Double-Precision

  void FsgnjD(FRegister frd,
              FRegister frs1,
              FRegister frs2);  // Floating-point Sign Inject, Double-Precision
  void FsgnjnD(FRegister frd,
               FRegister frs1,
               FRegister frs2);  // Floating-point Sign Inject-Negate, Double-Precision
  void FsgnjxD(FRegister frd,
               FRegister frs1,
               FRegister frs2);  // Floating-point Sign Inject-Xor, Double-Precision

  // Compare
  void FeqD(XRegister rd,
            FRegister frs1,
            FRegister frs2);  // Floating-point Equals, Double-Precision
  void FleD(XRegister rd,
            FRegister frs1,
            FRegister frs2);  // Floating-point Less Than or Equals, Double-Precision
  void FltD(XRegister rd,
            FRegister frs1,
            FRegister frs2);  // Floating-point Less Than, Double-Precision

  // Load
  void Fld(FRegister frd, XRegister rs1, int16_t imm12);  // Floating-point Load Double Word

  // Store
  void Fsd(FRegister frs2, XRegister rs1, int16_t imm12);  // Floating-point Store Double Word

  /** Floating Double-Precision ends **/

  void Ebreak(void);
  void Nop(void);
  void Mv(XRegister rd, XRegister rs1);

  void Addiw32(XRegister rd, XRegister rs1, int32_t value);
  void Addi64(XRegister rd, XRegister rs1, int64_t value);
  void Seqz(XRegister rd, XRegister rs1);
  void AdjustBaseAndOffset(XRegister& base, int32_t& offset, bool is_doubleword);

  //
  // Heap poisoning.
  //

  // Poison a heap reference contained in `src` and store it in `dst`.
  void PoisonHeapReference(XRegister dst, XRegister src) {
    // dst = -src.
    // Negate the 32-bit ref.
    Sub(dst, Zero, src);
    // And constrain it to 32 bits (zero-extend into bits 32 through 63) as on Arm64 and x86/64.
    Slli(dst, dst, 32);
    Srli(dst, dst, 32);
  }

  // Poison a heap reference contained in `reg`.
  void PoisonHeapReference(XRegister reg) {
    // reg = -reg.
    PoisonHeapReference(reg, reg);
  }
  // Unpoison a heap reference contained in `reg`.
  void UnpoisonHeapReference(XRegister reg) {
    // reg = -reg.
    // Negate the 32-bit ref.
    Sub(reg, Zero, reg);
    // And constrain it to 32 bits (zero-extend into bits 32 through 63) as on Arm64 and x86/64.
    Slli(reg, reg, 32);
    Srli(reg, reg, 32);
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

  void J(Riscv64Label* label);
  void Call(XRegister rd, Riscv64Label* label);
  void Call(Riscv64Label* label);
  void Beq(XRegister rs1, XRegister rs2, Riscv64Label* label);
  void Bne(XRegister rs1, XRegister rs2, Riscv64Label* label);
  void Blt(XRegister rs1, XRegister rs2, Riscv64Label* label);
  void Bge(XRegister rs1, XRegister rs2, Riscv64Label* label);
  void Bltu(XRegister rs1, XRegister rs2, Riscv64Label* label);
  void Bgeu(XRegister rs1, XRegister rs2, Riscv64Label* label);
  void Bgt(XRegister rs1, XRegister rs2, Riscv64Label* label);
  void Ble(XRegister rs1, XRegister rs2, Riscv64Label* label);
  void Bgtu(XRegister rs1, XRegister rs2, Riscv64Label* label);
  void Bleu(XRegister rs1, XRegister rs2, Riscv64Label* label);
  void Blez(XRegister rs1, Riscv64Label* label);
  void Bgez(XRegister rs1, Riscv64Label* label);
  void Bltz(XRegister rs1, Riscv64Label* label);
  void Bgtz(XRegister rs1, Riscv64Label* label);
  void Beqz(XRegister rs1, Riscv64Label* label);
  void Bnez(XRegister rs1, Riscv64Label* label);

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
        Li(reg, value);
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
        Li(reg, low);
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
              Li(reg, high);
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
    CHECK_NE(base, AT);  // Must not overwrite the register `base` while loading `offset`.
    CHECK_NE(reg, AT);   // Must not overwrite the register `base` while loading `offset`.

    // after AdjustBaseAndOffset, base could be AT.
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
        if (!IsAligned<kRiscv64DoublewordSize>(offset)) {
          CHECK_ALIGNED(offset, kRiscv64WordSize);
          // If base == reg, the x[base] could be falsely changed after 1st Lwu(),
          // which will cause issue during 2nd Lwu() with the changed value of X[base]
          CHECK_NE(base, reg);
          Lwu(reg, base, offset);
          null_checker();
          Lwu(AT, base, offset + kRiscv64WordSize);
          Slli(AT, AT, 32);
          Add(reg, reg, AT);
        } else {
          Ld(reg, base, offset);
          null_checker();
        }
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
    CHECK_NE(base, AT);    // Must not overwrite the register `base` while loading `offset`.
    CHECK_NE(base, TMP2);  // Must not overwrite the register `base` while loading `offset`.

    if (type != kLoadQuadword) {
      // after AdjustBaseAndOffset, base could be AT.
      AdjustBaseAndOffset(base, offset, /* is_doubleword= */ (type == kLoadDoubleword));
    } else {
      // AdjustBaseOffsetAndElementSizeShift(base, offset, element_size_shift);
      UNIMPLEMENTED(FATAL) << "We don't support Load Quadword";
    }

    switch (type) {
      case kLoadWord:
        CHECK_ALIGNED(offset, kRiscv64WordSize);
        Flw(reg, base, offset);
        null_checker();
        break;
      case kLoadDoubleword:
        if (!IsAligned<kRiscv64DoublewordSize>(offset)) {
          CHECK_ALIGNED(offset, kRiscv64WordSize);
          // Note: base could be AT after calling AdjustBaseAndOffset(), carefully use TMP2/AT here
          Lwu(TMP2, base, offset + kRiscv64WordSize);
          // Implicit null checks must be associated with the first 32-bit load/store in a pair and
          // not the last
          null_checker();
          Slli(TMP2, TMP2, 32);
          Lwu(AT, base, offset);
          Add(AT, AT, TMP2);
          FmvDX(reg, AT);
        } else {
          Fld(reg, base, offset);
          null_checker();
        }
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
    CHECK_NE(reg, TMP2);
    CHECK_NE(base, AT);
    CHECK_NE(base, TMP2);

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
        if (!IsAligned<kRiscv64DoublewordSize>(offset)) {
          CHECK_ALIGNED(offset, kRiscv64WordSize);
          Sw(reg, base, offset);
          // Implicit null checks must be associated with the first 32-bit load/store in a pair and
          // not the last.
          null_checker();
          Srli(TMP2, reg, 32);
          Sw(TMP2, base, offset + kRiscv64WordSize);
        } else {
          Sd(reg, base, offset);
          null_checker();
        }
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
    CHECK_NE(base, AT);

    if (type != kStoreQuadword) {
      AdjustBaseAndOffset(base, offset, /* is_doubleword= */ (type == kStoreDoubleword));
    } else {
      // AdjustBaseOffsetAndElementSizeShift(base, offset, element_size_shift);
      UNIMPLEMENTED(FATAL) << "We don't support Load Quadword";
    }

    switch (type) {
      case kStoreWord:
        CHECK_ALIGNED(offset, kRiscv64WordSize);
        Fsw(reg, base, offset);
        null_checker();
        break;
      case kStoreDoubleword:
        if (!IsAligned<kRiscv64DoublewordSize>(offset)) {
          CHECK_ALIGNED(offset, kRiscv64WordSize);
          // Note: base could be AT after calling AdjustBaseAndOffset(), carefully use TMP2/AT here
          FmvXD(TMP2, reg);
          Sw(TMP2, base, offset);
          // Implicit null checks must be associated with the first 32-bit load/store in a pair and
          // not the last.
          null_checker();
          Srli(TMP2, TMP2, 32);
          Sw(TMP2, base, offset + kRiscv64WordSize);
        } else {
          Fsd(reg, base, offset);
          null_checker();
        }
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

  void IncreaseFrameSize(size_t adjust);
  void DecreaseFrameSize(size_t adjust);

  // Emit slow paths queued during assembly and promote short branches to long if needed.
  void FinalizeCode() override;

  // Emit branches and finalize all instructions.
  void FinalizeInstructions(const MemoryRegion& region) override;

  // Returns the (always-)current location of a label (can be used in class CodeGeneratorRiscv64,
  // must be used instead of Riscv64Label::GetPosition()).
  uint32_t GetLabelLocation(const Riscv64Label* label) const;

  // Get the final position of a label after local fixup based on the old position
  // recorded before FinalizeCode().
  uint32_t GetAdjustedPosition(uint32_t old_position);

  bool getOverWriting() const { return overwriting_; }

  // Note that PC-relative literal loads are handled as pseudo branches because they need very
  // similar relocation and may similarly expand in size to accomodate for larger offsets relative
  // to PC.
  enum BranchCondition {
    kCondEQ,
    kCondNE,
    kCondLT,
    kCondGE,
    kCondLTU,
    kCondGEU,
    kCondGT,
    kCondLE,
    kCondGTU,
    kCondLEU,
    kCondEQZ,
    kCondNEZ,
    kCondLEZ,
    kCondGEZ,
    kCondLTZ,
    kCondGTZ,
    // kCondF,    // Floating-point Branch is not supported.
    // kCondT,    // Floating-point Branch is not supported.
    kUncond,
  };
  // friend std::ostream& operator<<(std::ostream& os, const BranchCondition& rhs);

  class Branch {
   public:
    enum Type {
      // short branches (can be promoted to long).
      kUncondBranch,
      kCondBranch,
      kCall,
      // Near label.
      kLabel,
      // Near literals.
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
      kOffset13 = 13,
      kOffset21 = 21,
      kOffset32 = 32,
      kOffsetNeedtoCheck = 0xffffffff,
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
      // Different RISCV64 instructions with PC-relative offsets apply said offsets to slightly
      // different origins, e.g. to PC or PC+4. Encode the origin distance (as a number of 4-byte
      // instructions) from the instruction containing the offset.
      uint32_t pc_org;
      // How large (in bits) a PC-relative offset can be for a given type of branch (kCondBranch
      // and kBareCondBranch are an exception: use kOffset23 for beqzc/bnezc).
      OffsetBits offset_size;
      // Some RISCV instructions with PC-relative offsets shift the offset by 2. Encode the shift
      // count.
      int offset_shift;
    };
    static const BranchInfo branch_info_[/* Type */];

    // Unconditional branch or call.
    Branch(uint32_t location, uint32_t target, bool is_call, XRegister lhs_reg = Zero);
    // Conditional branch.
    Branch(uint32_t location,
           uint32_t target,
           BranchCondition condition,
           XRegister lhs_reg,
           XRegister rhs_reg);
    // Label address (in literal area) or literal.
    Branch(uint32_t location, XRegister dest_reg, Type label_or_literal_type);

    // Some conditional branches with lhs = rhs are effectively NOPs, while some
    // others are effectively unconditional. MIPSR6 conditional branches require lhs != rhs.
    // So, we need a way to identify such branches in order to emit no instructions for them
    // or change them to unconditional.
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

 private:
  class Register {
   public:
    Register(XRegister r) {  // NOLINT [runtime/explicit] [5]
      CHECK_NE(r, kNoXRegister);
      value_ = static_cast<uint32_t>(r);
    }
    Register(FRegister r) {  // NOLINT [runtime/explicit] [5]
      CHECK_NE(r, kNoFRegister);
      value_ = static_cast<uint32_t>(r);
    }
    operator uint32_t() const { return value_; }

   private:
    uint32_t value_;
    DISALLOW_IMPLICIT_CONSTRUCTORS(Register);
  };

  void EmitR(
      int32_t funct7, Register rs2, Register rs1, int32_t funct3, Register rd, int32_t opcode);
  void EmitR4(Register rs3,
              int32_t funct2,
              Register rs2,
              Register rs1,
              int32_t funct3,
              Register rd,
              int32_t opcode);
  void EmitI(uint16_t imm12, XRegister rs1, int32_t funct3, Register rd, int32_t opcode);
  void EmitS(
      uint16_t imm7, Register rs2, XRegister rs1, int32_t funct3, uint16_t imm5, int32_t opcode);
  void EmitB(uint16_t imm12, XRegister rs2, XRegister rs1, int32_t funct3, int32_t opcode);
  void EmitU(uint32_t imm20, XRegister rd, int32_t opcode);
  void EmitJ(uint32_t imm20, XRegister rd, int32_t opcode);
  void EmitBcond(BranchCondition cond, XRegister rs1, XRegister rs2, uint32_t imm12);
  void Buncond(Riscv64Label* label);
  void Bcond(Riscv64Label* label, BranchCondition condition, XRegister lhs, XRegister rhs = Zero);
  void BuncondCall(XRegister rd, Riscv64Label* label);
  void FinalizeLabeledBranch(Riscv64Label* label);

  Branch* GetBranch(uint32_t branch_id);
  const Branch* GetBranch(uint32_t branch_id) const;

  void EmitLiterals();
  void ReserveJumpTableSpace();
  void EmitJumpTables();
  void PromoteBranches();
  void EmitBranch(Branch* branch);
  void EmitBranches();
  void PatchCFI();

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

  // const bool has_msa_;
  friend class AssemblerRiscv64TargetTest;

  DISALLOW_COPY_AND_ASSIGN(Riscv64Assembler);
};

}  // namespace riscv64
}  // namespace art

#endif  // ART_COMPILER_UTILS_RISCV64_ASSEMBLER_RISCV64_H_
