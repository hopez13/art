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

/*
 * Refresh rINST.
 * At enter to handler rINST does not contain the opcode number.
 * However some utilities require the full value, so this macro
 * restores the opcode number.
 */
.macro REFRESH_INST _opnum
    unimp
.endm

/*
 * Fetch the next instruction from rPC into rINSTw.  Does not advance rPC.
 */
.macro FETCH_INST
    unimp
.endm

/*
 * Remove opcode from rINST, compute the address of handler and jump to it.
 */
.macro GOTO_NEXT
    unimp
.endm

/*
 * Advance rPC by instruction count.
 */
.macro ADVANCE_PC _count
    unimp
.endm

/*
 * Advance rPC by instruction count, fetch instruction and jump to handler.
 */
.macro ADVANCE_PC_FETCH_AND_GOTO_NEXT _count
    unimp
.endm

.macro GET_VREG _reg _vreg
    unimp
.endm

.macro GET_VREG_OBJECT _reg _vreg
    unimp
.endm

/* Read wide value. */
.macro GET_WIDE_VREG _reg _vreg
    unimp
.endm

.macro SET_VREG _reg _vreg
    unimp
.endm

/* Write wide value. reg is clobbered. */
.macro SET_WIDE_VREG _reg _vreg
    unimp
.endm

.macro SET_VREG_OBJECT _reg _vreg
    unimp
.endm

.macro GET_VREG_HIGH _reg _vreg
    unimp
.endm

.macro SET_VREG_HIGH _reg _vreg
    unimp
.endm

.macro CLEAR_REF _vreg
    unimp
.endm

.macro CLEAR_WIDE_REF _vreg
    unimp
.endm

.macro GET_VREG_XMMs _xmmreg _vreg
    unimp
.endm
.macro GET_VREG_XMMd _xmmreg _vreg
    movsd VREG_ADDRESS(\_vreg), \_xmmreg
.endm

.macro SET_VREG_XMMs _xmmreg _vreg
    unimp
.endm

.macro SET_VREG_XMMd _xmmreg _vreg
    unimp
.endm

// An assembly entry that has a OatQuickMethodHeader prefix.
.macro OAT_ENTRY name, end
    .type \name, @function
    .hidden \name
    .global \name
    .balign 16
		unimp
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
    /* XXX Cache alignment for function entry */
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
    unimp
.endm

.macro CLEAR_VOLATILE_MARKER reg
    unimp
.endm

.macro EXPORT_PC
    unimp
.endm


.macro BRANCH
    unimp
.endm

// Expects:
// - XXX
// Outputs:
// - \registers contains the dex registers size
// - \outs contains the outs size
// - if load_ins is 1, \ins contains the ins
// - \code_item is replace with a pointer to the instructions
.macro FETCH_CODE_ITEM_INFO code_item, registers, outs, ins, load_ins
    unimp
.endm

// Setup the stack to start executing the method. Expects:
// - XXX
//
// Outputs
// - XXX
//.macro SETUP_STACK_FRAME code_item, refs, refs32, fp, cfi_refs, load_ins
.macro SETUP_STACK_FRAME code_item, refs, refs32, fp, load_ins
    unimp
.endm

// Puts the next floating point argument into the expected register,
// fetching values based on a non-range invoke.
// Uses XXX as temporary.
//
// TODO: XXX We could simplify a lot of code by loading the G argument into
// the "inst" register. Given that we enter the handler with "1(rPC)" in
// the rINST, we can just add rINST<<16 to the args and we don't even
// need to pass "arg_index" around.
.macro LOOP_OVER_SHORTY_LOADING_XMMS xmm_reg, inst, shorty, arg_index, finished
    unimp
.endm

// Puts the next int/long/object argument in the expected register,
// fetching values based on a non-range invoke.
// Uses XXX as temporary.
.macro LOOP_OVER_SHORTY_LOADING_GPRS gpr_reg64, gpr_reg32, inst, shorty, arg_index, finished
    unimp
.endm

// Puts the next floating point argument into the expected register,
// fetching values based on a range invoke.
// Uses XXX as temporary.
.macro LOOP_RANGE_OVER_SHORTY_LOADING_XMMS xmm_reg, shorty, arg_index, stack_index, finished
    unimp
.endm

// Puts the next floating point argument into the expected stack slot,
// fetching values based on a range invoke.
// Uses XXX as temporary.
//
// TODO: We could just copy all the vregs to the stack slots in a simple loop
// (or XXX) without looking at the shorty at all. (We could also drop
// the "stack_index" from the macros for loading registers.) We could also do
// that conditionally if argument word count > XXX; otherwise we know that all
// args fit into registers.
.macro LOOP_RANGE_OVER_FPs shorty, arg_index, stack_index, finished
    unimp
.endm

// Puts the next int/long/object argument in the expected register,
// fetching values based on a range invoke.
// Uses XXX as temporary.
.macro LOOP_RANGE_OVER_SHORTY_LOADING_GPRS gpr_reg64, gpr_reg32, shorty, arg_index, stack_index, finished
    unimp
.endm

// Puts the next int/long/object argument in the expected stack slot,
// fetching values based on a range invoke.
// Uses XXX as temporary.
.macro LOOP_RANGE_OVER_INTs shorty, arg_index, stack_index, finished
    unimp
.endm

// Puts the next floating point parameter passed in physical register
// in the expected dex register array entry.
// Uses XXX as temporary.
.macro LOOP_OVER_SHORTY_STORING_XMMS xmm_reg, shorty, arg_index, fp, finished
    unimp
.endm

// Puts the next int/long/object parameter passed in physical register
// in the expected dex register array entry, and in case of object in the
// expected reference array entry.
// Uses XXX as temporary.
.macro LOOP_OVER_SHORTY_STORING_GPRS gpr_reg64, gpr_reg32, shorty, arg_index, regs, refs, finished
    unimp
.endm

// Puts the next floating point parameter passed in stack
// in the expected dex register array entry.
// Uses XXX as temporary.
//
// TODO XXX: Or we could just spill regs to the reserved slots in the caller's
// frame and copy all regs in a simple loop. This time, however, we would
// need to look at the shorty anyway to look for the references.
// (The trade-off is different for passing arguments and receiving them.)
.macro LOOP_OVER_FPs shorty, arg_index, regs, stack_ptr, finished
    unimp
.endm

// Puts the next int/long/object parameter passed in stack
// in the expected dex register array entry, and in case of object in the
// expected reference array entry.
// Uses XXX as temporary.
.macro LOOP_OVER_INTs shorty, arg_index, regs, refs, stack_ptr, finished
    unimp
.endm

// Increase method hotness and do suspend check before starting executing the method.
.macro START_EXECUTING_INSTRUCTIONS
    unimp
.endm

.macro SPILL_ALL_CALLEE_SAVES
    unimp
.endm

.macro RESTORE_ALL_CALLEE_SAVES
    unimp
.endm

// Helper to setup the stack after doing a nterp to nterp call. This will setup:
// - rNEW_FP: the new pointer to dex registers
// - rNEW_REFS: the new pointer to references
// - rPC: the new PC pointer to execute
// - XXX: number of arguments
// - XXX: first dex register
//
// This helper expects:
// - XXX to contain the code item
.macro SETUP_STACK_FOR_INVOKE
    unimp
.endm

// Setup arguments based on a non-range nterp to nterp call, and start executing
// the method. We expect:
// - rNEW_FP: the new pointer to dex registers
// - rNEW_REFS: the new pointer to references
// - rPC: the new PC pointer to execute
// - XXX: number of arguments
// - XXX: first dex register
// - XXX: top of dex register array
// - XXX: receiver if non-static.
.macro SETUP_NON_RANGE_ARGUMENTS_AND_EXECUTE is_static=0, is_string_init=0
    unimp
.endm

// Setup arguments based on a range nterp to nterp call, and start executing
// the method.
.macro SETUP_RANGE_ARGUMENTS_AND_EXECUTE is_static=0, is_string_init=0
    unimp
.endm

.macro GET_SHORTY dest, is_interface, is_polymorphic, is_custom
    unimp
.endm

.macro GET_SHORTY_SLOW_PATH dest, is_interface
    unimp
.endm

// Uses XXX as temporary.
.macro DO_ENTRY_POINT_CHECK call_compiled_code
    unimp
.endm

// Uses XXX and XXX as temporary
.macro UPDATE_REGISTERS_FOR_STRING_INIT old_value, new_value
    unimp
.endm

.macro COMMON_INVOKE_NON_RANGE is_static=0, is_interface=0, suffix="", is_string_init=0, is_polymorphic=0, is_custom=0
    unimp
.endm

.macro COMMON_INVOKE_RANGE is_static=0, is_interface=0, suffix="", is_string_init=0, is_polymorphic=0, is_custom=0
    unimp
.endm

// Helper for static field get.
.macro OP_SGET load="movl", wide="0"
    unimp
.endm

// Helper for static field put.
.macro OP_SPUT rINST_reg="rINST", store="movl", wide="0":
    unimp
.endm


.macro OP_IPUT_INTERNAL rINST_reg="rINST", store="movl", wide="0":
    unimp
.endm

// Helper for instance field put.
.macro OP_IPUT rINST_reg="rINST", store="movl", wide="0":
    unimp
.endm

// Helper for instance field get.
.macro OP_IGET load="movl", wide="0"
    unimp
.endm

.macro SETUP_REFERENCE_PARAMETER_IN_GPR gpr32, regs, refs, ins, arg_offset, finished
    unimp
.endm

// Uses XXX as temporary
.macro SETUP_REFERENCE_PARAMETERS_IN_STACK regs, refs, ins, stack_ptr, arg_offset
    unimp
.endm

.macro CHECK_AND_UPDATE_SHARED_MEMORY_METHOD if_hot, if_not_hot
    unimp
.endm

.macro DO_SUSPEND_CHECK continue_label
    unimp
.endm

%def entry():
/*
 * ArtMethod entry point.
 *
 * On entry:
 *  XXX   ArtMethod* callee
 *  rest  method parameters
 */

OAT_ENTRY ExecuteNterpWithClinitImpl, EndExecuteNterpWithClinitImpl
    // For simplicity, we don't do a read barrier here, but instead rely
    // on art_quick_resolution_trampoline to always have a suspend point before
    // calling back here.
    unimp
EndExecuteNterpWithClinitImpl:

OAT_ENTRY ExecuteNterpImpl, EndExecuteNterpImpl
    .cfi_startproc
    unimp

%def opcode_pre():

%def fetch_from_thread_cache(dest_reg, miss_label):

%def footer():

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

// gen_mterp.py will inline the following definitions
// within [ExecuteNterpImpl, EndExecuteNterpImpl).
%def instruction_end():
		.type artNterpAsmInstructionEnd, @function
		.hidden artNterpAsmInstructionEnd
		.global artNterpAsmInstructionEnd
artNterpAsmInstructionEnd:
		unimp

%def instruction_start():
		.type artNterpAsmInstructionStart, @function
		.hidden artNterpAsmInstructionStart
		.global artNterpAsmInstructionStart
artNterpAsmInstructionStart = .L_op_nop
    .text

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

