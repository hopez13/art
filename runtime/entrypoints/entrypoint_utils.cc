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

#include "entrypoints/entrypoint_utils.h"

#include "art_field-inl.h"
#include "art_method-inl.h"
#include "base/enums.h"
#include "base/mutex.h"
#include "class_linker-inl.h"
#include "dex/dex_file-inl.h"
#include "entrypoints/entrypoint_utils-inl.h"
#include "entrypoints/quick/callee_save_frame.h"
#include "entrypoints/runtime_asm_entrypoints.h"
#include "gc/accounting/card_table-inl.h"
#include "java_vm_ext.h"
#include "mirror/class-inl.h"
#include "mirror/method.h"
#include "mirror/object-inl.h"
#include "mirror/object_array-inl.h"
#include "nth_caller_visitor.h"
#include "oat_quick_method_header.h"
#include "reflection.h"
#include "scoped_thread_state_change-inl.h"
#include "stack_map.h"
#include "well_known_classes.h"

namespace art {

ArtMethod* GetResolvedMethod(ArtMethod* outer_method,
                             const MethodInfo& method_info,
                             const InlineInfo& inline_info,
                             const InlineInfoEncoding& encoding,
                             uint8_t inlining_depth) {
  DCHECK(!outer_method->IsObsolete());

  // This method is being used by artQuickResolutionTrampoline, before it sets up
  // the passed parameters in a GC friendly way. Therefore we must never be
  // suspended while executing it.
  ScopedAssertNoThreadSuspension sants(__FUNCTION__);

  if (inline_info.EncodesArtMethodAtDepth(encoding, inlining_depth)) {
    return inline_info.GetArtMethodAtDepth(encoding, inlining_depth);
  }

  uint32_t method_index = inline_info.GetMethodIndexAtDepth(encoding, method_info, inlining_depth);
  if (inline_info.GetDexPcAtDepth(encoding, inlining_depth) == static_cast<uint32_t>(-1)) {
    // "charAt" special case. It is the only non-leaf method we inline across dex files.
    ArtMethod* inlined_method = jni::DecodeArtMethod(WellKnownClasses::java_lang_String_charAt);
    DCHECK_EQ(inlined_method->GetDexMethodIndex(), method_index);
    return inlined_method;
  }

  // Find which method did the call in the inlining hierarchy.
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  ArtMethod* method = outer_method;
  for (uint32_t depth = 0, end = inlining_depth + 1u; depth != end; ++depth) {
    DCHECK(!inline_info.EncodesArtMethodAtDepth(encoding, depth));
    DCHECK_NE(inline_info.GetDexPcAtDepth(encoding, depth), static_cast<uint32_t>(-1));
    method_index = inline_info.GetMethodIndexAtDepth(encoding, method_info, depth);
    ArtMethod* inlined_method = class_linker->LookupResolvedMethod(method_index,
                                                                   method->GetDexCache(),
                                                                   method->GetClassLoader());
    if (UNLIKELY(inlined_method == nullptr)) {
      LOG(FATAL) << "Could not find an inlined method from an .oat file: "
                 << method->GetDexFile()->PrettyMethod(method_index) << " . "
                 << "This must be due to duplicate classes or playing wrongly with class loaders";
      UNREACHABLE();
    }
    DCHECK(!inlined_method->IsRuntimeMethod());
    if (UNLIKELY(inlined_method->GetDexFile() != method->GetDexFile())) {
      // TODO: We could permit inlining within a multi-dex oat file and the boot image,
      // even going back from boot image methods to the same oat file. However, this is
      // not currently implemented in the compiler. Therefore crossing dex file boundary
      // indicates that the inlined definition is not the same as the one used at runtime.
      LOG(FATAL) << "Inlined method resolution crossed dex file boundary: from "
                 << method->PrettyMethod()
                 << " in " << method->GetDexFile()->GetLocation() << "/"
                 << static_cast<const void*>(method->GetDexFile())
                 << " to " << inlined_method->PrettyMethod()
                 << " in " << inlined_method->GetDexFile()->GetLocation() << "/"
                 << static_cast<const void*>(inlined_method->GetDexFile()) << ". "
                 << "This must be due to duplicate classes or playing wrongly with class loaders";
      UNREACHABLE();
    }
    method = inlined_method;
  }

  return method;
}

void CheckReferenceResult(Handle<mirror::Object> o, Thread* self) {
  if (o == nullptr) {
    return;
  }
  // Make sure that the result is an instance of the type this method was expected to return.
  ArtMethod* method = self->GetCurrentMethod(nullptr);
  ObjPtr<mirror::Class> return_type = method->ResolveReturnType();

  if (!o->InstanceOf(return_type)) {
    Runtime::Current()->GetJavaVM()->JniAbortF(nullptr,
                                               "attempt to return an instance of %s from %s",
                                               o->PrettyTypeOf().c_str(),
                                               method->PrettyMethod().c_str());
  }
}

JValue InvokeProxyInvocationHandler(ScopedObjectAccessAlreadyRunnable& soa, const char* shorty,
                                    jobject rcvr_jobj, jobject interface_method_jobj,
                                    std::vector<jvalue>& args) {
  DCHECK(soa.Env()->IsInstanceOf(rcvr_jobj, WellKnownClasses::java_lang_reflect_Proxy));

  // Build argument array possibly triggering GC.
  soa.Self()->AssertThreadSuspensionIsAllowable();
  jobjectArray args_jobj = nullptr;
  const JValue zero;
  int32_t target_sdk_version = Runtime::Current()->GetTargetSdkVersion();
  // Do not create empty arrays unless needed to maintain Dalvik bug compatibility.
  if (args.size() > 0 || (target_sdk_version > 0 && target_sdk_version <= 21)) {
    args_jobj = soa.Env()->NewObjectArray(args.size(), WellKnownClasses::java_lang_Object, nullptr);
    if (args_jobj == nullptr) {
      CHECK(soa.Self()->IsExceptionPending());
      return zero;
    }
    for (size_t i = 0; i < args.size(); ++i) {
      if (shorty[i + 1] == 'L') {
        jobject val = args.at(i).l;
        soa.Env()->SetObjectArrayElement(args_jobj, i, val);
      } else {
        JValue jv;
        jv.SetJ(args.at(i).j);
        mirror::Object* val = BoxPrimitive(Primitive::GetType(shorty[i + 1]), jv).Ptr();
        if (val == nullptr) {
          CHECK(soa.Self()->IsExceptionPending());
          return zero;
        }
        soa.Decode<mirror::ObjectArray<mirror::Object>>(args_jobj)->Set<false>(i, val);
      }
    }
  }

  // Call Proxy.invoke(Proxy proxy, Method method, Object[] args).
  jvalue invocation_args[3];
  invocation_args[0].l = rcvr_jobj;
  invocation_args[1].l = interface_method_jobj;
  invocation_args[2].l = args_jobj;
  jobject result =
      soa.Env()->CallStaticObjectMethodA(WellKnownClasses::java_lang_reflect_Proxy,
                                         WellKnownClasses::java_lang_reflect_Proxy_invoke,
                                         invocation_args);

  // Unbox result and handle error conditions.
  if (LIKELY(!soa.Self()->IsExceptionPending())) {
    if (shorty[0] == 'V' || (shorty[0] == 'L' && result == nullptr)) {
      // Do nothing.
      return zero;
    } else {
      ArtMethod* interface_method =
          soa.Decode<mirror::Method>(interface_method_jobj)->GetArtMethod();
      // This can cause thread suspension.
      ObjPtr<mirror::Class> result_type = interface_method->ResolveReturnType();
      ObjPtr<mirror::Object> result_ref = soa.Decode<mirror::Object>(result);
      JValue result_unboxed;
      if (!UnboxPrimitiveForResult(result_ref.Ptr(), result_type, &result_unboxed)) {
        DCHECK(soa.Self()->IsExceptionPending());
        return zero;
      }
      return result_unboxed;
    }
  } else {
    // In the case of checked exceptions that aren't declared, the exception must be wrapped by
    // a UndeclaredThrowableException.
    mirror::Throwable* exception = soa.Self()->GetException();
    if (exception->IsCheckedException()) {
      bool declares_exception = false;
      {
        ScopedAssertNoThreadSuspension ants(__FUNCTION__);
        ObjPtr<mirror::Object> rcvr = soa.Decode<mirror::Object>(rcvr_jobj);
        mirror::Class* proxy_class = rcvr->GetClass();
        ObjPtr<mirror::Method> interface_method = soa.Decode<mirror::Method>(interface_method_jobj);
        ArtMethod* proxy_method = rcvr->GetClass()->FindVirtualMethodForInterface(
            interface_method->GetArtMethod(), kRuntimePointerSize);
        auto virtual_methods = proxy_class->GetVirtualMethodsSlice(kRuntimePointerSize);
        size_t num_virtuals = proxy_class->NumVirtualMethods();
        size_t method_size = ArtMethod::Size(kRuntimePointerSize);
        // Rely on the fact that the methods are contiguous to determine the index of the method in
        // the slice.
        int throws_index = (reinterpret_cast<uintptr_t>(proxy_method) -
            reinterpret_cast<uintptr_t>(&virtual_methods[0])) / method_size;
        CHECK_LT(throws_index, static_cast<int>(num_virtuals));
        mirror::ObjectArray<mirror::Class>* declared_exceptions =
            proxy_class->GetProxyThrows()->Get(throws_index);
        mirror::Class* exception_class = exception->GetClass();
        for (int32_t i = 0; i < declared_exceptions->GetLength() && !declares_exception; i++) {
          mirror::Class* declared_exception = declared_exceptions->Get(i);
          declares_exception = declared_exception->IsAssignableFrom(exception_class);
        }
      }
      if (!declares_exception) {
        soa.Self()->ThrowNewWrappedException("Ljava/lang/reflect/UndeclaredThrowableException;",
                                             nullptr);
      }
    }
    return zero;
  }
}

bool FillArrayData(ObjPtr<mirror::Object> obj, const Instruction::ArrayDataPayload* payload) {
  DCHECK_EQ(payload->ident, static_cast<uint16_t>(Instruction::kArrayDataSignature));
  if (UNLIKELY(obj == nullptr)) {
    ThrowNullPointerException("null array in FILL_ARRAY_DATA");
    return false;
  }
  mirror::Array* array = obj->AsArray();
  DCHECK(!array->IsObjectArray());
  if (UNLIKELY(static_cast<int32_t>(payload->element_count) > array->GetLength())) {
    Thread* self = Thread::Current();
    self->ThrowNewExceptionF("Ljava/lang/ArrayIndexOutOfBoundsException;",
                             "failed FILL_ARRAY_DATA; length=%d, index=%d",
                             array->GetLength(), payload->element_count);
    return false;
  }
  // Copy data from dex file to memory assuming both are little endian.
  uint32_t size_in_bytes = payload->element_count * payload->element_width;
  memcpy(array->GetRawData(payload->element_width, 0), payload->data, size_in_bytes);
  return true;
}

static inline std::pair<ArtMethod*, uintptr_t> DoGetCalleeSaveMethodOuterCallerAndPc(
    ArtMethod** sp, CalleeSaveType type) REQUIRES_SHARED(Locks::mutator_lock_) {
  DCHECK_EQ(*sp, Runtime::Current()->GetCalleeSaveMethod(type));

  const size_t callee_frame_size = GetCalleeSaveFrameSize(kRuntimeISA, type);
  auto** caller_sp = reinterpret_cast<ArtMethod**>(
      reinterpret_cast<uintptr_t>(sp) + callee_frame_size);
  const size_t callee_return_pc_offset = GetCalleeSaveReturnPcOffset(kRuntimeISA, type);
  uintptr_t caller_pc = *reinterpret_cast<uintptr_t*>(
      (reinterpret_cast<uint8_t*>(sp) + callee_return_pc_offset));
  ArtMethod* outer_method = *caller_sp;
  return std::make_pair(outer_method, caller_pc);
}

static inline ArtMethod* DoGetCalleeSaveMethodCaller(ArtMethod* outer_method,
                                                     uintptr_t caller_pc,
                                                     bool do_caller_check)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ArtMethod* caller = outer_method;
  if (LIKELY(caller_pc != reinterpret_cast<uintptr_t>(GetQuickInstrumentationExitPc()))) {
    if (outer_method != nullptr) {
      const OatQuickMethodHeader* current_code = outer_method->GetOatQuickMethodHeader(caller_pc);
      DCHECK(current_code != nullptr);
      DCHECK(current_code->IsOptimized());
      uintptr_t native_pc_offset = current_code->NativeQuickPcOffset(caller_pc);
      CodeInfo code_info = current_code->GetOptimizedCodeInfo();
      MethodInfo method_info = current_code->GetOptimizedMethodInfo();
      CodeInfoEncoding encoding = code_info.ExtractEncoding();
      StackMap stack_map = code_info.GetStackMapForNativePcOffset(native_pc_offset, encoding);
      DCHECK(stack_map.IsValid());
      if (stack_map.HasInlineInfo(encoding.stack_map.encoding)) {
        InlineInfo inline_info = code_info.GetInlineInfoOf(stack_map, encoding);
        caller = GetResolvedMethod(outer_method,
                                   method_info,
                                   inline_info,
                                   encoding.inline_info.encoding,
                                   inline_info.GetDepth(encoding.inline_info.encoding) - 1);
      }
    }
    if (kIsDebugBuild && do_caller_check) {
      // Note that do_caller_check is optional, as this method can be called by
      // stubs, and tests without a proper call stack.
      NthCallerVisitor visitor(Thread::Current(), 1, true);
      visitor.WalkStack();
      CHECK_EQ(caller, visitor.caller);
    }
  } else {
    // We're instrumenting, just use the StackVisitor which knows how to
    // handle instrumented frames.
    NthCallerVisitor visitor(Thread::Current(), 1, true);
    visitor.WalkStack();
    caller = visitor.caller;
  }
  return caller;
}

ArtMethod* GetCalleeSaveMethodCaller(ArtMethod** sp, CalleeSaveType type, bool do_caller_check)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ScopedAssertNoThreadSuspension ants(__FUNCTION__);
  auto outer_caller_and_pc = DoGetCalleeSaveMethodOuterCallerAndPc(sp, type);
  ArtMethod* outer_method = outer_caller_and_pc.first;
  uintptr_t caller_pc = outer_caller_and_pc.second;
  ArtMethod* caller = DoGetCalleeSaveMethodCaller(outer_method, caller_pc, do_caller_check);
  return caller;
}

CallerAndOuterMethod GetCalleeSaveMethodCallerAndOuterMethod(Thread* self, CalleeSaveType type) {
  CallerAndOuterMethod result;
  ScopedAssertNoThreadSuspension ants(__FUNCTION__);
  ArtMethod** sp = self->GetManagedStack()->GetTopQuickFrameKnownNotTagged();
  auto outer_caller_and_pc = DoGetCalleeSaveMethodOuterCallerAndPc(sp, type);
  result.outer_method = outer_caller_and_pc.first;
  uintptr_t caller_pc = outer_caller_and_pc.second;
  result.caller =
      DoGetCalleeSaveMethodCaller(result.outer_method, caller_pc, /* do_caller_check */ true);
  return result;
}

ArtMethod* GetCalleeSaveOuterMethod(Thread* self, CalleeSaveType type) {
  ScopedAssertNoThreadSuspension ants(__FUNCTION__);
  ArtMethod** sp = self->GetManagedStack()->GetTopQuickFrameKnownNotTagged();
  return DoGetCalleeSaveMethodOuterCallerAndPc(sp, type).first;
}

}  // namespace art
