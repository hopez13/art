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
#include <nativehelper/scoped_local_ref.h>
#include <string.h>

#include <algorithm>
#include <atomic>
#include <iomanip>
#include <iostream>
#include <istream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace typecount {

#define CHECK_JVMTI(x) CHECK_EQ((x), JVMTI_ERROR_NONE)

// Special art ti-version number. We will use this as a fallback if we cannot get a regular JVMTI
// env.
static constexpr jint kArtTiVersion = JVMTI_VERSION_1_2 | 0x40000000;

static JavaVM* java_vm = nullptr;

static jint SetupJvmtiEnv(JavaVM* vm, jvmtiEnv** jvmti) {
  jint res = 0;
  res = vm->GetEnv(reinterpret_cast<void**>(jvmti), JVMTI_VERSION_1_1);

  if (res != JNI_OK || *jvmti == nullptr) {
    LOG(ERROR) << "Unable to access JVMTI, error code " << res;
    return vm->GetEnv(reinterpret_cast<void**>(jvmti), kArtTiVersion);
  }
  return res;
}

static void DataDumpRequestCb(jvmtiEnv* jvmti) {
  JNIEnv* env = nullptr;
  CHECK_EQ(java_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6), JNI_OK);
  jclass class_class = env->FindClass("java/lang/Class");
  CHECK(class_class != nullptr);
  jlong current_tag = 0;
  // Get all the classes tagged
  jvmtiHeapCallbacks cb_tag_classes {
    .heap_iteration_callback = [](jlong class_tag ATTRIBUTE_UNUSED,
                                  jlong size ATTRIBUTE_UNUSED,
                                  jlong* tag_ptr,
                                  jint length ATTRIBUTE_UNUSED,
                                  void* tag_void) -> int {
      jlong* last_tag = reinterpret_cast<jlong*>(tag_void);
      *tag_ptr = ++(*last_tag);
      return 0;
    },
  };
  jvmti->IterateThroughHeap(0, class_class, &cb_tag_classes, &current_tag);
  // Get all the classes objects
  static constexpr auto TagToIdx = [](jlong t) { return t - 1; };
  static constexpr auto IdxToTag = [](jlong i) { return i + 1; };
  std::vector<std::string> klasses(current_tag, "");
  std::vector<std::vector<jlong>> supers(current_tag, std::vector<jlong>{});
  std::vector<jlong> tags(current_tag, 0);
  jlong c = 0;
  std::generate(tags.begin(), tags.end(), [&]() { return ++c; });
  jlong* tag_res;
  jobject* objs_res;
  jint cnt = 0;
  jvmti->GetObjectsWithTags(tags.size(), tags.data(), &cnt, &objs_res, &tag_res);
  for (jint i = 0; i < cnt; i++) {
    char* sig;
    jclass cur_class = static_cast<jclass>(objs_res[i]);
    jvmti->GetClassSignature(cur_class, &sig, nullptr);
    jlong my_idx = TagToIdx(tag_res[i]);
    klasses[my_idx] = sig;
    jvmti->Deallocate(reinterpret_cast<unsigned char*>(sig));
    do {
      jlong tag;
      jvmti->GetTag(cur_class, &tag);
      supers[my_idx].push_back(tag);
    } while ((cur_class = env->GetSuperclass(cur_class)) != nullptr);
  }

  // Create counting array
  std::vector<jlong> counts(current_tag, 0);
  // Count instances
  jvmtiHeapCallbacks cb_count {
    .heap_iteration_callback = [](jlong class_tag,
                                  jlong size ATTRIBUTE_UNUSED,
                                  jlong* tag_ptr ATTRIBUTE_UNUSED,
                                  jint length ATTRIBUTE_UNUSED,
                                  void* count_void) -> int {
      std::vector<jlong>* counts_ptr =
          reinterpret_cast<std::vector<jlong>*>(count_void);
      (*counts_ptr)[TagToIdx(class_tag)]++;
      return 0;
    },
  };
  jvmti->IterateThroughHeap(0, nullptr, &cb_count, &counts);
  // Combine superclasses.
  struct TagPair {
    jlong tag;
    jlong count;
  };
  std::vector<TagPair> totals(counts.size(), TagPair{});
  for (size_t i = 0; i < counts.size(); i++) {
    totals[i].tag = IdxToTag(i);
    for (jlong tag : supers[i]) {
      totals[TagToIdx(tag)].count += counts[i];
    }
  }

  // Sort in reverse by count.
  std::sort(totals.begin(), totals.end(), [](auto a, auto b) { return a.count > b.count; });
  LOG(INFO) << "TYPECOUNT: Printing 100 most common types";
  std::for_each(totals.begin(),
                totals.begin() + std::min<size_t>(100lu, totals.size()),
                [&](TagPair& p) {
                  LOG(INFO) << "TYPECOUNT: " << klasses[TagToIdx(p.tag)] << "\t" << p.count;
                });
}

static void VMDeathCb(jvmtiEnv* jvmti, JNIEnv* env ATTRIBUTE_UNUSED) {
  DataDumpRequestCb(jvmti);
}

static void VMInitCb(jvmtiEnv* jvmti, JNIEnv* env ATTRIBUTE_UNUSED, jobject thr ATTRIBUTE_UNUSED) {
  CHECK_JVMTI(jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_DEATH, nullptr));
  CHECK_JVMTI(
      jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_DATA_DUMP_REQUEST, nullptr));
  DataDumpRequestCb(jvmti);
}

static jint AgentStart(JavaVM* vm, char* options ATTRIBUTE_UNUSED, bool is_onload) {
  android::base::InitLogging(/* argv= */ nullptr);
  java_vm = vm;
  jvmtiEnv* jvmti = nullptr;
  if (SetupJvmtiEnv(vm, &jvmti) != JNI_OK) {
    LOG(ERROR) << "Could not get JVMTI env or ArtTiEnv!";
    return JNI_ERR;
  }
  jvmtiCapabilities caps {
    .can_tag_objects = 1,
  };
  CHECK_JVMTI(jvmti->AddCapabilities(&caps));
  jvmtiEventCallbacks cb {
    .VMInit = VMInitCb,
    .VMDeath = VMDeathCb,
    .DataDumpRequest = DataDumpRequestCb,
  };
  CHECK_JVMTI(jvmti->SetEventCallbacks(&cb, sizeof(cb)));
  if (is_onload) {
    CHECK_JVMTI(jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_INIT, nullptr));
  } else {
    JNIEnv* env = nullptr;
    CHECK_EQ(vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6), JNI_OK);
    CHECK_JVMTI(jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_DEATH, nullptr));
    CHECK_JVMTI(
        jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_DATA_DUMP_REQUEST, nullptr));
    DataDumpRequestCb(jvmti);
  }
  return JNI_OK;
}

// Late attachment (e.g. 'am attach-agent').
extern "C" JNIEXPORT jint JNICALL Agent_OnAttach(JavaVM* vm,
                                                 char* options,
                                                 void* reserved ATTRIBUTE_UNUSED) {
  return AgentStart(vm, options, /*is_onload=*/false);
}

// Early attachment
extern "C" JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM* jvm,
                                               char* options,
                                               void* reserved ATTRIBUTE_UNUSED) {
  return AgentStart(jvm, options, /*is_onload=*/true);
}

}  // namespace typecount
