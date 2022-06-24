/*
 * Copyright (C) 2022 The Android Open Source Project
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


#include "java_lang_invoke_MethodHandles.h"

#include "art_method-inl.h"
#include "common_throws.h"
#include "mirror/method.h"
#include "mirror/method_type.h"
#include "nativehelper/scoped_utf_chars.h"
#include "scoped_thread_state_change-inl.h"

namespace art {

static std::function<hiddenapi::AccessContext()> GetHiddenapiAccessContextFunction(Thread* self) {
  return [=]() REQUIRES_SHARED(Locks::mutator_lock_) {
    return hiddenapi::GetReflectionCallerAccessContext(self);
  };
}

static void MethodHandles_Lookup_checkHiddenApi(JNIEnv* env,
                                                jclass /*clazz*/,
                                                jobject method) {
  ScopedObjectAccess soa(env);
  ArtMethod* art_method = soa.Decode<mirror::Method>(method)->GetArtMethod();

  bool hidden_jni =
      hiddenapi::ShouldDenyAccessToMember(art_method,
                                          GetHiddenapiAccessContextFunction(soa.Self()),
                                          hiddenapi::AccessMethod::kJNI);
  if (hidden_jni) {
    ThrowNoSuchMethodException(art_method->GetDeclaringClass(), art_method->GetNameView());
  }
}

static ObjPtr<mirror::Class> EnsureInitialized(Thread* self, ObjPtr<mirror::Class> klass)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (LIKELY(klass->IsInitialized())) {
    return klass;
  }
  StackHandleScope<1> hs(self);
  Handle<mirror::Class> h_klass(hs.NewHandle(klass));
  if (!Runtime::Current()->GetClassLinker()->EnsureInitialized(self, h_klass, true, true)) {
    return nullptr;
  }
  return h_klass.Get();
}

static ArtMethod* lookupHelper(ObjPtr<mirror::Class> c,
                               const char* name,
                               const char* sig)  REQUIRES_SHARED(Locks::mutator_lock_) {
  ArtMethod* method = nullptr;
  auto pointer_size = Runtime::Current()->GetClassLinker()->GetImagePointerSize();
  if (c->IsInterface()) {
    method = c->FindInterfaceMethod(name, sig, pointer_size);
  } else {
    method = c->FindClassMethod(name, sig, pointer_size);
  }
  // TODO(oth): Missing interface tweak in FindMethodJNI when ShouldDenyAccessToMember.
  return method;
}

static jobject MethodHandles_Lookup_lookupMethod(JNIEnv* env,
                                                 jclass /*clazz*/,
                                                 jclass refc,
                                                 jstring methodName,
                                                 jobject methodType) {
  ScopedObjectAccess soa(env);

  ScopedUtfChars utfName(env, methodName);
  if (env->ExceptionCheck()) {
    return nullptr;
  }

  const std::string signature = soa.Decode<mirror::MethodType>(methodType)->GetSignature();

  ObjPtr<mirror::Class> c = EnsureInitialized(soa.Self(), soa.Decode<mirror::Class>(refc));
  if (c == nullptr) {
    return nullptr;
  }

  ArtMethod* art_method = lookupHelper(c, utfName.c_str(), signature.c_str());
  if (art_method == nullptr || env->ExceptionCheck()) {
    return nullptr;
  }

  // We never return miranda methods that were synthesized by the runtime.
  if (art_method->IsMiranda()) {
    return nullptr;
  }

  ObjPtr<mirror::Method> result =
      mirror::Method::CreateFromArtMethod<kRuntimePointerSize>(soa.Self(), art_method);

  return soa.AddLocalReference<jobject>(result);
}

static JNINativeMethod gMethods[] = {
  {
    "checkHiddenApi",
    "(Ljava/lang/reflect/Method;)V",
    reinterpret_cast<void*>(MethodHandles_Lookup_checkHiddenApi)
  },
  {
    "lookupMethod",
    "(Ljava/lang/Class;Ljava/lang/String;Ljava/lang/invoke/MethodType;)Ljava/lang/reflect/Method;",
    reinterpret_cast<void*>(MethodHandles_Lookup_lookupMethod)
  }
};

void register_java_lang_invoke_MethodHandles_Lookup(JNIEnv* env) {
  jclass clazz = env->FindClass("java/lang/invoke/MethodHandles$Lookup");
  if (clazz == nullptr) {
    LOG(FATAL) << "Class not found";
  }
  if (env->RegisterNatives(clazz, gMethods, std::size(gMethods)) != JNI_OK) {
    LOG(FATAL) << "Failed to register method.";
  }
  env->DeleteLocalRef(clazz);
}

}  // namespace art
