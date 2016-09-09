/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "java_lang_reflect_Executable.h"

#include "art_method-inl.h"
#include "dex_file_annotations.h"
#include "jni_internal.h"
#include "mirror/class-inl.h"
#include "mirror/object-inl.h"
#include "mirror/object_array-inl.h"
#include "reflection.h"
#include "scoped_fast_native_object_access.h"
#include "well_known_classes.h"

namespace art {

static jobjectArray Executable_getDeclaredAnnotationsNative(JNIEnv* env, jobject javaMethod) {
  ScopedFastNativeObjectAccess soa(env);
  ArtMethod* method = ArtMethod::FromReflectedMethod(soa, javaMethod);
  if (method->GetDeclaringClass()->IsProxyClass()) {
    // Return an empty array instead of a null pointer.
    mirror::Class* annotation_array_class =
        soa.Decode<mirror::Class*>(WellKnownClasses::java_lang_annotation_Annotation__array);
    mirror::ObjectArray<mirror::Object>* empty_array =
        mirror::ObjectArray<mirror::Object>::Alloc(soa.Self(), annotation_array_class, 0);
    return soa.AddLocalReference<jobjectArray>(empty_array);
  }
  return soa.AddLocalReference<jobjectArray>(annotations::GetAnnotationsForMethod(method));
}

static jobject Executable_getAnnotationNative(JNIEnv* env,
                                              jobject javaMethod,
                                              jclass annotationType) {
  ScopedFastNativeObjectAccess soa(env);
  StackHandleScope<1> hs(soa.Self());
  ArtMethod* method = ArtMethod::FromReflectedMethod(soa, javaMethod);
  if (method->IsProxyMethod()) {
    return nullptr;
  } else {
    Handle<mirror::Class> klass(hs.NewHandle(soa.Decode<mirror::Class*>(annotationType)));
    return soa.AddLocalReference<jobject>(annotations::GetAnnotationForMethod(method, klass));
  }
}

static jobjectArray Executable_getSignatureAnnotation(JNIEnv* env, jobject javaMethod) {
  ScopedFastNativeObjectAccess soa(env);
  ArtMethod* method = ArtMethod::FromReflectedMethod(soa, javaMethod);
  if (method->GetDeclaringClass()->IsProxyClass()) {
    return nullptr;
  }
  StackHandleScope<1> hs(soa.Self());
  return soa.AddLocalReference<jobjectArray>(annotations::GetSignatureAnnotationForMethod(method));
}


static jobjectArray Executable_getParameterAnnotationsNative(JNIEnv* env, jobject javaMethod) {
  ScopedFastNativeObjectAccess soa(env);
  ArtMethod* method = ArtMethod::FromReflectedMethod(soa, javaMethod);
  if (method->IsProxyMethod()) {
    return nullptr;
  } else {
    return soa.AddLocalReference<jobjectArray>(annotations::GetParameterAnnotations(method));
  }
}

static jobjectArray Executable_getParameters0(JNIEnv* env, jobject javaMethod) {
  ScopedFastNativeObjectAccess soa(env);
  ArtMethod* method = ArtMethod::FromReflectedMethod(soa, javaMethod);
  if (method->GetDeclaringClass()->IsProxyClass()) {
    return nullptr;
  }

  // Find the MethodParameters system annotation.
  mirror::ObjectArray<mirror::String>* names_array;
  mirror::IntArray* access_flags_array;
  if (!annotations::GetParametersMetadataForMethod(method, &names_array, &access_flags_array)) {
    return nullptr;
  }

  StackHandleScope<3> hs(soa.Self());
  Handle<mirror::ObjectArray<mirror::String>> names = hs.NewHandle(names_array);
  Handle<mirror::IntArray> access_flags = hs.NewHandle(access_flags_array);

  // Validate the MethodParameters system annotation data.
  if (UNLIKELY(names.Get() == nullptr || access_flags.Get() == nullptr)) {
    ThrowIllegalArgumentException(
        StringPrintf("Missing parameter metadata for names or access flags for %s",
                     PrettyMethod(method).c_str()).c_str());
    return nullptr;
  }

  // Check array sizes match each other
  int32_t names_count = names.Get()->GetLength();
  int32_t access_flags_count = access_flags.Get()->GetLength();
  if (names_count != access_flags_count) {
    ThrowIllegalArgumentException(
        StringPrintf(
            "Inconsistent parameter metadata for %s. names length: %d, access flags length: %d",
            PrettyMethod(method).c_str(),
            names_count,
            access_flags_count).c_str());
    return nullptr;
  }

  // Instantiate Parameter[] to hold the result.
  mirror::Class* parameter_array_class =
        soa.Decode<mirror::Class*>(WellKnownClasses::java_lang_reflect_Parameter__array);
  Handle<mirror::ObjectArray<mirror::Object>> result =
      hs.NewHandle(
          mirror::ObjectArray<mirror::Object>::Alloc(soa.Self(),
                                                     parameter_array_class,
                                                     names_count));
  if (UNLIKELY(result.Get() == nullptr)) {
    soa.Self()->AssertPendingException();
    return nullptr;
  }

  // Populate the Parameter[] to return.
  for (int32_t parameter_index = 0; parameter_index < names_count; parameter_index++) {
    mirror::String* name = names.Get()->Get(parameter_index);
    int32_t modifiers = access_flags.Get()->Get(parameter_index);

    // Create a local reference table to hold each Parameter and name String to avoid scaling local
    // references with the number of method parameters.
    env->PushLocalFrame(2);

    // Instantiate the Parameter.
    jobject parameter = env->NewObject(WellKnownClasses::java_lang_reflect_Parameter,
                                       WellKnownClasses::java_lang_reflect_Parameter_init,
                                       soa.AddLocalReference<jstring>(name),
                                       reinterpret_cast<jint>(modifiers),
                                       javaMethod,
                                       reinterpret_cast<jint>(parameter_index));
    if (soa.Self()->IsExceptionPending()) {
      LOG(INFO) << "Exception in Parameter.<init>";
      return nullptr;
    }

    // Store the Parameter in the Parameter[].
    result.Get()->Set(parameter_index, soa.Decode<mirror::Object*>(parameter));

    // Pop the local reference table created above now the result array holds a reference to the
    // Parameter.
    env->PopLocalFrame(nullptr);

    if (soa.Self()->IsExceptionPending()) {
      LOG(INFO) << "Exception when setting parameter array";
      return nullptr;
    }
  }
  return soa.AddLocalReference<jobjectArray>(result.Get());
}

static jboolean Executable_isAnnotationPresentNative(JNIEnv* env,
                                                     jobject javaMethod,
                                                     jclass annotationType) {
  ScopedFastNativeObjectAccess soa(env);
  ArtMethod* method = ArtMethod::FromReflectedMethod(soa, javaMethod);
  if (method->GetDeclaringClass()->IsProxyClass()) {
    return false;
  }
  StackHandleScope<1> hs(soa.Self());
  Handle<mirror::Class> klass(hs.NewHandle(soa.Decode<mirror::Class*>(annotationType)));
  return annotations::IsMethodAnnotationPresent(method, klass);
}

static JNINativeMethod gMethods[] = {
  NATIVE_METHOD(Executable, getAnnotationNative,
                "!(Ljava/lang/Class;)Ljava/lang/annotation/Annotation;"),
  NATIVE_METHOD(Executable, getDeclaredAnnotationsNative, "!()[Ljava/lang/annotation/Annotation;"),
  NATIVE_METHOD(Executable, getParameterAnnotationsNative,
                "!()[[Ljava/lang/annotation/Annotation;"),
  NATIVE_METHOD(Executable, getParameters0, "!()[Ljava/lang/reflect/Parameter;"),
  NATIVE_METHOD(Executable, getSignatureAnnotation, "!()[Ljava/lang/String;"),
  NATIVE_METHOD(Executable, isAnnotationPresentNative, "!(Ljava/lang/Class;)Z"),
};

void register_java_lang_reflect_Executable(JNIEnv* env) {
  REGISTER_NATIVE_METHODS("java/lang/reflect/Executable");
}

}  // namespace art
