/*
 * Copyright (C) 2012 The Android Open Source Project
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

#include "asm_support_x86_64.S"

    /*
     * Jni dlsym lookup stub.
     */
DEFINE_FUNCTION art_jni_dlsym_lookup_stub
    // Save callee and GPR args, mixed together to agree with core spills bitmap.
    PUSH r9   // Arg.
    PUSH r8   // Arg.
    PUSH rdi  // JniEnv.
    PUSH rsi  // Arg.
    PUSH rdx  // Arg.
    PUSH rcx  // Arg.
    // Create space for FPR args, plus padding for alignment
    subq LITERAL(72), %rsp
    CFI_ADJUST_CFA_OFFSET(72)
    // Save FPRs.
    movq %xmm0, 0(%rsp)
    movq %xmm1, 8(%rsp)
    movq %xmm2, 16(%rsp)
    movq %xmm3, 24(%rsp)
    movq %xmm4, 32(%rsp)
    movq %xmm5, 40(%rsp)
    movq %xmm6, 48(%rsp)
    movq %xmm7, 56(%rsp)
    // prepare call
    movq %gs:THREAD_SELF_OFFSET, %rdi      // RDI := Thread::Current()
    // Call artFindNativeMethod() for normal native and artFindNativeMethodRunnable()
    // for @FastNative or @CriticalNative.
    movq THREAD_TOP_QUICK_FRAME_OFFSET(%rdi), %rax   // uintptr_t tagged_quick_frame
    andq LITERAL(0xfffffffffffffffe), %rax           // ArtMethod** sp
    movq (%rax), %rax                                // ArtMethod* method
    testl LITERAL(ACCESS_FLAGS_METHOD_IS_FAST_NATIVE | ACCESS_FLAGS_METHOD_IS_CRITICAL_NATIVE), \
          ART_METHOD_ACCESS_FLAGS_OFFSET(%rax)
    jne .Llookup_stub_fast_native
    call SYMBOL(artFindNativeMethod)  // (Thread*)
    jmp .Llookup_stub_continue
.Llookup_stub_fast_native:
    call SYMBOL(artFindNativeMethodRunnable)  // (Thread*)
.Llookup_stub_continue:
    // restore arguments
    movq 0(%rsp), %xmm0
    movq 8(%rsp), %xmm1
    movq 16(%rsp), %xmm2
    movq 24(%rsp), %xmm3
    movq 32(%rsp), %xmm4
    movq 40(%rsp), %xmm5
    movq 48(%rsp), %xmm6
    movq 56(%rsp), %xmm7
    addq LITERAL(72), %rsp
    CFI_ADJUST_CFA_OFFSET(-72)
    POP rcx  // Arg.
    POP rdx  // Arg.
    POP rsi  // Arg.
    POP rdi  // JniEnv.
    POP r8   // Arg.
    POP r9   // Arg.
    testq %rax, %rax              // check if returned method code is null
    jz .Lno_native_code_found     // if null, jump to return to handle
    jmp *%rax                     // otherwise, tail call to intended method
.Lno_native_code_found:
    ret
END_FUNCTION art_jni_dlsym_lookup_stub

DEFINE_FUNCTION art_jni_dlsym_lookup_critical_stub
    // The hidden arg holding the tagged method (bit 0 set means GenericJNI) is r11.
    // For Generic JNI we already have a managed frame, so we reuse the art_jni_dlsym_lookup_stub.
    testl LITERAL(1), %r11d;
    jnz art_jni_dlsym_lookup_stub

    // UNIMPLEMENTED: CRASH
    movq LITERAL(0), %rax
    movq LITERAL(0), (%rax)
END_FUNCTION art_jni_dlsym_lookup_critical_stub
