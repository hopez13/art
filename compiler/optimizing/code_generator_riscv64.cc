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

#include "code_generator_riscv64.h"

#include "android-base/logging.h"
#include "android-base/macros.h"
#include "arch/riscv64/jni_frame_riscv64.h"
#include "arch/riscv64/registers_riscv64.h"
#include "base/macros.h"
#include "dwarf/register.h"
#include "jit/profiling_info.h"
#include "mirror/class-inl.h"
#include "optimizing/nodes.h"
#include "stack_map_stream.h"
#include "utils/label.h"
#include "utils/riscv64/assembler_riscv64.h"
#include "utils/stack_checks.h"

namespace art {
namespace riscv64 {

static constexpr XRegister kCoreCalleeSaves[] = {
    S1, S2, S3, S4, S5, S6, S7, S8, S9, S10, S11, S0, RA};
static constexpr FRegister kFpuCalleeSaves[] = {
    FS0, FS1, FS2, FS3, FS4, FS5, FS6, FS7, FS8, FS9, FS10, FS11};

#define QUICK_ENTRY_POINT(x) QUICK_ENTRYPOINT_OFFSET(kRiscv64PointerSize, x).Int32Value()

Location Riscv64ReturnLocation(DataType::Type return_type) {
  switch (return_type) {
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
    case DataType::Type::kUint32:
    case DataType::Type::kInt32:
    case DataType::Type::kReference:
    case DataType::Type::kUint64:
    case DataType::Type::kInt64:
      return Location::RegisterLocation(A0);

    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64:
      return Location::FpuRegisterLocation(FA0);

    case DataType::Type::kVoid:
      return Location();
  }
  UNREACHABLE();
}

static RegisterSet OneRegInReferenceOutSaveEverythingCallerSaves() {
  InvokeRuntimeCallingConvention calling_convention;
  RegisterSet caller_saves = RegisterSet::Empty();
  caller_saves.Add(Location::RegisterLocation(calling_convention.GetRegisterAt(0)));
  // The reference is returned in the same register. This differs from the standard return location.
  return caller_saves;
}

Location InvokeDexCallingConventionVisitorRISCV64::GetReturnLocation(DataType::Type type) const {
  return Riscv64ReturnLocation(type);
}

Location InvokeDexCallingConventionVisitorRISCV64::GetMethodLocation() const {
  return Location::RegisterLocation(kArtMethodRegister);
}

Location InvokeDexCallingConventionVisitorRISCV64::GetNextLocation(DataType::Type type) {
  Location next_location;
  if (type == DataType::Type::kVoid) {
    LOG(FATAL) << "Unexpected parameter type " << type;
  }

  if (DataType::IsFloatingPointType(type) &&
      float_index_ < calling_convention.GetNumberOfFpuRegisters()) {
    next_location =
        Location::FpuRegisterLocation(calling_convention.GetFpuRegisterAt(float_index_++));
  } else if (DataType::IsFloatingPointType(type) &&
             (gp_index_ < calling_convention.GetNumberOfRegisters())) {
    // Riscv64 will try GPR when FPRs are used out.
    // According to https://github.com/riscv-non-isa/riscv-elf-psabi-doc/blob/master/riscv-cc.adoc:
    // "A real floating-point argument is passed in a floating-point argument register
    // if it is no more than FLEN bits wide and at least one floating-point argument
    // register is available. Otherwise, it is passed according to the integer
    // calling convention."
    next_location = Location::RegisterLocation(calling_convention.GetRegisterAt(gp_index_++));
  } else if (!DataType::IsFloatingPointType(type) &&
             (gp_index_ < calling_convention.GetNumberOfRegisters())) {
    next_location = Location::RegisterLocation(calling_convention.GetRegisterAt(gp_index_++));
  } else {
    size_t stack_offset = calling_convention.GetStackOffsetOf(stack_index_);
    next_location = DataType::Is64BitType(type) ? Location::DoubleStackSlot(stack_offset) :
                                                  Location::StackSlot(stack_offset);
  }

  // Space on the stack is reserved for all arguments.
  stack_index_ += DataType::Is64BitType(type) ? 2 : 1;

  return next_location;
}

Location CriticalNativeCallingConventionVisitorRiscv64::GetNextLocation(DataType::Type type) {
  DCHECK_NE(type, DataType::Type::kReference);

  Location location = Location::NoLocation();
  if (DataType::IsFloatingPointType(type)) {
    if (fpr_index_ < kParameterFpuRegistersLength) {
      location = Location::FpuRegisterLocation(kParameterFpuRegisters[fpr_index_]);
      ++fpr_index_;
    }
  } else {
    // Native ABI uses the same registers as managed, except that the method register A0
    // is a normal argument.
    if (gpr_index_ < 1u + kParameterCoreRegistersLength) {
      location = Location::RegisterLocation(
          gpr_index_ == 0u ? A0 : kParameterCoreRegisters[gpr_index_ - 1u]);
      ++gpr_index_;
    }
  }
  if (location.IsInvalid()) {
    if (DataType::Is64BitType(type)) {
      location = Location::DoubleStackSlot(stack_offset_);
    } else {
      location = Location::StackSlot(stack_offset_);
    }
    stack_offset_ += kFramePointerSize;

    if (for_register_allocation_) {
      location = Location::Any();
    }
  }
  return location;
}

Location CriticalNativeCallingConventionVisitorRiscv64::GetReturnLocation(
    DataType::Type type) const {
  // We perform conversion to the managed ABI return register after the call if needed.
  InvokeDexCallingConventionVisitorRISCV64 dex_calling_convention;
  return dex_calling_convention.GetReturnLocation(type);
}

Location CriticalNativeCallingConventionVisitorRiscv64::GetMethodLocation() const {
  // Pass the method in the hidden argument T0.
  return Location::RegisterLocation(T0);
}

#define __ down_cast<CodeGeneratorRISCV64*>(codegen)->GetAssembler()->  // NOLINT

class CompileOptimizedSlowPathRISCV64 : public SlowPathCodeRISCV64 {
 public:
  CompileOptimizedSlowPathRISCV64() : SlowPathCodeRISCV64(/* instruction= */ nullptr) {}

  void EmitNativeCode(CodeGenerator* codegen) override {
    uint32_t entrypoint_offset =
        GetThreadOffset<kArm64PointerSize>(kQuickCompileOptimized).Int32Value();
    __ Bind(GetEntryLabel());
    __ Loadw(RA, TR, entrypoint_offset);
    // Note: we don't record the call here (and therefore don't generate a stack
    // map), as the entrypoint should never be suspended.
    __ Jalr(RA);
    __ J(GetExitLabel());
  }

  const char* GetDescription() const override { return "CompileOptimizedSlowPath"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(CompileOptimizedSlowPathRISCV64);
};

class NullCheckSlowPathRISCV64 : public SlowPathCodeRISCV64 {
 public:
  explicit NullCheckSlowPathRISCV64(HNullCheck* instr) : SlowPathCodeRISCV64(instr) {}

  void EmitNativeCode(CodeGenerator* codegen) override {
    CodeGeneratorRISCV64* riscv64_codegen = down_cast<CodeGeneratorRISCV64*>(codegen);
    __ Bind(GetEntryLabel());
    if (instruction_->CanThrowIntoCatchBlock()) {
      // Live registers will be restored in the catch block if caught.
      SaveLiveRegisters(codegen, instruction_->GetLocations());
    }
    riscv64_codegen->InvokeRuntime(
        kQuickThrowNullPointer, instruction_, instruction_->GetDexPc(), this);
    CheckEntrypointTypes<kQuickThrowNullPointer, void, void>();
  }

  bool IsFatal() const override { return true; }

  const char* GetDescription() const override { return "NullCheckSlowPathRISCV64"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(NullCheckSlowPathRISCV64);
};

class BoundsCheckSlowPathRISCV64 : public SlowPathCodeRISCV64 {
 public:
  explicit BoundsCheckSlowPathRISCV64(HBoundsCheck* instruction)
      : SlowPathCodeRISCV64(instruction) {}

  void EmitNativeCode(CodeGenerator* codegen) override {
    LocationSummary* locations = instruction_->GetLocations();
    CodeGeneratorRISCV64* riscv64_codegen = down_cast<CodeGeneratorRISCV64*>(codegen);
    __ Bind(GetEntryLabel());
    if (instruction_->CanThrowIntoCatchBlock()) {
      // Live registers will be restored in the catch block if caught.
      SaveLiveRegisters(codegen, instruction_->GetLocations());
    }
    // We're moving two locations to locations that could overlap, so we need a parallel
    // move resolver.
    InvokeRuntimeCallingConvention calling_convention;
    codegen->EmitParallelMoves(locations->InAt(0),
                               Location::RegisterLocation(calling_convention.GetRegisterAt(0)),
                               DataType::Type::kInt32,
                               locations->InAt(1),
                               Location::RegisterLocation(calling_convention.GetRegisterAt(1)),
                               DataType::Type::kInt32);
    QuickEntrypointEnum entrypoint = instruction_->AsBoundsCheck()->IsStringCharAt() ?
                                         kQuickThrowStringBounds :
                                         kQuickThrowArrayBounds;
    riscv64_codegen->InvokeRuntime(entrypoint, instruction_, instruction_->GetDexPc(), this);
    CheckEntrypointTypes<kQuickThrowStringBounds, void, int32_t, int32_t>();
    CheckEntrypointTypes<kQuickThrowArrayBounds, void, int32_t, int32_t>();
  }

  bool IsFatal() const override { return true; }

  const char* GetDescription() const override { return "BoundsCheckSlowPathRISCV64"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(BoundsCheckSlowPathRISCV64);
};

class LoadClassSlowPathRISCV64 : public SlowPathCodeRISCV64 {
 public:
  LoadClassSlowPathRISCV64(HLoadClass* cls, HInstruction* at) : SlowPathCodeRISCV64(at), cls_(cls) {
    DCHECK(at->IsLoadClass() || at->IsClinitCheck());
    DCHECK_EQ(instruction_->IsLoadClass(), cls_ == instruction_);
  }

  void EmitNativeCode(CodeGenerator* codegen) override {
    LocationSummary* locations = instruction_->GetLocations();
    Location out = locations->Out();
    const uint32_t dex_pc = instruction_->GetDexPc();
    bool must_resolve_type = instruction_->IsLoadClass() && cls_->MustResolveTypeOnSlowPath();
    bool must_do_clinit = instruction_->IsClinitCheck() || cls_->MustGenerateClinitCheck();

    CodeGeneratorRISCV64* riscv64_codegen = down_cast<CodeGeneratorRISCV64*>(codegen);
    __ Bind(GetEntryLabel());
    SaveLiveRegisters(codegen, locations);

    InvokeRuntimeCallingConvention calling_convention;
    if (must_resolve_type) {
      DCHECK(IsSameDexFile(cls_->GetDexFile(), riscv64_codegen->GetGraph()->GetDexFile()));
      dex::TypeIndex type_index = cls_->GetTypeIndex();
      __ LoadConst32(calling_convention.GetRegisterAt(0), type_index.index_);
      if (cls_->NeedsAccessCheck()) {
        CheckEntrypointTypes<kQuickResolveTypeAndVerifyAccess, void*, uint32_t>();
        riscv64_codegen->InvokeRuntime(
            kQuickResolveTypeAndVerifyAccess, instruction_, dex_pc, this);
      } else {
        CheckEntrypointTypes<kQuickResolveType, void*, uint32_t>();
        riscv64_codegen->InvokeRuntime(kQuickResolveType, instruction_, dex_pc, this);
      }
      // If we also must_do_clinit, the resolved type is now in the correct register.
    } else {
      DCHECK(must_do_clinit);
      Location source = instruction_->IsLoadClass() ? out : locations->InAt(0);
      riscv64_codegen->MoveLocation(
          Location::RegisterLocation(calling_convention.GetRegisterAt(0)), source, cls_->GetType());
    }
    if (must_do_clinit) {
      riscv64_codegen->InvokeRuntime(kQuickInitializeStaticStorage, instruction_, dex_pc, this);
      CheckEntrypointTypes<kQuickInitializeStaticStorage, void*, mirror::Class*>();
    }

    // Move the class to the desired location.
    if (out.IsValid()) {
      DCHECK(out.IsRegister() && !locations->GetLiveRegisters()->ContainsCoreRegister(out.reg()));
      DataType::Type type = instruction_->GetType();
      riscv64_codegen->MoveLocation(
          out, Location::RegisterLocation(calling_convention.GetRegisterAt(0)), type);
    }
    RestoreLiveRegisters(codegen, locations);

    __ J(GetExitLabel());
  }

  const char* GetDescription() const override { return "LoadClassSlowPathRISCV64"; }

 private:
  // The class this slow path will load.
  HLoadClass* const cls_;

  DISALLOW_COPY_AND_ASSIGN(LoadClassSlowPathRISCV64);
};

#undef __
#define __ down_cast<CodeGeneratorRISCV64*>(codegen_)->GetAssembler()->  // NOLINT

void LocationsBuilderRISCV64::HandleInvoke(HInvoke* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}

Location LocationsBuilderRISCV64::RegisterOrZeroConstant(HInstruction* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
  UNREACHABLE();
}
Location LocationsBuilderRISCV64::FpuRegisterOrConstantForStore(HInstruction* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
  UNREACHABLE();
}

InstructionCodeGeneratorRISCV64::InstructionCodeGeneratorRISCV64(HGraph* graph,
                                                                 CodeGeneratorRISCV64* codegen)
    : InstructionCodeGenerator(graph, codegen),
      assembler_(codegen->GetAssembler()),
      codegen_(codegen) {}

void InstructionCodeGeneratorRISCV64::GenerateClassInitializationCheck(
    SlowPathCodeRISCV64* slow_path, XRegister class_reg) {
  constexpr size_t status_lsb_position = SubtypeCheckBits::BitStructSizeOf();
  const size_t status_byte_offset =
      mirror::Class::StatusOffset().SizeValue() + (status_lsb_position / kBitsPerByte);
  constexpr uint32_t shifted_initialized_value = enum_cast<uint32_t>(ClassStatus::kInitialized)
                                                 << (status_lsb_position % kBitsPerByte);

  __ Loadbu(TMP2, class_reg, status_byte_offset);
  __ Sltiu(TMP, TMP2, shifted_initialized_value);
  __ Bnez(TMP, slow_path->GetEntryLabel());
  __ Bind(slow_path->GetExitLabel());
}

void InstructionCodeGeneratorRISCV64::GenerateBitstringTypeCheckCompare(
    HTypeCheckInstruction* instruction, XRegister temp) {
  UNUSED(instruction);
  UNUSED(temp);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::GenerateSuspendCheck(HSuspendCheck* instruction,
                                                           HBasicBlock* successor) {
  UNUSED(instruction);
  UNUSED(successor);
  LOG(FATAL) << "Unimplemented";
}

void InstructionCodeGeneratorRISCV64::GenerateMinMaxInt(LocationSummary* locations, bool is_min) {
  UNUSED(locations);
  UNUSED(is_min);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::GenerateMinMaxFP(LocationSummary* locations,
                                                       bool is_min,
                                                       DataType::Type type) {
  UNUSED(locations);
  UNUSED(is_min);
  UNUSED(type);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::GenerateMinMax(HBinaryOperation* instruction, bool is_min) {
  UNUSED(instruction);
  UNUSED(is_min);
  LOG(FATAL) << "Unimplemented";
}

// Generate a heap reference load using one register `out`:
//
//   out <- *(out + offset)
//
// while honoring heap poisoning and/or read barriers (if any).
//
// Location `maybe_temp` is used when generating a read barrier and
// shall be a register in that case; it may be an invalid location
// otherwise.
void InstructionCodeGeneratorRISCV64::GenerateReferenceLoadOneRegister(
    HInstruction* instruction,
    Location out,
    uint32_t offset,
    Location maybe_temp,
    ReadBarrierOption read_barrier_option) {
  UNUSED(instruction);
  UNUSED(out);
  UNUSED(offset);
  UNUSED(maybe_temp);
  UNUSED(read_barrier_option);
  LOG(FATAL) << "Unimplemented";
}
// Generate a heap reference load using two different registers
// `out` and `obj`:
//
//   out <- *(obj + offset)
//
// while honoring heap poisoning and/or read barriers (if any).
//
// Location `maybe_temp` is used when generating a Baker's (fast
// path) read barrier and shall be a register in that case; it may
// be an invalid location otherwise.
void InstructionCodeGeneratorRISCV64::GenerateReferenceLoadTwoRegisters(
    HInstruction* instruction,
    Location out,
    Location obj,
    uint32_t offset,
    Location maybe_temp,
    ReadBarrierOption read_barrier_option) {
  UNUSED(instruction);
  UNUSED(out);
  UNUSED(offset);
  UNUSED(maybe_temp);
  UNUSED(read_barrier_option);
  UNUSED(obj);
  LOG(FATAL) << "Unimplemented";
}

// Generate a GC root reference load:
//
//   root <- *(obj + offset)
//
// while honoring read barriers (if any).
void InstructionCodeGeneratorRISCV64::GenerateGcRootFieldLoad(HInstruction* instruction,
                                                              Location root,
                                                              XRegister obj,
                                                              uint32_t offset,
                                                              ReadBarrierOption read_barrier_option,
                                                              Riscv64Label* label_low) {
  UNUSED(instruction);
  UNUSED(root);
  UNUSED(obj);
  UNUSED(offset);
  UNUSED(read_barrier_option);
  UNUSED(label_low);
  LOG(FATAL) << "Unimplemented";
}

void InstructionCodeGeneratorRISCV64::GenerateTestAndBranch(HInstruction* instruction,
                                                            size_t condition_input_index,
                                                            Riscv64Label* true_target,
                                                            Riscv64Label* false_target) {
  UNUSED(instruction);
  UNUSED(condition_input_index);
  UNUSED(true_target);
  UNUSED(false_target);
  LOG(FATAL) << "Unimplemented";
}

void InstructionCodeGeneratorRISCV64::DivRemOneOrMinusOne(HBinaryOperation* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}

void InstructionCodeGeneratorRISCV64::DivRemByPowerOfTwo(HBinaryOperation* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}

void InstructionCodeGeneratorRISCV64::GenerateDivRemWithAnyConstant(HBinaryOperation* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}

void InstructionCodeGeneratorRISCV64::GenerateDivRemIntegral(HBinaryOperation* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::GenerateIntLongCompare(IfCondition cond,
                                                             bool is_64bit,
                                                             LocationSummary* locations) {
  XRegister rd = locations->Out().AsRegister<XRegister>();
  XRegister rs1 = locations->InAt(0).AsRegister<XRegister>();
  Location rs2_location = locations->InAt(1);
  XRegister rs2 = Zero;
  int64_t imm = 0;
  bool use_imm = rs2_location.IsConstant();
  if (use_imm) {
    if (is_64bit) {
      imm = CodeGenerator::GetInt64ValueOf(rs2_location.GetConstant());
    } else {
      imm = CodeGenerator::GetInt32ValueOf(rs2_location.GetConstant());
    }
  } else {
    rs2 = rs2_location.AsRegister<XRegister>();
  }
  switch (cond) {
    case kCondEQ:
    case kCondNE:
      //////////////
      if (use_imm) {
        if (imm == 0) {
          if (cond == kCondEQ) {
            __ Seqz(rd, rs1);
          } else {
            __ Snez(rd, rs1);
          }
        } else {
          if (is_64bit) {
            if (IsInt<11>(abs(imm)) || (imm == 2048)) {
              __ Addi(rd, rs1, (-imm) & 0xfff);
            } else {
              __ Li(rd, -imm);
              __ Add(rd, rs1, rd);
            }
          } else {
            if (IsInt<12>(abs(imm)) || (imm == 2048)) {
              __ Addiw(rd, rs1, (-imm) & 0xfff);
            } else {
              __ Li(rd, imm);
              __ Addw(rd, TMP, rs1);
            }
          }
          if (cond == kCondEQ) {
            __ Seqz(rd, rd);
          } else {
            __ Snez(rd, rd);
          }
        }
      } else {
        // register.
        __ Sub(rd, rs1, rs2);
        if (cond == kCondEQ) {
          __ Sltiu(rd, rd, 1);
        } else {
          __ Sltu(rd, Zero, rd);
        }
      }
      break;

    case kCondLT:
    case kCondGE:
      // Use 11-bit here for avoiding sign-extension.
      if (use_imm) {
        if (IsInt<11>(abs(imm)) || (imm == -2048)) {
          __ Slti(rd, rs1, imm & 0xfff);
        } else {
          __ Li(rd, imm);
          __ Slt(rd, rs1, rd);
        }
      } else {
        __ Slt(rd, rs1, rs2);
      }
      if (cond == kCondGE) {
        // Simulate rs1 >= rs2 via !(rs1 < rs2) since there's
        // only the slt instruction but no sge.
        __ Xori(rd, rd, 1);
      }
      break;

    case kCondLE:
    case kCondGT:
      if (use_imm) {
        imm += 1;
        if (IsInt<11>(abs(imm)) || (imm == -2048)) {
          __ Slti(rd, rs1, imm & 0xfff);
        } else {
          __ Li(rd, imm);
          __ Slt(rd, rd, rs1);
        }
      } else {
        __ Slt(rd, rs2, rs1);
        __ Xori(rd, rd, 1);
      }
      if (cond == kCondGT) {
        // Simulate rs1 > rs2 via !(rs1 <= rs2) since there's
        // only the slti instruction but no sgti.
        __ Xori(rd, rd, 1);
      }
      break;

    case kCondB:
    case kCondAE:
      if (use_imm) {
        if (IsInt<11>(abs(imm)) || imm == -2048) {
          // Sltiu sign-extends its 16-bit immediate operand before
          // the comparison and thus lets us compare directly with
          // unsigned values in the ranges [0, 0x7fff] and
          // [0x[ffffffff]ffff8000, 0x[ffffffff]ffffffff].
          __ Sltiu(rd, rs1, imm & 0xfff);
        } else {
          __ Li(rd, imm);
          __ Sltu(rd, rs1, rs2);
        }
      } else {
        __ Sltu(rd, rs1, rs2);
      }
      if (cond == kCondAE) {
        // Simulate rs1 >= rs2 via !(rs1 < rs2) since there's
        // only the sltu instruction but no sgeu.
        __ Xori(rd, rd, 1);
      }
      break;

    case kCondBE:
    case kCondA:
      // Use 11-bit here for avoiding sign-extension.
      if (use_imm) {
        imm += 1;
        if (IsInt<11>(abs(imm)) || imm == -2048) {
          // Simulate rs1 <= rs2 via rs1 < rs2 + 1.
          // Note that this only works if rs2 + 1 does not overflow
          // to 0, hence the check above.
          // Sltiu sign-extends its 12-bit immediate operand before
          // the comparison and thus lets us compare directly with
          // unsigned values in the ranges [0, 0x7fff] and
          // [0x[ffffffff]fffff800, 0x[ffffffff]ffffffff].
          __ Sltiu(rd, rs1, imm & 0xfff);
        } else {
          __ Li(rd, imm - 1);
          __ Sltu(rd, rd, rs1);
        }
        if (cond == kCondA) {
          // Simulate rs1 > rs2 via !(rs1 <= rs2) since there's
          // only the sltiu instruction but no sgtiu.
          __ Xori(rd, rd, 1);
        }
      } else {
        __ Sltu(rd, rs2, rs1);
        if (cond == kCondBE) {
          // Simulate rs1 <= rs2 via !(rs2 < rs1) since there's
          // only the sltu instruction but no sleu.
          __ Xori(rd, rd, 1);
        }
      }
      break;
  }
}

// When the function returns `false` it means that the condition holds if `rd` is non-Zero
// and doesn't hold if `rd` is Zero. If it returns `true`, the roles of Zero and non-Zero
// `rd` are exchanged.
bool InstructionCodeGeneratorRISCV64::MaterializeIntLongCompare(IfCondition cond,
                                                                bool is_64bit,
                                                                LocationSummary* locations,
                                                                XRegister dest) {
  UNUSED(cond);
  UNUSED(is_64bit);
  UNUSED(locations);
  UNUSED(dest);
  LOG(FATAL) << "UniMplemented";
  UNREACHABLE();
}

void InstructionCodeGeneratorRISCV64::GenerateIntLongCompareAndBranch(IfCondition cond,
                                                                      bool is_64bit,
                                                                      LocationSummary* locations,
                                                                      Riscv64Label* label) {
  UNUSED(cond);
  UNUSED(is_64bit);
  UNUSED(locations);
  UNUSED(label);
  LOG(FATAL) << "UniMplemented";
}

void InstructionCodeGeneratorRISCV64::CheckNanAndGotoLabel(XRegister tmp,
                                                           FRegister fr,
                                                           Riscv64Label* label,
                                                           bool is_double) {
  // if rs1 or rs2 is NaN, set rd to 1.
  // fclass.s/d examines the value in floating-point register rs1 and writes to integer
  // register rd a 10-bit mask that indicates the class of the floating-point number.
  // rd[8]: Singaling NaN
  // rd[9]: Quiet NaN

  if (!is_double) {
    __ FClassS(tmp, fr);
  } else {
    __ FClassD(tmp, fr);
  }
  __ Srli(tmp, tmp, 8);
  __ Bnez(tmp, label);  // goto label.
}

void InstructionCodeGeneratorRISCV64::GenerateFpCompare(IfCondition cond,
                                                        bool gt_bias,
                                                        DataType::Type type,
                                                        LocationSummary* locations) {
  XRegister rd = locations->Out().AsRegister<XRegister>();
  FRegister rs1 = locations->InAt(0).AsFpuRegister<FRegister>();
  FRegister rs2 = locations->InAt(1).AsFpuRegister<FRegister>();
  if (type == DataType::Type::kFloat32) {
    switch (cond) {
      case kCondEQ:
        __ FEqS(rd, rs1, rs2);
        break;
      case kCondNE:
        __ FEqS(rd, rs1, rs2);
        __ Xori(rd, rd, 1);
        break;
      case kCondLT:
        if (gt_bias) {
          Riscv64Label label;
          // Do compare.
          CheckNanAndGotoLabel(rd, rs1, &label, false);
          CheckNanAndGotoLabel(rd, rs2, &label, false);
          __ FLtS(rd, rs1, rs2);
          __ Jal(Zero, 8);  // Skip "li rd, 1"

          __ Bind(&label);
          __ Li(rd, 1);
          break;
        } else {
          __ FLtS(rd, rs1, rs2);
        }
        break;
      case kCondLE:
        if (gt_bias) {
          Riscv64Label label;
          CheckNanAndGotoLabel(rd, rs1, &label, false);
          CheckNanAndGotoLabel(rd, rs2, &label, false);
          __ FLeS(rd, rs1, rs2);
          __ Jal(Zero, 8);  // Skip "li rd, 1"

          __ Bind(&label);
          __ Li(rd, 1);
        } else {
          __ FLeS(rd, rs1, rs2);
        }
        break;
      case kCondGT:
        if (gt_bias) {
          Riscv64Label label;
          CheckNanAndGotoLabel(rd, rs1, &label, false);
          CheckNanAndGotoLabel(rd, rs2, &label, false);
          __ FLtS(rd, rs2, rs1);
          __ Jal(Zero, 8);  // Skip "li rd, 1"

          __ Bind(&label);
          __ Li(rd, 1);
        } else {
          __ FLtS(rd, rs2, rs1);
        }
        break;
      case kCondGE:
        if (gt_bias) {
          Riscv64Label label;
          CheckNanAndGotoLabel(rd, rs1, &label, false);
          CheckNanAndGotoLabel(rd, rs2, &label, false);
          __ FLeS(rd, rs2, rs1);
          __ Jal(Zero, 8);  // Skip "li rd, 1"

          __ Bind(&label);
          __ Li(rd, 1);
        } else {
          __ FLeS(rd, rs2, rs1);
        }
        break;
      default:
        LOG(FATAL) << "Unexpected non-floating-point condition " << cond;
        UNREACHABLE();
    }
  } else {
    DCHECK_EQ(type, DataType::Type::kFloat64);
    switch (cond) {
      case kCondEQ:
        __ FEqD(rd, rs1, rs2);
        break;
      case kCondNE:
        __ FEqD(rd, rs1, rs2);
        __ Xori(rd, rd, 1);
        break;
      case kCondLT:
        if (gt_bias) {
          Riscv64Label label;
          // Do compare.
          CheckNanAndGotoLabel(rd, rs1, &label, false);
          CheckNanAndGotoLabel(rd, rs2, &label, false);
          __ FLtD(rd, rs1, rs2);
          __ Jal(Zero, 8);  // Skip "li rd, 1"

          __ Bind(&label);
          __ Li(rd, 1);
        } else {
          __ FLtD(rd, rs1, rs2);
        }
        break;
      case kCondLE:
        if (gt_bias) {
          Riscv64Label label;
          CheckNanAndGotoLabel(rd, rs1, &label, false);
          CheckNanAndGotoLabel(rd, rs2, &label, false);
          __ FLeD(rd, rs1, rs2);
          __ Jal(Zero, 8);  // Skip "li rd, 1"

          __ Bind(&label);
          __ Li(rd, 1);
        } else {
          __ FLeD(rd, rs1, rs2);
        }
        break;
      case kCondGT:
        if (gt_bias) {
          Riscv64Label label;
          CheckNanAndGotoLabel(rd, rs1, &label, false);
          CheckNanAndGotoLabel(rd, rs2, &label, false);
          __ FLtD(rd, rs2, rs1);
          __ Jal(Zero, 8);  // Skip "li rd, 1"

          __ Bind(&label);
          __ Li(rd, 1);
        } else {
          __ FLtD(rd, rs2, rs1);
        }
        break;
      case kCondGE:
        if (gt_bias) {
          Riscv64Label label;
          CheckNanAndGotoLabel(rd, rs1, &label, false);
          CheckNanAndGotoLabel(rd, rs2, &label, false);
          __ FLeS(rd, rs2, rs1);
          __ Jal(Zero, 8);  // Skip "li rd, 1"

          __ Bind(&label);
          __ Li(rd, 1);
        } else {
          __ FLeD(rd, rs2, rs1);
        }
        break;
      default:
        LOG(FATAL) << "Unexpected non-floating-point condition " << cond;
        UNREACHABLE();
    }
  }
}

// When the function returns `false` it means that the condition holds if `rd` is non-Zero
// and doesn't hold if `rd` is Zero. If it returns `true`, the roles of Zero and non-Zero
// `rd` are exchanged.
bool InstructionCodeGeneratorRISCV64::MaterializeFpCompare(IfCondition cond,
                                                           bool gt_bias,
                                                           DataType::Type type,
                                                           LocationSummary* locations,
                                                           XRegister dest) {
  UNUSED(cond);
  UNUSED(gt_bias);
  UNUSED(type);
  UNUSED(locations);
  UNUSED(dest);
  LOG(FATAL) << "Unimplemented";
  UNREACHABLE();
}

void InstructionCodeGeneratorRISCV64::GenerateFpCompareAndBranch(IfCondition cond,
                                                                 bool gt_bias,
                                                                 DataType::Type type,
                                                                 LocationSummary* locations,
                                                                 Riscv64Label* label) {
  UNUSED(cond);
  UNUSED(gt_bias);
  UNUSED(type);
  UNUSED(locations);
  UNUSED(label);
  LOG(FATAL) << "Unimplemented";
}

void InstructionCodeGeneratorRISCV64::HandleGoto(HInstruction* instruction,
                                                 HBasicBlock* successor) {
  UNUSED(instruction);
  UNUSED(successor);
  LOG(FATAL) << "Unimplemented";
}

void InstructionCodeGeneratorRISCV64::GenPackedSwitchWithCompares(XRegister reg,
                                                                  int32_t lower_bound,
                                                                  uint32_t num_entries,
                                                                  HBasicBlock* switch_block,
                                                                  HBasicBlock* default_block) {
  UNUSED(reg);
  UNUSED(lower_bound);
  UNUSED(num_entries);
  UNUSED(switch_block);
  UNUSED(default_block);
  LOG(FATAL) << "Unimplemented";
}

void InstructionCodeGeneratorRISCV64::GenTableBasedPackedSwitch(XRegister reg,
                                                                int32_t lower_bound,
                                                                uint32_t num_entries,
                                                                HBasicBlock* switch_block,
                                                                HBasicBlock* default_block) {
  UNUSED(reg);
  UNUSED(lower_bound);
  UNUSED(num_entries);
  UNUSED(switch_block);
  UNUSED(default_block);
  LOG(FATAL) << "Unimplemented";
}

int32_t InstructionCodeGeneratorRISCV64::VecAddress(LocationSummary* locations,
                                                    size_t size,
                                                    /* out */ XRegister* adjusted_base) {
  UNUSED(locations);
  UNUSED(size);
  UNUSED(adjusted_base);
  LOG(FATAL) << "Unimplemented";
  UNREACHABLE();
}

void InstructionCodeGeneratorRISCV64::GenConditionalMove(HSelect* select) {
  UNUSED(select);
  LOG(FATAL) << "Unimplemented";
}

void LocationsBuilderRISCV64::HandleBinaryOp(HBinaryOperation* instruction) {
  DCHECK_EQ(instruction->InputCount(), 2U);
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(instruction);
  DataType::Type type = instruction->GetResultType();
  switch (type) {
    case DataType::Type::kInt32:
    case DataType::Type::kInt64: {
      locations->SetInAt(0, Location::RequiresRegister());
      HInstruction* right = instruction->InputAt(1);
      bool can_use_imm = false;
      if (right->IsConstant()) {
        int64_t imm = CodeGenerator::GetInt64ValueOf(right->AsConstant());
        if (instruction->IsAnd() || instruction->IsOr() || instruction->IsXor()) {
          can_use_imm = IsUint<11>(imm);
        } else {
          DCHECK(instruction->IsAdd() || instruction->IsSub());
          if (instruction->IsSub()) {
            if (!(type == DataType::Type::kInt32 && imm == INT32_MIN)) {
              imm = -imm;
            }
          }
          can_use_imm = IsInt<11>(imm);
        }
      }
      if (can_use_imm)
        locations->SetInAt(1, Location::ConstantLocation(right->AsConstant()));
      else
        locations->SetInAt(1, Location::RequiresRegister());
      locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
    } break;

    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64:
      locations->SetInAt(0, Location::RequiresFpuRegister());
      locations->SetInAt(1, Location::RequiresFpuRegister());
      locations->SetOut(Location::RequiresFpuRegister(), Location::kNoOutputOverlap);
      break;

    default:
      LOG(FATAL) << "Unexpected " << instruction->DebugName() << " type " << type;
  }
}

void InstructionCodeGeneratorRISCV64::HandleBinaryOp(HBinaryOperation* instruction) {
  DataType::Type type = instruction->GetType();
  LocationSummary* locations = instruction->GetLocations();

  switch (type) {
    case DataType::Type::kInt32:
    case DataType::Type::kInt64: {
      XRegister rd = locations->Out().AsRegister<XRegister>();
      XRegister rs1 = locations->InAt(0).AsRegister<XRegister>();
      Location rs2_location = locations->InAt(1);

      XRegister rs2 = Zero;
      int64_t imm = 0;
      bool use_imm = rs2_location.IsConstant();
      if (use_imm) {
        imm = CodeGenerator::GetInt64ValueOf(rs2_location.GetConstant());
      } else {
        rs2 = rs2_location.AsRegister<XRegister>();
      }

      if (instruction->IsAnd()) {
        if (use_imm) __
          Andi(rd, rs1, imm);
        else __
          And(rd, rs1, rs2);
      } else if (instruction->IsOr()) {
        if (use_imm) __
          Ori(rd, rs1, imm);
        else __
          Or(rd, rs1, rs2);
      } else if (instruction->IsXor()) {
        if (use_imm) __
          Xori(rd, rs1, imm);
        else __
          Xor(rd, rs1, rs2);
      } else if (instruction->IsAdd() || instruction->IsSub()) {
        if (instruction->IsSub()) {
          imm = -imm;
        }
        if (type == DataType::Type::kInt32) {
          if (use_imm) {
            if (IsInt<11>(imm)) {
              __ Addiw(rd, rs1, imm);
            } else {
              __ LoadConst32(TMP2, imm);
              __ Addw(rd, rs1, TMP2);
            }
          } else {
            if (instruction->IsAdd()) {
              __ Addw(rd, rs1, rs2);
            } else {
              DCHECK(instruction->IsSub());
              __ Subw(rd, rs1, rs2);
            }
          }
        } else {
          if (use_imm) {
            if (IsInt<11>(imm)) {
              __ Addi(rd, rs1, imm);
            } else if (IsInt<32>(imm)) {
              __ LoadConst32(TMP2, imm);
              __ Add(rd, rs1, TMP2);
            } else {
              __ LoadConst64(TMP2, imm);
              __ Add(rd, rs1, TMP2);
            }
          } else if (instruction->IsAdd()) {
            __ Add(rd, rs1, rs2);
          } else {
            DCHECK(instruction->IsSub());
            __ Sub(rd, rs1, rs2);
          }
        }
      }
      break;
    }
    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64: {
      FRegister rd = locations->Out().AsFpuRegister<FRegister>();
      FRegister rs1 = locations->InAt(0).AsFpuRegister<FRegister>();
      FRegister rs2 = locations->InAt(1).AsFpuRegister<FRegister>();
      if (instruction->IsAdd()) {
        if (type == DataType::Type::kFloat32) __
          FAddS(rd, rs1, rs2);
        else __
          FAddD(rd, rs1, rs2);
      } else if (instruction->IsSub()) {
        if (type == DataType::Type::kFloat32) __
          FSubS(rd, rs1, rs2);
        else __
          FSubD(rd, rs1, rs2);
      } else {
        LOG(FATAL) << "Unexpected floating-point binary operation";
      }
      break;
    }
    default:
      LOG(FATAL) << "Unexpected binary operation type " << type;
  }
}

void LocationsBuilderRISCV64::HandleCondition(HCondition* instruction) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(instruction);
  switch (instruction->InputAt(0)->GetType()) {
    default:
    case DataType::Type::kInt64:
      locations->SetInAt(0, Location::RequiresRegister());
      locations->SetInAt(1, Location::RegisterOrConstant(instruction->InputAt(1)));
      break;

    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64:
      locations->SetInAt(0, Location::RequiresFpuRegister());
      locations->SetInAt(1, Location::RequiresFpuRegister());
      break;
  }
  if (!instruction->IsEmittedAtUseSite()) {
    locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
  }
}

void InstructionCodeGeneratorRISCV64::HandleCondition(HCondition* instruction) {
  if (instruction->IsEmittedAtUseSite()) {
    return;
  }

  DataType::Type type = instruction->InputAt(0)->GetType();
  LocationSummary* locations = instruction->GetLocations();
  switch (type) {
    default:
      // Integer case.
      GenerateIntLongCompare(instruction->GetCondition(), /* is_64bit= */ false, locations);
      return;
    case DataType::Type::kInt64:
      GenerateIntLongCompare(instruction->GetCondition(), /* is_64bit= */ true, locations);
      return;
    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64:
      GenerateFpCompare(instruction->GetCondition(), instruction->IsGtBias(), type, locations);
      return;
  }
}

void LocationsBuilderRISCV64::HandleShift(HBinaryOperation* instruction) {
  DCHECK(instruction->IsShl() || instruction->IsShr() || instruction->IsUShr() ||
         instruction->IsRor());

  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(instruction);
  DataType::Type type = instruction->GetResultType();
  switch (type) {
    case DataType::Type::kInt32:
    case DataType::Type::kInt64: {
      locations->SetInAt(0, Location::RequiresRegister());
      locations->SetInAt(1, Location::RegisterOrConstant(instruction->InputAt(1)));
      locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
      break;
    }
    default:
      LOG(FATAL) << "Unexpected shift type " << type;
  }
}

void InstructionCodeGeneratorRISCV64::HandleShift(HBinaryOperation* instruction) {
  DCHECK(instruction->IsShl() || instruction->IsShr() || instruction->IsUShr() ||
         instruction->IsRor());
  LocationSummary* locations = instruction->GetLocations();
  DataType::Type type = instruction->GetType();

  switch (type) {
    case DataType::Type::kInt32:
    case DataType::Type::kInt64: {
      XRegister rd = locations->Out().AsRegister<XRegister>();
      XRegister rs1 = locations->InAt(0).AsRegister<XRegister>();
      Location rs2_location = locations->InAt(1);

      XRegister rs2 = Zero;
      int64_t imm = 0;
      bool use_imm = rs2_location.IsConstant();
      if (use_imm) {
        imm = CodeGenerator::GetInt64ValueOf(rs2_location.GetConstant());
      } else {
        rs2 = rs2_location.AsRegister<XRegister>();
      }

      if (use_imm) {
        uint32_t shamt =
            imm & (type == DataType::Type::kInt32 ? kMaxIntShiftDistance : kMaxLongShiftDistance);

        if (shamt == 0) {
          if (rd != rs1) {
            __ Mv(rd, rs1);
          }
        } else if (type == DataType::Type::kInt32) {
          if (instruction->IsShl()) {
            __ Slliw(rd, rs1, shamt);
          } else if (instruction->IsShr()) {
            __ Sraiw(rd, rs1, shamt);
          } else if (instruction->IsUShr()) {
            __ Srliw(rd, rs1, shamt);
          } else {
            // Riscv64 codegen don't use the blocked registers for rd, rt, rs till now.
            __ Srliw(TMP, rs1, shamt);
            __ Slliw(rd, rs1, 32 - shamt);  // logical shift left (32 -shamt)
            __ Or(rd, rd, TMP);
          }
        } else {
          if (instruction->IsShl()) {
            __ Slli(rd, rs1, shamt);
          } else if (instruction->IsShr()) {
            __ Srai(rd, rs1, shamt);
          } else if (instruction->IsUShr()) {
            __ Srli(rd, rs1, shamt);
          } else {
            // Riscv64 codegen don't use the blocked registers for rd, rt, rs till now.
            // It's safe to use scratch registers here.
            __ Srli(TMP, rs1, shamt);
            __ Slli(rd, rs1, (64 - shamt));
            __ Or(rd, rd, TMP);
          }
        }
      } else {
        if (type == DataType::Type::kInt32) {
          if (instruction->IsShl()) {
            __ Sllw(rd, rs1, rs2);
          } else if (instruction->IsShr()) {
            __ Sraw(rd, rs1, rs2);
          } else if (instruction->IsUShr()) {
            __ Srlw(rd, rs1, rs2);
          } else {
            // Riscv64 codegen don't use the blocked registers for rd, rt, rs till now.
            __ Srl(TMP, rs1, rs2);
            __ Sub(rd, Zero, rs2);  // rd = -rs
            __ Addi(rd, rd, 64);    // rd = 64 -rs
            __ Sll(rd, rs1, rd);
            __ Or(rd, rd, TMP);
          }
        } else {
          if (instruction->IsShl()) {
            __ Sll(rd, rs1, rs2);
          } else if (instruction->IsShr()) {
            __ Sra(rd, rs1, rs2);
          } else if (instruction->IsUShr()) {
            __ Srl(rd, rs1, rs2);
          } else {
            // Riscv64 codegen don't use the blocked registers for rd, rt, rs till now.
            __ Srl(TMP, rs1, rs2);
            __ Sub(rd, Zero, rs2);  // rd = -rs
            __ Addi(rd, rd, 64);    // rd = 64 -rs
            __ Sll(rd, rs1, rd);
            __ Or(rd, rd, TMP);
          }
        }
      }
      break;
    }
    default:
      LOG(FATAL) << "Unexpected shift operation type " << type;
  }
}

void LocationsBuilderRISCV64::HandleFieldSet(HInstruction* instruction,
                                             const FieldInfo& field_info) {
  UNUSED(instruction);
  UNUSED(field_info);
  LOG(FATAL) << "Unimplemented";
}

void InstructionCodeGeneratorRISCV64::HandleFieldSet(HInstruction* instruction,
                                                     const FieldInfo& field_info,
                                                     bool value_can_be_null) {
  UNUSED(instruction);
  UNUSED(field_info);
  UNUSED(value_can_be_null);
  LOG(FATAL) << "Unimplemented";
}

void LocationsBuilderRISCV64::HandleFieldGet(HInstruction* instruction,
                                             const FieldInfo& field_info) {
  UNUSED(instruction);
  UNUSED(field_info);
  LOG(FATAL) << "Unimplemented";
}

void InstructionCodeGeneratorRISCV64::HandleFieldGet(HInstruction* instruction,
                                                     const FieldInfo& field_info) {
  UNUSED(instruction);
  UNUSED(field_info);
  LOG(FATAL) << "Unimplemented";
}

void LocationsBuilderRISCV64::VisitAbove(HAbove* instruction) { HandleCondition(instruction); }
void InstructionCodeGeneratorRISCV64::VisitAbove(HAbove* instruction) {
  HandleCondition(instruction);
}

void LocationsBuilderRISCV64::VisitAboveOrEqual(HAboveOrEqual* instruction) {
  HandleCondition(instruction);
}
void InstructionCodeGeneratorRISCV64::VisitAboveOrEqual(HAboveOrEqual* instruction) {
  HandleCondition(instruction);
}

void LocationsBuilderRISCV64::VisitAbs(HAbs* abs) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(abs);
  switch (abs->GetResultType()) {
    case DataType::Type::kInt32:
    case DataType::Type::kInt64:
      locations->SetInAt(0, Location::RequiresRegister());
      locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
      break;
    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64:
      locations->SetInAt(0, Location::RequiresFpuRegister());
      locations->SetOut(Location::RequiresFpuRegister(), Location::kNoOutputOverlap);
      break;
    default:
      LOG(FATAL) << "Unexpected abs type " << abs->GetResultType();
  }
}

void InstructionCodeGeneratorRISCV64::VisitAbs(HAbs* abs) {
  LocationSummary* locations = abs->GetLocations();
  switch (abs->GetResultType()) {
    case DataType::Type::kInt32: {
      XRegister in = locations->InAt(0).AsRegister<XRegister>();
      XRegister out = locations->Out().AsRegister<XRegister>();
      __ Sraiw(TMP, in, 31);
      __ Xor(out, in, TMP);
      __ Subw(out, out, TMP);
      break;
    }
    case DataType::Type::kInt64: {
      XRegister in = locations->InAt(0).AsRegister<XRegister>();
      XRegister out = locations->Out().AsRegister<XRegister>();
      __ Srai(TMP, in, 63);
      __ Xor(out, in, TMP);
      __ Sub(out, out, TMP);
      break;
    }
    case DataType::Type::kFloat32: {
      FRegister in = locations->InAt(0).AsFpuRegister<FRegister>();
      FRegister out = locations->Out().AsFpuRegister<FRegister>();
      __ FAbsS(out, in);
      break;
    }
    case DataType::Type::kFloat64: {
      FRegister in = locations->InAt(0).AsFpuRegister<FRegister>();
      FRegister out = locations->Out().AsFpuRegister<FRegister>();
      __ FAbsD(out, in);
      break;
    }
    default:
      LOG(FATAL) << "Unexpected abs type " << abs->GetResultType();
  }
}

void LocationsBuilderRISCV64::VisitAdd(HAdd* instruction) { HandleBinaryOp(instruction); }
void InstructionCodeGeneratorRISCV64::VisitAdd(HAdd* instruction) { HandleBinaryOp(instruction); }

void LocationsBuilderRISCV64::VisitAnd(HAnd* instruction) { HandleBinaryOp(instruction); }
void InstructionCodeGeneratorRISCV64::VisitAnd(HAnd* instruction) { HandleBinaryOp(instruction); }

void LocationsBuilderRISCV64::VisitArrayGet(HArrayGet* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitArrayGet(HArrayGet* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitArrayLength(HArrayLength* instruction) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(instruction);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
}
void InstructionCodeGeneratorRISCV64::VisitArrayLength(HArrayLength* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  uint32_t offset = CodeGenerator::GetArrayLengthOffset(instruction);
  XRegister obj = locations->InAt(0).AsRegister<XRegister>();
  XRegister out = locations->Out().AsRegister<XRegister>();
  __ Loadw(out, obj, offset);
  codegen_->MaybeRecordImplicitNullCheck(instruction);
  // Mask out compression flag from String's array length.
  if (mirror::kUseStringCompression && instruction->IsStringLength()) {
    __ Srliw(out, out, 1u);
  }
}

void LocationsBuilderRISCV64::VisitArraySet(HArraySet* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitArraySet(HArraySet* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}

void LocationsBuilderRISCV64::VisitBelow(HBelow* instruction) { HandleCondition(instruction); }
void InstructionCodeGeneratorRISCV64::VisitBelow(HBelow* instruction) {
  HandleCondition(instruction);
}

void LocationsBuilderRISCV64::VisitBelowOrEqual(HBelowOrEqual* instruction) {
  HandleCondition(instruction);
}
void InstructionCodeGeneratorRISCV64::VisitBelowOrEqual(HBelowOrEqual* instruction) {
  HandleCondition(instruction);
}

void LocationsBuilderRISCV64::VisitBooleanNot(HBooleanNot* instruction) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(instruction);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
}
void InstructionCodeGeneratorRISCV64::VisitBooleanNot(HBooleanNot* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  __ Xori(locations->Out().AsRegister<XRegister>(), locations->InAt(0).AsRegister<XRegister>(), 1);
}

void LocationsBuilderRISCV64::VisitBoundsCheck(HBoundsCheck* instruction) {
  RegisterSet caller_saves = RegisterSet::Empty();
  InvokeRuntimeCallingConvention calling_convention;
  caller_saves.Add(Location::RegisterLocation(calling_convention.GetRegisterAt(0)));
  caller_saves.Add(Location::RegisterLocation(calling_convention.GetRegisterAt(1)));
  LocationSummary* locations = codegen_->CreateThrowingSlowPathLocations(instruction, caller_saves);

  HInstruction* index = instruction->InputAt(0);
  HInstruction* length = instruction->InputAt(1);

  bool const_index = false;
  bool const_length = false;

  if (index->IsConstant()) {
    if (length->IsConstant()) {
      const_index = true;
      const_length = true;
    } else {
      int32_t index_value = index->AsIntConstant()->GetValue();
      if (index_value < 0 || IsInt<11>(index_value + 1)) {
        const_index = true;
      }
    }
  } else if (length->IsConstant()) {
    int32_t length_value = length->AsIntConstant()->GetValue();
    if (IsUint<11>(length_value)) {
      const_length = true;
    }
  }

  locations->SetInAt(
      0,
      const_index ? Location::ConstantLocation(index->AsConstant()) : Location::RequiresRegister());
  locations->SetInAt(1,
                     const_length ? Location::ConstantLocation(length->AsConstant()) :
                                    Location::RequiresRegister());
}

void InstructionCodeGeneratorRISCV64::VisitBoundsCheck(HBoundsCheck* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  Location index_loc = locations->InAt(0);
  Location length_loc = locations->InAt(1);

  if (length_loc.IsConstant()) {
    int32_t length = length_loc.GetConstant()->AsIntConstant()->GetValue();
    if (index_loc.IsConstant()) {
      int32_t index = index_loc.GetConstant()->AsIntConstant()->GetValue();
      if (index < 0 || index >= length) {
        BoundsCheckSlowPathRISCV64* slow_path =
            new (codegen_->GetScopedAllocator()) BoundsCheckSlowPathRISCV64(instruction);
        codegen_->AddSlowPath(slow_path);
        __ J(slow_path->GetEntryLabel());
      } else {
        // Nothing to be done.
      }
      return;
    }

    BoundsCheckSlowPathRISCV64* slow_path =
        new (codegen_->GetScopedAllocator()) BoundsCheckSlowPathRISCV64(instruction);
    codegen_->AddSlowPath(slow_path);
    XRegister index = index_loc.AsRegister<XRegister>();
    if (length == 0) {
      __ J(slow_path->GetEntryLabel());
    } else if (length == 1) {
      __ Bnez(index, slow_path->GetEntryLabel());
    } else {
      DCHECK(IsUint<11>(length)) << length;
      __ Sltiu(TMP, index, length);
      __ Beqz(TMP, slow_path->GetEntryLabel());
    }
  } else {
    XRegister length = length_loc.AsRegister<XRegister>();
    BoundsCheckSlowPathRISCV64* slow_path =
        new (codegen_->GetScopedAllocator()) BoundsCheckSlowPathRISCV64(instruction);
    codegen_->AddSlowPath(slow_path);
    if (index_loc.IsConstant()) {
      int32_t index = index_loc.GetConstant()->AsIntConstant()->GetValue();
      if (index < 0) {
        __ J(slow_path->GetEntryLabel());
      } else if (index == 0) {
        __ Blez(length, slow_path->GetEntryLabel());
      } else {
        DCHECK(IsInt<11>(index + 1)) << index;
        __ Sltiu(TMP, length, index + 1);
        __ Bnez(TMP, slow_path->GetEntryLabel());
      }
    } else {
      XRegister index = index_loc.AsRegister<XRegister>();
      __ Bgeu(index, length, slow_path->GetEntryLabel());
    }
  }
}

void LocationsBuilderRISCV64::VisitBoundType([[maybe_unused]] HBoundType* instruction) {
  // Nothing to do, this should be removed during prepare for register allocator.
  LOG(FATAL) << "Unreachable";
}
void InstructionCodeGeneratorRISCV64::VisitBoundType([[maybe_unused]] HBoundType* instruction) {
  // Nothing to do, this should be removed during prepare for register allocator.
  LOG(FATAL) << "Unreachable";
}

void LocationsBuilderRISCV64::VisitCheckCast(HCheckCast* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitCheckCast(HCheckCast* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}

void LocationsBuilderRISCV64::VisitClassTableGet(HClassTableGet* instruction) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(instruction, LocationSummary::kNoCall);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister());
}
void InstructionCodeGeneratorRISCV64::VisitClassTableGet(HClassTableGet* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  if (instruction->GetTableKind() == HClassTableGet::TableKind::kVTable) {
    uint32_t method_offset =
        mirror::Class::EmbeddedVTableEntryOffset(instruction->GetIndex(), kRiscv64PointerSize)
            .SizeValue();
    __ Loadd(locations->Out().AsRegister<XRegister>(),
             locations->InAt(0).AsRegister<XRegister>(),
             method_offset);
  } else {
    uint32_t method_offset = static_cast<uint32_t>(
        ImTable::OffsetOfElement(instruction->GetIndex(), kRiscv64PointerSize));
    __ Loadd(locations->Out().AsRegister<XRegister>(),
             locations->InAt(0).AsRegister<XRegister>(),
             mirror::Class::ImtPtrOffset(kRiscv64PointerSize).Uint32Value());
    __ Loadd(locations->Out().AsRegister<XRegister>(),
             locations->Out().AsRegister<XRegister>(),
             method_offset);
  }
}

static int32_t GetExceptionTlsOffset() {
  return Thread::ExceptionOffset<kRiscv64PointerSize>().Int32Value();
}
void LocationsBuilderRISCV64::VisitClearException(HClearException* instruction) {
  new (GetGraph()->GetAllocator()) LocationSummary(instruction, LocationSummary::kNoCall);
}
void InstructionCodeGeneratorRISCV64::VisitClearException(
    [[maybe_unused]] HClearException* instruction) {
  __ Storew(Zero, TR, GetExceptionTlsOffset());
}

void LocationsBuilderRISCV64::VisitClinitCheck(HClinitCheck* instruction) {
  LocationSummary* locations = new (GetGraph()->GetAllocator())
      LocationSummary(instruction, LocationSummary::kCallOnSlowPath);
  locations->SetInAt(0, Location::RequiresRegister());
  if (instruction->HasUses()) {
    locations->SetOut(Location::SameAsFirstInput());
  }
  // Rely on the type initialization to save everything we need.
  locations->SetCustomSlowPathCallerSaves(OneRegInReferenceOutSaveEverythingCallerSaves());
}
void InstructionCodeGeneratorRISCV64::VisitClinitCheck(HClinitCheck* instruction) {
  // We assume the class is not null.
  SlowPathCodeRISCV64* slow_path = new (codegen_->GetScopedAllocator())
      LoadClassSlowPathRISCV64(instruction->GetLoadClass(), instruction);
  codegen_->AddSlowPath(slow_path);
  GenerateClassInitializationCheck(slow_path,
                                   instruction->GetLocations()->InAt(0).AsRegister<XRegister>());
}

void LocationsBuilderRISCV64::VisitCompare(HCompare* instruction) {
  DataType::Type in_type = instruction->InputAt(0)->GetType();

  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(instruction);

  switch (in_type) {
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
    case DataType::Type::kInt32:
    case DataType::Type::kInt64:
      locations->SetInAt(0, Location::RequiresRegister());
      locations->SetInAt(1, Location::RegisterOrConstant(instruction->InputAt(1)));
      locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
      break;

    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64:
      locations->SetInAt(0, Location::RequiresFpuRegister());
      locations->SetInAt(1, Location::RequiresFpuRegister());
      locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
      break;

    default:
      LOG(FATAL) << "Unexpected type for compare operation " << in_type;
  }
}
void InstructionCodeGeneratorRISCV64::VisitCompare(HCompare* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  XRegister result = locations->Out().AsRegister<XRegister>();
  DataType::Type in_type = instruction->InputAt(0)->GetType();

  //  0 if: left == right
  //  1 if: left  > right
  // -1 if: left  < right
  switch (in_type) {
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
    case DataType::Type::kInt32:
    case DataType::Type::kInt64: {
      XRegister left = locations->InAt(0).AsRegister<XRegister>();
      Location right_location = locations->InAt(1);
      bool use_imm = right_location.IsConstant();
      XRegister right = Zero;
      if (use_imm) {
        if (in_type == DataType::Type::kInt64) {
          int64_t value =
              CodeGenerator::GetInt64ValueOf(right_location.GetConstant()->AsConstant());
          if (value != 0) {
            // use result as the right register.
            right = result;
            __ LoadConst64(right, value);
          }
        } else {
          int32_t value =
              CodeGenerator::GetInt32ValueOf(right_location.GetConstant()->AsConstant());
          if (value != 0) {
            // use result as the right register.
            right = result;
            __ LoadConst32(right, value);
          }
        }
      } else {
        right = right_location.AsRegister<XRegister>();
      }
      __ Slt(TMP, left, right);
      __ Slt(result, right, left);
      __ Sub(result, result, TMP);
      break;
    }

    case DataType::Type::kFloat32: {
      FRegister left = locations->InAt(0).AsFpuRegister<FRegister>();
      FRegister right = locations->InAt(1).AsFpuRegister<FRegister>();
      Riscv64Label done;
      __ FEqS(TMP, left, right);
      __ LoadConst32(result, 0);
      __ Bnez(TMP, &done);
      if (instruction->IsGtBias()) {
        __ FLtS(TMP, left, right);
        __ LoadConst32(result, -1);
        __ Bnez(TMP, &done);
        __ LoadConst32(result, 1);
      } else {
        __ FLtS(TMP, right, left);
        __ LoadConst32(result, 1);
        __ Bnez(TMP, &done);
        __ LoadConst32(result, -1);
      }
      __ Bind(&done);
      break;
    }

    case DataType::Type::kFloat64: {
      FRegister left = locations->InAt(0).AsFpuRegister<FRegister>();
      FRegister right = locations->InAt(1).AsFpuRegister<FRegister>();
      Riscv64Label done;
      __ FEqD(TMP, left, right);
      __ LoadConst32(result, 0);
      __ Bnez(TMP, &done);
      if (instruction->IsGtBias()) {
        __ FLtD(TMP, left, right);
        __ LoadConst32(result, -1);
        __ Bnez(TMP, &done);
        __ LoadConst32(result, 1);
      } else {
        __ FLtD(TMP, right, left);
        __ LoadConst32(result, 1);
        __ Bnez(TMP, &done);
        __ LoadConst32(result, -1);
      }
      __ Bind(&done);
      break;
    }

    default:
      LOG(FATAL) << "Unimplemented compare type " << in_type;
  }
}

void LocationsBuilderRISCV64::VisitConstructorFence(HConstructorFence* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitConstructorFence(HConstructorFence* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitCurrentMethod(HCurrentMethod* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitCurrentMethod(HCurrentMethod* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitShouldDeoptimizeFlag(HShouldDeoptimizeFlag* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitShouldDeoptimizeFlag(
    HShouldDeoptimizeFlag* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitDeoptimize(HDeoptimize* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitDeoptimize(HDeoptimize* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitDiv(HDiv* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitDiv(HDiv* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitDivZeroCheck(HDivZeroCheck* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitDivZeroCheck(HDivZeroCheck* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitDoubleConstant(HDoubleConstant* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitDoubleConstant(HDoubleConstant* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}

void LocationsBuilderRISCV64::VisitEqual(HEqual* instruction) { HandleCondition(instruction); }
void InstructionCodeGeneratorRISCV64::VisitEqual(HEqual* instruction) {
  HandleCondition(instruction);
}

void LocationsBuilderRISCV64::VisitExit(HExit* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitExit(HExit* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitFloatConstant(HFloatConstant* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitFloatConstant(HFloatConstant* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitGoto(HGoto* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitGoto(HGoto* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}

void LocationsBuilderRISCV64::VisitGreaterThan(HGreaterThan* instruction) {
  HandleCondition(instruction);
}
void InstructionCodeGeneratorRISCV64::VisitGreaterThan(HGreaterThan* instruction) {
  HandleCondition(instruction);
}

void LocationsBuilderRISCV64::VisitGreaterThanOrEqual(HGreaterThanOrEqual* instruction) {
  HandleCondition(instruction);
}
void InstructionCodeGeneratorRISCV64::VisitGreaterThanOrEqual(HGreaterThanOrEqual* instruction) {
  HandleCondition(instruction);
}

void LocationsBuilderRISCV64::VisitIf(HIf* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitIf(HIf* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitInstanceFieldGet(HInstanceFieldGet* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitInstanceFieldGet(HInstanceFieldGet* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitInstanceFieldSet(HInstanceFieldSet* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitInstanceFieldSet(HInstanceFieldSet* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitPredicatedInstanceFieldGet(
    HPredicatedInstanceFieldGet* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitPredicatedInstanceFieldGet(
    HPredicatedInstanceFieldGet* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitInstanceOf(HInstanceOf* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitInstanceOf(HInstanceOf* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitIntConstant(HIntConstant* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitIntConstant(HIntConstant* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitIntermediateAddress(HIntermediateAddress* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitIntermediateAddress(HIntermediateAddress* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitInvokeUnresolved(HInvokeUnresolved* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitInvokeUnresolved(HInvokeUnresolved* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitInvokeInterface(HInvokeInterface* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitInvokeInterface(HInvokeInterface* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitInvokeStaticOrDirect(HInvokeStaticOrDirect* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitInvokeStaticOrDirect(
    HInvokeStaticOrDirect* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitInvokeVirtual(HInvokeVirtual* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitInvokeVirtual(HInvokeVirtual* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitInvokePolymorphic(HInvokePolymorphic* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitInvokePolymorphic(HInvokePolymorphic* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitInvokeCustom(HInvokeCustom* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitInvokeCustom(HInvokeCustom* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}

void LocationsBuilderRISCV64::VisitLessThan(HLessThan* instruction) {
  HandleCondition(instruction);
}
void InstructionCodeGeneratorRISCV64::VisitLessThan(HLessThan* instruction) {
  HandleCondition(instruction);
}

void LocationsBuilderRISCV64::VisitLessThanOrEqual(HLessThanOrEqual* instruction) {
  HandleCondition(instruction);
}
void InstructionCodeGeneratorRISCV64::VisitLessThanOrEqual(HLessThanOrEqual* instruction) {
  HandleCondition(instruction);
}

void LocationsBuilderRISCV64::VisitLoadClass(HLoadClass* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitLoadClass(HLoadClass* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitLoadException(HLoadException* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitLoadException(HLoadException* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitLoadMethodHandle(HLoadMethodHandle* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitLoadMethodHandle(HLoadMethodHandle* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitLoadMethodType(HLoadMethodType* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitLoadMethodType(HLoadMethodType* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitLoadString(HLoadString* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitLoadString(HLoadString* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitLongConstant(HLongConstant* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitLongConstant(HLongConstant* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitMax(HMax* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitMax(HMax* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitMemoryBarrier(HMemoryBarrier* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitMemoryBarrier(HMemoryBarrier* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitMethodEntryHook(HMethodEntryHook* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitMethodEntryHook(HMethodEntryHook* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitMethodExitHook(HMethodExitHook* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitMethodExitHook(HMethodExitHook* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitMin(HMin* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitMin(HMin* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitMonitorOperation(HMonitorOperation* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitMonitorOperation(HMonitorOperation* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitMul(HMul* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitMul(HMul* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitNeg(HNeg* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitNeg(HNeg* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitNewArray(HNewArray* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitNewArray(HNewArray* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitNewInstance(HNewInstance* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitNewInstance(HNewInstance* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitNop(HNop* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitNop(HNop* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitNot(HNot* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitNot(HNot* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}

void LocationsBuilderRISCV64::VisitNotEqual(HNotEqual* instruction) {
  HandleCondition(instruction);
}
void InstructionCodeGeneratorRISCV64::VisitNotEqual(HNotEqual* instruction) {
  HandleCondition(instruction);
}

void LocationsBuilderRISCV64::VisitNullConstant(HNullConstant* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitNullConstant(HNullConstant* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitNullCheck(HNullCheck* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitNullCheck(HNullCheck* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}

void LocationsBuilderRISCV64::VisitOr(HOr* instruction) { HandleBinaryOp(instruction); }
void InstructionCodeGeneratorRISCV64::VisitOr(HOr* instruction) { HandleBinaryOp(instruction); }

void LocationsBuilderRISCV64::VisitPackedSwitch(HPackedSwitch* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitPackedSwitch(HPackedSwitch* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitParallelMove(HParallelMove* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitParallelMove(HParallelMove* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitParameterValue(HParameterValue* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitParameterValue(HParameterValue* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitPhi(HPhi* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitPhi(HPhi* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitRem(HRem* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitRem(HRem* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitReturn(HReturn* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitReturn(HReturn* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitReturnVoid(HReturnVoid* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitReturnVoid(HReturnVoid* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}

void LocationsBuilderRISCV64::VisitRor(HRor* instruction) { HandleShift(instruction); }
void InstructionCodeGeneratorRISCV64::VisitRor(HRor* instruction) { HandleShift(instruction); }

void LocationsBuilderRISCV64::VisitShl(HShl* instruction) { HandleShift(instruction); }
void InstructionCodeGeneratorRISCV64::VisitShl(HShl* instruction) { HandleShift(instruction); }

void LocationsBuilderRISCV64::VisitShr(HShr* instruction) { HandleShift(instruction); }
void InstructionCodeGeneratorRISCV64::VisitShr(HShr* instruction) { HandleShift(instruction); }

void LocationsBuilderRISCV64::VisitStaticFieldGet(HStaticFieldGet* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitStaticFieldGet(HStaticFieldGet* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitStaticFieldSet(HStaticFieldSet* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitStaticFieldSet(HStaticFieldSet* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitStringBuilderAppend(HStringBuilderAppend* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitStringBuilderAppend(HStringBuilderAppend* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitUnresolvedInstanceFieldGet(
    HUnresolvedInstanceFieldGet* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitUnresolvedInstanceFieldGet(
    HUnresolvedInstanceFieldGet* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitUnresolvedInstanceFieldSet(
    HUnresolvedInstanceFieldSet* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitUnresolvedInstanceFieldSet(
    HUnresolvedInstanceFieldSet* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitUnresolvedStaticFieldGet(
    HUnresolvedStaticFieldGet* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitUnresolvedStaticFieldGet(
    HUnresolvedStaticFieldGet* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitUnresolvedStaticFieldSet(
    HUnresolvedStaticFieldSet* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitUnresolvedStaticFieldSet(
    HUnresolvedStaticFieldSet* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitSelect(HSelect* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitSelect(HSelect* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}

void LocationsBuilderRISCV64::VisitSub(HSub* instruction) { HandleBinaryOp(instruction); }
void InstructionCodeGeneratorRISCV64::VisitSub(HSub* instruction) { HandleBinaryOp(instruction); }

void LocationsBuilderRISCV64::VisitSuspendCheck(HSuspendCheck* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitSuspendCheck(HSuspendCheck* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitThrow(HThrow* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitThrow(HThrow* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitTryBoundary(HTryBoundary* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitTryBoundary(HTryBoundary* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitTypeConversion(HTypeConversion* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitTypeConversion(HTypeConversion* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}

void LocationsBuilderRISCV64::VisitUShr(HUShr* instruction) { HandleShift(instruction); }
void InstructionCodeGeneratorRISCV64::VisitUShr(HUShr* instruction) { HandleShift(instruction); }

void LocationsBuilderRISCV64::VisitXor(HXor* instruction) { HandleBinaryOp(instruction); }
void InstructionCodeGeneratorRISCV64::VisitXor(HXor* instruction) { HandleBinaryOp(instruction); }

void LocationsBuilderRISCV64::VisitVecReplicateScalar(HVecReplicateScalar* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitVecReplicateScalar(HVecReplicateScalar* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitVecExtractScalar(HVecExtractScalar* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitVecExtractScalar(HVecExtractScalar* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitVecReduce(HVecReduce* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitVecReduce(HVecReduce* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitVecCnv(HVecCnv* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitVecCnv(HVecCnv* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitVecNeg(HVecNeg* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitVecNeg(HVecNeg* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitVecAbs(HVecAbs* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitVecAbs(HVecAbs* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitVecNot(HVecNot* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitVecNot(HVecNot* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitVecAdd(HVecAdd* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitVecAdd(HVecAdd* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitVecHalvingAdd(HVecHalvingAdd* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitVecHalvingAdd(HVecHalvingAdd* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitVecSub(HVecSub* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitVecSub(HVecSub* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitVecMul(HVecMul* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitVecMul(HVecMul* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitVecDiv(HVecDiv* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitVecDiv(HVecDiv* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitVecMin(HVecMin* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitVecMin(HVecMin* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitVecMax(HVecMax* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitVecMax(HVecMax* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitVecAnd(HVecAnd* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitVecAnd(HVecAnd* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitVecAndNot(HVecAndNot* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitVecAndNot(HVecAndNot* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitVecOr(HVecOr* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitVecOr(HVecOr* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitVecXor(HVecXor* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitVecXor(HVecXor* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitVecSaturationAdd(HVecSaturationAdd* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitVecSaturationAdd(HVecSaturationAdd* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitVecSaturationSub(HVecSaturationSub* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitVecSaturationSub(HVecSaturationSub* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitVecShl(HVecShl* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitVecShl(HVecShl* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitVecShr(HVecShr* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitVecShr(HVecShr* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitVecUShr(HVecUShr* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitVecUShr(HVecUShr* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitVecSetScalars(HVecSetScalars* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitVecSetScalars(HVecSetScalars* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitVecMultiplyAccumulate(HVecMultiplyAccumulate* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitVecMultiplyAccumulate(
    HVecMultiplyAccumulate* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitVecSADAccumulate(HVecSADAccumulate* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitVecSADAccumulate(HVecSADAccumulate* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitVecDotProd(HVecDotProd* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitVecDotProd(HVecDotProd* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitVecLoad(HVecLoad* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitVecLoad(HVecLoad* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitVecStore(HVecStore* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitVecStore(HVecStore* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitVecPredSetAll(HVecPredSetAll* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitVecPredSetAll(HVecPredSetAll* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitVecPredWhile(HVecPredWhile* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitVecPredWhile(HVecPredWhile* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitVecPredCondition(HVecPredCondition* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitVecPredCondition(HVecPredCondition* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}

#undef __

namespace detail {
// Mark which intrinsics we don't have handcrafted code for.
template <Intrinsics T>
struct IsUnimplemented {
  bool is_unimplemented = false;
};

#define TRUE_OVERRIDE(Name, ...)                \
  template <>                                   \
  struct IsUnimplemented<Intrinsics::k##Name> { \
    bool is_unimplemented = true;               \
  };
#include "intrinsics_list.h"
UNIMPLEMENTED_INTRINSIC_LIST_RISCV64(TRUE_OVERRIDE)
#undef TRUE_OVERRIDE

#include "intrinsics_list.h"
// clang-format off
static constexpr bool kIsIntrinsicUnimplemented[] = {
  false,  // kNone
#define IS_UNIMPLEMENTED(Intrinsic, ...) \
  IsUnimplemented<Intrinsics::k##Intrinsic>().is_unimplemented,
  INTRINSICS_LIST(IS_UNIMPLEMENTED)
// clang-format on
#undef IS_UNIMPLEMENTED
};
#undef INTRINSICS_LIST

}  // namespace detail

#define __ down_cast<Riscv64Assembler*>(GetAssembler())->  // NOLINT

CodeGeneratorRISCV64::CodeGeneratorRISCV64(HGraph* graph,
                                           const CompilerOptions& compiler_options,
                                           OptimizingCompilerStats* stats)
    : CodeGenerator(graph,
                    kNumberOfXRegisters,
                    kNumberOfFRegisters,
                    /* number_of_register_pairs */ 0,
                    ComputeRegisterMask(reinterpret_cast<const int*>(kCoreCalleeSaves),
                                        arraysize(kCoreCalleeSaves)),
                    ComputeRegisterMask(reinterpret_cast<const int*>(kFpuCalleeSaves),
                                        arraysize(kFpuCalleeSaves)),
                    compiler_options,
                    stats,
                    ArrayRef<const bool>(detail::kIsIntrinsicUnimplemented)),
      assembler_(graph->GetAllocator(),
                 compiler_options.GetInstructionSetFeatures()->AsRiscv64InstructionSetFeatures()),
      location_builder_(graph, this),
      block_labels_(nullptr),
      uint32_literals_(std::less<uint32_t>(),
                       graph->GetAllocator()->Adapter(kArenaAllocCodeGenerator)),
      uint64_literals_(std::less<uint64_t>(),
                       graph->GetAllocator()->Adapter(kArenaAllocCodeGenerator)),
      boot_image_method_patches_(graph->GetAllocator()->Adapter(kArenaAllocCodeGenerator)),
      method_bss_entry_patches_(graph->GetAllocator()->Adapter(kArenaAllocCodeGenerator)),
      boot_image_type_patches_(graph->GetAllocator()->Adapter(kArenaAllocCodeGenerator)),
      type_bss_entry_patches_(graph->GetAllocator()->Adapter(kArenaAllocCodeGenerator)),
      public_type_bss_entry_patches_(graph->GetAllocator()->Adapter(kArenaAllocCodeGenerator)),
      package_type_bss_entry_patches_(graph->GetAllocator()->Adapter(kArenaAllocCodeGenerator)),
      boot_image_string_patches_(graph->GetAllocator()->Adapter(kArenaAllocCodeGenerator)),
      string_bss_entry_patches_(graph->GetAllocator()->Adapter(kArenaAllocCodeGenerator)),
      boot_image_jni_entrypoint_patches_(graph->GetAllocator()->Adapter(kArenaAllocCodeGenerator)),
      boot_image_other_patches_(graph->GetAllocator()->Adapter(kArenaAllocCodeGenerator)) {}

void CodeGeneratorRISCV64::MaybeIncrementHotness(bool is_frame_entry) {
  if (GetCompilerOptions().CountHotnessInCompiledCode()) {
    XRegister method = is_frame_entry ? kArtMethodRegister : TMP;
    XRegister counter = T5;
    if (!is_frame_entry) {
      __ Loadw(method, SP, 0);
    }
    __ Loadhu(counter, method, ArtMethod::HotnessCountOffset().Int32Value());
    Riscv64Label done;
    DCHECK_EQ(0u, interpreter::kNterpHotnessValue);
    __ Beqz(counter, &done);
    __ Addi(counter, counter, -1);
    __ Storeh(counter, method, ArtMethod::HotnessCountOffset().Int32Value());
    __ Bind(&done);
  }

  if (GetGraph()->IsCompilingBaseline() && !Runtime::Current()->IsAotCompiler()) {
    SlowPathCodeRISCV64* slow_path = new (GetScopedAllocator()) CompileOptimizedSlowPathRISCV64();
    AddSlowPath(slow_path);
    ProfilingInfo* info = GetGraph()->GetProfilingInfo();
    DCHECK(info != nullptr);
    DCHECK(!HasEmptyFrame());
    uint64_t address = reinterpret_cast64<uint64_t>(info);
    Riscv64Label done;
    XRegister counter = T5;
    __ LoadConst64(TMP2, address);
    __ Loadd(TMP2, TMP2, 0);
    __ Loadhu(counter, TMP2, ProfilingInfo::BaselineHotnessCountOffset().Int32Value());
    __ Beqz(counter, slow_path->GetEntryLabel());
    __ Addi(counter, counter, -1);
    __ Storeh(counter, TMP2, ProfilingInfo::BaselineHotnessCountOffset().Int32Value());
    __ Bind(slow_path->GetExitLabel());
  }
}

void CodeGeneratorRISCV64::GenerateMemoryBarrier(MemBarrierKind kind) {
  switch (kind) {
    case MemBarrierKind::kAnyAny:
    case MemBarrierKind::kAnyStore:
    case MemBarrierKind::kLoadAny:
    case MemBarrierKind::kStoreStore: {
      __ Fence();
      break;
    }

    default:
      __ Fence();
      LOG(FATAL) << "Unexpected memory barrier " << kind;
  }
}

void CodeGeneratorRISCV64::GenerateFrameEntry() {
  // Check if we need to generate the clinit check. We will jump to the
  // resolution stub if the class is not initialized and the executing thread is
  // not the thread initializing it.
  // We do this before constructing the frame to get the correct stack trace if
  // an exception is thrown.
  if (GetCompilerOptions().ShouldCompileWithClinitCheck(GetGraph()->GetArtMethod())) {
    Riscv64Label resolution;
    Riscv64Label memory_barrier;
    // Check if we're visibly initialized.

    // We don't emit a read barrier here to save on code size. We rely on the
    // resolution trampoline to do a suspend check before re-entering this code.
    __ Loadd(TMP2, kArtMethodRegister, ArtMethod::DeclaringClassOffset().Int32Value());
    __ Loadb(TMP, TMP2, status_byte_offset);

    __ Li(TMP2, shifted_visibly_initialized_value);
    __ Bgeu(TMP, TMP2, &frame_entry_label_);

    // Check if we're initialized and jump to code that does a memory barrier if
    // so.
    __ Li(TMP2, shifted_initialized_value);
    __ Bgeu(TMP, TMP2, &memory_barrier);

    // Check if we're initializing and the thread initializing is the one
    // executing the code.
    __ Li(TMP2, shifted_initializing_value);
    __ Bltu(TMP, TMP2, &resolution);

    __ Loadd(TMP2, kArtMethodRegister, ArtMethod::DeclaringClassOffset().Int32Value());
    __ Loadw(TMP, TMP2, mirror::Class::ClinitThreadIdOffset().Int32Value());
    __ Loadw(T5, TR, Thread::TidOffset<kArm64PointerSize>().Int32Value());
    __ Beq(TMP, T5, &frame_entry_label_);
    __ Bind(&resolution);

    // Jump to the resolution stub.
    ThreadOffset64 entrypoint_offset =
        GetThreadOffset<kArm64PointerSize>(kQuickQuickResolutionTrampoline);
    __ Loadw(TMP, TR, entrypoint_offset.Int32Value());
    __ Jalr(TMP);

    __ Bind(&memory_barrier);
    GenerateMemoryBarrier(MemBarrierKind::kAnyAny);
  }
  __ Bind(&frame_entry_label_);

  bool do_overflow_check =
      FrameNeedsStackCheck(GetFrameSize(), InstructionSet::kRiscv64) || !IsLeafMethod();

  if (do_overflow_check) {
    __ Loadw(
        Zero, SP, -static_cast<int32_t>(GetStackOverflowReservedBytes(InstructionSet::kRiscv64)));
    RecordPcInfo(nullptr, 0);
  }

  if (!HasEmptyFrame()) {
    // Make sure the frame size isn't unreasonably large.
    if (GetFrameSize() > GetStackOverflowReservedBytes(InstructionSet::kRiscv64)) {
      LOG(FATAL) << "Stack frame larger than "
                 << GetStackOverflowReservedBytes(InstructionSet::kRiscv64) << " bytes";
    }

    // Spill callee-saved registers.

    uint32_t frame_size = GetFrameSize();

    IncreaseFrame(frame_size);

    for (int i = arraysize(kCoreCalleeSaves) - 1; i >= 0; --i) {
      XRegister reg = kCoreCalleeSaves[i];
      if (allocated_registers_.ContainsCoreRegister(reg)) {
        frame_size -= kRiscv64DoublewordSize;
        __ Stored(reg, SP, frame_size);
        __ cfi().RelOffset(dwarf::Reg::Riscv64Core(reg), frame_size);
      }
    }

    for (int i = arraysize(kFpuCalleeSaves) - 1; i >= 0; --i) {
      FRegister reg = kFpuCalleeSaves[i];
      if (allocated_registers_.ContainsFloatingPointRegister(reg)) {
        frame_size -= kRiscv64DoublewordSize;
        __ FStored(reg, SP, frame_size);
        __ cfi().RelOffset(dwarf::Reg::Riscv64Fp(reg), frame_size);
      }
    }

    // Save the current method if we need it. Note that we do not
    // do this in HCurrentMethod, as the instruction might have been removed
    // in the SSA graph.
    if (RequiresCurrentMethod()) {
      __ Stored(kArtMethodRegister, SP, 0);
    }

    if (GetGraph()->HasShouldDeoptimizeFlag()) {
      // Initialize should_deoptimize flag to 0.
      __ Storew(Zero, SP, GetStackOffsetOfShouldDeoptimizeFlag());
    }
  }
  MaybeIncrementHotness(/* is_frame_entry= */ true);
}
void CodeGeneratorRISCV64::GenerateFrameExit() {
  __ cfi().RememberState();

  if (!HasEmptyFrame()) {
    // Restore callee-saved registers.

    // For better instruction scheduling restore RA before other registers.
    uint32_t ofs = GetFrameSize();
    for (int i = arraysize(kCoreCalleeSaves) - 1; i >= 0; --i) {
      XRegister reg = kCoreCalleeSaves[i];
      if (allocated_registers_.ContainsCoreRegister(reg)) {
        ofs -= kRiscv64DoublewordSize;
        __ Loadd(reg, SP, ofs);
        __ cfi().Restore(dwarf::Reg::Riscv64Core(reg));
      }
    }

    for (int i = arraysize(kFpuCalleeSaves) - 1; i >= 0; --i) {
      FRegister reg = kFpuCalleeSaves[i];
      if (allocated_registers_.ContainsFloatingPointRegister(reg)) {
        ofs -= kRiscv64DoublewordSize;
        __ FLoadd(reg, SP, ofs);
        __ cfi().Restore(dwarf::Reg::Riscv64Fp(reg));
      }
    }

    DecreaseFrame(GetFrameSize());
  }

  __ Jr(RA);

  __ cfi().RestoreState();
  __ cfi().DefCFAOffset(GetFrameSize());
}

void CodeGeneratorRISCV64::Bind(HBasicBlock* block) { __ Bind(GetLabelOf(block)); }

void CodeGeneratorRISCV64::MoveConstant(Location location, int32_t value) {
  DCHECK(location.IsRegister());
  __ LoadConst32(location.AsRegister<XRegister>(), value);
}

void CodeGeneratorRISCV64::MoveLocation(Location destination,
                                        Location source,
                                        DataType::Type dst_type) {
  if (source.Equals(destination)) {
    return;
  }

  // A valid move can always be inferred from the destination and source
  // locations. When moving from and to a register, the argument type can be
  // used to generate 32bit instead of 64bit moves.
  bool unspecified_type = (dst_type == DataType::Type::kVoid);
  DCHECK_EQ(unspecified_type, false);

  if (destination.IsRegister() || destination.IsFpuRegister()) {
    if (unspecified_type) {
      HConstant* src_cst = source.IsConstant() ? source.GetConstant() : nullptr;
      if (source.IsStackSlot() ||
          (src_cst != nullptr &&
           (src_cst->IsIntConstant() || src_cst->IsFloatConstant() || src_cst->IsNullConstant()))) {
        // For stack slots and 32bit constants, a 64bit type is appropriate.
        dst_type = destination.IsRegister() ? DataType::Type::kInt32 : DataType::Type::kFloat32;
      } else {
        // If the source is a double stack slot or a 64bit constant, a 64bit
        // type is appropriate. Else the source is a register, and since the
        // type has not been specified, we chose a 64bit type to force a 64bit
        // move.
        dst_type = destination.IsRegister() ? DataType::Type::kInt64 : DataType::Type::kFloat64;
      }
    }
    DCHECK((destination.IsFpuRegister() && DataType::IsFloatingPointType(dst_type)) ||
           (destination.IsRegister() && !DataType::IsFloatingPointType(dst_type)));

    if (source.IsStackSlot() || source.IsDoubleStackSlot()) {
      // Move to GPR/FPR from stack
      if (DataType::IsFloatingPointType(dst_type)) {
        if (DataType::Is64BitType(dst_type)) __
          FLoadd(destination.AsFpuRegister<FRegister>(), SP, source.GetStackIndex());
        else __
          FLoadw(destination.AsFpuRegister<FRegister>(), SP, source.GetStackIndex());
      } else {
        if (DataType::Is64BitType(dst_type)) __
          Loadd(destination.AsRegister<XRegister>(), SP, source.GetStackIndex());
        else __
          Loadwu(destination.AsRegister<XRegister>(), SP, source.GetStackIndex());
      }
    } else if (source.IsConstant()) {
      // Move to GPR/FPR from constant
      XRegister gpr = TMP;
      if (!DataType::IsFloatingPointType(dst_type)) {
        gpr = destination.AsRegister<XRegister>();
      }
      if (dst_type == DataType::Type::kInt32 || dst_type == DataType::Type::kFloat32) {
        int32_t value = GetInt32ValueOf(source.GetConstant()->AsConstant());
        if (DataType::IsFloatingPointType(dst_type) && value == 0) {
          gpr = Zero;
        } else {
          __ LoadConst32(gpr, value);
        }
      } else {
        int64_t value = GetInt64ValueOf(source.GetConstant()->AsConstant());
        if (DataType::IsFloatingPointType(dst_type) && value == 0) {
          gpr = Zero;
        } else {
          __ LoadConst64(gpr, value);
        }
      }
      if (dst_type == DataType::Type::kFloat32) {
        __ FMvWX(destination.AsFpuRegister<FRegister>(), gpr);
      } else if (dst_type == DataType::Type::kFloat64) {
        __ FMvDX(destination.AsFpuRegister<FRegister>(), gpr);
      }
    } else if (source.IsRegister()) {
      if (destination.IsRegister()) {
        // Move to GPR from GPR
        __ Mv(destination.AsRegister<XRegister>(), source.AsRegister<XRegister>());
      } else {
        DCHECK(destination.IsFpuRegister());
        if (DataType::Is64BitType(dst_type)) {
          __ FMvDX(destination.AsFpuRegister<FRegister>(), source.AsRegister<XRegister>());
        } else {
          __ FMvWX(destination.AsFpuRegister<FRegister>(), source.AsRegister<XRegister>());
        }
      }
    } else if (source.IsFpuRegister()) {
      if (destination.IsFpuRegister()) {
        if (GetGraph()->HasSIMD()) {
          LOG(FATAL) << "SIMD is unsupported";
        } else {
          // Move to FPR from FPR
          if (dst_type == DataType::Type::kFloat32) {
            __ FMvS(destination.AsFpuRegister<FRegister>(), source.AsFpuRegister<FRegister>());
          } else {
            DCHECK_EQ(dst_type, DataType::Type::kFloat64);
            __ FMvD(destination.AsFpuRegister<FRegister>(), source.AsFpuRegister<FRegister>());
          }
        }
      } else {
        DCHECK(destination.IsRegister());
        if (DataType::Is64BitType(dst_type)) {
          __ FMvXD(destination.AsRegister<XRegister>(), source.AsFpuRegister<FRegister>());
        } else {
          __ FMvXW(destination.AsRegister<XRegister>(), source.AsFpuRegister<FRegister>());
        }
      }
    }
  } else if (destination.IsSIMDStackSlot()) {
    if (source.IsFpuRegister()) {
      __ FStored(source.AsFpuRegister<FRegister>(), SP, destination.GetStackIndex());
    } else {
      DCHECK(source.IsSIMDStackSlot());
      LOG(FATAL) << "SIMD is unsupported";
    }
  } else {  // The destination is not a register. It must be a stack slot.
    DCHECK(destination.IsStackSlot() || destination.IsDoubleStackSlot());
    if (source.IsRegister() || source.IsFpuRegister()) {
      if (unspecified_type) {
        if (source.IsRegister()) {
          dst_type = destination.IsStackSlot() ? DataType::Type::kInt32 : DataType::Type::kInt64;
        } else {
          dst_type =
              destination.IsStackSlot() ? DataType::Type::kFloat32 : DataType::Type::kFloat64;
        }
      }
      DCHECK((destination.IsDoubleStackSlot() == DataType::Is64BitType(dst_type)) &&
             (source.IsFpuRegister() == DataType::IsFloatingPointType(dst_type)));
      // Move to stack from GPR/FPR
      if (DataType::Is64BitType(dst_type)) {
        if (source.IsRegister()) {
          __ Stored(source.AsRegister<XRegister>(), SP, destination.GetStackIndex());
        } else {
          __ FStored(source.AsFpuRegister<FRegister>(), SP, destination.GetStackIndex());
        }
      } else {
        if (source.IsRegister()) {
          __ Storew(source.AsRegister<XRegister>(), SP, destination.GetStackIndex());
        } else {
          __ FStorew(source.AsFpuRegister<FRegister>(), SP, destination.GetStackIndex());
        }
      }
    } else if (source.IsConstant()) {
      // Move to stack from constant
      HConstant* src_cst = source.GetConstant();
      XRegister gpr = Zero;
      if (destination.IsStackSlot()) {
        int32_t value = GetInt32ValueOf(src_cst->AsConstant());
        if (value != 0) {
          gpr = TMP;
          __ LoadConst32(gpr, value);
        }
        __ Storew(gpr, SP, destination.GetStackIndex());
      } else {
        DCHECK(destination.IsDoubleStackSlot());
        int64_t value = GetInt64ValueOf(src_cst->AsConstant());
        if (value != 0) {
          gpr = TMP2;
          __ LoadConst64(gpr, value);
        }
        __ Stored(gpr, SP, destination.GetStackIndex());
      }
    } else {
      DCHECK(source.IsStackSlot() || source.IsDoubleStackSlot());
      DCHECK_EQ(source.IsDoubleStackSlot(), destination.IsDoubleStackSlot());
      // Move to stack from stack
      if (destination.IsStackSlot()) {
        __ Loadw(TMP, SP, source.GetStackIndex());
        __ Storew(TMP, SP, destination.GetStackIndex());
      } else {
        __ Loadd(TMP, SP, source.GetStackIndex());
        __ Stored(TMP, SP, destination.GetStackIndex());
      }
    }
  }
}

void CodeGeneratorRISCV64::AddLocationAsTemp(Location location, LocationSummary* locations) {
  if (location.IsRegister()) {
    locations->AddTemp(location);
  } else {
    UNIMPLEMENTED(FATAL) << "AddLocationAsTemp not implemented for location " << location;
  }
}

void CodeGeneratorRISCV64::SetupBlockedRegisters() const {
  // ZERO, GP, SP, RA TP are always reserved and can't be allocated.
  blocked_core_registers_[Zero] = true;
  blocked_core_registers_[GP] = true;
  blocked_core_registers_[SP] = true;
  blocked_core_registers_[RA] = true;
  blocked_core_registers_[TP] = true;

  // TMP(T6), TMP2(T5), FT11, FT10 are used as temporary/scratch
  // registers.
  blocked_core_registers_[TMP] = true;
  blocked_core_registers_[TMP2] = true;
  blocked_fpu_registers_[FT10] = true;
  blocked_fpu_registers_[FT11] = true;

  // Reserve suspend and self registers.
  blocked_core_registers_[S11] = true;
  blocked_core_registers_[S1] = true;

  if (GetGraph()->IsDebuggable()) {
    // Stubs do not save callee-save floating point registers. If the graph
    // is debuggable, we need to deal with these registers differently. For
    // now, just block them.
    for (size_t i = 0; i < arraysize(kFpuCalleeSaves); ++i) {
      blocked_fpu_registers_[kFpuCalleeSaves[i]] = true;
    }
  }
}

size_t CodeGeneratorRISCV64::SaveCoreRegister(size_t stack_index, uint32_t reg_id) {
  __ Stored(XRegister(reg_id), SP, stack_index);
  return kRiscv64DoublewordSize;
}

size_t CodeGeneratorRISCV64::RestoreCoreRegister(size_t stack_index, uint32_t reg_id) {
  __ Loadd(XRegister(reg_id), SP, stack_index);
  return kRiscv64DoublewordSize;
}

size_t CodeGeneratorRISCV64::SaveFloatingPointRegister(size_t stack_index, uint32_t reg_id) {
  if (GetGraph()->HasSIMD()) {
    // TODO: RISCV vector extension.
    UNIMPLEMENTED(FATAL) << "SIMD is unsupported";
  }

  __ FStored(FRegister(reg_id), SP, stack_index);
  return kRiscv64FloatRegSizeInBytes;
}

size_t CodeGeneratorRISCV64::RestoreFloatingPointRegister(size_t stack_index, uint32_t reg_id) {
  if (GetGraph()->HasSIMD()) {
    // TODO: RISCV vector extension.
    UNIMPLEMENTED(FATAL) << "SIMD is unsupported";
  }
  __ FLoadd(FRegister(reg_id), SP, stack_index);
  return kRiscv64FloatRegSizeInBytes;
}

void CodeGeneratorRISCV64::DumpCoreRegister(std::ostream& stream, int reg) const {
  stream << XRegister(reg);
}

void CodeGeneratorRISCV64::DumpFloatingPointRegister(std::ostream& stream, int reg) const {
  stream << FRegister(reg);
}

void CodeGeneratorRISCV64::Finalize() {
  // Ensure that we fix up branches.
  __ FinalizeCode();

  // Adjust native pc offsets in stack maps.
  StackMapStream* stack_map_stream = GetStackMapStream();
  for (size_t i = 0, num = stack_map_stream->GetNumberOfStackMaps(); i != num; ++i) {
    uint32_t old_position = stack_map_stream->GetStackMapNativePcOffset(i);
    uint32_t new_position = __ GetAdjustedPosition(old_position);
    DCHECK_GE(new_position, old_position);
    stack_map_stream->SetStackMapNativePcOffset(i, new_position);
  }

  // Adjust pc offsets for the disassembly information.
  if (disasm_info_ != nullptr) {
    GeneratedCodeInterval* frame_entry_interval = disasm_info_->GetFrameEntryInterval();
    frame_entry_interval->start = __ GetAdjustedPosition(frame_entry_interval->start);
    frame_entry_interval->end = __ GetAdjustedPosition(frame_entry_interval->end);
    for (auto& it : *disasm_info_->GetInstructionIntervals()) {
      it.second.start = __ GetAdjustedPosition(it.second.start);
      it.second.end = __ GetAdjustedPosition(it.second.end);
    }
    for (auto& it : *disasm_info_->GetSlowPathIntervals()) {
      it.code_interval.start = __ GetAdjustedPosition(it.code_interval.start);
      it.code_interval.end = __ GetAdjustedPosition(it.code_interval.end);
    }
  }

  CodeGenerator::Finalize();
}

// Generate code to invoke a runtime entry point.
void CodeGeneratorRISCV64::InvokeRuntime(QuickEntrypointEnum entrypoint,
                                         HInstruction* instruction,
                                         uint32_t dex_pc,
                                         SlowPathCode* slow_path) {
  ValidateInvokeRuntime(entrypoint, instruction, slow_path);

  ThreadOffset64 entrypoint_offset = GetThreadOffset<kRiscv64PointerSize>(entrypoint);

  __ Loadd(RA, TR, entrypoint_offset.Int32Value());
  __ Jalr(RA);
  if (EntrypointRequiresStackMap(entrypoint)) {
    RecordPcInfo(instruction, dex_pc, slow_path);
  }
  // TODO: Reduce code size for AOT by using shared trampolines for slow path runtime calls across
  // the entire oat file.
}

// Generate code to invoke a runtime entry point, but do not record
// PC-related information in a stack map.
void CodeGeneratorRISCV64::InvokeRuntimeWithoutRecordingPcInfo(int32_t entry_point_offset,
                                                               HInstruction* instruction,
                                                               SlowPathCode* slow_path) {
  ValidateInvokeRuntimeWithoutRecordingPcInfo(instruction, slow_path);
  __ Loadd(TMP2, TR, entry_point_offset);
  __ Jalr(TMP2);
}

void CodeGeneratorRISCV64::IncreaseFrame(size_t adjustment) {
  if (IsUint<12>(adjustment)) {
    __ Addi(SP, SP, -adjustment);
  } else {
    __ Li(TMP, -adjustment);
    __ Add(SP, SP, TMP);
  }
  GetAssembler()->cfi().AdjustCFAOffset(adjustment);
}

void CodeGeneratorRISCV64::DecreaseFrame(size_t adjustment) {
  if (IsUint<12>(adjustment)) {
    __ Addi(SP, SP, adjustment);
  } else {
    __ Li(TMP, adjustment);
    __ Add(SP, SP, TMP);
  }
}

void CodeGeneratorRISCV64::GenerateNop() { __ Nop(); }

void CodeGeneratorRISCV64::GenerateImplicitNullCheck(HNullCheck* instruction) {
  if (CanMoveNullCheckToUser(instruction)) {
    return;
  }
  Location obj = instruction->GetLocations()->InAt(0);

  __ Lw(Zero, obj.AsRegister<XRegister>(), 0);
  RecordPcInfo(instruction, instruction->GetDexPc());
}

void CodeGeneratorRISCV64::GenerateExplicitNullCheck(HNullCheck* instruction) {
  SlowPathCodeRISCV64* slow_path = new (GetScopedAllocator()) NullCheckSlowPathRISCV64(instruction);
  AddSlowPath(slow_path);

  Location obj = instruction->GetLocations()->InAt(0);

  __ Beqz(obj.AsRegister<XRegister>(), slow_path->GetEntryLabel());
}

// Check if the desired_string_load_kind is supported. If it is, return it,
// otherwise return a fall-back kind that should be used instead.
HLoadString::LoadKind CodeGeneratorRISCV64::GetSupportedLoadStringKind(
    HLoadString::LoadKind desired_string_load_kind) {
  bool fallback_load = false;
  switch (desired_string_load_kind) {
    case HLoadString::LoadKind::kBootImageLinkTimePcRelative:
    case HLoadString::LoadKind::kBootImageRelRo:
    case HLoadString::LoadKind::kBssEntry:
      DCHECK(!Runtime::Current()->UseJitCompilation());
      break;
    case HLoadString::LoadKind::kJitBootImageAddress:
    case HLoadString::LoadKind::kJitTableAddress:
      DCHECK(Runtime::Current()->UseJitCompilation());
      break;
    case HLoadString::LoadKind::kRuntimeCall:
      break;
  }
  if (fallback_load) {
    desired_string_load_kind = HLoadString::LoadKind::kRuntimeCall;
  }
  return desired_string_load_kind;
}

// Check if the desired_class_load_kind is supported. If it is, return it,
// otherwise return a fall-back kind that should be used instead.
HLoadClass::LoadKind CodeGeneratorRISCV64::GetSupportedLoadClassKind(
    HLoadClass::LoadKind desired_class_load_kind) {
  bool fallback_load = false;
  switch (desired_class_load_kind) {
    case HLoadClass::LoadKind::kInvalid:
      LOG(FATAL) << "UNREACHABLE";
      UNREACHABLE();
    case HLoadClass::LoadKind::kReferrersClass:
      break;
    case HLoadClass::LoadKind::kBootImageLinkTimePcRelative:
    case HLoadClass::LoadKind::kBootImageRelRo:
    case HLoadClass::LoadKind::kBssEntry:
    case HLoadClass::LoadKind::kBssEntryPublic:
    case HLoadClass::LoadKind::kBssEntryPackage:
      DCHECK(!Runtime::Current()->UseJitCompilation());
      break;
    case HLoadClass::LoadKind::kJitBootImageAddress:
    case HLoadClass::LoadKind::kJitTableAddress:
      DCHECK(Runtime::Current()->UseJitCompilation());
      break;
    case HLoadClass::LoadKind::kRuntimeCall:
      break;
  }
  if (fallback_load) {
    desired_class_load_kind = HLoadClass::LoadKind::kRuntimeCall;
  }
  return desired_class_load_kind;
}

// Check if the desired_dispatch_info is supported. If it is, return it,
// otherwise return a fall-back info that should be used instead.
HInvokeStaticOrDirect::DispatchInfo CodeGeneratorRISCV64::GetSupportedInvokeStaticOrDirectDispatch(
    const HInvokeStaticOrDirect::DispatchInfo& desired_dispatch_info, ArtMethod* method) {
  UNUSED(method);
  // On RISCV64 we support all dispatch types.
  return desired_dispatch_info;
}

CodeGeneratorRISCV64::PcRelativePatchInfo* CodeGeneratorRISCV64::NewBootImageIntrinsicPatch(
    uint32_t intrinsic_data, const PcRelativePatchInfo* info_high) {
  return NewPcRelativePatch(
      /* dex_file= */ nullptr, intrinsic_data, info_high, &boot_image_other_patches_);
}

CodeGeneratorRISCV64::PcRelativePatchInfo* CodeGeneratorRISCV64::NewBootImageRelRoPatch(
    uint32_t boot_image_offset, const PcRelativePatchInfo* info_high) {
  return NewPcRelativePatch(
      /* dex_file= */ nullptr, boot_image_offset, info_high, &boot_image_other_patches_);
}

CodeGeneratorRISCV64::PcRelativePatchInfo* CodeGeneratorRISCV64::NewBootImageMethodPatch(
    MethodReference target_method, const PcRelativePatchInfo* info_high) {
  return NewPcRelativePatch(
      target_method.dex_file, target_method.index, info_high, &boot_image_method_patches_);
}

CodeGeneratorRISCV64::PcRelativePatchInfo* CodeGeneratorRISCV64::NewMethodBssEntryPatch(
    MethodReference target_method, const PcRelativePatchInfo* info_high) {
  return NewPcRelativePatch(
      target_method.dex_file, target_method.index, info_high, &method_bss_entry_patches_);
}

CodeGeneratorRISCV64::PcRelativePatchInfo* CodeGeneratorRISCV64::NewBootImageTypePatch(
    const DexFile& dex_file, dex::TypeIndex type_index, const PcRelativePatchInfo* info_high) {
  return NewPcRelativePatch(&dex_file, type_index.index_, info_high, &boot_image_type_patches_);
}

CodeGeneratorRISCV64::PcRelativePatchInfo* CodeGeneratorRISCV64::NewBootImageJniEntrypointPatch(
    MethodReference target_method, const PcRelativePatchInfo* info_high) {
  return NewPcRelativePatch(
      target_method.dex_file, target_method.index, info_high, &boot_image_jni_entrypoint_patches_);
}

CodeGeneratorRISCV64::PcRelativePatchInfo* CodeGeneratorRISCV64::NewTypeBssEntryPatch(
    HLoadClass* load_class,
    const DexFile& dex_file,
    dex::TypeIndex type_index,
    const PcRelativePatchInfo* info_high) {
  ArenaDeque<PcRelativePatchInfo>* patches = nullptr;
  switch (load_class->GetLoadKind()) {
    case HLoadClass::LoadKind::kBssEntry:
      patches = &type_bss_entry_patches_;
      break;
    case HLoadClass::LoadKind::kBssEntryPublic:
      patches = &public_type_bss_entry_patches_;
      break;
    case HLoadClass::LoadKind::kBssEntryPackage:
      patches = &package_type_bss_entry_patches_;
      break;
    default:
      LOG(FATAL) << "Unexpected load kind: " << load_class->GetLoadKind();
      UNREACHABLE();
  }
  return NewPcRelativePatch(&dex_file, type_index.index_, info_high, patches);
}

CodeGeneratorRISCV64::PcRelativePatchInfo* CodeGeneratorRISCV64::NewBootImageStringPatch(
    const DexFile& dex_file, dex::StringIndex string_index, const PcRelativePatchInfo* info_high) {
  return NewPcRelativePatch(&dex_file, string_index.index_, info_high, &boot_image_string_patches_);
}

CodeGeneratorRISCV64::PcRelativePatchInfo* CodeGeneratorRISCV64::NewStringBssEntryPatch(
    const DexFile& dex_file, dex::StringIndex string_index, const PcRelativePatchInfo* info_high) {
  return NewPcRelativePatch(&dex_file, string_index.index_, info_high, &string_bss_entry_patches_);
}

CodeGeneratorRISCV64::PcRelativePatchInfo* CodeGeneratorRISCV64::NewPcRelativePatch(
    const DexFile* dex_file,
    uint32_t offset_or_index,
    const PcRelativePatchInfo* info_high,
    ArenaDeque<PcRelativePatchInfo>* patches) {
  patches->emplace_back(dex_file, offset_or_index, info_high);
  return &patches->back();
}

Literal* CodeGeneratorRISCV64::DeduplicateUint32Literal(uint32_t value, Uint32ToLiteralMap* map) {
  return map->GetOrCreate(value, [this, value]() { return __ NewLiteral<uint32_t>(value); });
}

Literal* CodeGeneratorRISCV64::DeduplicateUint64Literal(uint64_t value) {
  return uint64_literals_.GetOrCreate(value,
                                      [this, value]() { return __ NewLiteral<uint64_t>(value); });
}

Literal* CodeGeneratorRISCV64::DeduplicateBootImageAddressLiteral(uint64_t address) {
  return DeduplicateUint32Literal(dchecked_integral_cast<uint32_t>(address), &uint32_literals_);
}

void CodeGeneratorRISCV64::EmitPcRelativeAddressPlaceholderHigh(PcRelativePatchInfo* info_high,
                                                                XRegister out,
                                                                PcRelativePatchInfo* info_low) {
  DCHECK(!info_high->patch_info_high);
  __ Bind(&info_high->label);
  // Add the high 20-bit of a 32-bit offset to PC.
  __ Auipc(out, /* imm20= */ 0x12345);
  // A following instruction will add the sign-extended low half of the 32-bit
  // offset to `out` (e.g. ld, jialc, daddiu).
  if (info_low != nullptr) {
    DCHECK_EQ(info_low->patch_info_high, info_high);
    __ Bind(&info_low->label);
  }
}

void CodeGeneratorRISCV64::LoadMethod(MethodLoadKind load_kind, Location temp, HInvoke* invoke) {
  switch (load_kind) {
    case MethodLoadKind::kBootImageLinkTimePcRelative: {
      DCHECK(GetCompilerOptions().IsBootImage() || GetCompilerOptions().IsBootImageExtension());
      CodeGeneratorRISCV64::PcRelativePatchInfo* info_high =
          NewBootImageMethodPatch(invoke->GetResolvedMethodReference());
      CodeGeneratorRISCV64::PcRelativePatchInfo* info_low =
          NewBootImageMethodPatch(invoke->GetResolvedMethodReference(), info_high);
      EmitPcRelativeAddressPlaceholderHigh(info_high, temp.AsRegister<XRegister>(), info_low);
      __ Addi(temp.AsRegister<XRegister>(), temp.AsRegister<XRegister>(), /* imm12= */ 0x678);
      break;
    }
    case MethodLoadKind::kBootImageRelRo: {
      uint32_t boot_image_offset = GetBootImageOffset(invoke);
      PcRelativePatchInfo* info_high = NewBootImageRelRoPatch(boot_image_offset);
      PcRelativePatchInfo* info_low = NewBootImageRelRoPatch(boot_image_offset, info_high);
      EmitPcRelativeAddressPlaceholderHigh(info_high, temp.AsRegister<XRegister>(), info_low);
      // Note: Boot image is in the low 4GiB and the entry is 32-bit, so emit a 32-bit load.
      __ Lwu(temp.AsRegister<XRegister>(), temp.AsRegister<XRegister>(), /* imm12= */ 0x678);
      break;
    }
    case MethodLoadKind::kBssEntry: {
      PcRelativePatchInfo* info_high = NewMethodBssEntryPatch(invoke->GetMethodReference());
      PcRelativePatchInfo* info_low =
          NewMethodBssEntryPatch(invoke->GetMethodReference(), info_high);
      EmitPcRelativeAddressPlaceholderHigh(info_high, temp.AsRegister<XRegister>(), info_low);
      __ Ld(temp.AsRegister<XRegister>(), temp.AsRegister<XRegister>(), /* imm12= */ 0x678);
      break;
    }
    case MethodLoadKind::kJitDirectAddress: {
      __ Li(temp.AsFpuRegister<XRegister>(),
            reinterpret_cast<uint64_t>(invoke->GetResolvedMethod()));
      __ Ld(temp.AsRegister<XRegister>(), temp.AsFpuRegister<XRegister>(), 0);
      break;
    }
    case MethodLoadKind::kRuntimeCall: {
      // Test situation, don't do anything.
      break;
    }
    default: {
      LOG(FATAL) << "Load kind should have already been handled " << load_kind;
      UNREACHABLE();
    }
  }
}

void CodeGeneratorRISCV64::GenerateStaticOrDirectCall(HInvokeStaticOrDirect* invoke,
                                                      Location temp,
                                                      SlowPathCode* slow_path) {
  // All registers are assumed to be correctly set up per the calling convention.
  Location callee_method = temp;  // For all kinds except kRecursive, callee will be in temp.

  switch (invoke->GetMethodLoadKind()) {
    case MethodLoadKind::kStringInit: {
      // temp = thread->string_init_entrypoint
      uint32_t offset =
          GetThreadOffset<kRiscv64PointerSize>(invoke->GetStringInitEntryPoint()).Int32Value();
      __ Loadd(temp.AsRegister<XRegister>(), TR, offset);
      break;
    }
    case MethodLoadKind::kRecursive:
      callee_method = invoke->GetLocations()->InAt(invoke->GetCurrentMethodIndex());
      break;
    case MethodLoadKind::kRuntimeCall: {
      GenerateInvokeStaticOrDirectRuntimeCall(invoke, temp, slow_path);
      return;  // No code pointer retrieval; the runtime performs the call directly.
    }
    case MethodLoadKind::kBootImageLinkTimePcRelative:
      DCHECK(GetCompilerOptions().IsBootImage() || GetCompilerOptions().IsBootImageExtension());
      if (invoke->GetCodePtrLocation() == CodePtrLocation::kCallCriticalNative) {
        // Do not materialize the method pointer, load directly the entrypoint.
        CodeGeneratorRISCV64::PcRelativePatchInfo* info_high =
            NewBootImageJniEntrypointPatch(invoke->GetResolvedMethodReference());
        CodeGeneratorRISCV64::PcRelativePatchInfo* info_low =
            NewBootImageJniEntrypointPatch(invoke->GetResolvedMethodReference(), info_high);
        EmitPcRelativeAddressPlaceholderHigh(info_high, T6, info_low);
        __ Addi(TMP, TMP, /* imm12= */ 0x678);
        __ Ld(TMP, TMP, 0);
        break;
      }
      FALLTHROUGH_INTENDED;
    default: {
      LoadMethod(invoke->GetMethodLoadKind(), temp, invoke);
      break;
    }
  }

  switch (invoke->GetCodePtrLocation()) {
    case CodePtrLocation::kCallSelf:
      __ Jal(&frame_entry_label_);
      RecordPcInfo(invoke, invoke->GetDexPc(), slow_path);
      break;
    case CodePtrLocation::kCallArtMethod:
      // TMP2 = callee_method->entry_point_from_quick_compiled_code_;
      __ Loadd(TMP2,
               callee_method.AsRegister<XRegister>(),
               ArtMethod::EntryPointFromQuickCompiledCodeOffset(kRiscv64PointerSize).Int32Value());
      // TMP2()
      __ Jalr(TMP2);
      RecordPcInfo(invoke, invoke->GetDexPc(), slow_path);
      break;
    case CodePtrLocation::kCallCriticalNative: {
      size_t out_frame_size =
          PrepareCriticalNativeCall<CriticalNativeCallingConventionVisitorRiscv64,
                                    kNativeStackAlignment,
                                    GetCriticalNativeDirectCallFrameSize>(invoke);
      if (invoke->GetMethodLoadKind() == MethodLoadKind::kBootImageLinkTimePcRelative) {
        __ Jalr(TMP);
      } else {
        // TMP2 = callee_method->ptr_sized_fields_.data_;  // EntryPointFromJni
        MemberOffset offset = ArtMethod::EntryPointFromJniOffset(kRiscv64PointerSize);
        __ Loadd(TMP2, callee_method.AsRegister<XRegister>(), offset.Int32Value());
        __ Jalr(TMP2);
      }
      RecordPcInfo(invoke, invoke->GetDexPc(), slow_path);
      // Zero-/sign-extend the result when needed due to native and managed ABI mismatch.
      switch (invoke->GetType()) {
        case DataType::Type::kBool:
          __ Andi(A0, A0, 0xff);
          break;
        case DataType::Type::kInt8:
          __ Slli(A0, A0, 24);
          __ Srai(A0, A0, 24);
          break;
        case DataType::Type::kUint16:
          __ Slli(A0, A0, 16);
          __ Srli(A0, A0, 16);
          break;
        case DataType::Type::kInt16:
          __ Slli(A0, A0, 16);
          __ Srai(A0, A0, 16);
          break;
        case DataType::Type::kInt32:
        case DataType::Type::kInt64:
        case DataType::Type::kFloat32:
        case DataType::Type::kFloat64:
        case DataType::Type::kVoid:
          break;
        default:
          DCHECK(false) << invoke->GetType();
          break;
      }
      if (out_frame_size != 0u) {
        DecreaseFrame(out_frame_size);
      }
      break;
    }
  }

  DCHECK(!IsLeafMethod());
}

void CodeGeneratorRISCV64::MaybeGenerateInlineCacheCheck(HInstruction* instruction,
                                                         XRegister klass) {
  // We know the destination of an intrinsic, so no need to record inline caches.
  if (!instruction->GetLocations()->Intrinsified() && GetGraph()->IsCompilingBaseline() &&
      !Runtime::Current()->IsAotCompiler()) {
    DCHECK(!instruction->GetEnvironment()->IsFromInlinedInvoke());
    ProfilingInfo* info = GetGraph()->GetProfilingInfo();
    if (info != nullptr) {
      InlineCache* cache = info->GetInlineCache(instruction->GetDexPc());
      uint64_t address = reinterpret_cast64<uint64_t>(cache);
      Riscv64Label done;
      __ LoadConst64(T0, address);
      __ Loadd(T1, T0, InlineCache::ClassesOffset().Int32Value());
      // Fast path for a monomorphic cache.
      __ Beq(klass, T1, &done);
      InvokeRuntime(kQuickUpdateInlineCache, instruction, instruction->GetDexPc());

      __ Bind(&done);
    }
  }
}

void CodeGeneratorRISCV64::GenerateVirtualCall(HInvokeVirtual* invoke,
                                               Location temp_location,
                                               SlowPathCode* slow_path) {
  // Use the calling convention instead of the location of the receiver, as
  // intrinsics may have put the receiver in a different register. In the intrinsics
  // slow path, the arguments have been moved to the right place, so here we are
  // guaranteed that the receiver is the first register of the calling convention.
  InvokeDexCallingConvention calling_convention;
  XRegister receiver = calling_convention.GetRegisterAt(0);

  XRegister temp = temp_location.AsRegister<XRegister>();
  size_t method_offset =
      mirror::Class::EmbeddedVTableEntryOffset(invoke->GetVTableIndex(), kRiscv64PointerSize)
          .SizeValue();
  uint32_t class_offset = mirror::Object::ClassOffset().Int32Value();
  Offset entry_point = ArtMethod::EntryPointFromQuickCompiledCodeOffset(kRiscv64PointerSize);

  // temp = object->GetClass();
  __ Loadwu(temp, receiver, class_offset);
  MaybeRecordImplicitNullCheck(invoke);
  // Instead of simply (possibly) unpoisoning `temp` here, we should
  // emit a read barrier for the previous class reference load.
  // However this is not required in practice, as this is an
  // intermediate/temporary reference and because the current
  // concurrent copying collector keeps the from-space memory
  // intact/accessible until the end of the marking phase (the
  // concurrent copying collector may not in the future).
  __ MaybeUnpoisonHeapReference(temp);

  // If we're compiling baseline, update the inline cache.
  MaybeGenerateInlineCacheCheck(invoke, temp);

  // temp = temp->GetMethodAt(method_offset);
  __ Loadd(temp, temp, method_offset);
  // TMP2 = temp->GetEntryPoint();
  __ Loadd(TMP2, temp, entry_point.Int32Value());
  // TMP2();
  __ Jalr(TMP2);
  RecordPcInfo(invoke, invoke->GetDexPc(), slow_path);
}

void CodeGeneratorRISCV64::MoveFromReturnRegister(Location trg, DataType::Type type) {
  if (!trg.IsValid()) {
    DCHECK(type == DataType::Type::kVoid);
    return;
  }

  DCHECK_NE(type, DataType::Type::kVoid);

  if (DataType::IsIntegralType(type) || type == DataType::Type::kReference) {
    XRegister trg_reg = trg.AsRegister<XRegister>();
    XRegister res_reg = Riscv64ReturnLocation(type).AsRegister<XRegister>();
    __ Mv(trg_reg, res_reg);
  } else {
    FRegister trg_reg = trg.AsFpuRegister<FRegister>();
    FRegister res_reg = Riscv64ReturnLocation(type).AsFpuRegister<FRegister>();
    __ FMvD(trg_reg, res_reg);
  }
}

}  // namespace riscv64
}  // namespace art
