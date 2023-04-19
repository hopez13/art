%def header():
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

/*
 * This is a #include, not a %include, because we want the C pre-processor
 * to expand the macros into assembler assignment statements.
 */
#include "asm_support.h"
#include "arch/riscv64/asm_support_riscv64.S"

/**
 * RISC-V 64 ABI general notes
 *
 * References
 * - https://github.com/riscv-non-isa/riscv-elf-psabi-doc/blob/master/riscv-cc.adoc
 * - runtime/arch/riscv64/registers_riscv64.h
 *
 * 32 general purposes registers
 * - fixed purpose: zero, ra, sp, gp, tp, s1
 *     gp/scs: shadow call stack - do not use!
 *     s1/tr: ART thread register - do not use!
 * - temporaries: t0-t6
 * - arguments: a0-a7
 * - return: a0, a1
 * - callee saved: ra, s0/fp, s2-s11
 *     s0 is flexible, available to use as a frame pointer if needed.
 *
 * 32 floating point registers
 * - temporaries: ft0-ft11
 * - arguments: fa0-fa7
 * - return: fa0, fa1
 * - callee saved: fs0-fs11
 */

// Fixed register usages in Nterp.
//    nickname  ABI    reg   purpose
#define xSELF    s1  // x9,   Thread* self pointer
#define xFP      s2  // x18,  interpreted frame pointer: to access locals and args
#define xPC      s3  // x19,  interpreted program counter: to fetch instructions
#define xINST    s4  // x20,  first 16-bit code unit of current instruction
#define xIBASE   s5  // x21,  interpreted instruction base pointer: for computed goto
#define xREFS    s6  // x22,  base of object references of dex registers

#define CFI_TMP  10  // DWARF register number for       a0/x10
#define CFI_DEX  19  // DWARF register number for xPC  /s3/x19
#define CFI_REFS 22  // DWARF register number for xREFS/s6/x22

// +8 bytes for the ArtMethod* of the caller.
#define OFFSET_TO_FIRST_ARGUMENT_IN_STACK (FRAME_SIZE_SAVE_ALL_CALLEE_SAVES + 8)

// An assembly entry that has a OatQuickMethodHeader prefix.
.macro OAT_ENTRY name, end
    .type \name, @function
    .hidden \name
    .global \name
    .balign 16
    // Padding of 3 * 4 bytes to get 16 bytes alignment of code entry.
    .4byte 0, 0, 0
    // OatQuickMethodHeader `data_` field. Note that the top two bits must be clear.
    .4byte (\end - \name)
\name:
.endm

.macro SIZE name
    .size \name, .-\name
.endm

// Similar to ENTRY but without the CFI directives.
.macro NAME_START name
    .type \name, @function
    .hidden \name  // Hide this as a global symbol, so we do not incur plt calls.
    .global \name
    /* Cache alignment for function entry */
    .balign 16
\name:
.endm

.macro NAME_END name
  SIZE \name
.endm

// Macro for defining entrypoints into runtime. We don't need to save registers
// (we're not holding references there), but there is no
// kDontSave runtime method. So just use the kSaveRefsOnly runtime method.
.macro NTERP_TRAMPOLINE name, helper
ENTRY \name
    SETUP_SAVE_REFS_ONLY_FRAME
    jal \helper
    RESTORE_SAVE_REFS_ONLY_FRAME
    lw t0, THREAD_EXCEPTION_OFFSET(xSELF)
    bnez t0, nterp_deliver_pending_exception
    ret
END \name
.endm

// Unpack code items from dex format.
// Input: \code_item
// Output:
//   - \registers: register count
//   - \outs: out count
//   - \ins: in count
//   - \code_item: holds instruction array on exit
// Clobbers: t0
.macro FETCH_CODE_ITEM_INFO code_item, registers, outs, ins
    // Check LSB of \code_item. If 1, it's a compact dex file.
    andi t0, \code_item, 0x1
    beqz t0, 1f  // Regular dex.
    unimp  // Compact dex: unimplemented.
1:
    // Unpack values from regular dex format.
    lh \registers, CODE_ITEM_REGISTERS_SIZE_OFFSET(\code_item)
    lh \outs, CODE_ITEM_OUTS_SIZE_OFFSET(\code_item)
    lh \ins, CODE_ITEM_INS_SIZE_OFFSET(\code_item)
    addi \code_item, \code_item, CODE_ITEM_INSNS_OFFSET
.endm

// Set up the stack to start executing the method.
// See runtime/nterp_helpers.cc for a diagram of the setup.
// Input:
//   - a0: ArtMethod*
//   - sp
//   - \code_item: CodeItem*
//   - \cfi_refs: DWARF register number for \refs
// Output:
//   - \code_item: pointer to instruction array `insns_*` on exit
//   - \refs: pointer to obj reference array
//   - \fp: pointer to dex register array
//   - \regs: count of dex registers
//   - \ins: count of in-registers
//   - \old_sp: old stack pointer
//   - sp modified
//
// Clobbers: t0, t1
.macro SETUP_STACK_FRAME code_item, cfi_refs, refs, fp, regs, ins, old_sp
    FETCH_CODE_ITEM_INFO \code_item, \regs, /*out count*/ t1, \ins

    // Compute required frame size: ((2 * \regs) + t1) * 4 + 24
    // - The register array and reference array are each |\regs| in length.
    // - The out array is |t1| in length.
    // - Each register is 4 bytes.
    // - Additional 24 bytes for 3 fields: saved frame pointer, dex pc, and ArtMethod*.
    sll t0, \regs, 1
    add t0, t0, t1
    sll t0, t0, 2
    add t0, t0, 24

    // Compute new stack pointer in t0.
    sub t0, sp, t0
    // 16-byte alignment.
    andi t0, t0, -0x10

    // Set \refs to base of reference array. Align to pointer size for the frame pointer and
    // dex pc pointer, below the reference array.
    sll t1, t1, 2  // 4 bytes per entry.
    add t1, t0, t1
    add t1, t1, 28  // 24 bytes from 3 fields mentioned earlier, plus 4 for alignment slack.
    andi \refs, t1, -__SIZEOF_POINTER__
    // t1 now free

    // Set \fp to base of register array, above the reference array. This region is already aligned.
    sll t1, \regs, 2
    add \fp, \refs, t1

    // Set up the stack pointer.
    mv \old_sp, sp
    .cfi_def_cfa_register \old_sp
    mv sp, t0
    // t0 now free
    sw \old_sp, -8(\refs)
    CFI_DEF_CFA_BREG_PLUS_UCONST \cfi_refs, -8, FRAME_SIZE_SAVE_ALL_CALLEE_SAVES

    // Put nulls in reference array.
    beqz \regs, 2f
    mv t1, \refs  // t1 as iterator
1:
    // Write in 8-byte increments, so vreg(0) gets zero'ed too, if \regs is odd.
    sd zero, (t1)
    addi t1, t1, 8
    blt t1, \fp, 1b
2:
    // Save the ArtMethod*.
    sd a0, (sp)
.endm

// Set up method arguments in register and reference arrays.
// Input:
//   - arg: method argument
//   - regs_base: pointer to dex register array
//   - refs_base: pointer to obj reference array
//   - ins_count: count of in registers
//   - arg_offset: arg's offset into regs and refs arrays
//   - finished: destination after final argument
// Output:
//   - register array modified
//   - reference array modified
//   - ins_count decremented
//   - arg_offset incremented
// Clobbers: t0
.macro SETUP_REFERENCE_PARAMETER_IN_GPR arg, regs_base, refs_base, ins_count, arg_offset, finished
    add t0, \arg_offset, \regs_base
    sw \arg, (t0)
    add t0, \arg_offset, \refs_base
    sw \arg, (t0)
    addi \ins_count, \ins_count, -1
    addi \arg_offset, \arg_offset, 4
    beqz \ins_count, \finished
.endm

// Set up remaining method arguments by reading from the stack.
// Input:
//   - regs_base: pointer to dex register array
//   - refs_base: pointer to obj reference array
//   - ins_count: count of in registers
//   - arg_offset: arg's offset into regs and refs arrays
//   - stack_ptr: pointer to stack-located arguments
// Output:
//   - register array modified
//   - reference array modified
//   - ins_count decremented to 0
//   - arg_offset incremented
// Clobbers: t0, t4
.macro SETUP_REFERENCE_PARAMETERS_IN_STACK regs_base, refs_base, ins_count, arg_offset, stack_ptr
1:
    add t0, \arg_offset, \stack_ptr
    lw t4, (t0)
    addi \ins_count, \ins_count, -1
    add t0, \arg_offset, \regs_base
    sw t4, (t0)
    add t0, \arg_offset, \refs_base
    sw t4, (t0)
    addi \arg_offset, \arg_offset, 4
    bnez \ins_count, 1b
.endm

// Place next int/long/object parameter passed in physical register
// into the dex register array, and in case of an object reference,
// into the reference array. Exit after one parameter is processed.
// Input:
//   - gpr: argument
//   - shorty: pointer to C string
//   - regs_base: pointer to dex register array
//   - refs_base: pointer to obj reference array
//   - arg_offset: starting offset into regs and refs array
//   - finished: destination after finishing shorty
// Output:
//   - shorty: pointer incremented
//   - arg_offset: value incremented
// Clobbers: t4, t5, t6
.macro LOOP_OVER_SHORTY_STORING_GPRS gpr, shorty, regs_base, refs_base, arg_offset, finished
1:  // LOOP
    lb t4, (\shorty)          // t4 holds next character from shorty.
    addi \shorty, \shorty, 1  // Increment char ptr.
    beqz t4, \finished        // if (t4 == '\0') goto finished
    addi t5, t4, -74          // if (t4 == 'J') goto STORE_8
    beqz t5, 3f
    addi t6, t4, -70          // if (t4 == 'F') goto SKIP_4
    beqz t6, 4f
    addi t5, t4, -68          // if (t4 == 'D') goto SKIP_8
    beqz t5, 5f

    // STORE_4
    add t6, \regs_base, \arg_offset
    sw \gpr, (t6)
    addi t5, t4, -76          // if (t4 == 'L') goto INC_4_EXIT
    bnez t5, 2f

    // STORE_4 REFERENCE
    add t6, \refs_base, \arg_offset
    sw \gpr, (t6)
    // fall through to INC_4_EXIT
2:  // INC_4_EXIT
    addi \arg_offset, \arg_offset, 4
    j 6f
3:  // STORE_8
    add t6, \regs_base, \arg_offset
    sd \gpr, (t6)
    addi \arg_offset, \arg_offset, 8
    j 6f
4:  // SKIP_4
    addi \arg_offset, \arg_offset, 4
    j 1b
5:  // SKIP_8
    addi \arg_offset, \arg_offset, 8
    j 1b
6:
.endm

// Clobbers: t4, t5, t6, a1 - we have already read out a1
.macro LOOP_OVER_STACK_GPRS shorty, regs_base, refs_base, arg_offset, stack_ptr, finished
1:  // LOOP
    lb t4, (\shorty)          // t4 holds next character from shorty.
    addi \shorty, \shorty, 1  // Increment char ptr.
    beqz t4, \finished        // if (t4 == '\0') goto finished
    addi t5, t4, -70          // if (t4 == 'F') goto SKIP_4
    beqz t5, 2f
    addi t5, t4, -68          // if (t4 == 'D') goto SKIP_8
    beqz t5, 4f

    add a1, \stack_ptr, \arg_offset
    addi a1, a1, OFFSET_TO_FIRST_ARGUMENT_IN_STACK
    add t5, \regs_base, \arg_offset

    addi t5, t4, -74          // if (t4 == 'J') goto STORE_8
    beqz t5, 3f

    // STORE_4
    lw a1, (a1)
    sw a1, (t5)
    addi t6, t4, -76          // if (t4 != 'L') goto SKIP_4
    bnez t6, 2f

    // STORE_4 REFERENCE
    add t5, \refs_base, \arg_offset
    sw a1, (t5)
    // fall through to SKIP_4
2:  // SKIP_4
    addi \arg_offset, \arg_offset, 4
    j 1b
3:  // STORE_8
    ld a1, (a1)
    sd a1, (t5)
    // fall through to SKIP_8
4:  // SKIP_8
    addi \arg_offset, \arg_offset, 8
    j 1b
.endm

// Process one FP argument, exit.
// Clobbers: t4, t5, t6
.macro LOOP_OVER_SHORTY_STORING_FPS fpr, shorty, regs_base, arg_offset, finished
1:  // LOOP
    lb t4, (\shorty)          // t4 holds next character from shorty.
    addi \shorty, \shorty, 1  // Increment char ptr.
    beqz t4, \finished        // if (t4 == '\0') goto finished
    addi t5, t4, -70          // if (t4 == 'F') goto STORE_4
    beqz t5, 2f
    addi t5, t4, -68          // if (t4 == 'D') goto STORE_8
    beqz t5, 3f

    addi \arg_offset, \arg_offset, 4
    // Handle extra argument in arg array, taken by a long.
    addi t5, t4, -74          // if (t4 != 'J') goto LOOP
    bnez t5, 1b
    addi \arg_offset, \arg_offset, 4
    j 1b                      // goto LOOP
2:  // STORE_4
    add t6, \regs_base, \arg_offset
    fsw \fpr, (t6)
    addi \arg_offset, \arg_offset, 4
    j 4f
3:  // STORE_8
    add t6, \regs_base, \arg_offset
    fsd \fpr, (t6)
    addi \arg_offset, \arg_offset, 8
4:
.endm

// Clobbers: t4, t5, t6, ft0
.macro LOOP_OVER_STACK_FPS shorty, regs_base, arg_offset, stack_ptr, finished
1:  // LOOP
    lb t4, (\shorty)          // t4 holds next character from shorty.
    addi \shorty, \shorty, 1  // Increment char ptr.
    beqz t4, \finished        // if (t4 == '\0') goto finished

    addi t5, t4, -70          // if (t4 == 'F') goto STORE_4
    beqz t5, 2f
    addi t5, t4, -68          // if (t4 == 'D') goto STORE_8
    beqz t5, 3f

    // SKIP_4
    addi \arg_offset, \arg_offset, 4
    addi t5, t4, -74          // if (r4 != 'J') goto LOOP
    bnez t5, 1b

    // SKIP_ANOTHER_4
    add \arg_offset, \arg_offset, 4
    j 1b                      // goto LOOP

2:  // STORE_4
    add t6, \stack_ptr, \arg_offset
    addi t6, t6, OFFSET_TO_FIRST_ARGUMENT_IN_STACK
    flw ft0, (t6)
    add t5, \regs_base, \arg_offset
    addi \arg_offset, \arg_offset, 4
    fsw ft0, (t5)
    j 1b
3:  // STORE_8
    add t6, \stack_ptr, \arg_offset
    addi t6, t6, OFFSET_TO_FIRST_ARGUMENT_IN_STACK
    fld ft0, (t6)
    add t5, \regs_base, \arg_offset
    addi \arg_offset, \arg_offset, 8
    fsd ft0, (t5)
    j 1b
.endm

.macro EXPORT_PC
    sd xPC, -16(xREFS)
.endm

.macro DO_SUSPEND_CHECK continue
    addi t0, xSELF, THREAD_FLAGS_OFFSET
    lw t0, (t0)
    addi t0, t0, -THREAD_SUSPEND_OR_CHECKPOINT_REQUEST
    beqz t0, \continue
    EXPORT_PC
    jal art_quick_test_suspend
.endm

// Fetch the next instruction, from xPC into xINST.
// Does not advance xPC.
.macro FETCH_INST
    lh xINST, (xPC)
.endm

// Fetch the next instruction, from xPC into xINST.
// Advances xPC by \count units; one \count is 2 bytes.
//
// Immediates are limited to a 12-bit offset range from xPC.
// Thus, \count can range from -1024 to 1023.
//
// XXX still true?
// Must be placed AFTER anything that can throw an exception,
// or the exception catch may miss. Thus, this macro must be
// place after EXPORT_PC too.
.macro FETCH_ADVANCE_INST count
    lh xINST, (\count*2)(xPC)
    addi xPC, xPC, (\count*2)
.endm

// Clobbers: \reg
.macro GET_INST_OPCODE reg
    and \reg, xINST, 0xFF
.endm

// Clobbers: \reg
.macro GOTO_OPCODE reg
    srliw \reg, \reg, ${handler_size_bits}
    add \reg, xIBASE, \reg
    jr \reg
.endm

// Inputs:
//   - a0
//   - xSELF
// Clobbers: t0
.macro CHECK_AND_UPDATE_SHARED_MEMORY_METHOD if_hot, if_not_hot
    lw t0, ART_METHOD_ACCESS_FLAGS_OFFSET(a0)
    srliw t0, t0, ART_METHOD_IS_MEMORY_SHARED_FLAG_BIT
    andi t0, t0, 0x1
    beqz t0, \if_hot

    lw t0, THREAD_SHARED_METHOD_HOTNESS_OFFSET(xSELF)
    beqz t0, \if_hot

    addi t0, t0, -1  // Reduce hotness
    sw t0,  THREAD_SHARED_METHOD_HOTNESS_OFFSET(xSELF)
    j \if_not_hot
.endm

// Increase method hotness before starting the method.
// Clobbers: a0, t0
.macro START_EXECUTING_INSTRUCTIONS
    ld a0, (sp)
    addi t0, a0, ART_METHOD_HOTNESS_COUNT_OFFSET
    lh t1, (t0)
#if (NTERP_HOTNESS_VALUE != 0)
#error Expected 0 for hotness value
#endif
    // If the counter is at zero, handle it in the runtime.
    beqz t1, 3f
    addi t1, t1, -1
    sh t1, (t0)
1:
    DO_SUSPEND_CHECK continue=2f
2:
    FETCH_INST
    GET_INST_OPCODE t0
    GOTO_OPCODE t0
3:
    CHECK_AND_UPDATE_SHARED_MEMORY_METHOD if_hot=4f, if_not_hot=1b
4:
    mv a1, zero  // dex_pc_ptr=nullptr
    mv a2, zero  // vergs=nullptr
    jal nterp_hot_method
.endm

%def entry():
/*
 * ArtMethod entry point.
 *
 * On entry:
 *  a0   ArtMethod* callee
 *  a1-a7  method parameters
 */

OAT_ENTRY ExecuteNterpWithClinitImpl, EndExecuteNterpWithClinitImpl
    // For simplicity, we don't do a read barrier here, but instead rely
    // on art_quick_resolution_trampoline to always have a suspend point before
    // calling back here.
    unimp
EndExecuteNterpWithClinitImpl:

OAT_ENTRY ExecuteNterpImpl, EndExecuteNterpImpl
    .cfi_startproc

    // Check guard page for stack overflow.
    li t0, -STACK_OVERFLOW_RESERVED_BYTES
    add t0, t0, sp
    ld zero, (t0)

    INCREASE_FRAME FRAME_SIZE_SAVE_ALL_CALLEE_SAVES
    SAVE_ALL_CALLEE_SAVES

    ld xPC, ART_METHOD_DATA_OFFSET_64(a0)
    SETUP_STACK_FRAME xPC, CFI_REFS, xREFS, xFP, /*reg count*/ s7, /*in count*/s8, /*old sp*/s9

    beqz s8, .Lsetup_execution  // no args

    // Set up the arguments.
    sub t0, s7, s8
    // s10 holds method bit flags.
    lw s10, ART_METHOD_ACCESS_FLAGS_OFFSET(a0)
    // s11 holds offset for inputs into the register array.
    sll s11, t0, 2

    add t1, xFP, s11  // t1 holds pointer to arguments in register array.
    add t2, xREFS, s11  // t2 holds pointer to arguments in reference array.

    srliw t0, s10, ART_METHOD_NTERP_ENTRY_POINT_FAST_PATH_FLAG_BIT
    andi t0, t0, 0x1
    beqz t0, .Lsetup_slow_path

    mv t3, zero  // t3 used as offset accumulator.
    SETUP_REFERENCE_PARAMETER_IN_GPR a1, /*regs*/t1, /*refs*/t2, /*ins_count*/s8, /*arg_offset*/t3, .Lsetup_execution
    SETUP_REFERENCE_PARAMETER_IN_GPR a2, /*regs*/t1, /*refs*/t2, /*ins_count*/s8, /*arg_offset*/t3, .Lsetup_execution
    SETUP_REFERENCE_PARAMETER_IN_GPR a3, /*regs*/t1, /*refs*/t2, /*ins_count*/s8, /*arg_offset*/t3, .Lsetup_execution
    SETUP_REFERENCE_PARAMETER_IN_GPR a4, /*regs*/t1, /*refs*/t2, /*ins_count*/s8, /*arg_offset*/t3, .Lsetup_execution
    SETUP_REFERENCE_PARAMETER_IN_GPR a5, /*regs*/t1, /*refs*/t2, /*ins_count*/s8, /*arg_offset*/t3, .Lsetup_execution
    SETUP_REFERENCE_PARAMETER_IN_GPR a6, /*regs*/t1, /*refs*/t2, /*ins_count*/s8, /*arg_offset*/t3, .Lsetup_execution
    SETUP_REFERENCE_PARAMETER_IN_GPR a7, /*regs*/t1, /*refs*/t2, /*ins_count*/s8, /*arg_offset*/t3, .Lsetup_execution
    // If more than 7 method args, continue on stack.
    addi s9, s9, OFFSET_TO_FIRST_ARGUMENT_IN_STACK
    SETUP_REFERENCE_PARAMETERS_IN_STACK /*regs*/t1, /*refs*/t2, /*ins_count*/s8, /*arg_offset*/t3, /*stack_ptr*/s9
    j .Lsetup_execution

.Lsetup_slow_path:
    // Static method: skip `this` setup.
    srliw t0, s10, ART_METHOD_IS_STATIC_FLAG_BIT
    andi t0, t0, 0x1
    bnez t0, .Lsetup_with_shorty

    sw a1, (t1)
    sw a1, (t2)

    // Non-static method with one argument ('this'): skip shorty.
    addi t0, s8, -1
    beqz t0, .Lsetup_execution

.Lsetup_with_shorty:
    // TODO: Better way to get shorty
    // Spill args, call NterpGetShorty, restore args.
    addi sp, sp, -128
    sd   a0,  0(sp)
    sd   a1,  8(sp)
    sd   a2, 16(sp)
    sd   a3, 24(sp)
    sd   a4, 32(sp)
    sd   a5, 40(sp)
    sd   a6, 48(sp)
    sd   a7, 56(sp)
    fsd fa0, 64(sp)
    fsd fa1, 72(sp)
    fsd fa2, 80(sp)
    fsd fa3, 88(sp)
    fsd fa4, 96(sp)
    fsd fa5, 104(sp)
    fsd fa6, 112(sp)
    fsd fa7, 120(sp)
    jal NterpGetShorty
    mv xIBASE, a0  // Return value
    ld   a0,  0(sp)
    ld   a1,  8(sp)
    ld   a2, 16(sp)
    ld   a3, 24(sp)
    ld   a4, 32(sp)
    ld   a5, 40(sp)
    ld   a6, 48(sp)
    ld   a7, 56(sp)
    fld fa0, 64(sp)
    fld fa1, 72(sp)
    fld fa2, 80(sp)
    fld fa3, 88(sp)
    fld fa4, 96(sp)
    fld fa5, 104(sp)
    fld fa6, 112(sp)
    fld fa7, 120(sp)
    addi sp, sp, 128
    // Restored: sp, a0-a7, fa0-fa7, s0, s2-s11, fs0-fs11

    add t1, xFP, s11  // t1 holds pointer to arguments in register array.
    add t2, xREFS, s11  // t2 holds pointer to arguments in reference array.
    mv t3, zero  // t3 used as offset accumulator.

    addi t0, xIBASE, 1  // shorty + 1  ; ie skip return arg character

    srliw t4, s10, ART_METHOD_IS_STATIC_FLAG_BIT
    andi t4, t4, 0x1
    bnez t4, .Lsetup_static_method

    addi t1, t1, 4
    addi t2, t2, 4
    addi s9, s9, 4
    j .Lsetup_gprs

.Lsetup_static_method:
    LOOP_OVER_SHORTY_STORING_GPRS /*gpr*/a1, /*shorty*/t0, /*regs*/t1, /*refs*/t2, /*arg_offset*/t3, .Lsetup_fps
.Lsetup_gprs:
    LOOP_OVER_SHORTY_STORING_GPRS /*gpr*/a2, /*shorty*/t0, /*regs*/t1, /*refs*/t2, /*arg_offset*/t3, .Lsetup_fps
    LOOP_OVER_SHORTY_STORING_GPRS /*gpr*/a3, /*shorty*/t0, /*regs*/t1, /*refs*/t2, /*arg_offset*/t3, .Lsetup_fps
    LOOP_OVER_SHORTY_STORING_GPRS /*gpr*/a4, /*shorty*/t0, /*regs*/t1, /*refs*/t2, /*arg_offset*/t3, .Lsetup_fps
    LOOP_OVER_SHORTY_STORING_GPRS /*gpr*/a5, /*shorty*/t0, /*regs*/t1, /*refs*/t2, /*arg_offset*/t3, .Lsetup_fps
    LOOP_OVER_SHORTY_STORING_GPRS /*gpr*/a6, /*shorty*/t0, /*regs*/t1, /*refs*/t2, /*arg_offset*/t3, .Lsetup_fps
    LOOP_OVER_SHORTY_STORING_GPRS /*gpr*/a7, /*shorty*/t0, /*regs*/t1, /*refs*/t2, /*arg_offset*/t3, .Lsetup_fps
    LOOP_OVER_STACK_GPRS /*shorty*/t0, /*regs*/t1, /*refs*/t2, /*arg_offset*/t3, /*stack_ptr*/s9, .Lsetup_fps
.Lsetup_fps:
    addi t0, xIBASE, 1  // shorty + 1  ; ie skip return arg character
    mv t3, zero  // Reset offset accumulator.
    LOOP_OVER_SHORTY_STORING_FPS /*fpr*/fa0, /*shorty*/t0, /*regs*/t1, /*arg_offset*/t3, .Lsetup_execution
    LOOP_OVER_SHORTY_STORING_FPS /*fpr*/fa1, /*shorty*/t0, /*regs*/t1, /*arg_offset*/t3, .Lsetup_execution
    LOOP_OVER_SHORTY_STORING_FPS /*fpr*/fa2, /*shorty*/t0, /*regs*/t1, /*arg_offset*/t3, .Lsetup_execution
    LOOP_OVER_SHORTY_STORING_FPS /*fpr*/fa3, /*shorty*/t0, /*regs*/t1, /*arg_offset*/t3, .Lsetup_execution
    LOOP_OVER_SHORTY_STORING_FPS /*fpr*/fa4, /*shorty*/t0, /*regs*/t1, /*arg_offset*/t3, .Lsetup_execution
    LOOP_OVER_SHORTY_STORING_FPS /*fpr*/fa5, /*shorty*/t0, /*regs*/t1, /*arg_offset*/t3, .Lsetup_execution
    LOOP_OVER_SHORTY_STORING_FPS /*fpr*/fa6, /*shorty*/t0, /*regs*/t1, /*arg_offset*/t3, .Lsetup_execution
    LOOP_OVER_SHORTY_STORING_FPS /*fpr*/fa7, /*shorty*/t0, /*regs*/t1, /*arg_offset*/t3, .Lsetup_execution
    LOOP_OVER_STACK_FPS /*shorty*/t0, /*regs*/t1, /*arg_offset*/t3, /*stack_ptr*/s9, .Lsetup_execution
.Lsetup_execution:
    CFI_DEFINE_DEX_PC_WITH_OFFSET(/*tmpReg*/CFI_TMP, /*dexReg*/CFI_DEX, /*dexOffset*/0)

    la xIBASE, artNterpAsmInstructionStart
    START_EXECUTING_INSTRUCTIONS
    // NOTE: no fallthrough
    // cfi info continues, and covers the whole nterp implementation.
    SIZE ExecuteNterpImpl

%def fetch_from_thread_cache(dest_reg, miss_label):

%def footer():
/*
 * ===========================================================================
 *  Common subroutines and data
 * ===========================================================================
 */

    .text
    .align  2


// Enclose all code below in a symbol (which gets printed in backtraces).
NAME_START nterp_helper
// This is the logical end of ExecuteNterpImpl, where the frame info applies.
// EndExecuteNterpImpl includes the methods below as we want the runtime to
// see them as part of the Nterp PCs.
.cfi_endproc
NAME_END nterp_helper

// This is the end of PCs contained by the OatQuickMethodHeader created for the interpreter
// entry point.
    .type EndExecuteNterpImpl, @function
    .hidden EndExecuteNterpImpl
    .global EndExecuteNterpImpl
EndExecuteNterpImpl:

// Entrypoints into runtime.
NTERP_TRAMPOLINE nterp_hot_method, NterpHotMethod

ENTRY nterp_deliver_pending_exception
    DELIVER_PENDING_EXCEPTION
END nterp_deliver_pending_exception

// gen_mterp.py will inline the following definitions
// within (ExecuteNterpImpl, EndExecuteNterpImpl).
%def instruction_start():
    .type artNterpAsmInstructionStart, @function
    .hidden artNterpAsmInstructionStart
    .global artNterpAsmInstructionStart
artNterpAsmInstructionStart = .L_op_nop
    .text

%def instruction_end():
    .type artNterpAsmInstructionEnd, @function
    .hidden artNterpAsmInstructionEnd
    .global artNterpAsmInstructionEnd
artNterpAsmInstructionEnd:
    unimp

%def opcode_pre():
%   pass
%def opcode_name_prefix():
%   return "nterp_"
%def opcode_start():
    NAME_START nterp_${opcode}
%def opcode_end():
    NAME_END nterp_${opcode}
    unimp
%def opcode_slow_path_start(name):
    NAME_START ${name}
%def opcode_slow_path_end(name):
    NAME_END ${name}
