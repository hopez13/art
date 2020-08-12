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

#include "android-base/logging.h"
#include "jni.h"

#include "lock_word.h"
#include "mirror/object-inl.h"
#include "monitor-inl.h"
#include "scoped_thread_state_change-inl.h"
#include "test_env.h"
#include "thread-inl.h"


namespace art {
namespace Test1932MonitorEventsMiscInternal {

extern "C" JNIEXPORT void JNICALL Java_Main_DeflateMonitor(JNIEnv* env, jclass, jobject lock) {
  if (IsJVM()) {
    return;
  }
  ScopedObjectAccess soa(env);
  // Ignore the return-value we don't care if we succeeded
  Monitor::Deflate(soa.Self(), soa.Decode<mirror::Object>(lock));
}

}  // namespace Test1932MonitorEventsMiscInternal
}  // namespace art
