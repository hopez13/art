// Copyright (C) 2017 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <android-base/logging.h>
#include <atomic>
#include <dlfcn.h>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <jni.h>
#include <jvmti.h>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace breakpointtest {

struct BreakpointTarget {
  std::string class_name;
  std::string method_name;
  std::string method_sig;
  jlong location;
};

static void vmInitCB(jvmtiEnv* jvmti, JNIEnv* env, jthread thr ATTRIBUTE_UNUSED) {
  BreakpointTarget* target = nullptr;
  jvmtiError err = jvmti->GetEnvironmentLocalStorage(reinterpret_cast<void**>(&target));
  if (err != JVMTI_ERROR_NONE || target == nullptr) {
    LOG(FATAL) << "unable to get breakpoint target";
  }
  jclass k = env->FindClass(target->class_name.c_str());
  if (env->ExceptionCheck()) {
    env->ExceptionDescribe();
    env->FatalError("Could not find class!");
    return;
  }
  jmethodID m = env->GetMethodID(k, target->method_name.c_str(), target->method_sig.c_str());
  if (env->ExceptionCheck()) {
    env->ExceptionDescribe();
    env->FatalError("Could not find method!");
    return;
  }
  err = jvmti->SetBreakpoint(m, target->location);
  if (err != JVMTI_ERROR_NONE) {
    LOG(FATAL) << "unable to set breakpoint";
  }
}
static void breakpointCB(jvmtiEnv* jvmti_env ATTRIBUTE_UNUSED,
                         JNIEnv* env ATTRIBUTE_UNUSED,
                         jthread thread ATTRIBUTE_UNUSED,
                         jmethodID method ATTRIBUTE_UNUSED,
                         jlocation location ATTRIBUTE_UNUSED) {
}

static std::string substrOf(const std::string& s, size_t start, size_t end) {
  if (end == start) {
    return "";
  } else if (end == std::string::npos) {
    end = s.size();
  }
  return s.substr(start, end - start);
}

static bool ParseArgs(const std::string& start_options,
                      /*out*/std::string* class_name,
                      /*out*/std::string* method_name,
                      /*out*/std::string* method_sig,
                      /*out*/jlong* location) {
  std::string options = start_options;
  if (options.find(',') == std::string::npos) {
    LOG(ERROR) << "no class in " << options;
    return false;
  }
  *class_name = substrOf(options, 0, options.find(','));
  options = substrOf(options, options.find(',') + 1, std::string::npos);
  *method_name = substrOf(options, 0, options.find(','));
  options = substrOf(options, options.find(',') + 1, std::string::npos);
  *method_sig = substrOf(options, 0, options.find(','));
  options = substrOf(options, options.find(',') + 1, std::string::npos);
  *location = std::stol(options);
  return true;
}

enum class StartType {
  OnAttach, OnLoad,
};

static jint AgentStart(StartType start,
                       JavaVM* vm,
                       char* options,
                       void* reserved ATTRIBUTE_UNUSED) {
  std::string class_name;
  std::string method_name;
  std::string method_sig;
  jlong location;
  if (!ParseArgs(options,
                 /*out*/ &class_name,
                 /*out*/ &method_name,
                 /*out*/ &method_sig,
                 /*out*/ &location)) {
    return JNI_ERR;
  }
  jvmtiEnv* jvmti = nullptr;
  {
    jint res = 0;
    res = vm->GetEnv(reinterpret_cast<void**>(&jvmti), JVMTI_VERSION_1_1);

    if (res != JNI_OK || jvmti == nullptr) {
      LOG(FATAL) << "Unable to access JVMTI, error code " << res;
    }
  }

  jvmtiError error = JVMTI_ERROR_NONE;
  jvmtiCapabilities caps = {};
  caps.can_generate_breakpoint_events = 1;
  error = jvmti->AddCapabilities(&caps);
  if (error != JVMTI_ERROR_NONE) {
    LOG(ERROR) << "Unable to set caps";
    return JNI_ERR;
  }

  void* bp_target_mem = nullptr;
  error = jvmti->Allocate(sizeof(BreakpointTarget),
                          reinterpret_cast<unsigned char**>(&bp_target_mem));
  if (error != JVMTI_ERROR_NONE) {
    LOG(ERROR) << "Unable to alloc memory for breakpoint target data";
    return JNI_ERR;
  }

  BreakpointTarget* data =
      new(bp_target_mem) BreakpointTarget { class_name, method_name, method_sig, location };
  error = jvmti->SetEnvironmentLocalStorage(data);

  if (error != JVMTI_ERROR_NONE) {
    LOG(ERROR) << "Unable to set local storage";
    return JNI_ERR;
  }

  jvmtiEventCallbacks callbacks = {};
  callbacks.Breakpoint = &breakpointCB;
  callbacks.VMInit = vmInitCB;

  error = jvmti->SetEventCallbacks(&callbacks, static_cast<jint>(sizeof(callbacks)));

  if (error != JVMTI_ERROR_NONE) {
    LOG(ERROR) << "Unable to set event callbacks.";
    return JNI_ERR;
  }

  error = jvmti->SetEventNotificationMode(JVMTI_ENABLE,
                                          JVMTI_EVENT_BREAKPOINT,
                                          nullptr /* all threads */);
  if (error != JVMTI_ERROR_NONE) {
    LOG(ERROR) << "Unable to enable breakpoint event";
    return JNI_ERR;
  }
  if (start == StartType::OnAttach) {
    JNIEnv* env = nullptr;
    jint res = 0;
    res = vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_2);
    if (res != JNI_OK || env == nullptr) {
      LOG(FATAL) << "Unable to get jnienv";
    }
    vmInitCB(jvmti, env, nullptr);
  } else {
    error = jvmti->SetEventNotificationMode(JVMTI_ENABLE,
                                            JVMTI_EVENT_VM_INIT,
                                            nullptr /* all threads */);
    if (error != JVMTI_ERROR_NONE) {
      LOG(ERROR) << "Unable to set event vminit";
      return JNI_ERR;
    }
  }
  return JNI_OK;
}

// Late attachment (e.g. 'am attach-agent').
extern "C" JNIEXPORT jint JNICALL Agent_OnAttach(JavaVM *vm, char* options, void* reserved) {
  return AgentStart(StartType::OnAttach, vm, options, reserved);
}

// Early attachment
extern "C" JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM* jvm, char* options, void* reserved) {
  return AgentStart(StartType::OnLoad, jvm, options, reserved);
}

}  // namespace breakpointtest

