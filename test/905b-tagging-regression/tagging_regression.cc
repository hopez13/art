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

#include "tagging_regression.h"

#include <stddef.h>
#include <stdio.h>

#include "base/logging.h"
#include "jni.h"
#include "openjdkjvmti/jvmti.h"
#include "ti-agent/common_load.h"
#include "utils.h"

namespace art {
namespace Test905bTaggingRegression {

static int32_t object_tag = 0;
static int32_t object_free_count = 0;

static jint JNICALL HeapIterationCallback(jlong class_tag ATTRIBUTE_UNUSED,
                                          jlong size ATTRIBUTE_UNUSED,
                                          jlong* tag_ptr ATTRIBUTE_UNUSED,
                                          jint length ATTRIBUTE_UNUSED,
                                          void* user_data ATTRIBUTE_UNUSED) {
  return JVMTI_VISIT_OBJECTS;
}

static void JNICALL ObjectAllocated(jvmtiEnv* jvmti_env,
                                    JNIEnv* jni_env ATTRIBUTE_UNUSED,
                                    jthread thread ATTRIBUTE_UNUSED,
                                    jobject object,
                                    jclass object_klass ATTRIBUTE_UNUSED,
                                    jlong size ATTRIBUTE_UNUSED) {
  jvmti_env->SetTag(object, object_tag++);
}

static void JNICALL ObjectFree(jvmtiEnv* ti_env ATTRIBUTE_UNUSED,
                               jlong tag ATTRIBUTE_UNUSED) {
  object_free_count++;
}

extern "C" JNIEXPORT void JNICALL Java_Main_performIterateHeap(
    JNIEnv* env ATTRIBUTE_UNUSED, jclass klass ATTRIBUTE_UNUSED) {
  jvmtiHeapCallbacks heap_callbacks;
  memset(&heap_callbacks, 0, sizeof(heap_callbacks));
  heap_callbacks.heap_iteration_callback = &HeapIterationCallback;
  jvmti_env->IterateThroughHeap(JVMTI_HEAP_FILTER_CLASS_TAGGED, nullptr,
                                &heap_callbacks, nullptr);
  return;
}

extern "C" JNIEXPORT void JNICALL Java_Main_enableObjectTagging(
    JNIEnv* env ATTRIBUTE_UNUSED, jclass klass ATTRIBUTE_UNUSED,
    jboolean enable, jboolean reset_tag) {
  object_free_count = 0;
  if (reset_tag) {
    object_tag = 0;
  }

  jvmtiError ret =
      jvmti_env->SetEventNotificationMode(enable ? JVMTI_ENABLE : JVMTI_DISABLE,
                                          JVMTI_EVENT_VM_OBJECT_ALLOC, nullptr);
  if (ret != JVMTI_ERROR_NONE) {
    char* err;
    jvmti_env->GetErrorName(ret, &err);
    printf("Error enabling/disabling JVMTI_EVENT_VM_OBJECT_ALLOC: %s\n", err);
  }

  ret = jvmti_env->SetEventNotificationMode(
      enable ? JVMTI_ENABLE : JVMTI_DISABLE, JVMTI_EVENT_OBJECT_FREE, nullptr);
  if (ret != JVMTI_ERROR_NONE) {
    char* err;
    jvmti_env->GetErrorName(ret, &err);
    printf("Error enabling/disabling JVMTI_EVENT_OBJECT_FREE: %s\n", err);
  }
}

extern "C" JNIEXPORT void JNICALL Java_Main_setupObjectAllocationCallbacks(
    JNIEnv* env ATTRIBUTE_UNUSED, jclass klass ATTRIBUTE_UNUSED) {
  jvmtiEventCallbacks callbacks;
  memset(&callbacks, 0, sizeof(jvmtiEventCallbacks));
  callbacks.VMObjectAlloc = ObjectAllocated;
  callbacks.ObjectFree = ObjectFree;

  jvmtiError ret = jvmti_env->SetEventCallbacks(&callbacks, sizeof(callbacks));
  if (ret != JVMTI_ERROR_NONE) {
    char* err;
    jvmti_env->GetErrorName(ret, &err);
    printf("Error setting callbacks: %s\n", err);
  }
}

// Don't do anything
jint OnLoad(JavaVM* vm, char* options ATTRIBUTE_UNUSED,
            void* reserved ATTRIBUTE_UNUSED) {
  if (vm->GetEnv(reinterpret_cast<void**>(&jvmti_env), JVMTI_VERSION_1_0)) {
    printf("Unable to get jvmti env!\n");
    return 1;
  }
  return 0;
}

}  // namespace Test905bTaggingRegression
}  // namespace art
