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

#include "tracking.h"

#include <pthread.h>
#include <stdio.h>
#include <iostream>
#include <vector>

#include "ScopedLocalRef.h"
#include "ScopedUtfChars.h"
#include "base/logging.h"
#include "jni.h"
#include "openjdkjvmti/jvmti.h"
#include "ti-agent/common_load.h"
#include "utils.h"

namespace art {
namespace Test904bObjectAllocationFailing {

static void JNICALL ObjectAllocated(jvmtiEnv* ti_env ATTRIBUTE_UNUSED,
                                    JNIEnv* jni_env ATTRIBUTE_UNUSED,
                                    jthread thread ATTRIBUTE_UNUSED,
                                    jobject object ATTRIBUTE_UNUSED,
                                    jclass object_klass ATTRIBUTE_UNUSED,
                                    jlong size ATTRIBUTE_UNUSED) {
  printf("ObjectAllocated");
}

// Setup VMObjectAlloc callback and event notification/
jint OnLoad(JavaVM* vm, char* options ATTRIBUTE_UNUSED,
            void* reserved ATTRIBUTE_UNUSED) {
  if (vm->GetEnv(reinterpret_cast<void**>(&jvmti_env), JVMTI_VERSION_1_0)) {
    printf("Unable to get jvmti env!\n");
    return 1;
  }

  jvmtiEventCallbacks callbacks;
  memset(&callbacks, 0, sizeof(jvmtiEventCallbacks));
  callbacks.VMObjectAlloc = ObjectAllocated;

  jvmtiError ret = jvmti_env->SetEventCallbacks(&callbacks, sizeof(callbacks));
  if (ret != JVMTI_ERROR_NONE) {
    char* err;
    jvmti_env->GetErrorName(ret, &err);
    printf("Error setting callbacks: %s\n", err);
  }

  ret = jvmti_env->SetEventNotificationMode(
      JVMTI_ENABLE, JVMTI_EVENT_VM_OBJECT_ALLOC, nullptr);
  if (ret != JVMTI_ERROR_NONE) {
    char* err;
    jvmti_env->GetErrorName(ret, &err);
    printf("Error enabling allocation tracking: %s\n", err);
  }

  return 0;
}

void OnUnload(JavaVM* vm ATTRIBUTE_UNUSED) {
  jvmtiError ret =
      jvmti_env->SetEventNotificationMode(JVMTI_DISABLE,
                                          JVMTI_EVENT_VM_OBJECT_ALLOC, nullptr);
  if (ret != JVMTI_ERROR_NONE) {
    char* err;
    jvmti_env->GetErrorName(ret, &err);
    printf("Error disabling allocation tracking: %s\n", err);
  }
}

}  // namespace Test904bObjectAllocationFailing
}  // namespace art
