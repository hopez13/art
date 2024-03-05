/*
 * Copyright (C) 2024 The Android Open Source Project
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

#include "android_os_Debug.h"

#include <android-base/logging.h>

#include "base/array_ref.h"
#include "debugger.h"
#include "jni/jni_internal.h"
#include "native_util.h"
#include "nativehelper/jni_macros.h"
#include "nativehelper/scoped_primitive_array.h"
#include "scoped_fast_native_object_access-inl.h"

namespace art HIDDEN {

static void Debug_onProcessNamed(JNIEnv* env, jclass, jstring process_name) {
  ScopedFastNativeObjectAccess soa(env);

  // Android application ID naming convention states:
  // "The name can contain uppercase or lowercase letters, numbers, and underscores ('_')"
  // This is fine to convert to std::string
  const char* c_process_name = env->GetStringUTFChars(process_name, NULL);
  Runtime::Current()->GetRuntimeCallbacks()->OnProcessNamed(std::string(c_process_name));
  env->ReleaseStringUTFChars(process_name, c_process_name);
}

static void Debug_onApplicationAdded(JNIEnv* env, jclass, jstring package_name) {
  ScopedFastNativeObjectAccess soa(env);

  // Android application ID naming convention states:
  // "The name can contain uppercase or lowercase letters, numbers, and underscores ('_')"
  // This is fine to convert to std::string
  const char* c_package_name = env->GetStringUTFChars(package_name, NULL);
  Runtime::Current()->GetRuntimeCallbacks()->OnApplicationAdded(std::string(c_package_name));
  env->ReleaseStringUTFChars(package_name, c_package_name);
}

static void Debug_onWaitingForDebugger(JNIEnv* env, jclass, jboolean waiting) {
  ScopedFastNativeObjectAccess soa(env);
  Runtime::Current()->GetRuntimeCallbacks()->OnWaitingForDebugger(waiting);
}

static void Debug_onUserIdKnown(JNIEnv* env, jclass, jint user_id) {
  ScopedFastNativeObjectAccess soa(env);
  Runtime::Current()->GetRuntimeCallbacks()->OnUserIdKnown(user_id);
}

static JNINativeMethod gMethods[] = {
    FAST_NATIVE_METHOD(Debug, onProcessNamed, "(Ljava/lang/String;)V"),
    FAST_NATIVE_METHOD(Debug, onApplicationAdded, "(Ljava/lang/String;)V"),
    FAST_NATIVE_METHOD(Debug, onWaitingForDebugger, "(Z)V"),
    FAST_NATIVE_METHOD(Debug, onUserIdKnown, "(I)V"),
};

void register_android_os_Debug(JNIEnv* env) { REGISTER_NATIVE_METHODS("android/os/Debug"); }

}  // namespace art HIDDEN
