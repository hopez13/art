/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include "base/time_utils.h"
#include "base/utils.h"
#include "jni/jni_env_ext.h"
#include "jni/jni_internal.h"

#include "jni.h"

namespace art {

static void maybePrintTime() {
  constexpr bool printTimes = false;
  if (printTimes) {
    printf("At %u msecs:", static_cast<int>(MilliTime()));
  }
}


extern "C" [[noreturn]] JNIEXPORT void JNICALL Java_Main_monitorShutdown(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED) {
  bool foundShutdown = false;
  bool foundRuntimeDeleted = false;
  JNIEnvExt* const extEnv = down_cast<JNIEnvExt*>(env);
  while (true) {
    if (!foundShutdown && env->functions == GetRuntimeShutdownNativeInterface()) {
      foundShutdown = true;
      maybePrintTime();
      printf("Saw RuntimeShutdownFunctions\n");
    }
    if (!foundRuntimeDeleted && extEnv->IsRuntimeDeleted()) {
      foundRuntimeDeleted = true;
      maybePrintTime();
      printf("Saw RuntimeDeleted\n");
    }
    if (foundShutdown && foundRuntimeDeleted) {
      SleepForever();
    }
  }
}

}  // namespace art


