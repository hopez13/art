/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef ART_RUNTIME_ARCH_MIPS_ENTRYPOINTS_DIRECT_MIPS_H_
#define ART_RUNTIME_ARCH_MIPS_ENTRYPOINTS_DIRECT_MIPS_H_

#include "entrypoints/quick/quick_entrypoints_enum.h"

namespace art {

/* Returns true if entrypoint contains direct reference to
   native implementation. The list is required as direct
   entrypoints need additional handling during invocation.

   Direct functions allocate space on the call stack to save
   the argument values passed into the function via registers
   $a0-$a3. These entry points directly call native implementations.
   The called functions (rather than the calling function) use this
   space to save registers $a0-$a3.*/
static constexpr bool IsDirectEntrypoint(QuickEntrypointEnum entrypoint) {
#pragma clang diagnostic push
#pragma clang diagnostic error "-Wswitch-enum"
#pragma clang diagnostic error "-Wswitch"

  // You must not add a "default" case to the following switch statement.
  // The switch statement acts as a compile time check. MIPS32 differentiates
  // between direct functions, and indirect. If new functions are added to
  // the definition of the macro QUICK_ENTRYPOINT_LIST in the file
  // runtime/entrypoints/quick/quick_entrypoints_list.h then compilation of the
  // switch statement in this function will fail. When that happens you will
  // need to add a case label for the enum value which corresponds to the new
  // function. The diagnostic message from the clang compiler will tell you
  // what that new enum value is. If the function is direct you'll need to add
  // the label before the statements "return true; break;" below. If the new
  // function is an indirect function the new case label must be added after
  // the "return true; break;" statements.
  //
  // After adding the case label here you will also need to add a new
  // static_assert statement to art::InitEntryPoints() in the file
  // runtime/arch/mips/entrypoints_init_mips.cc, which correctly asserts
  // whether the new function is direct, or indirect.
  switch (entrypoint) {
    case kQuickInstanceofNonTrivial:
    case kQuickA64Load:
    case kQuickA64Store:
    case kQuickFmod:
    case kQuickFmodf:
    case kQuickMemcpy:
    case kQuickL2d:
    case kQuickL2f:
    case kQuickD2iz:
    case kQuickF2iz:
    case kQuickD2l:
    case kQuickF2l:
    case kQuickLdiv:
    case kQuickLmod:
    case kQuickLmul:
    case kQuickCmpgDouble:
    case kQuickCmpgFloat:
    case kQuickCmplDouble:
    case kQuickCmplFloat:
    case kQuickReadBarrierJni:
    case kQuickReadBarrierSlow:
    case kQuickReadBarrierForRootSlow:
    case kQuickCos:
    case kQuickSin:
    case kQuickAcos:
    case kQuickAsin:
    case kQuickAtan:
    case kQuickAtan2:
    case kQuickCbrt:
    case kQuickCosh:
    case kQuickExp:
    case kQuickExpm1:
    case kQuickHypot:
    case kQuickLog:
    case kQuickLog10:
    case kQuickNextAfter:
    case kQuickSinh:
    case kQuickTan:
    case kQuickTanh:
      return true;
      break;
    case kQuickAllocArrayResolved:
    case kQuickAllocArrayResolved8:
    case kQuickAllocArrayResolved16:
    case kQuickAllocArrayResolved32:
    case kQuickAllocArrayResolved64:
    case kQuickAllocObjectResolved:
    case kQuickAllocObjectInitialized:
    case kQuickAllocObjectWithChecks:
    case kQuickAllocStringFromBytes:
    case kQuickAllocStringFromChars:
    case kQuickAllocStringFromString:
    case kQuickCheckInstanceOf:
    case kQuickInitializeStaticStorage:
    case kQuickInitializeTypeAndVerifyAccess:
    case kQuickInitializeType:
    case kQuickResolveString:
    case kQuickSet8Instance:
    case kQuickSet8Static:
    case kQuickSet16Instance:
    case kQuickSet16Static:
    case kQuickSet32Instance:
    case kQuickSet32Static:
    case kQuickSet64Instance:
    case kQuickSet64Static:
    case kQuickSetObjInstance:
    case kQuickSetObjStatic:
    case kQuickGetByteInstance:
    case kQuickGetBooleanInstance:
    case kQuickGetByteStatic:
    case kQuickGetBooleanStatic:
    case kQuickGetShortInstance:
    case kQuickGetCharInstance:
    case kQuickGetShortStatic:
    case kQuickGetCharStatic:
    case kQuickGet32Instance:
    case kQuickGet32Static:
    case kQuickGet64Instance:
    case kQuickGet64Static:
    case kQuickGetObjInstance:
    case kQuickGetObjStatic:
    case kQuickAputObject:
    case kQuickJniMethodStart:
    case kQuickJniMethodFastStart:
    case kQuickJniMethodStartSynchronized:
    case kQuickJniMethodEnd:
    case kQuickJniMethodFastEnd:
    case kQuickJniMethodEndSynchronized:
    case kQuickJniMethodEndWithReference:
    case kQuickJniMethodFastEndWithReference:
    case kQuickJniMethodEndWithReferenceSynchronized:
    case kQuickQuickGenericJniTrampoline:
    case kQuickLockObject:
    case kQuickUnlockObject:
    case kQuickIdivmod:
    case kQuickShlLong:
    case kQuickShrLong:
    case kQuickUshrLong:
    case kQuickIndexOf:
    case kQuickStringCompareTo:
    case kQuickQuickImtConflictTrampoline:
    case kQuickQuickResolutionTrampoline:
    case kQuickQuickToInterpreterBridge:
    case kQuickInvokeDirectTrampolineWithAccessCheck:
    case kQuickInvokeInterfaceTrampolineWithAccessCheck:
    case kQuickInvokeStaticTrampolineWithAccessCheck:
    case kQuickInvokeSuperTrampolineWithAccessCheck:
    case kQuickInvokeVirtualTrampolineWithAccessCheck:
    case kQuickInvokePolymorphic:
    case kQuickTestSuspend:
    case kQuickDeliverException:
    case kQuickThrowArrayBounds:
    case kQuickThrowDivZero:
    case kQuickThrowNullPointer:
    case kQuickThrowStackOverflow:
    case kQuickThrowStringBounds:
    case kQuickDeoptimize:
    case kQuickNewEmptyString:
    case kQuickNewStringFromBytes_B:
    case kQuickNewStringFromBytes_BI:
    case kQuickNewStringFromBytes_BII:
    case kQuickNewStringFromBytes_BIII:
    case kQuickNewStringFromBytes_BIIString:
    case kQuickNewStringFromBytes_BString:
    case kQuickNewStringFromBytes_BIICharset:
    case kQuickNewStringFromBytes_BCharset:
    case kQuickNewStringFromChars_C:
    case kQuickNewStringFromChars_CII:
    case kQuickNewStringFromChars_IIC:
    case kQuickNewStringFromCodePoints:
    case kQuickNewStringFromString:
    case kQuickNewStringFromStringBuffer:
    case kQuickNewStringFromStringBuilder:
    case kQuickReadBarrierMarkReg00:
    case kQuickReadBarrierMarkReg01:
    case kQuickReadBarrierMarkReg02:
    case kQuickReadBarrierMarkReg03:
    case kQuickReadBarrierMarkReg04:
    case kQuickReadBarrierMarkReg05:
    case kQuickReadBarrierMarkReg06:
    case kQuickReadBarrierMarkReg07:
    case kQuickReadBarrierMarkReg08:
    case kQuickReadBarrierMarkReg09:
    case kQuickReadBarrierMarkReg10:
    case kQuickReadBarrierMarkReg11:
    case kQuickReadBarrierMarkReg12:
    case kQuickReadBarrierMarkReg13:
    case kQuickReadBarrierMarkReg14:
    case kQuickReadBarrierMarkReg15:
    case kQuickReadBarrierMarkReg16:
    case kQuickReadBarrierMarkReg17:
    case kQuickReadBarrierMarkReg18:
    case kQuickReadBarrierMarkReg19:
    case kQuickReadBarrierMarkReg20:
    case kQuickReadBarrierMarkReg21:
    case kQuickReadBarrierMarkReg22:
    case kQuickReadBarrierMarkReg23:
    case kQuickReadBarrierMarkReg24:
    case kQuickReadBarrierMarkReg25:
    case kQuickReadBarrierMarkReg26:
    case kQuickReadBarrierMarkReg27:
    case kQuickReadBarrierMarkReg28:
    case kQuickReadBarrierMarkReg29:
      break;
  }
  return false;

#pragma clang diagnostic pop
}

}  // namespace art

#endif  // ART_RUNTIME_ARCH_MIPS_ENTRYPOINTS_DIRECT_MIPS_H_
