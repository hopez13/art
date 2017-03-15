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

#include "base/casts.h"
#include "base/logging.h"
#include "jni.h"

namespace art {

#if defined(__arm__)
static float a = 6.0f;
#endif

extern "C" JNIEXPORT void JNICALL Java_Main_holdFpTemporaries(JNIEnv* env, jclass cls) {
  jmethodID mid = env->GetStaticMethodID(cls, "caller", "(IIJ)V");
  CHECK(mid != 0);
  // This looks very superficial, but unfortunately C compilers will put fp values
  // in gprs for callee-saves.
#if defined(__arm__)
  // s29 is a callee-save, so should be preserved.
  __asm__ __volatile__("vmov.f32 s29, %[a]": /* no output */ : [a] "r" (a));
#endif
  env->CallStaticVoidMethod(cls, mid, 1, 1, 1L);
#if defined(__arm__)
  float b;
  __asm__ __volatile__("vmov.f32 %[b], s29": [b] "=r" (b));
  CHECK_EQ(bit_cast<int32_t>(a), bit_cast<int32_t>(b));
#endif
}

}  // namespace art
