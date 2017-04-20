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

#include <inttypes.h>
#include <memory>
#include <stdio.h>
#include <dlfcn.h>

#include "android-base/stringprintf.h"
#include "jni.h"
#include "jvmti.h"

// Test infrastructure
#include "jni_binder.h"
#include "jvmti_helper.h"
#include "test_env.h"
#include "scoped_local_ref.h"

// This is very similar to 986-native-method-bind/native-bind.cc with some different strings.
// These are functions contained in the ti-agent.
namespace art {
namespace Test987NativeAutoBind {

static void doJvmtiMethodBind(jvmtiEnv* jvmtienv ATTRIBUTE_UNUSED,
                              JNIEnv* env,
                              jthread thread ATTRIBUTE_UNUSED,
                              jmethodID m,
                              void* address ATTRIBUTE_UNUSED,
                              /*out*/void** out_address) {
  ScopedLocalRef<jclass> method_class(env, env->FindClass("java/lang/reflect/Method"));
  ScopedLocalRef<jobject> method_obj(env, env->ToReflectedMethod(method_class.get(), m, false));
  ScopedLocalRef<jclass> klass(env, env->FindClass("Main"));
  jmethodID upcallMethod = env->GetStaticMethodID(klass.get(),
                                                  "doNativeMethodBind",
                                                  "(Ljava/lang/reflect/Method;)J");
  if (env->ExceptionCheck()) {
    return;
  }
  jlong res = env->CallStaticLongMethod(klass.get(), upcallMethod, method_obj.get());
  if (res != 0) {
    *out_address = reinterpret_cast<void*>(static_cast<uintptr_t>(res));
  }
}

extern "C" JNIEXPORT void JNICALL Java_Main_setupNativeBindNotify(
    JNIEnv* env ATTRIBUTE_UNUSED, jclass klass ATTRIBUTE_UNUSED) {
  jvmtiEventCallbacks cb;
  memset(&cb, 0, sizeof(cb));
  cb.NativeMethodBind = doJvmtiMethodBind;
  jvmti_env->SetEventCallbacks(&cb, sizeof(cb));
}

extern "C" JNIEXPORT void JNICALL Java_Main_setNativeBindNotify(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED, jboolean enable) {
  jvmtiError res = jvmti_env->SetEventNotificationMode(enable ? JVMTI_ENABLE : JVMTI_DISABLE,
                                                       JVMTI_EVENT_NATIVE_METHOD_BIND,
                                                       nullptr);
  if (res != JVMTI_ERROR_NONE) {
    JvmtiErrorToException(env, jvmti_env, res);
  }
}

}  // namespace Test987NativeAutoBind
}  // namespace art
