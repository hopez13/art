// Copyright (C) 2018 The Android Open Source Project
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
#include <jni.h>
#include <jvmti.h>

#include "base/runtime_debug.h"
#include "jit/jit.h"
#include "runtime-inl.h"
#include "scoped_thread_state_change-inl.h"
#include "thread-inl.h"
#include "thread_list.h"

namespace jitload {

#define CHECK_CALL_SUCCESS(c) \
  do { \
    auto vc = (c); \
    CHECK(vc == JNI_OK || vc == JVMTI_ERROR_NONE) << "call " << #c  << " did not succeed\n"; \
  } while (false)

// Late attachment (e.g. 'am attach-agent').
extern "C" JNIEXPORT jint JNICALL Agent_OnAttach(JavaVM *vm ATTRIBUTE_UNUSED,
                                                 char* options ATTRIBUTE_UNUSED,
                                                 void* reserved ATTRIBUTE_UNUSED) {
  LOG(FATAL) << "Late attachment not supported at the moment.";
  UNREACHABLE();
}

static jthread GetJitThread() {
  art::ScopedObjectAccess soa(art::Thread::Current());
  auto* jit = art::Runtime::Current()->GetJit();
  if (jit == nullptr) {
    return nullptr;
  }
  auto* thread_pool = jit->GetThreadPool();
  if (thread_pool == nullptr) {
    return nullptr;
  }
  // Currently we only have a single jit thread so we only look at that one.
  return soa.AddLocalReference<jthread>(
          thread_pool->GetWorkers()[0]->GetThread()->GetPeerFromOtherThread());
}

JNICALL void VmInitCb(jvmtiEnv* jvmti,
                      JNIEnv* env ATTRIBUTE_UNUSED,
                      jthread curthread ATTRIBUTE_UNUSED) {
  jthread jit_thread = GetJitThread();
  if (jit_thread != nullptr) {
    CHECK_EQ(jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_CLASS_PREPARE, jit_thread),
             JVMTI_ERROR_NONE);
  }
}

struct AgentOptions {
  bool fatal;
};

JNICALL void ClassPrepareJit(jvmtiEnv* jvmti,
                             JNIEnv* jni_env ATTRIBUTE_UNUSED,
                             jthread thr ATTRIBUTE_UNUSED,
                             jclass klass) {
  AgentOptions* ops;
  CHECK_CALL_SUCCESS(jvmti->GetEnvironmentLocalStorage(reinterpret_cast<void**>(&ops)));
  char* klass_name;
  CHECK_CALL_SUCCESS(jvmti->GetClassSignature(klass, &klass_name, nullptr));
  (ops->fatal ? LOG_STREAM(FATAL)
              : LOG_STREAM(ERROR)) << "Loaded " << klass_name << " on jit thread!";
  CHECK_CALL_SUCCESS(jvmti->Deallocate(reinterpret_cast<unsigned char*>(klass_name)));
}

// Early attachment (e.g. 'java -agent[lib|path]:filename.so').
extern "C" JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM* vm, char* options, void* /* reserved */) {
  android::base::InitLogging(/* argv */nullptr);

  jvmtiEnv* jvmti = nullptr;
  CHECK_CALL_SUCCESS(vm->GetEnv(reinterpret_cast<void**>(&jvmti), JVMTI_VERSION_1_0));
  jvmtiEventCallbacks cb {
        .VMInit = VmInitCb,
        .ClassPrepare = ClassPrepareJit,
  };
  AgentOptions* ops;
  CHECK_CALL_SUCCESS(
      jvmti->Allocate(sizeof(AgentOptions), reinterpret_cast<unsigned char**>(&ops)));
  ops->fatal = (strcmp(options, "fatal") == 0);
  CHECK_CALL_SUCCESS(jvmti->SetEnvironmentLocalStorage(ops));
  CHECK_CALL_SUCCESS(jvmti->SetEventCallbacks(&cb, sizeof(cb)));
  CHECK_CALL_SUCCESS(jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_INIT, nullptr));

  return JNI_OK;
}

#undef CHECK_CALL_SUCCESS

}  // namespace jitload

