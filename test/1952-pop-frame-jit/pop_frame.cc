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
namespace Test1952PopFrameJit {

struct TestData {
  jlocation target_loc;
  jmethodID target_method;
  jclass target_klass;
  jfieldID target_field;
  jrawMonitorID notify_monitor;
  bool hit_location;

  TestData(jvmtiEnv* jvmti, JNIEnv* env, jlocation loc, jobject meth, jclass klass, jobject field)
      : target_loc(loc),
        target_method(meth != nullptr ? env->FromReflectedMethod(meth) : nullptr),
        target_klass(reinterpret_cast<jclass>(env->NewGlobalRef(klass))),
        target_field(field != nullptr ? env->FromReflectedField(field) : nullptr),
        hit_location(false) {
    JvmtiErrorToException(env, jvmti, jvmti->CreateRawMonitor("SuspendStopMonitor",
                                                              &notify_monitor));
  }

  void PerformSuspend(jvmtiEnv* jvmti, JNIEnv* env) {
    // Wake up the waiting thread.
    JvmtiErrorToException(env, jvmti, jvmti->RawMonitorEnter(notify_monitor));
    hit_location = true;
    JvmtiErrorToException(env, jvmti, jvmti->RawMonitorNotifyAll(notify_monitor));
    JvmtiErrorToException(env, jvmti, jvmti->RawMonitorExit(notify_monitor));
    // Suspend ourself
    jvmti->SuspendThread(nullptr);
  }
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
  data->PerformSuspend(jvmti, env);
}

void JNICALL cbExceptionCatch(jvmtiEnv *jvmti,
                              JNIEnv* env,
                              jthread thr,
                              jmethodID method,
                              jlocation location ATTRIBUTE_UNUSED,
                              jobject exception ATTRIBUTE_UNUSED) {
  TestData *data;
  if (JvmtiErrorToException(env,
                            jvmti,
                            jvmti->GetThreadLocalStorage(thr, reinterpret_cast<void**>(&data)))) {
    return;
  }
  CHECK(data != nullptr);
  if (method != data->target_method) {
    return;
  }
  data->PerformSuspend(jvmti, env);
}

void JNICALL cbException(jvmtiEnv *jvmti,
                         JNIEnv* env,
                         jthread thr,
                         jmethodID method,
                         jlocation location ATTRIBUTE_UNUSED,
                         jobject exception ATTRIBUTE_UNUSED,
                         jmethodID catch_method ATTRIBUTE_UNUSED,
                         jlocation catch_location ATTRIBUTE_UNUSED) {
  TestData *data;
  if (JvmtiErrorToException(env,
                            jvmti,
                            jvmti->GetThreadLocalStorage(thr, reinterpret_cast<void**>(&data)))) {
    return;
  }
  CHECK(data != nullptr);
  if (method != data->target_method) {
    return;
  }
  data->PerformSuspend(jvmti, env);
}

void JNICALL cbMethodEntry(jvmtiEnv *jvmti,
                           JNIEnv* env,
                           jthread thr,
                           jmethodID method) {
  TestData *data;
  if (JvmtiErrorToException(env,
                            jvmti,
                            jvmti->GetThreadLocalStorage(thr, reinterpret_cast<void**>(&data)))) {
    return;
  }
  CHECK(data != nullptr);
  if (method != data->target_method) {
    return;
  }
  data->PerformSuspend(jvmti, env);
}

void JNICALL cbMethodExit(jvmtiEnv *jvmti,
                          JNIEnv* env,
                          jthread thr,
                          jmethodID method,
                          jboolean was_popped_by_exception ATTRIBUTE_UNUSED,
                          jvalue return_value ATTRIBUTE_UNUSED) {
  TestData *data;
  if (JvmtiErrorToException(env,
                            jvmti,
                            jvmti->GetThreadLocalStorage(thr, reinterpret_cast<void**>(&data)))) {
    return;
  }
  CHECK(data != nullptr);
  if (method != data->target_method) {
    return;
  }
  data->PerformSuspend(jvmti, env);
}

void JNICALL cbFieldModification(jvmtiEnv* jvmti,
                                 JNIEnv* env,
                                 jthread thr,
                                 jmethodID method ATTRIBUTE_UNUSED,
                                 jlocation location ATTRIBUTE_UNUSED,
                                 jclass field_klass ATTRIBUTE_UNUSED,
                                 jobject object ATTRIBUTE_UNUSED,
                                 jfieldID field,
                                 char signature_type ATTRIBUTE_UNUSED,
                                 jvalue new_value ATTRIBUTE_UNUSED) {
  TestData *data;
  if (JvmtiErrorToException(env,
                            jvmti,
                            jvmti->GetThreadLocalStorage(thr, reinterpret_cast<void**>(&data)))) {
    return;
  }
  CHECK(data != nullptr);
  if (field != data->target_field) {
    // TODO What to do here.
    LOG(FATAL) << "Strange, shouldn't get here!";
  }
  data->PerformSuspend(jvmti, env);
}

void JNICALL cbFieldAccess(jvmtiEnv* jvmti,
                           JNIEnv* env,
                           jthread thr,
                           jmethodID method ATTRIBUTE_UNUSED,
                           jlocation location ATTRIBUTE_UNUSED,
                           jclass field_klass,
                           jobject object ATTRIBUTE_UNUSED,
                           jfieldID field) {
  TestData *data;
  if (JvmtiErrorToException(env,
                            jvmti,
                            jvmti->GetThreadLocalStorage(thr, reinterpret_cast<void**>(&data)))) {
    return;
  }
  CHECK(data != nullptr);
  if (field != data->target_field || !env->IsSameObject(field_klass, data->target_klass)) {
    // TODO What to do here.
    LOG(FATAL) << "Strange, shouldn't get here!";
  }
  data->PerformSuspend(jvmti, env);
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
  data->PerformSuspend(jvmti, env);
}

extern "C" JNIEXPORT
void JNICALL Java_Main_setupTest(JNIEnv* env, jclass klass ATTRIBUTE_UNUSED) {
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
  cb.FieldAccess = cbFieldAccess;
  cb.FieldModification = cbFieldModification;
  cb.MethodEntry = cbMethodEntry;
  cb.MethodExit = cbMethodExit;
  cb.Exception = cbException;
  cb.ExceptionCatch = cbExceptionCatch;
  JvmtiErrorToException(env, jvmti_env, jvmti_env->SetEventCallbacks(&cb, sizeof(cb)));
}

static bool DeleteTestData(JNIEnv* env, jthread thr, TestData* data) {
  env->DeleteGlobalRef(data->target_klass);
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->SetThreadLocalStorage(thr, nullptr))) {
    return false;
  }
  return JvmtiErrorToException(env,
                               jvmti_env,
                               jvmti_env->Deallocate(reinterpret_cast<uint8_t*>(data)));
}

static TestData* SetupTestData(
    JNIEnv* env, jobject meth, jlocation loc, jclass target_klass, jobject field) {
  void* data_ptr;
  TestData *data;
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->Allocate(sizeof(TestData),
                                                reinterpret_cast<uint8_t**>(&data_ptr)))) {
    return nullptr;
  }
  data = new (data_ptr) TestData(jvmti_env, env, loc, meth, target_klass, field);
  if (env->ExceptionCheck()) {
    env->DeleteGlobalRef(data->target_klass);
    jvmti_env->Deallocate(reinterpret_cast<uint8_t*>(data));
    return nullptr;
  }
  return data;
}


extern "C" JNIEXPORT
void JNICALL Java_Main_setupSuspendSingleStepAt(JNIEnv* env,
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
  data = SetupTestData(env, meth, loc, nullptr, nullptr);
  if (data == nullptr) {
    return;
  }
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->SetThreadLocalStorage(thr, data))) {
    return;
  }
  JvmtiErrorToException(env, jvmti_env, jvmti_env->SetEventNotificationMode(JVMTI_ENABLE,
                                                                            JVMTI_EVENT_SINGLE_STEP,
                                                                            thr));
}

extern "C" JNIEXPORT
void JNICALL Java_Main_clearSuspendSingleStepFor(JNIEnv* env,
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
                                                                JVMTI_EVENT_SINGLE_STEP,
                                                                thr))) {
    return;
  }
  DeleteTestData(env, thr, data);
}

extern "C" JNIEXPORT
void JNICALL Java_Main_setupSuspendBreakpointFor(JNIEnv* env,
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
  data = SetupTestData(env, meth, loc, nullptr, nullptr);
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
void JNICALL Java_Main_clearSuspendBreakpointFor(JNIEnv* env,
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
  DeleteTestData(env, thr, data);
}

extern "C" JNIEXPORT
void JNICALL Java_Main_setupSuspendExceptionEvent(JNIEnv* env,
                                                  jclass klass ATTRIBUTE_UNUSED,
                                                  jobject method,
                                                  jboolean is_catch,
                                                  jthread thr) {
  TestData *data;
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->GetThreadLocalStorage(
                                thr, reinterpret_cast<void**>(&data)))) {
    return;
  }
  CHECK(data == nullptr) << "Data was not cleared!";
  data = SetupTestData(env, method, 0, nullptr, nullptr);
  if (data == nullptr) {
    return;
  }
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->SetThreadLocalStorage(thr, data))) {
    return;
  }
  JvmtiErrorToException(env,
                        jvmti_env,
                        jvmti_env->SetEventNotificationMode(
                            JVMTI_ENABLE,
                            is_catch ? JVMTI_EVENT_EXCEPTION_CATCH : JVMTI_EVENT_EXCEPTION,
                            thr));
}

extern "C" JNIEXPORT
void JNICALL Java_Main_clearSuspendExceptionEvent(JNIEnv* env,
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
                                                                JVMTI_EVENT_EXCEPTION_CATCH,
                                                                thr))) {
    return;
  }
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->SetEventNotificationMode(JVMTI_DISABLE,
                                                                JVMTI_EVENT_EXCEPTION,
                                                                thr))) {
    return;
  }
  DeleteTestData(env, thr, data);
}

extern "C" JNIEXPORT
void JNICALL Java_Main_setupSuspendMethodEvent(JNIEnv* env,
                                               jclass klass ATTRIBUTE_UNUSED,
                                               jobject method,
                                               jboolean enter,
                                               jthread thr) {
  TestData *data;
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->GetThreadLocalStorage(
                                thr, reinterpret_cast<void**>(&data)))) {
    return;
  }
  CHECK(data == nullptr) << "Data was not cleared!";
  data = SetupTestData(env, method, 0, nullptr, nullptr);
  if (data == nullptr) {
    return;
  }
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->SetThreadLocalStorage(thr, data))) {
    return;
  }
  JvmtiErrorToException(env,
                        jvmti_env,
                        jvmti_env->SetEventNotificationMode(
                            JVMTI_ENABLE,
                            enter ? JVMTI_EVENT_METHOD_ENTRY : JVMTI_EVENT_METHOD_EXIT,
                            thr));
}

extern "C" JNIEXPORT
void JNICALL Java_Main_clearSuspendMethodEvent(JNIEnv* env,
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
                                                                JVMTI_EVENT_METHOD_EXIT,
                                                                thr))) {
    return;
  }
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->SetEventNotificationMode(JVMTI_DISABLE,
                                                                JVMTI_EVENT_METHOD_ENTRY,
                                                                thr))) {
    return;
  }
  DeleteTestData(env, thr, data);
}

extern "C" JNIEXPORT
void JNICALL Java_Main_setupFieldSuspendFor(JNIEnv* env,
                                            jclass klass ATTRIBUTE_UNUSED,
                                            jclass target_klass,
                                            jobject field,
                                            jboolean access,
                                            jthread thr) {
  TestData *data;
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->GetThreadLocalStorage(
                                thr, reinterpret_cast<void**>(&data)))) {
    return;
  }
  CHECK(data == nullptr) << "Data was not cleared!";
  data = SetupTestData(env, nullptr, 0, target_klass, field);
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
                            jvmti_env->SetEventNotificationMode(
                                JVMTI_ENABLE,
                                access ? JVMTI_EVENT_FIELD_ACCESS : JVMTI_EVENT_FIELD_MODIFICATION,
                                thr))) {
    return;
  }
  if (access) {
    JvmtiErrorToException(env, jvmti_env, jvmti_env->SetFieldAccessWatch(data->target_klass,
                                                                         data->target_field));
  } else {
    JvmtiErrorToException(env, jvmti_env, jvmti_env->SetFieldModificationWatch(data->target_klass,
                                                                               data->target_field));
  }
}

extern "C" JNIEXPORT
void JNICALL Java_Main_clearFieldSuspendFor(JNIEnv* env,
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
                                                                JVMTI_EVENT_FIELD_ACCESS,
                                                                thr))) {
    return;
  }
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->SetEventNotificationMode(JVMTI_DISABLE,
                                                                JVMTI_EVENT_FIELD_MODIFICATION,
                                                                thr))) {
    return;
  }
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->ClearFieldModificationWatch(
                                data->target_klass, data->target_field)) &&
      JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->ClearFieldAccessWatch(
                                data->target_klass, data->target_field))) {
    return;
  } else {
    env->ExceptionClear();
  }
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->SetThreadLocalStorage(thr, nullptr))) {
    return;
  }
  DeleteTestData(env, thr, data);
}

extern "C" JNIEXPORT
void JNICALL Java_Main_waitForSuspendHit(JNIEnv* env,
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
void JNICALL Java_Main_popFrame(JNIEnv* env,
                                jclass klass ATTRIBUTE_UNUSED,
                                jthread thr) {
  JvmtiErrorToException(env, jvmti_env, jvmti_env->PopFrame(thr));
}

}  // namespace Test1952PopFrameJit
}  // namespace art

