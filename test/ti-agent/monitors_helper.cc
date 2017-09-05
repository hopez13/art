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

#include <vector>

#include "jvmti_helper.h"
#include "test_env.h"
#include "scoped_local_ref.h"

namespace art {
namespace common_monitors {

struct MonitorsData {
  jclass test_klass;
  jmethodID monitor_enter;
  jmethodID monitor_entered;
  jmethodID monitor_wait;
  jmethodID monitor_waited;
  jclass monitor_klass;
};

static void monitorEnterCB(jvmtiEnv* jvmti,
                           JNIEnv* jnienv,
                           jthread thr,
                           jobject obj) {
  MonitorsData* data = nullptr;
  if (JvmtiErrorToException(jnienv, jvmti,
                            jvmti->GetEnvironmentLocalStorage(reinterpret_cast<void**>(&data)))) {
    return;
  }
  if (!jnienv->IsInstanceOf(obj, data->monitor_klass)) {
    return;
  }
  jnienv->CallStaticVoidMethod(data->test_klass, data->monitor_enter, thr, obj);
}
static void monitorEnteredCB(jvmtiEnv* jvmti,
                             JNIEnv* jnienv,
                             jthread thr,
                             jobject obj) {
  MonitorsData* data = nullptr;
  if (JvmtiErrorToException(jnienv, jvmti,
                            jvmti->GetEnvironmentLocalStorage(reinterpret_cast<void**>(&data)))) {
    return;
  }
  if (!jnienv->IsInstanceOf(obj, data->monitor_klass)) {
    return;
  }
  jnienv->CallStaticVoidMethod(data->test_klass, data->monitor_entered, thr, obj);
}
static void monitorWaitCB(jvmtiEnv* jvmti,
                          JNIEnv* jnienv,
                          jthread thr,
                          jobject obj,
                          jlong timeout) {
  MonitorsData* data = nullptr;
  if (JvmtiErrorToException(jnienv, jvmti,
                            jvmti->GetEnvironmentLocalStorage(reinterpret_cast<void**>(&data)))) {
    return;
  }
  if (!jnienv->IsInstanceOf(obj, data->monitor_klass)) {
    return;
  }
  jnienv->CallStaticVoidMethod(data->test_klass, data->monitor_wait, thr, obj, timeout);
}
static void monitorWaitedCB(jvmtiEnv* jvmti,
                            JNIEnv* jnienv,
                            jthread thr,
                            jobject obj,
                            jboolean timed_out) {
  MonitorsData* data = nullptr;
  if (JvmtiErrorToException(jnienv, jvmti,
                            jvmti->GetEnvironmentLocalStorage(reinterpret_cast<void**>(&data)))) {
    return;
  }
  if (!jnienv->IsInstanceOf(obj, data->monitor_klass)) {
    return;
  }
  jnienv->CallStaticVoidMethod(data->test_klass, data->monitor_waited, thr, obj, timed_out);
}

extern "C" JNIEXPORT void JNICALL Java_art_Monitors_setupMonitorEvents(
    JNIEnv* env,
    jclass,
    jclass test_klass,
    jobject monitor_enter,
    jobject monitor_entered,
    jobject monitor_wait,
    jobject monitor_waited,
    jclass monitor_klass,
    jthread thr) {
  MonitorsData* data = nullptr;
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->Allocate(sizeof(MonitorsData),
                                                reinterpret_cast<unsigned char**>(&data)))) {
    return;
  }
  memset(data, 0, sizeof(MonitorsData));
  data->test_klass = reinterpret_cast<jclass>(env->NewGlobalRef(test_klass));
  data->monitor_enter = env->FromReflectedMethod(monitor_enter);
  data->monitor_entered = env->FromReflectedMethod(monitor_entered);
  data->monitor_wait = env->FromReflectedMethod(monitor_wait);
  data->monitor_waited = env->FromReflectedMethod(monitor_waited);
  data->monitor_klass = reinterpret_cast<jclass>(env->NewGlobalRef(monitor_klass));
  MonitorsData* old_data = nullptr;
  if (JvmtiErrorToException(env, jvmti_env,
                            jvmti_env->GetEnvironmentLocalStorage(
                                reinterpret_cast<void**>(&old_data)))) {
    return;
  } else if (old_data != nullptr && old_data->test_klass != nullptr) {
    ScopedLocalRef<jclass> rt_exception(env, env->FindClass("java/lang/RuntimeException"));
    env->ThrowNew(rt_exception.get(), "Environment already has local storage set!");
    return;
  }
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->SetEnvironmentLocalStorage(data))) {
    return;
  }

  jvmtiEventCallbacks cb;
  memset(&cb, 0, sizeof(cb));
  cb.MonitorContendedEnter = monitorEnterCB;
  cb.MonitorContendedEntered = monitorEnteredCB;
  cb.MonitorWait = monitorWaitCB;
  cb.MonitorWaited = monitorWaitedCB;
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->SetEventCallbacks(&cb, sizeof(cb)))) {
    return;
  }
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->SetEventNotificationMode(
                                JVMTI_ENABLE, JVMTI_EVENT_MONITOR_CONTENDED_ENTER, thr))) {
    return;
  }
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->SetEventNotificationMode(
                                JVMTI_ENABLE, JVMTI_EVENT_MONITOR_CONTENDED_ENTERED, thr))) {
    return;
  }
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->SetEventNotificationMode(
                                JVMTI_ENABLE, JVMTI_EVENT_MONITOR_WAIT, thr))) {
    return;
  }
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->SetEventNotificationMode(
                                JVMTI_ENABLE, JVMTI_EVENT_MONITOR_WAITED, thr))) {
    return;
  }
}

// extern "C" JNIEXPORT jboolean JNICALL Java_art_Suspension_isSuspended(
//     JNIEnv* env, jclass, jthread thr) {
//   jint state;
//   if (JvmtiErrorToException(env, jvmti_env, jvmti_env->GetThreadState(thr, &state))) {
//     return false;
//   }
//   return (state & JVMTI_THREAD_STATE_SUSPENDED) != 0;
// }

// static std::vector<jthread> CopyToVector(JNIEnv* env, jobjectArray thrs) {
//   jsize len = env->GetArrayLength(thrs);
//   std::vector<jthread> ret;
//   for (jsize i = 0; i < len; i++) {
//     ret.push_back(reinterpret_cast<jthread>(env->GetObjectArrayElement(thrs, i)));
//   }
//   return ret;
// }

// extern "C" JNIEXPORT jintArray JNICALL Java_art_Suspension_resumeList(JNIEnv* env,
//                                                                       jclass,
//                                                                       jobjectArray thr) {
//   static_assert(sizeof(jvmtiError) == sizeof(jint), "cannot use jintArray as jvmtiError array");
//   std::vector<jthread> threads(CopyToVector(env, thr));
//   if (env->ExceptionCheck()) {
//     return nullptr;
//   }
//   jintArray ret = env->NewIntArray(threads.size());
//   if (env->ExceptionCheck()) {
//     return nullptr;
//   }
//   jint* elems = env->GetIntArrayElements(ret, nullptr);
//   JvmtiErrorToException(env, jvmti_env,
//                         jvmti_env->ResumeThreadList(threads.size(),
//                                                     threads.data(),
//                                                     reinterpret_cast<jvmtiError*>(elems)));
//   env->ReleaseIntArrayElements(ret, elems, 0);
//   return ret;
// }

// extern "C" JNIEXPORT jintArray JNICALL Java_art_Suspension_suspendList(JNIEnv* env,
//                                                                        jclass,
//                                                                        jobjectArray thrs) {
//   static_assert(sizeof(jvmtiError) == sizeof(jint), "cannot use jintArray as jvmtiError array");
//   std::vector<jthread> threads(CopyToVector(env, thrs));
//   if (env->ExceptionCheck()) {
//     return nullptr;
//   }
//   jintArray ret = env->NewIntArray(threads.size());
//   if (env->ExceptionCheck()) {
//     return nullptr;
//   }
//   jint* elems = env->GetIntArrayElements(ret, nullptr);
//   JvmtiErrorToException(env, jvmti_env,
//                         jvmti_env->SuspendThreadList(threads.size(),
//                                                      threads.data(),
//                                                      reinterpret_cast<jvmtiError*>(elems)));
//   env->ReleaseIntArrayElements(ret, elems, 0);
//   return ret;
// }

// extern "C" JNIEXPORT void JNICALL Java_art_Suspension_resume(JNIEnv* env, jclass, jthread thr) {
//   JvmtiErrorToException(env, jvmti_env, jvmti_env->ResumeThread(thr));
// }

// extern "C" JNIEXPORT void JNICALL Java_art_Suspension_suspend(JNIEnv* env, jclass, jthread thr) {
//   JvmtiErrorToException(env, jvmti_env, jvmti_env->SuspendThread(thr));
// }

}  // namespace common_monitors
}  // namespace art

