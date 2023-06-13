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
#include "arch/riscv64/registers_riscv64.h"
#include "base/macros.h"
#include "dwarf/register.h"
#include "jit/profiling_info.h"
#include "optimizing/nodes.h"
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

#define __ down_cast<CodeGeneratorRISCV64*>(codegen)->GetAssembler()->  // NOLINT

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

#undef __
#define __ down_cast<Riscv64Assembler*>(GetAssembler())->  // NOLINT

InstructionCodeGeneratorRISCV64::InstructionCodeGeneratorRISCV64(HGraph* graph,
                                                                 CodeGeneratorRISCV64* codegen)
    : InstructionCodeGenerator(graph, codegen),
      assembler_(codegen->GetAssembler()),
      codegen_(codegen) {}
void InstructionCodeGeneratorRISCV64::GenerateClassInitializationCheck(
    SlowPathCodeRISCV64* slow_path, XRegister class_reg) {
  UNUSED(slow_path);
  UNUSED(class_reg);
  LOG(FATAL) << "Unimplemented";
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

void LocationsBuilderRISCV64::VisitAbs(HAbs* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitAbs(HAbs* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
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
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitArrayLength(HArrayLength* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
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
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitBooleanNot(HBooleanNot* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitBoundsCheck(HBoundsCheck* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitBoundsCheck(HBoundsCheck* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitBoundType(HBoundType* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitBoundType(HBoundType* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
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
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitClassTableGet(HClassTableGet* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitClearException(HClearException* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitClearException(HClearException* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitClinitCheck(HClinitCheck* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitClinitCheck(HClinitCheck* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void LocationsBuilderRISCV64::VisitCompare(HCompare* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void InstructionCodeGeneratorRISCV64::VisitCompare(HCompare* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
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
      block_labels_(nullptr) {}

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
  UNUSED(stack_index);
  UNUSED(reg_id);
  LOG(FATAL) << "Unimplemented";
  UNREACHABLE();
}

size_t CodeGeneratorRISCV64::RestoreFloatingPointRegister(size_t stack_index, uint32_t reg_id) {
  if (GetGraph()->HasSIMD()) {
    // TODO: RISCV vector extension.
    UNIMPLEMENTED(FATAL) << "SIMD is unsupported";
  }
  __ FStored(FRegister(reg_id), SP, stack_index);
  return kRiscv64FloatRegSizeInBytes;
}

void CodeGeneratorRISCV64::DumpCoreRegister(std::ostream& stream, int reg) const {
  UNUSED(stream);
  UNUSED(reg);
  LOG(FATAL) << "Unimplemented";
  UNREACHABLE();
}

void CodeGeneratorRISCV64::DumpFloatingPointRegister(std::ostream& stream, int reg) const {
  UNUSED(stream);
  UNUSED(reg);
  LOG(FATAL) << "Unimplemented";
  UNREACHABLE();
}

void CodeGeneratorRISCV64::Finalize() {
  LOG(FATAL) << "Unimplemented";
  UNREACHABLE();
}

// Generate code to invoke a runtime entry point.
void CodeGeneratorRISCV64::InvokeRuntime(QuickEntrypointEnum entrypoint,
                                         HInstruction* instruction,
                                         uint32_t dex_pc,
                                         SlowPathCode* slow_path) {
  UNUSED(entrypoint);
  UNUSED(instruction);
  UNUSED(dex_pc);
  UNUSED(slow_path);
  LOG(FATAL) << "Unimplemented";
}

// Generate code to invoke a runtime entry point, but do not record
// PC-related information in a stack map.
void CodeGeneratorRISCV64::InvokeRuntimeWithoutRecordingPcInfo(int32_t entry_point_offset,
                                                               HInstruction* instruction,
                                                               SlowPathCode* slow_path) {
  UNUSED(entry_point_offset);
  UNUSED(instruction);
  UNUSED(slow_path);
  LOG(FATAL) << "Unimplemented";
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

void CodeGeneratorRISCV64::GenerateNop() { LOG(FATAL) << "Unimplemented"; }

void CodeGeneratorRISCV64::GenerateImplicitNullCheck(HNullCheck* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}
void CodeGeneratorRISCV64::GenerateExplicitNullCheck(HNullCheck* instruction) {
  UNUSED(instruction);
  LOG(FATAL) << "Unimplemented";
}

// Check if the desired_string_load_kind is supported. If it is, return it,
// otherwise return a fall-back kind that should be used instead.
HLoadString::LoadKind CodeGeneratorRISCV64::GetSupportedLoadStringKind(
    HLoadString::LoadKind desired_string_load_kind) {
  UNUSED(desired_string_load_kind);
  LOG(FATAL) << "Unimplemented";
  UNREACHABLE();
}

// Check if the desired_class_load_kind is supported. If it is, return it,
// otherwise return a fall-back kind that should be used instead.
HLoadClass::LoadKind CodeGeneratorRISCV64::GetSupportedLoadClassKind(
    HLoadClass::LoadKind desired_class_load_kind) {
  UNUSED(desired_class_load_kind);
  LOG(FATAL) << "Unimplemented";
  UNREACHABLE();
}

// Check if the desired_dispatch_info is supported. If it is, return it,
// otherwise return a fall-back info that should be used instead.
HInvokeStaticOrDirect::DispatchInfo CodeGeneratorRISCV64::GetSupportedInvokeStaticOrDirectDispatch(
    const HInvokeStaticOrDirect::DispatchInfo& desired_dispatch_info, ArtMethod* method) {
  UNUSED(desired_dispatch_info);
  UNUSED(method);
  LOG(FATAL) << "Unimplemented";
  UNREACHABLE();
}

void CodeGeneratorRISCV64::LoadMethod(MethodLoadKind load_kind, Location temp, HInvoke* invoke) {
  UNUSED(load_kind);
  UNUSED(temp);
  UNUSED(invoke);
  LOG(FATAL) << "Unimplemented";
}

void CodeGeneratorRISCV64::GenerateStaticOrDirectCall(HInvokeStaticOrDirect* invoke,
                                                      Location temp,
                                                      SlowPathCode* slow_path) {
  UNUSED(temp);
  UNUSED(invoke);
  UNUSED(slow_path);
  LOG(FATAL) << "Unimplemented";
}

void CodeGeneratorRISCV64::GenerateVirtualCall(HInvokeVirtual* invoke,
                                               Location temp,
                                               SlowPathCode* slow_path) {
  UNUSED(temp);
  UNUSED(invoke);
  UNUSED(slow_path);
  LOG(FATAL) << "Unimplemented";
}

void CodeGeneratorRISCV64::MoveFromReturnRegister(Location trg, DataType::Type type) {
  UNUSED(trg);
  UNUSED(type);
  LOG(FATAL) << "Unimplemented";
}

}  // namespace riscv64
}  // namespace art
