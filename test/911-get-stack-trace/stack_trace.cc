/*
 * Copyright (C) 2013 The Android Open Source Project
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

#include "stack_trace.h"

#include <memory>
#include <stdio.h>

#include "base/logging.h"
#include "jni.h"
#include "openjdkjvmti/jvmti.h"
#include "ScopedLocalRef.h"
#include "ti-agent/common_load.h"

namespace art {
namespace Test911GetStackTrace {

extern "C" JNIEXPORT jobjectArray JNICALL Java_Main_getStackTrace(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED, jthread thread, jint start, jint max) {
  std::unique_ptr<jvmtiFrameInfo[]> frames(new jvmtiFrameInfo[max]);

  jint count;
  jvmtiError result = jvmti_env->GetStackTrace(thread, start, max, frames.get(), &count);
  if (result != JVMTI_ERROR_NONE) {
    char* err;
    jvmti_env->GetErrorName(result, &err);
    printf("Failure running GetStackTrace: %s\n", err);
    return nullptr;
  }

  ScopedLocalRef<jclass> obj_class(env, env->FindClass("java/lang/String"));
  if (obj_class.get() == nullptr) {
    return nullptr;
  }

  jobjectArray ret = env->NewObjectArray(count, obj_class.get(), nullptr);
  if (ret == nullptr) {
    return ret;
  }

  for (size_t i = 0; i < static_cast<size_t>(count); ++i) {
    ScopedLocalRef<jstring> trace(env, env->NewStringUTF("Dummy"));
    env->SetObjectArrayElement(ret, static_cast<jint>(i), trace.get());
  }

  return ret;
}

// Don't do anything
jint OnLoad(JavaVM* vm,
            char* options ATTRIBUTE_UNUSED,
            void* reserved ATTRIBUTE_UNUSED) {
  if (vm->GetEnv(reinterpret_cast<void**>(&jvmti_env), JVMTI_VERSION_1_0)) {
    printf("Unable to get jvmti env!\n");
    return 1;
  }
  return 0;
}

}  // namespace Test911GetStackTrace
}  // namespace art
