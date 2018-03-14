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

#include <inttypes.h>

#include <cstdio>
#include <memory>

#include "android-base/logging.h"
#include "android-base/stringprintf.h"

#include "jni.h"
#include "jvmti.h"
#include "scoped_local_ref.h"

// Test infrastructure
#include "jni_binder.h"
#include "jni_helper.h"
#include "jvmti_helper.h"
#include "test_env.h"
#include "ti_macros.h"

namespace art {
namespace Test1950PopFrame {

struct TestData {
  jlocation target_loc;
  jmethodID target_method;
  jrawMonitorID notify_monitor;
  bool hit_location;
};

void JNICALL cbSingleStep(jvmtiEnv* jvmti,
                          JNIEnv* env,
                          jthread thr,
                          jmethodID meth,
                          jlocation loc) {
  TestData *data;
  if (JvmtiErrorToException(env,
                            jvmti,
                            jvmti->GetThreadLocalStorage(thr, reinterpret_cast<void**>(&data)))) {
    return;
  }
  CHECK(data != nullptr);
  if (meth != data->target_method || loc != data->target_loc) {
    return;
  }
  // Wake up the waiting thread.
  JvmtiErrorToException(env, jvmti, jvmti->RawMonitorEnter(data->notify_monitor));
  data->hit_location = true;
  JvmtiErrorToException(env, jvmti, jvmti->RawMonitorNotifyAll(data->notify_monitor));
  JvmtiErrorToException(env, jvmti, jvmti->RawMonitorExit(data->notify_monitor));
  // Suspend ourself
  jvmti->SuspendThread(nullptr);
}

void JNICALL cbBreakpointHit(jvmtiEnv* jvmti,
                             JNIEnv* env,
                             jthread thr,
                             jmethodID method,
                             jlocation loc) {
  TestData *data;
  if (JvmtiErrorToException(env,
                            jvmti,
                            jvmti->GetThreadLocalStorage(thr, reinterpret_cast<void**>(&data)))) {
    return;
  }
  CHECK(data != nullptr);
  if (method != data->target_method || loc != data->target_loc) {
    // TODO What to do here.
    LOG(FATAL) << "Strange, shouldn't get here!";
  }
  // Wake up the waiting thread.
  JvmtiErrorToException(env, jvmti, jvmti->RawMonitorEnter(data->notify_monitor));
  data->hit_location = true;
  JvmtiErrorToException(env, jvmti, jvmti->RawMonitorNotifyAll(data->notify_monitor));
  JvmtiErrorToException(env, jvmti, jvmti->RawMonitorExit(data->notify_monitor));
  // Suspend ourself
  jvmti->SuspendThread(nullptr);
}

extern "C" JNIEXPORT
void JNICALL Java_art_Test1950_setupTest(JNIEnv* env, jclass klass ATTRIBUTE_UNUSED) {
  jvmtiCapabilities caps;
  memset(&caps, 0, sizeof(caps));
  // Most of these will already be there but might as well be complete.
  caps.can_pop_frame = 1;
  caps.can_generate_single_step_events = 1;
  caps.can_generate_breakpoint_events = 1;
  caps.can_suspend = 1;
  caps.can_generate_method_entry_events = 1;
  caps.can_generate_method_exit_events = 1;
  caps.can_generate_monitor_events = 1;
  caps.can_generate_exception_events = 1;
  caps.can_generate_frame_pop_events = 1;
  caps.can_generate_field_access_events = 1;
  caps.can_generate_field_modification_events = 1;
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->AddCapabilities(&caps))) {
    return;
  }
  jvmtiEventCallbacks cb;
  memset(&cb, 0, sizeof(cb));
  // TODO Add the rest of these.
  cb.Breakpoint = cbBreakpointHit;
  cb.SingleStep = cbSingleStep;
  JvmtiErrorToException(env, jvmti_env, jvmti_env->SetEventCallbacks(&cb, sizeof(cb)));
}

static TestData* SetupTestData(JNIEnv* env, jobject meth, jlocation loc) {
  TestData *data;
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->Allocate(sizeof(TestData),
                                                reinterpret_cast<uint8_t**>(&data)))) {
    return nullptr;
  }
  data->target_loc = loc;
  data->target_method = env->FromReflectedMethod(meth);
  data->hit_location = false;
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->CreateRawMonitor("SuspendStopMonitor",
                                                        &data->notify_monitor))) {
    jvmti_env->Deallocate(reinterpret_cast<uint8_t*>(data));
    return nullptr;
  }
  return data;
}

extern "C" JNIEXPORT
void JNICALL Java_art_Test1950_setupSuspendBreakpointFor(JNIEnv* env,
                                                         jclass klass ATTRIBUTE_UNUSED,
                                                         jobject meth,
                                                         jlocation loc,
                                                         jthread thr) {
  TestData *data;
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->GetThreadLocalStorage(thr,
                                                             reinterpret_cast<void**>(&data)))) {
    return;
  }
  CHECK(data == nullptr) << "Data was not cleared!";
  data = SetupTestData(env, meth, loc);
  if (data == nullptr) {
    return;
  }
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->SetThreadLocalStorage(thr, data))) {
    return;
  }
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->SetEventNotificationMode(JVMTI_ENABLE,
                                                                JVMTI_EVENT_BREAKPOINT,
                                                                thr))) {
    return;
  }
  JvmtiErrorToException(env, jvmti_env, jvmti_env->SetBreakpoint(data->target_method,
                                                                 data->target_loc));
}

extern "C" JNIEXPORT
void JNICALL Java_art_Test1950_clearSuspendBreakpointFor(JNIEnv* env,
                                                         jclass klass ATTRIBUTE_UNUSED,
                                                         jthread thr) {
  TestData *data;
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->GetThreadLocalStorage(thr,
                                                             reinterpret_cast<void**>(&data)))) {
    return;
  }
  CHECK(data != nullptr);
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->SetEventNotificationMode(JVMTI_DISABLE,
                                                                JVMTI_EVENT_BREAKPOINT,
                                                                thr))) {
    return;
  }
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->ClearBreakpoint(data->target_method,
                                                       data->target_loc))) {
    return;
  }
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->SetThreadLocalStorage(thr, nullptr))) {
    return;
  }
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->Deallocate(reinterpret_cast<uint8_t*>(data)))) {
    return;
  }
  // TODO
}

extern "C" JNIEXPORT
void JNICALL Java_art_Test1950_waitForSuspendHit(JNIEnv* env,
                                                 jclass klass ATTRIBUTE_UNUSED,
                                                 jthread thr) {
  TestData *data;
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->GetThreadLocalStorage(thr,
                                                             reinterpret_cast<void**>(&data)))) {
    return;
  }
  CHECK(data != nullptr);
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->RawMonitorEnter(data->notify_monitor))) {
    return;
  }
  while (!data->hit_location) {
    if (JvmtiErrorToException(env, jvmti_env, jvmti_env->RawMonitorWait(data->notify_monitor, -1))) {
      return;
    }
  }
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->RawMonitorExit(data->notify_monitor))) {
    return;
  }
  jint state = 0;
  while (!JvmtiErrorToException(env, jvmti_env, jvmti_env->GetThreadState(thr, &state)) &&
         (state & JVMTI_THREAD_STATE_SUSPENDED) == 0) { }
}

extern "C" JNIEXPORT
void JNICALL Java_art_Test1950_popFrame(JNIEnv* env,
                                        jclass klass ATTRIBUTE_UNUSED,
                                        jthread thr) {
  JvmtiErrorToException(env, jvmti_env, jvmti_env->PopFrame(thr));
}

}  // namespace Test1950PopFrame
}  // namespace art

