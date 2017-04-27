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

#include <inttypes.h>
#include <memory>
#include <sstream>
#include <stdio.h>
#include <dlfcn.h>

#include "android-base/stringprintf.h"
#include "jni.h"
#include "jvmti.h"

// Test infrastructure
#include "jni_binder.h"
#include "jvmti_helper.h"
#include "test_env.h"
#include "scoped_local_ref.h"

namespace art {
namespace Test988MethodTrace {

// Taken from art/runtime/modifiers.h
static constexpr uint32_t kAccStatic =       0x0008;  // field, method, ic

static jclass test_klass = nullptr;
static jmethodID enter_method = nullptr;
static jmethodID exit_method = nullptr;
static bool in_callback = false;

static jobject GetJavaMethod(jvmtiEnv* jvmti, JNIEnv* env, jmethodID m) {
  jint mods = 0;
  if (JvmtiErrorToException(env, jvmti, jvmti->GetMethodModifiers(m, &mods))) {
    return nullptr;
  }

  bool is_static = (mods & kAccStatic) != 0;
  jclass method_klass = nullptr;
  if (JvmtiErrorToException(env, jvmti, jvmti->GetMethodDeclaringClass(m, &method_klass))) {
    return nullptr;
  }
  jobject res = env->ToReflectedMethod(method_klass, m, is_static);
  env->DeleteLocalRef(method_klass);
  return res;
}

static jobject GetJavaValue(jvmtiEnv* jvmtienv,
                            JNIEnv* env,
                            jmethodID m,
                            jvalue value) {
  char *fname, *fsig, *fgen;
  if (JvmtiErrorToException(env, jvmtienv, jvmtienv->GetMethodName(m, &fname, &fsig, &fgen))) {
    return nullptr;
  }
  std::string type(fsig);
  type = type.substr(type.find(")") + 1);
  jvmtienv->Deallocate(reinterpret_cast<unsigned char*>(fsig));
  jvmtienv->Deallocate(reinterpret_cast<unsigned char*>(fname));
  jvmtienv->Deallocate(reinterpret_cast<unsigned char*>(fgen));
  std::string name;
  switch (type[0]) {
    case 'V':
      return nullptr;
    case '[':
    case 'L':
      return value.l;
    case 'Z':
      name = "java/lang/Boolean";
      break;
    case 'B':
      name = "java/lang/Byte";
      break;
    case 'C':
      name = "java/lang/Character";
      break;
    case 'S':
      name = "java/lang/Short";
      break;
    case 'I':
      name = "java/lang/Integer";
      break;
    case 'J':
      name = "java/lang/Long";
      break;
    case 'F':
      name = "java/lang/Float";
      break;
    case 'D':
      name = "java/lang/Double";
      break;
    default:
      LOG(FATAL) << "Unable to figure out type!";
      return nullptr;
  }
  std::ostringstream oss;
  oss << "(" << type[0] << ")L" << name << ";";
  std::string args = oss.str();
  jclass target = env->FindClass(name.c_str());
  jmethodID valueOfMethod = env->GetStaticMethodID(target, "valueOf", args.c_str());

  CHECK(valueOfMethod != nullptr) << args;
  jobject res = env->CallStaticObjectMethodA(target, valueOfMethod, &value);
  env->DeleteLocalRef(target);
  return res;
}

static void methodExitCB(jvmtiEnv* jvmti,
                         JNIEnv* jnienv,
                         jthread thr ATTRIBUTE_UNUSED,
                         jmethodID method,
                         jboolean was_popped_by_exception,
                         jvalue return_value) {
  if (method == exit_method || method == enter_method || in_callback) {
    // Don't do callback for either of these for infinite loop.
    return;
  }
  in_callback = true;
  jobject method_arg = GetJavaMethod(jvmti, jnienv, method);
  jobject result =
      was_popped_by_exception ? nullptr : GetJavaValue(jvmti, jnienv, method, return_value);
  if (jnienv->ExceptionCheck()) {
    in_callback = false;
    return;
  }
  jnienv->CallStaticVoidMethod(test_klass, exit_method,
                               method_arg, was_popped_by_exception, result);
  jnienv->DeleteLocalRef(method_arg);
  in_callback = false;
}

static void methodEntryCB(jvmtiEnv* jvmti,
                          JNIEnv* jnienv,
                          jthread thr ATTRIBUTE_UNUSED,
                          jmethodID method) {
  if (method == exit_method || method == enter_method || in_callback) {
    // Don't do callback for either of these for infinite loop.
    return;
  }
  in_callback = true;
  jobject method_arg = GetJavaMethod(jvmti, jnienv, method);
  if (jnienv->ExceptionCheck()) {
    return;
  }
  jnienv->CallStaticVoidMethod(test_klass, enter_method, method_arg);
  jnienv->DeleteLocalRef(method_arg);
  in_callback = false;
}

extern "C" JNIEXPORT void JNICALL Java_art_Test988_enableMethodTracing(
    JNIEnv* env, jclass klass, jobject enter, jobject exit) {
  test_klass = reinterpret_cast<jclass>(env->NewGlobalRef(klass));
  enter_method = env->FromReflectedMethod(enter);
  exit_method = env->FromReflectedMethod(exit);
  jvmtiEventCallbacks cb;
  memset(&cb, 0, sizeof(cb));
  cb.MethodEntry = methodEntryCB;
  cb.MethodExit = methodExitCB;
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->SetEventCallbacks(&cb, sizeof(cb)))) {
    return;
  }
  jthread thr;
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->GetCurrentThread(&thr))) {
    // Couldn't get current thread;
    return;
  }
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->SetEventNotificationMode(JVMTI_ENABLE,
                                                                JVMTI_EVENT_METHOD_ENTRY,
                                                                thr))) {
    return;
  }
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->SetEventNotificationMode(JVMTI_ENABLE,
                                                                JVMTI_EVENT_METHOD_EXIT,
                                                                thr))) {
    return;
  }
}

extern "C" JNIEXPORT void JNICALL Java_art_Test988_disableMethodTracing(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED) {
  jthread thr;
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->GetCurrentThread(&thr))) {
    // Couldn't get current thread;
    return;
  }
  jvmtiError res = jvmti_env->SetEventNotificationMode(JVMTI_DISABLE,
                                                       JVMTI_EVENT_METHOD_ENTRY,
                                                       thr);
  jvmtiError res2 = jvmti_env->SetEventNotificationMode(JVMTI_DISABLE,
                                                        JVMTI_EVENT_METHOD_EXIT,
                                                        thr);
  if (res != JVMTI_ERROR_NONE) {
    JvmtiErrorToException(env, jvmti_env, res);
    return;
  }
  if (res2 != JVMTI_ERROR_NONE) {
    JvmtiErrorToException(env, jvmti_env, res2);
    return;
  }
}

}  // namespace Test988MethodTrace
}  // namespace art
