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

#include "intrinsics_riscv64.h"

#include "code_generator_riscv64.h"

namespace art HIDDEN {
namespace riscv64 {

bool IntrinsicLocationsBuilderRISCV64::TryDispatch(HInvoke* invoke) {
  Dispatch(invoke);
  LocationSummary* res = invoke->GetLocations();
  if (res == nullptr) {
    return false;
  }
  return res->Intrinsified();
}

Riscv64Assembler* IntrinsicCodeGeneratorRISCV64::GetAssembler() {
  return codegen_->GetAssembler();
}

#define __ GetAssembler()->

static void CreateFPToIntLocations(ArenaAllocator* allocator, HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::RequiresFpuRegister());
  locations->SetOut(Location::RequiresRegister());
}

static void CreateIntToFPLocations(ArenaAllocator* allocator, HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::RequiresFpuRegister());
}

void IntrinsicLocationsBuilderRISCV64::VisitDoubleDoubleToRawLongBits(HInvoke* invoke) {
  CreateFPToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorRISCV64::VisitDoubleDoubleToRawLongBits(HInvoke* invoke) {
  LocationSummary* locations = invoke->GetLocations();
  __ FMvXD(locations->Out().AsRegister<XRegister>(), locations->InAt(0).AsFpuRegister<FRegister>());
}

void IntrinsicLocationsBuilderRISCV64::VisitDoubleLongBitsToDouble(HInvoke* invoke) {
  CreateIntToFPLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorRISCV64::VisitDoubleLongBitsToDouble(HInvoke* invoke) {
  LocationSummary* locations = invoke->GetLocations();
  __ FMvDX(locations->Out().AsFpuRegister<FRegister>(), locations->InAt(0).AsRegister<XRegister>());
}

void IntrinsicLocationsBuilderRISCV64::VisitFloatFloatToRawIntBits(HInvoke* invoke) {
  CreateFPToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorRISCV64::VisitFloatFloatToRawIntBits(HInvoke* invoke) {
  LocationSummary* locations = invoke->GetLocations();
  __ FMvXW(locations->Out().AsRegister<XRegister>(), locations->InAt(0).AsFpuRegister<FRegister>());
}

void IntrinsicLocationsBuilderRISCV64::VisitFloatIntBitsToFloat(HInvoke* invoke) {
  CreateIntToFPLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorRISCV64::VisitFloatIntBitsToFloat(HInvoke* invoke) {
  LocationSummary* locations = invoke->GetLocations();
  __ FMvWX(locations->Out().AsFpuRegister<FRegister>(), locations->InAt(0).AsRegister<XRegister>());
}

void IntrinsicLocationsBuilderRISCV64::VisitDoubleIsInfinite(HInvoke* invoke) {
  CreateFPToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorRISCV64::VisitDoubleIsInfinite(HInvoke* invoke) {
  LocationSummary* locations = invoke->GetLocations();
  XRegister out = locations->Out().AsRegister<XRegister>();
  __ FClassD(out, locations->InAt(0).AsFpuRegister<FRegister>());
  __ Andi(out, out, kPositiveInfinity | kNegativeInfinity);
  __ Snez(out, out);
}

void IntrinsicLocationsBuilderRISCV64::VisitFloatIsInfinite(HInvoke* invoke) {
  CreateFPToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorRISCV64::VisitFloatIsInfinite(HInvoke* invoke) {
  LocationSummary* locations = invoke->GetLocations();
  XRegister out = locations->Out().AsRegister<XRegister>();
  __ FClassS(out, locations->InAt(0).AsFpuRegister<FRegister>());
  __ Andi(out, out, kPositiveInfinity | kNegativeInfinity);
  __ Snez(out, out);
}

static void CreateIntToIntLocations(ArenaAllocator* allocator, HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
}

template <typename EmitOp>
void EmitMemoryPeek(HInvoke* invoke, EmitOp&& emit_op) {
  LocationSummary* locations = invoke->GetLocations();
  emit_op(locations->Out().AsRegister<XRegister>(), locations->InAt(0).AsRegister<XRegister>());
}

void IntrinsicLocationsBuilderRISCV64::VisitMemoryPeekByte(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorRISCV64::VisitMemoryPeekByte(HInvoke* invoke) {
  EmitMemoryPeek(invoke, [&](XRegister rd, XRegister rs1) { __ Lb(rd, rs1, 0); });
}

void IntrinsicLocationsBuilderRISCV64::VisitMemoryPeekIntNative(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorRISCV64::VisitMemoryPeekIntNative(HInvoke* invoke) {
  EmitMemoryPeek(invoke, [&](XRegister rd, XRegister rs1) { __ Lw(rd, rs1, 0); });
}

void IntrinsicLocationsBuilderRISCV64::VisitMemoryPeekLongNative(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorRISCV64::VisitMemoryPeekLongNative(HInvoke* invoke) {
  EmitMemoryPeek(invoke, [&](XRegister rd, XRegister rs1) { __ Ld(rd, rs1, 0); });
}

void IntrinsicLocationsBuilderRISCV64::VisitMemoryPeekShortNative(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorRISCV64::VisitMemoryPeekShortNative(HInvoke* invoke) {
  EmitMemoryPeek(invoke, [&](XRegister rd, XRegister rs1) { __ Lh(rd, rs1, 0); });
}

static void CreateIntIntToVoidLocations(ArenaAllocator* allocator, HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, Location::RequiresRegister());
}

template <typename EmitOp>
void EmitMemoryPoke(HInvoke* invoke, EmitOp&& emit_op) {
  LocationSummary* locations = invoke->GetLocations();
  emit_op(locations->InAt(1).AsRegister<XRegister>(), locations->InAt(0).AsRegister<XRegister>());
}

void IntrinsicLocationsBuilderRISCV64::VisitMemoryPokeByte(HInvoke* invoke) {
  CreateIntIntToVoidLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorRISCV64::VisitMemoryPokeByte(HInvoke* invoke) {
  EmitMemoryPoke(invoke, [&](XRegister rs2, XRegister rs1) { __ Sb(rs2, rs1, 0); });
}

void IntrinsicLocationsBuilderRISCV64::VisitMemoryPokeIntNative(HInvoke* invoke) {
  CreateIntIntToVoidLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorRISCV64::VisitMemoryPokeIntNative(HInvoke* invoke) {
  EmitMemoryPoke(invoke, [&](XRegister rs2, XRegister rs1) { __ Sw(rs2, rs1, 0); });
}

void IntrinsicLocationsBuilderRISCV64::VisitMemoryPokeLongNative(HInvoke* invoke) {
  CreateIntIntToVoidLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorRISCV64::VisitMemoryPokeLongNative(HInvoke* invoke) {
  EmitMemoryPoke(invoke, [&](XRegister rs2, XRegister rs1) { __ Sd(rs2, rs1, 0); });
}

void IntrinsicLocationsBuilderRISCV64::VisitMemoryPokeShortNative(HInvoke* invoke) {
  CreateIntIntToVoidLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorRISCV64::VisitMemoryPokeShortNative(HInvoke* invoke) {
  EmitMemoryPoke(invoke, [&](XRegister rs2, XRegister rs1) { __ Sh(rs2, rs1, 0); });
}

template <typename EmitOp>
void EmitIntegralUnOp(HInvoke* invoke, EmitOp&& emit_op) {
  LocationSummary* locations = invoke->GetLocations();
  emit_op(locations->Out().AsRegister<XRegister>(), locations->InAt(0).AsRegister<XRegister>());
}

void IntrinsicLocationsBuilderRISCV64::VisitIntegerReverseBytes(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorRISCV64::VisitIntegerReverseBytes(HInvoke* invoke) {
  EmitIntegralUnOp(invoke, [&](XRegister rd, XRegister rs1) {
    if (codegen_->GetInstructionSetFeatures().HasZbb()) {
      // There is no 32-bit reverse bytes instruction.
      __ Rev8(rd, rs1);
      __ Srai(rd, rd, 32);
    } else {
      ScratchRegisterScope srs(GetAssembler());
      XRegister tmp = srs.AllocateXRegister();
      XRegister tmp2 = srs.AllocateXRegister();
      __ Mv(tmp, rs1);
      __ Li(rd, 0);
      for (int i = 0; i < 32; i += 8) {
        if (i > 0) {
          __ Srli(tmp, tmp, 8);
        }
        __ Andi(tmp2, tmp, 0xff);
        __ Slli(tmp2, tmp2, 32 - 8 - i);
        __ Or(rd, rd, tmp2);
      }
    }
  });
}

void IntrinsicLocationsBuilderRISCV64::VisitLongReverseBytes(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorRISCV64::VisitLongReverseBytes(HInvoke* invoke) {
  EmitIntegralUnOp(invoke, [&](XRegister rd, XRegister rs1) {
    if (codegen_->GetInstructionSetFeatures().HasZbb()) {
      __ Rev8(rd, rs1);
    } else {
      ScratchRegisterScope srs(GetAssembler());
      XRegister tmp = srs.AllocateXRegister();
      XRegister tmp2 = srs.AllocateXRegister();
      __ Mv(tmp, rs1);
      __ Li(rd, 0);
      for (int i = 0; i < 64; i += 8) {
        if (i > 0) {
          __ Srli(tmp, tmp, 8);
        }
        __ Andi(tmp2, tmp, 0xff);
        __ Slli(tmp2, tmp2, 64 - 8 - i);
        __ Or(rd, rd, tmp2);
      }
    }
  });
}

void IntrinsicLocationsBuilderRISCV64::VisitShortReverseBytes(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorRISCV64::VisitShortReverseBytes(HInvoke* invoke) {
  EmitIntegralUnOp(invoke, [&](XRegister rd, XRegister rs1) {
    if (codegen_->GetInstructionSetFeatures().HasZbb()) {
      // There is no 16-bit reverse bytes instruction.
      __ Rev8(rd, rs1);
      __ Srai(rd, rd, 48);
    } else {
      ScratchRegisterScope srs(GetAssembler());
      XRegister tmp = srs.AllocateXRegister();
      XRegister tmp2 = srs.AllocateXRegister();
      __ Mv(tmp, rs1);
      __ Li(rd, 0);
      for (int i = 0; i < 16; i += 8) {
        if (i > 0) {
          __ Srli(tmp, tmp, 8);
        }
        __ Andi(tmp2, tmp, 0xff);
        __ Slli(tmp2, tmp2, 16 - 8 - i);
        __ Or(rd, rd, tmp2);
      }
    }
  });
}

void IntrinsicLocationsBuilderRISCV64::VisitIntegerBitCount(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorRISCV64::VisitIntegerBitCount(HInvoke* invoke) {
  EmitIntegralUnOp(invoke, [&](XRegister rd, XRegister rs1) {
    if (codegen_->GetInstructionSetFeatures().HasZbb()) {
      __ Cpopw(rd, rs1);
    } else {
      ScratchRegisterScope srs(GetAssembler());
      XRegister tmp = srs.AllocateXRegister();
      XRegister tmp2 = srs.AllocateXRegister();
      Riscv64Label loop;
      __ Mv(tmp, rs1);
      __ Li(rd, 0);
      __ Bind(&loop);
      __ Andi(tmp2, tmp, 1);
      __ Add(rd, rd, tmp2);
      __ Srli(tmp, tmp, 1);
      __ Bnez(tmp, &loop);
    }
  });
}

void IntrinsicLocationsBuilderRISCV64::VisitLongBitCount(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorRISCV64::VisitLongBitCount(HInvoke* invoke) {
  EmitIntegralUnOp(invoke, [&](XRegister rd, XRegister rs1) {
    if (codegen_->GetInstructionSetFeatures().HasZbb()) {
      __ Cpop(rd, rs1);
    } else {
      ScratchRegisterScope srs(GetAssembler());
      XRegister tmp = srs.AllocateXRegister();
      XRegister tmp2 = srs.AllocateXRegister();
      Riscv64Label loop;
      __ Mv(tmp, rs1);
      __ Li(rd, 0);
      __ Bind(&loop);
      __ Andi(tmp2, tmp, 1);
      __ Add(rd, rd, tmp2);
      __ Srli(tmp, tmp, 1);
      __ Bnez(tmp, &loop);
    }
  });
}

void IntrinsicLocationsBuilderRISCV64::VisitIntegerHighestOneBit(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorRISCV64::VisitIntegerHighestOneBit(HInvoke* invoke) {
  EmitIntegralUnOp(invoke, [&](XRegister rd, XRegister rs1) {
    ScratchRegisterScope srs(GetAssembler());
    XRegister tmp = srs.AllocateXRegister();
    if (codegen_->GetInstructionSetFeatures().HasZbb()) {
      XRegister tmp2 = srs.AllocateXRegister();
      __ Clzw(tmp, rs1);
      __ Li(tmp2, INT64_C(-0x80000000));
      __ Srlw(tmp2, tmp2, tmp);
      __ And(rd, rs1, tmp2);  // Make sure the result is zero if the input is zero.
    } else {
      __ Mv(rd, rs1);
      for (int i = 1; i < 32; i *= 2) {
        __ Srli(tmp, rd, i);
        __ Or(rd, rd, tmp);
      }
      __ Srli(tmp, rd, 1);
      __ Xor(rd, rd, tmp);
    }
  });
}

void IntrinsicLocationsBuilderRISCV64::VisitLongHighestOneBit(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorRISCV64::VisitLongHighestOneBit(HInvoke* invoke) {
  EmitIntegralUnOp(invoke, [&](XRegister rd, XRegister rs1) {
    ScratchRegisterScope srs(GetAssembler());
    XRegister tmp = srs.AllocateXRegister();
    if (codegen_->GetInstructionSetFeatures().HasZbb()) {
      XRegister tmp2 = srs.AllocateXRegister();
      __ Clz(tmp, rs1);
      __ Li(tmp2, INT64_C(-0x8000000000000000));
      __ Srl(tmp2, tmp2, tmp);
      __ And(rd, rs1, tmp2);  // Make sure the result is zero if the input is zero.
    } else {
      __ Mv(rd, rs1);
      for (int i = 1; i < 64; i *= 2) {
        __ Srli(tmp, rd, i);
        __ Or(rd, rd, tmp);
      }
      __ Srli(tmp, rd, 1);
      __ Xor(rd, rd, tmp);
    }
  });
}

void IntrinsicLocationsBuilderRISCV64::VisitIntegerLowestOneBit(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorRISCV64::VisitIntegerLowestOneBit(HInvoke* invoke) {
  EmitIntegralUnOp(invoke, [&](XRegister rd, XRegister rs1) {
    ScratchRegisterScope srs(GetAssembler());
    XRegister tmp = srs.AllocateXRegister();
    __ NegW(tmp, rs1);
    __ And(rd, rs1, tmp);
  });
}

void IntrinsicLocationsBuilderRISCV64::VisitLongLowestOneBit(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorRISCV64::VisitLongLowestOneBit(HInvoke* invoke) {
  EmitIntegralUnOp(invoke, [&](XRegister rd, XRegister rs1) {
    ScratchRegisterScope srs(GetAssembler());
    XRegister tmp = srs.AllocateXRegister();
    __ Neg(tmp, rs1);
    __ And(rd, rs1, tmp);
  });
}

void IntrinsicLocationsBuilderRISCV64::VisitIntegerNumberOfLeadingZeros(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorRISCV64::VisitIntegerNumberOfLeadingZeros(HInvoke* invoke) {
  EmitIntegralUnOp(invoke, [&](XRegister rd, XRegister rs1) {
    if (codegen_->GetInstructionSetFeatures().HasZbb()) {
      __ Clzw(rd, rs1);
    } else {
      ScratchRegisterScope srs(GetAssembler());
      XRegister tmp = srs.AllocateXRegister();
      Riscv64Label loop, start;
      __ Mv(tmp, rs1);
      __ Li(rd, INT64_C(32));
      __ J(&start);
      __ Bind(&loop);
      __ Srli(tmp, tmp, 1);
      __ Addi(rd, rd, -1);
      __ Bind(&start);
      __ Bnez(tmp, &loop);
    }
  });
}

void IntrinsicLocationsBuilderRISCV64::VisitLongNumberOfLeadingZeros(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorRISCV64::VisitLongNumberOfLeadingZeros(HInvoke* invoke) {
  EmitIntegralUnOp(invoke, [&](XRegister rd, XRegister rs1) {
    if (codegen_->GetInstructionSetFeatures().HasZbb()) {
      __ Clz(rd, rs1);
    } else {
      ScratchRegisterScope srs(GetAssembler());
      XRegister tmp = srs.AllocateXRegister();
      Riscv64Label loop, start;
      __ Mv(tmp, rs1);
      __ Li(rd, INT64_C(64));
      __ J(&start);
      __ Bind(&loop);
      __ Srli(tmp, tmp, 1);
      __ Addi(rd, rd, -1);
      __ Bind(&start);
      __ Bnez(tmp, &loop);
    }
  });
}

void IntrinsicLocationsBuilderRISCV64::VisitIntegerNumberOfTrailingZeros(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorRISCV64::VisitIntegerNumberOfTrailingZeros(HInvoke* invoke) {
  EmitIntegralUnOp(invoke, [&](XRegister rd, XRegister rs1) {
    if (codegen_->GetInstructionSetFeatures().HasZbb()) {
      __ Ctzw(rd, rs1);
    } else {
      ScratchRegisterScope srs(GetAssembler());
      XRegister tmp = srs.AllocateXRegister();
      Riscv64Label loop, start;
      __ Slli(tmp, rs1, 32);
      __ Li(rd, INT64_C(32));
      __ J(&start);
      __ Bind(&loop);
      __ Slli(tmp, tmp, 1);
      __ Addi(rd, rd, -1);
      __ Bind(&start);
      __ Bnez(tmp, &loop);
    }
  });
}

void IntrinsicLocationsBuilderRISCV64::VisitLongNumberOfTrailingZeros(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorRISCV64::VisitLongNumberOfTrailingZeros(HInvoke* invoke) {
  EmitIntegralUnOp(invoke, [&](XRegister rd, XRegister rs1) {
    if (codegen_->GetInstructionSetFeatures().HasZbb()) {
      __ Ctz(rd, rs1);
    } else {
      ScratchRegisterScope srs(GetAssembler());
      XRegister tmp = srs.AllocateXRegister();
      Riscv64Label loop, start;
      __ Mv(tmp, rs1);
      __ Li(rd, INT64_C(64));
      __ J(&start);
      __ Bind(&loop);
      __ Slli(tmp, tmp, 1);
      __ Addi(rd, rd, -1);
      __ Bind(&start);
      __ Bnez(tmp, &loop);
    }
  });
}

#define MARK_UNIMPLEMENTED(Name) UNIMPLEMENTED_INTRINSIC(RISCV64, Name)
UNIMPLEMENTED_INTRINSIC_LIST_RISCV64(MARK_UNIMPLEMENTED);
#undef MARK_UNIMPLEMENTED

UNREACHABLE_INTRINSICS(RISCV64)

}  // namespace riscv64
}  // namespace art
