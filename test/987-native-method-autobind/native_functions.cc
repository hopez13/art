/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include <string.h>

#include "jni.h"
#include "base/logging.h"

// These are functions of the Transform class and are part of libarttest.
namespace art {
namespace Test987NativeFunctions {

static void callJavaMethod(JNIEnv* env, jclass target, jobject method) {
  jmethodID m = env->FromReflectedMethod(method);
  env->CallStaticVoidMethod(target, m);
}

extern "C" JNIEXPORT void JNICALL Java_Transform_callOneMethod(JNIEnv* env,
                                                               jclass this_klass ATTRIBUTE_UNUSED,
                                                               jclass target_klass,
                                                               jobject method_a,
                                                               jobject method_b ATTRIBUTE_UNUSED) {
  callJavaMethod(env, target_klass, method_a);
}

extern "C" JNIEXPORT void JNICALL callOtherMethod(JNIEnv* env,
                                                  jclass this_klass ATTRIBUTE_UNUSED,
                                                  jclass target_klass,
                                                  jobject method_a ATTRIBUTE_UNUSED,
                                                  jobject method_b) {
  callJavaMethod(env, target_klass, method_b);
}

extern "C" JNIEXPORT void JNICALL callBothMethods(JNIEnv* env,
                                                  jclass this_klass ATTRIBUTE_UNUSED,
                                                  jclass target_klass,
                                                  jobject method_a,
                                                  jobject method_b) {
  callJavaMethod(env, target_klass, method_a);
  if (env->ExceptionCheck()) {
    return;
  }
  callJavaMethod(env, target_klass, method_b);
}

// Resets callOneMethod back to it's original implementation.
extern "C" JNIEXPORT void JNICALL Java_Transform_resetNativeImplementation(JNIEnv* env,
                                                                           jclass klass) {
  JNINativeMethod m;
  m.name = "callOneMethod";
  m.signature = "(Ljava/lang/Class;Ljava/lang/reflect/Method;Ljava/lang/reflect/Method;)V";
  m.fnPtr = reinterpret_cast<void*>(Java_Transform_callOneMethod);
  env->RegisterNatives(klass, &m, 1);
}

extern "C" JNIEXPORT jlong JNICALL Java_Transform_getPointerFor(JNIEnv* env,
                                                                jclass klass ATTRIBUTE_UNUSED,
                                                                jstring name) {
  const char* name_chars = env->GetStringUTFChars(name, nullptr);
  if (strcmp("callOtherMethod", name_chars) == 0) {
    return reinterpret_cast<uintptr_t>(callOtherMethod);
  } else if (strcmp("callBothMethods", name_chars) == 0) {
    return reinterpret_cast<uintptr_t>(callBothMethods);
  } else if (strcmp("callOneMethod", name_chars) == 0) {
    return reinterpret_cast<uintptr_t>(Java_Transform_callOneMethod);
  } else {
    jclass exception_class = env->FindClass("java/lang/Exception");
    env->ThrowNew(exception_class, "Unable to find function");
    env->DeleteLocalRef(exception_class);
  }
  env->ReleaseStringUTFChars(name, name_chars);
  return 0;
}

}  // namespace Test987NativeFunctions
}  // namespace art
