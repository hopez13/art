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

#include "jni.h"
#include "jvmti.h"

#include "jvmti_helper.h"
#include "test_env.h"

namespace art {
namespace common_suspension {

extern "C" JNIEXPORT jboolean JNICALL Java_art_Suspension_isSuspended(
    JNIEnv* env, jclass, jthread thr) {
  jint state;
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->GetThreadState(thr, &state))) {
    return false;
  }
  return (state & JVMTI_THREAD_STATE_SUSPENDED) != 0;
}

extern "C" JNIEXPORT void JNICALL Java_art_Suspension_resume(JNIEnv* env, jclass, jthread thr) {
  JvmtiErrorToException(env, jvmti_env, jvmti_env->ResumeThread(thr));
}

extern "C" JNIEXPORT void JNICALL Java_art_Suspension_suspend(JNIEnv* env, jclass, jthread thr) {
  JvmtiErrorToException(env, jvmti_env, jvmti_env->SuspendThread(thr));
}

}  // namespace common_suspension
}  // namespace art

