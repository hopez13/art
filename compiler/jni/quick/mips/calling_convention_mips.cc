/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include "calling_convention_mips.h"

#include "base/logging.h"
#include "handle_scope-inl.h"
#include "utils/mips/managed_register_mips.h"

namespace art {
namespace mips {

// Up to how many float-like (float, double) args can be enregistered.
// The rest of the args must go on the stack.
constexpr size_t kMaxFloatOrDoubleRegisterArguments = 2u;
// Up to how many integer-like (pointers, objects, longs, int, short, bool, etc) args can be
// enregistered. The rest of the args must go on the stack.
constexpr size_t kMaxIntLikeRegisterArguments = 4u;

static const Register kCoreArgumentRegisters[] = { A0, A1, A2, A3 };
static const FRegister kFArgumentRegisters[] = { F12, F14 };
static const DRegister kDArgumentRegisters[] = { D6, D7 };

static constexpr ManagedRegister kCalleeSaveRegisters[] = {
    // Core registers.
    MipsManagedRegister::FromCoreRegister(S2),
    MipsManagedRegister::FromCoreRegister(S3),
    MipsManagedRegister::FromCoreRegister(S4),
    MipsManagedRegister::FromCoreRegister(S5),
    MipsManagedRegister::FromCoreRegister(S6),
    MipsManagedRegister::FromCoreRegister(S7),
    MipsManagedRegister::FromCoreRegister(FP),
    // No hard float callee saves.
};

static constexpr uint32_t CalculateCoreCalleeSpillMask() {
  // RA is a special callee save which is not reported by CalleeSaveRegisters().
  uint32_t result = 1 << RA;
  for (auto&& r : kCalleeSaveRegisters) {
    if (r.AsMips().IsCoreRegister()) {
      result |= (1 << r.AsMips().AsCoreRegister());
    }
  }
  return result;
}

static constexpr uint32_t kCoreCalleeSpillMask = CalculateCoreCalleeSpillMask();
static constexpr uint32_t kFpCalleeSpillMask = 0u;

// Calling convention
ManagedRegister MipsManagedRuntimeCallingConvention::InterproceduralScratchRegister() {
  return MipsManagedRegister::FromCoreRegister(T9);
}

ManagedRegister MipsJniCallingConvention::InterproceduralScratchRegister() {
  return MipsManagedRegister::FromCoreRegister(T9);
}

static ManagedRegister ReturnRegisterForShorty(const char* shorty) {
  if (shorty[0] == 'F') {
    return MipsManagedRegister::FromFRegister(F0);
  } else if (shorty[0] == 'D') {
    return MipsManagedRegister::FromDRegister(D0);
  } else if (shorty[0] == 'J') {
    return MipsManagedRegister::FromRegisterPair(V0_V1);
  } else if (shorty[0] == 'V') {
    return MipsManagedRegister::NoRegister();
  } else {
    return MipsManagedRegister::FromCoreRegister(V0);
  }
}

ManagedRegister MipsManagedRuntimeCallingConvention::ReturnRegister() {
  return ReturnRegisterForShorty(GetShorty());
}

ManagedRegister MipsJniCallingConvention::ReturnRegister() {
  return ReturnRegisterForShorty(GetShorty());
}

ManagedRegister MipsJniCallingConvention::IntReturnRegister() {
  return MipsManagedRegister::FromCoreRegister(V0);
}

// Managed runtime calling convention

ManagedRegister MipsManagedRuntimeCallingConvention::MethodRegister() {
  return MipsManagedRegister::FromCoreRegister(A0);
}

bool MipsManagedRuntimeCallingConvention::IsCurrentParamInRegister() {
  return false;  // Everything moved to stack on entry.
}

bool MipsManagedRuntimeCallingConvention::IsCurrentParamOnStack() {
  return true;
}

ManagedRegister MipsManagedRuntimeCallingConvention::CurrentParamRegister() {
  LOG(FATAL) << "Should not reach here";
  return ManagedRegister::NoRegister();
}

FrameOffset MipsManagedRuntimeCallingConvention::CurrentParamStackOffset() {
  CHECK(IsCurrentParamOnStack());
  FrameOffset result =
      FrameOffset(displacement_.Int32Value() +        // displacement
                  kFramePointerSize +                 // Method*
                  (itr_slots_ * kFramePointerSize));  // offset into in args
  return result;
}

const ManagedRegisterEntrySpills& MipsManagedRuntimeCallingConvention::EntrySpills() {
  // We spill the argument registers on MIPS to free them up for scratch use, we then assume
  // all arguments are on the stack.
  if ((entry_spills_.size() == 0) && (NumArgs() > 0)) {
    uint32_t gpr_index = 1;  // Skip A0, it is used for ArtMethod*.
    uint32_t fpr_index = 0;

    for (ResetIterator(FrameOffset(0)); HasNext(); Next()) {
      if (IsCurrentParamAFloatOrDouble()) {
        if (IsCurrentParamADouble()) {
          if (fpr_index < arraysize(kDArgumentRegisters)) {
            entry_spills_.push_back(
                MipsManagedRegister::FromDRegister(kDArgumentRegisters[fpr_index++]));
          } else {
            entry_spills_.push_back(ManagedRegister::NoRegister(), 8);
          }
        } else {
          if (fpr_index < arraysize(kFArgumentRegisters)) {
            entry_spills_.push_back(
                MipsManagedRegister::FromFRegister(kFArgumentRegisters[fpr_index++]));
          } else {
            entry_spills_.push_back(ManagedRegister::NoRegister(), 4);
          }
        }
      } else {
        if (IsCurrentParamALong() && !IsCurrentParamAReference()) {
          if (gpr_index == 1) {
            // Don't use a1-a2 as a register pair, move to a2-a3 instead.
            gpr_index++;
          }
          if (gpr_index < arraysize(kCoreArgumentRegisters) - 1) {
            entry_spills_.push_back(
                MipsManagedRegister::FromCoreRegister(kCoreArgumentRegisters[gpr_index++]));
          } else if (gpr_index == arraysize(kCoreArgumentRegisters) - 1) {
            gpr_index++;
            entry_spills_.push_back(ManagedRegister::NoRegister(), 4);
          } else {
            entry_spills_.push_back(ManagedRegister::NoRegister(), 4);
          }
        }

        if (gpr_index < arraysize(kCoreArgumentRegisters)) {
          entry_spills_.push_back(
            MipsManagedRegister::FromCoreRegister(kCoreArgumentRegisters[gpr_index++]));
        } else {
          entry_spills_.push_back(ManagedRegister::NoRegister(), 4);
        }
      }
    }
  }
  return entry_spills_;
}
// JNI calling convention

MipsJniCallingConvention::MipsJniCallingConvention(bool is_static,
                                                   bool is_synchronized,
                                                   bool is_critical_native,
                                                   const char* shorty)
    : JniCallingConvention(is_static,
                           is_synchronized,
                           is_critical_native,
                           shorty,
                           kMipsPointerSize) {
  // Compute padding to ensure longs and doubles are not split in o32.
  size_t padding = 0;
  size_t cur_arg, cur_reg;
  if (LIKELY(HasExtraArgumentsForJni())) {
    cur_arg = NumImplicitArgs();
    cur_reg = 2;
  } else {
    cur_arg = 0;
    cur_reg = 0;
  }

  for (; cur_arg < NumArgs(); cur_arg++) {
    if (IsParamALongOrDouble(cur_arg)) {
      if ((cur_reg & 1) != 0) {
        padding += 4;
        cur_reg++;  // additional bump to ensure alignment
      }
      cur_reg++;  // additional bump to skip extra long word
    }
    cur_reg++;  // bump the iterator for every argument
  }
  if (cur_reg < kMaxIntLikeRegisterArguments) {
    padding_ = 0;
  } else {
    padding_ = padding;
  }

  use_fp_arg_registers_ = false;
  if (is_critical_native) {
    if (NumArgs() > 0) {
      if (IsParamAFloatOrDouble(0)) {
        use_fp_arg_registers_ = true;
      }
    }
  }
}

uint32_t MipsJniCallingConvention::CoreSpillMask() const {
  return kCoreCalleeSpillMask;
}

uint32_t MipsJniCallingConvention::FpSpillMask() const {
  return kFpCalleeSpillMask;
}

ManagedRegister MipsJniCallingConvention::ReturnScratchRegister() const {
  return MipsManagedRegister::FromCoreRegister(AT);
}

size_t MipsJniCallingConvention::FrameSize() {
  // ArtMethod*, RA and callee save area size, local reference segment state.
  const size_t method_ptr_size = static_cast<size_t>(kMipsPointerSize);
  const size_t ra_return_addr_size = kFramePointerSize;
  const size_t callee_save_area_size = CalleeSaveRegisters().size() * kFramePointerSize;

  size_t frame_data_size = method_ptr_size + ra_return_addr_size + callee_save_area_size;

  if (LIKELY(HasLocalReferenceSegmentState())) {
    // Local reference segment state.
    frame_data_size += kFramePointerSize;
  }

  // References plus 2 words for HandleScope header.
  const size_t handle_scope_size = HandleScope::SizeOf(kMipsPointerSize, ReferenceCount());

  size_t total_size = frame_data_size;
  if (LIKELY(HasHandleScope())) {
    // HandleScope is sometimes excluded.
    total_size += handle_scope_size;    // Handle scope size.
  }

  // Plus return value spill area size.
  total_size += SizeOfReturnValue();

  return RoundUp(total_size, kStackAlignment);
}

size_t MipsJniCallingConvention::OutArgSize() {
  return RoundUp(NumberOfOutgoingStackArgs() * kFramePointerSize + padding_, kStackAlignment);
}

ArrayRef<const ManagedRegister> MipsJniCallingConvention::CalleeSaveRegisters() const {
  return ArrayRef<const ManagedRegister>(kCalleeSaveRegisters);
}

// JniCallingConvention ABI follows o32 where longs and doubles must occur
// in even register numbers and stack slots.
void MipsJniCallingConvention::Next() {
  JniCallingConvention::Next();

  if (LIKELY(HasNext())) {  // Avoid CHECK failure for IsCurrentParam
    // Ensure slot is 8-byte aligned for longs/doubles (o32).
    if (IsCurrentParamALongOrDouble() && ((itr_slots_ & 0x1u) != 0)) {
      // itr_slots_ needs to be an even number, according to o32.
      itr_slots_++;
    }
  }
}

bool MipsJniCallingConvention::IsCurrentParamInRegister() {
  return itr_slots_ < kMaxIntLikeRegisterArguments;
}

bool MipsJniCallingConvention::IsCurrentParamOnStack() {
  return !IsCurrentParamInRegister();
}

ManagedRegister MipsJniCallingConvention::CurrentParamRegister() {
  CHECK_LT(itr_slots_, kMaxIntLikeRegisterArguments);
  if (use_fp_arg_registers_ && (itr_args_ < kMaxFloatOrDoubleRegisterArguments)) {
    if (IsCurrentParamAFloatOrDouble()) {
      if (IsCurrentParamADouble()) {
        return MipsManagedRegister::FromDRegister(kDArgumentRegisters[itr_args_]);
      } else {
        return MipsManagedRegister::FromFRegister(kFArgumentRegisters[itr_args_]);
      }
    }
  }
  if (IsCurrentParamALongOrDouble()) {
    if (itr_slots_ == 0u) {
      return MipsManagedRegister::FromRegisterPair(A0_A1);
    } else {
      CHECK_EQ(itr_slots_, 2u);
      return MipsManagedRegister::FromRegisterPair(A2_A3);
    }
  } else {
    return MipsManagedRegister::FromCoreRegister(kCoreArgumentRegisters[itr_slots_]);
  }
}

FrameOffset MipsJniCallingConvention::CurrentParamStackOffset() {
  CHECK_GE(itr_slots_, kMaxIntLikeRegisterArguments);
  size_t offset = displacement_.Int32Value() - OutArgSize() + (itr_slots_ * kFramePointerSize);
  CHECK_LT(offset, OutArgSize());
  return FrameOffset(offset);
}

size_t MipsJniCallingConvention::NumberOfOutgoingStackArgs() {
  size_t static_args = IsStatic() ? 1 : 0;  // count jclass
  // regular argument parameters and this
  size_t param_args = NumArgs() + NumLongOrDoubleArgs();
  // count JNIEnv*
  return static_args + param_args + 1;
}
}  // namespace mips
}  // namespace art
