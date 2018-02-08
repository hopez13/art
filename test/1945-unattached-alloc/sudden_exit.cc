/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include <stdlib.h>

#include "jni.h"
#include "jvmti.h"

#include "jvmti_helper.h"
#include "test_env.h"

namespace art {
namespace Test1945UnattachedAlloc {

void deallocUnattached(unsigned char* data, /*out*/jvmtiError* err) {
  *err = jvmti_env->Deallocate(data);
}

void allocUnattached(unsigned char** data, /*out*/jvmtiError* err) {
  *err = jvmti_env->Allocate(128, data);
}

extern "C" JNIEXPORT void JNICALL Java_art_Test1945_deallocOtherThread(JNIEnv* env, jclass) {
  jvmtiError err = JVMTI_ERROR_INTERNAL;
  unsigned char* data = nullptr;
  jvmti_env->Allocate(128, &data);
  std::thread t1(deallocUnattached, data, &err);
  t1.join();
  JvmtiErrorToException(env, jvmti_env, err);
}

extern "C" JNIEXPORT void JNICALL Java_art_Test1945_allocOtherThread(JNIEnv* env, jclass) {
  jvmtiError err = JVMTI_ERROR_INTERNAL;
  unsigned char* data = nullptr;
  std::thread t1(allocUnattached, &data, &err);
  t1.join();
  jvmti_env->Deallocate(data);
  JvmtiErrorToException(env, jvmti_env, err);
}

}  // namespace Test1945UnattachedAlloc
}  // namespace art

