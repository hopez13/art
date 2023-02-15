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

#ifndef ART_RUNTIME_ARCH_RISCV64_ASM_SUPPORT_RISCV64_S_
#define ART_RUNTIME_ARCH_RISCV64_ASM_SUPPORT_RISCV64_S_

#include "asm_support_riscv64.h"
#include "interpreter/cfi_asm_support.h"

// Define special registers.

// Register holding Thread::Current().
#define xSELF s1


.macro ENTRY name
    .hidden \name  // Hide this as a global symbol, so we do not incur plt calls.
    .global \name
    // ART-compiled functions have OatQuickMethodHeader but assembly functions do not.
    // Prefix the assembly code with 0xFFs, which means there is no method header.
    .byte 0xFF, 0xFF, 0xFF, 0xFF
    // Cache alignment for function entry.
    // Use 0xFF as the last 4 bytes of alignment stand for OatQuickMethodHeader.
    .balign 16, 0xFF
\name:
    .cfi_startproc
.endm


.macro END name
    .cfi_endproc
.endm


.macro UNDEFINED name
    ENTRY \name
        unimp
    END \name
.endm


// The spec is not clear whether the CFA is part of the saved state and tools
// differ in the behaviour, so explicitly set the CFA to avoid any ambiguity.
// The restored CFA state should match the CFA state during CFI_REMEMBER_STATE.
.macro CFI_RESTORE_STATE_AND_DEF_CFA reg, offset
    .cfi_restore_state
    .cfi_def_cfa \reg, \offset
.endm


.macro INCREASE_FRAME frame_adjustment
    addi sp, sp, -(\frame_adjustment)
    .cfi_adjust_cfa_offset (\frame_adjustment)
.endm


.macro DECREASE_FRAME frame_adjustment
    addi sp, sp, (\frame_adjustment)
    .cfi_adjust_cfa_offset -(\frame_adjustment)
.endm


.macro LOAD_RUNTIME_INSTANCE reg
#if __has_feature(hwaddress_sanitizer) && __clang_major__ >= 10
    la \reg, :pg_hi21_nc:_ZN3art7Runtime9instance_E
#else
    la \reg, _ZN3art7Runtime9instance_E
#endif
    ld \reg, 0(\reg)
.endm


// We need to save callee-save GPRs on the stack in as they may contain references, and must be
// visible to GC (unless the called method holds mutator lock and prevents GC from happening).
// FP callee-saves shall be preserved by whatever runtime function we call, so they do not need
// to be saved.
.macro SETUP_SAVE_REFS_AND_ARGS_FRAME_INTERNAL
#if (FRAME_SIZE_SAVE_REFS_AND_ARGS != 8*(1 + 8 + 7 + 10 + 1 + 1))
#error "FRAME_SIZE_SAVE_REFS_AND_ARGS(RISCV64) size not as expected."
#endif
    // stack slot (0*8)(sp) is for ArtMethod*
    // stack slot (1*8)(sp) is for padding

    fsd  fa0,  (2*8)(sp)
    fsd  fa1,  (3*8)(sp)
    fsd  fa2,  (4*8)(sp)
    fsd  fa3,  (5*8)(sp)
    fsd  fa4,  (6*8)(sp)
    fsd  fa5,  (7*8)(sp)
    fsd  fa6,  (8*8)(sp)
    fsd  fa7,  (9*8)(sp)

    sd   fp,  (10*8)(sp)  // x8, frame pointer

    // a0 is the method pointer
    sd   a1,  (11*8)(sp)  // x11
    sd   a2,  (12*8)(sp)  // x12
    sd   a3,  (13*8)(sp)  // x13
    sd   a4,  (14*8)(sp)  // x14
    sd   a5,  (15*8)(sp)  // x15
    sd   a6,  (16*8)(sp)  // x16
    sd   a7,  (17*8)(sp)  // x17

    sd   s3,  (18*8)(sp)  // x19
    sd   s4,  (19*8)(sp)  // x20
    sd   s5,  (20*8)(sp)  // x21
    sd   s6,  (21*8)(sp)  // x22
    sd   s7,  (22*8)(sp)  // x23
    sd   s8,  (23*8)(sp)  // x24
    sd   s9,  (24*8)(sp)  // x25
    sd   s10, (25*8)(sp)  // x26
    sd   s11, (26*8)(sp)  // x27

    sd   ra,  (27*8)(sp)  // x1, return address
.endm


.macro RESTORE_SAVE_REFS_AND_ARGS_FRAME_INTERNAL base
    // stack slot (0*8)(sp) is for ArtMethod*
    // stack slot (1*8)(sp) is for padding

    fld  fa0,  (2*8)(sp)
    fld  fa1,  (3*8)(sp)
    fld  fa2,  (4*8)(sp)
    fld  fa3,  (5*8)(sp)
    fld  fa4,  (6*8)(sp)
    fld  fa5,  (7*8)(sp)
    fld  fa6,  (8*8)(sp)
    fld  fa7,  (9*8)(sp)

    ld   fp,  (10*8)(sp)  // x8, frame pointer

    // a0 is the method pointer
    ld   a1,  (11*8)(sp)  // x11
    ld   a2,  (12*8)(sp)  // x12
    ld   a3,  (13*8)(sp)  // x13
    ld   a4,  (14*8)(sp)  // x14
    ld   a5,  (15*8)(sp)  // x15
    ld   a6,  (16*8)(sp)  // x16
    ld   a7,  (17*8)(sp)  // x17

    ld   s3,  (18*8)(sp)  // x19
    ld   s4,  (19*8)(sp)  // x20
    ld   s5,  (20*8)(sp)  // x21
    ld   s6,  (21*8)(sp)  // x22
    ld   s7,  (22*8)(sp)  // x23
    ld   s8,  (23*8)(sp)  // x24
    ld   s9,  (24*8)(sp)  // x25
    ld   s10, (25*8)(sp)  // x26
    ld   s11, (26*8)(sp)  // x27

    ld   ra,  (27*8)(sp)  // x1, return address
.endm


.macro SETUP_SAVE_REFS_AND_ARGS_FRAME
    INCREASE_FRAME FRAME_SIZE_SAVE_REFS_AND_ARGS
    SETUP_SAVE_REFS_AND_ARGS_FRAME_INTERNAL

    // art::Runtime* t0 = art::Runtime::instance_;
    LOAD_RUNTIME_INSTANCE t0
    // ArtMethod* t0 = Runtime::instance_->callee_save_methods_[kSaveRefAndArgs];
    ld t0, RUNTIME_SAVE_REFS_AND_ARGS_METHOD_OFFSET(t0)
    // Store ArtMethod* at the bottom of the stack.
    sd t0, (sp)

    // Place sp in Thread::Current()->top_quick_frame.
    sd sp, THREAD_TOP_QUICK_FRAME_OFFSET(xSELF)
.endm


.macro SETUP_SAVE_REFS_AND_ARGS_FRAME_WITH_METHOD_IN_A0
    INCREASE_FRAME FRAME_SIZE_SAVE_REFS_AND_ARGS
    SETUP_SAVE_REFS_AND_ARGS_FRAME_INTERNAL

    // Store ArtMethod* at the bottom of the stack.
    sd a0, (sp)

    // Place sp in Thread::Current()->top_quick_frame.
    sd sp, THREAD_TOP_QUICK_FRAME_OFFSET(xSELF)
.endm


.macro RESTORE_SAVE_REFS_AND_ARGS_FRAME
    RESTORE_SAVE_REFS_AND_ARGS_FRAME_INTERNAL
    DECREASE_FRAME FRAME_SIZE_SAVE_REFS_AND_ARGS
.endm


.macro SAVE_ALL_CALLEE_SAVES
#if (FRAME_SIZE_SAVE_ALL_CALLEE_SAVES != 8*(1 + 12 + 10 + 1))
#error "FRAME_SIZE_SAVE_ALL_CALLEE_SAVES(RISCV64) size not as expected."
#endif
    // stack slot (0*8)(sp) is for ArtMethod*

    // FP callee-saves.
    fsd  fs0,  (8*1)(sp)   // f8
    fsd  fs1,  (8*2)(sp)   // f9
    fsd  fs2,  (8*3)(sp)   // f18
    fsd  fs3,  (8*4)(sp)   // f19
    fsd  fs4,  (8*5)(sp)   // f20
    fsd  fs5,  (8*6)(sp)   // f21
    fsd  fs6,  (8*7)(sp)   // f22
    fsd  fs7,  (8*8)(sp)   // f23
    fsd  fs8,  (8*9)(sp)   // f24
    fsd  fs9,  (8*10)(sp)  // f25
    fsd  fs10, (8*11)(sp)  // f26
    fsd  fs11, (8*12)(sp)  // f27

    // GP callee-saves
    sd   s0,  (8*13)(sp)  // x8/fp, frame pointer
    sd   s3,  (8*14)(sp)  // x19
    sd   s4,  (8*15)(sp)  // x20
    sd   s5,  (8*16)(sp)  // x21
    sd   s6,  (8*17)(sp)  // x22
    sd   s7,  (8*18)(sp)  // x23
    sd   s8,  (8*19)(sp)  // x24
    sd   s9,  (8*20)(sp)  // x25
    sd   s10, (8*21)(sp)  // x26
    sd   s11, (8*22)(sp)  // x27

    sd   ra,  (8*23)(sp)  // x1, return address
.endm


.macro SETUP_SAVE_ALL_CALLEE_SAVES_FRAME
    INCREASE_FRAME FRAME_SIZE_SAVE_ALL_CALLEE_SAVES
    SAVE_ALL_CALLEE_SAVES

    // art::Runtime* t0 = art::Runtime::instance_;
    LOAD_RUNTIME_INSTANCE t0
    // ArtMethod* t0 = Runtime::instance_->callee_save_methods_[kSaveAllCalleeSaves];
    ld t0, RUNTIME_SAVE_ALL_CALLEE_SAVES_METHOD_OFFSET(t0)
    // Store ArtMethod* at the bottom of the stack
    sd t0, (sp)

    // Place sp in Thread::Current()->top_quick_frame.
    sd sp, THREAD_TOP_QUICK_FRAME_OFFSET(xSELF)
.endm


.macro SETUP_SAVE_EVERYTHING_FRAME
#if (FRAME_SIZE_SAVE_EVERYTHING != 8*(1 + 32 + 26 + 1))
#error "FRAME_SIZE_SAVE_EVERYTHING(ARM64) size not as expected."
#endif
    INCREASE_FRAME FRAME_SIZE_SAVE_EVERYTHING

    // stack slot (8*0)(sp) is for ArtMethod*
    // stack slot (8*1)(sp) is for padding

    // 32 slots for FPRs
    fsd  ft0,   8*2(sp)   // f0
    fsd  ft1,   8*3(sp)   // f1
    fsd  ft2,   8*4(sp)   // f2
    fsd  ft3,   8*5(sp)   // f3
    fsd  ft4,   8*6(sp)   // f4
    fsd  ft5,   8*7(sp)   // f5
    fsd  ft6,   8*8(sp)   // f6
    fsd  ft7,   8*9(sp)   // f7
    fsd  fs0,   8*10(sp)  // f8
    fsd  fs1,   8*11(sp)  // f9
# define SAVE_EVERYTHING_FRAME_OFFSET_FA0 (8*12)
    fsd  fa0,   8*12(sp)  // f10, offset must equal SAVE_EVERYTHING_FRAME_OFFSET_FA0
    fsd  fa1,   8*13(sp)  // f11
    fsd  fa2,   8*14(sp)  // f12
    fsd  fa3,   8*15(sp)  // f13
    fsd  fa4,   8*16(sp)  // f14
    fsd  fa5,   8*17(sp)  // f15
    fsd  fa6,   8*18(sp)  // f16
    fsd  fa7,   8*19(sp)  // f17
    fsd  fs2,   8*20(sp)  // f18
    fsd  fs3,   8*21(sp)  // f19
    fsd  fs4,   8*22(sp)  // f20
    fsd  fs5,   8*23(sp)  // f21
    fsd  fs6,   8*24(sp)  // f22
    fsd  fs7,   8*25(sp)  // f23
    fsd  fs8,   8*26(sp)  // f24
    fsd  fs9,   8*27(sp)  // f25
    fsd  fs10,  8*28(sp)  // f26
    fsd  fs11,  8*29(sp)  // f27
    fsd  ft8,   8*30(sp)  // f28
    fsd  ft9,   8*31(sp)  // f29
    fsd  ft10,  8*32(sp)  // f30
    fsd  ft11,  8*33(sp)  // f31

    // 26 slots for GPRs (excluded: zero/x0, sp/x2, tp/x3, gp/x4, s1/x9, s2/x18)
    sd  t0,   8*34(sp)  // x5
    sd  t1,   8*35(sp)  // x6
    sd  t2,   8*36(sp)  // x7
    sd  s0,   8*37(sp)  // x8
# define SAVE_EVERYTHING_FRAME_OFFSET_A0 (8*38)
    sd  a0,   8*38(sp)  // x10, offset must equal SAVE_EVERYTHING_FRAME_OFFSET_A0
    sd  a1,   8*39(sp)  // x11
    sd  a2,   8*40(sp)  // x12
    sd  a3,   8*41(sp)  // x13
    sd  a4,   8*42(sp)  // x14
    sd  a5,   8*43(sp)  // x15
    sd  a6,   8*44(sp)  // x16
    sd  a7,   8*45(sp)  // x17
    sd  s3,   8*46(sp)  // x19
    sd  s4,   8*47(sp)  // x20
    sd  s5,   8*48(sp)  // x21
    sd  s6,   8*49(sp)  // x22
    sd  s7,   8*50(sp)  // x23
    sd  s8,   8*51(sp)  // x24
    sd  s9,   8*52(sp)  // x25
    sd  s10,  8*53(sp)  // x26
    sd  s11,  8*54(sp)  // x27
    sd  t3,   8*55(sp)  // x28
    sd  t4,   8*56(sp)  // x29
    sd  t5,   8*57(sp)  // x30
    sd  t6,   8*58(sp)  // x31

    sd  ra,   8*59(sp) // x1, return address

    // art::Runtime* t0 = art::Runtime::instance_;
    LOAD_RUNTIME_INSTANCE t0
    // ArtMethod* t0 = Runtime::instance_->callee_save_methods_[kSaveEverything];
    ld t0, RUNTIME_SAVE_EVERYTHING_METHOD_OFFSET(t0)
    // Store ArtMethod* at the bottom of the stack.
    sd t0, (sp)

    // Place sp in Thread::Current()->top_quick_frame.
    sd sp, THREAD_TOP_QUICK_FRAME_OFFSET(xSELF)
.endm


.macro RESTORE_SAVE_EVERYTHING_FRAME
    // stack slot (8*0)(sp) is for ArtMethod*
    // stack slot (8*1)(sp) is for padding

    // 32 slots for FPRs
    fld  ft0,   8*2(sp)   // f0
    fld  ft1,   8*3(sp)   // f1
    fld  ft2,   8*4(sp)   // f2
    fld  ft3,   8*5(sp)   // f3
    fld  ft4,   8*6(sp)   // f4
    fld  ft5,   8*7(sp)   // f5
    fld  ft6,   8*8(sp)   // f6
    fld  ft7,   8*9(sp)   // f7
    fld  fs0,   8*10(sp)  // f8
    fld  fs1,   8*11(sp)  // f9
    fld  fa0,   8*12(sp)  // f10
    fld  fa1,   8*13(sp)  // f11
    fld  fa2,   8*14(sp)  // f12
    fld  fa3,   8*15(sp)  // f13
    fld  fa4,   8*16(sp)  // f14
    fld  fa5,   8*17(sp)  // f15
    fld  fa6,   8*18(sp)  // f16
    fld  fa7,   8*19(sp)  // f17
    fld  fs2,   8*20(sp)  // f18
    fld  fs3,   8*21(sp)  // f19
    fld  fs4,   8*22(sp)  // f20
    fld  fs5,   8*23(sp)  // f21
    fld  fs6,   8*24(sp)  // f22
    fld  fs7,   8*25(sp)  // f23
    fld  fs8,   8*26(sp)  // f24
    fld  fs9,   8*27(sp)  // f25
    fld  fs10,  8*28(sp)  // f26
    fld  fs11,  8*29(sp)  // f27
    fld  ft8,   8*30(sp)  // f28
    fld  ft9,   8*31(sp)  // f29
    fld  ft10,  8*32(sp)  // f30
    fld  ft11,  8*33(sp)  // f31

    // 26 slots for GPRs (excluded: zero/x0, sp/x2, tp/x3, gp/x4, s1/x9, s2/x18)
    ld  t0,   8*34(sp)  // x5
    ld  t1,   8*35(sp)  // x6
    ld  t2,   8*36(sp)  // x7
    ld  s0,   8*37(sp)  // x8
    ld  a0,   8*38(sp)  // x10
    ld  a1,   8*39(sp)  // x11
    ld  a2,   8*40(sp)  // x12
    ld  a3,   8*41(sp)  // x13
    ld  a4,   8*42(sp)  // x14
    ld  a5,   8*43(sp)  // x15
    ld  a6,   8*44(sp)  // x16
    ld  a7,   8*45(sp)  // x17
    ld  s3,   8*46(sp)  // x19
    ld  s4,   8*47(sp)  // x20
    ld  s5,   8*48(sp)  // x21
    ld  s6,   8*49(sp)  // x22
    ld  s7,   8*50(sp)  // x23
    ld  s8,   8*51(sp)  // x24
    ld  s9,   8*52(sp)  // x25
    ld  s10,  8*53(sp)  // x26
    ld  s11,  8*54(sp)  // x27
    ld  t3,   8*55(sp)  // x28
    ld  t4,   8*56(sp)  // x29
    ld  t5,   8*57(sp)  // x30
    ld  t6,   8*58(sp)  // x31

    ld  ra,   8*59(sp) // x1, return address

    DECREASE_FRAME FRAME_SIZE_SAVE_EVERYTHING
.endm


// Macro that calls through to artDeliverPendingExceptionFromCode, where the pending exception is
// Thread::Current()->exception_ when the runtime method frame is ready.
.macro DELIVER_PENDING_EXCEPTION_FRAME_READY
    mv a0, xSELF
    call artDeliverPendingExceptionFromCode  // Point of no return.
    unimp                                    // Unreachable.
.endm


// Macro that calls through to artDeliverPendingExceptionFromCode, where the pending exception is
// Thread::Current()->exception_.
.macro DELIVER_PENDING_EXCEPTION
    SETUP_SAVE_ALL_CALLEE_SAVES_FRAME
    DELIVER_PENDING_EXCEPTION_FRAME_READY
.endm


.macro RETURN_OR_DELIVER_PENDING_EXCEPTION_REG reg
    ld \reg, THREAD_EXCEPTION_OFFSET(xSELF)
    bnez \reg, 1f
    ret
1:
    DELIVER_PENDING_EXCEPTION
.endm

#endif  // ART_RUNTIME_ARCH_RISCV64_ASM_SUPPORT_RISCV64_S_
