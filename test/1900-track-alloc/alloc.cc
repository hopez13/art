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

#include "jvmti.h"

// Test infrastructure
#include "jvmti_helper.h"
#include "scoped_local_ref.h"
#include "test_env.h"

namespace art {
namespace Test1900TrackAlloc {

typedef jvmtiError (*GetGlobalStats)(jvmtiEnv* env, jlong* allocated, jlong* deallocated);
typedef jvmtiError (*TrackGlobalStats)(jvmtiEnv* env);

struct AllocTrackingData {
  GetGlobalStats get_global_stats;
  TrackGlobalStats track_global_stats;
};

template <typename T>
static void Dealloc(T* t) {
  jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(t));
}

template <typename T, typename ...Rest>
static void Dealloc(T* t, Rest... rs) {
  Dealloc(t);
  Dealloc(rs...);
}

extern "C" JNIEXPORT void JNICALL Java_art_Test1900_doDeallocate(JNIEnv* env, jclass, jlong ptr) {
  JvmtiErrorToException(env,
                        jvmti_env,
                        jvmti_env->Deallocate(
                            reinterpret_cast<unsigned char*>(static_cast<intptr_t>(ptr))));
}

extern "C" JNIEXPORT jlong JNICALL Java_art_Test1900_doAllocate(JNIEnv* env, jclass, jlong size) {
  unsigned char* res = nullptr;
  JvmtiErrorToException(env, jvmti_env, jvmti_env->Allocate(size, &res));
  return static_cast<jlong>(reinterpret_cast<intptr_t>(res));
}

extern "C" JNIEXPORT jlong JNICALL Java_art_Test1900_getAmountFreed(JNIEnv* env, jclass) {
  AllocTrackingData* data = nullptr;
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->GetEnvironmentLocalStorage(reinterpret_cast<void**>(&data)))) {
    return -1;
  }
  if (data == nullptr || data->get_global_stats == nullptr) {
    ScopedLocalRef<jclass> rt_exception(env, env->FindClass("java/lang/RuntimeException"));
    env->ThrowNew(rt_exception.get(), "Alloc tracking data not initialized.");
    return -1;
  }
  jlong allocated = -1;
  jlong freed = -1;
  JvmtiErrorToException(env, jvmti_env, data->get_global_stats(jvmti_env, &allocated, &freed));
  return freed;
}

extern "C" JNIEXPORT jlong JNICALL Java_art_Test1900_getAmountAllocated(JNIEnv* env, jclass) {
  AllocTrackingData* data = nullptr;
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->GetEnvironmentLocalStorage(reinterpret_cast<void**>(&data)))) {
    return -1;
  }
  if (data == nullptr || data->get_global_stats == nullptr) {
    ScopedLocalRef<jclass> rt_exception(env, env->FindClass("java/lang/RuntimeException"));
    env->ThrowNew(rt_exception.get(), "Alloc tracking data not initialized.");
    return -1;
  }
  jlong allocated = -1;
  jlong freed = -1;
  JvmtiErrorToException(env, jvmti_env, data->get_global_stats(jvmti_env, &allocated, &freed));
  return allocated;
}

extern "C" JNIEXPORT void JNICALL Java_art_Test1900_startTrackingAllocations(JNIEnv* env, jclass) {
  AllocTrackingData* data = nullptr;
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->GetEnvironmentLocalStorage(reinterpret_cast<void**>(&data)))) {
    return;
  }
  if (data == nullptr || data->track_global_stats == nullptr) {
    ScopedLocalRef<jclass> rt_exception(env, env->FindClass("java/lang/RuntimeException"));
    env->ThrowNew(rt_exception.get(), "Alloc tracking data not initialized.");
    return;
  }
  JvmtiErrorToException(env, jvmti_env, data->track_global_stats(jvmti_env));
}

static void DeallocParams(jvmtiParamInfo* params, jint n_params) {
  for (jint i = 0; i < n_params; i++) {
    Dealloc(params[i].name);
  }
}

extern "C" JNIEXPORT void JNICALL Java_art_Test1900_initializeTest(JNIEnv* env, jclass) {
  void* old_data = nullptr;
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->GetEnvironmentLocalStorage(&old_data))) {
    return;
  } else if (old_data != nullptr) {
    ScopedLocalRef<jclass> rt_exception(env, env->FindClass("java/lang/RuntimeException"));
    env->ThrowNew(rt_exception.get(), "Environment already has local storage set!");
    return;
  }
  AllocTrackingData* data = nullptr;
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->Allocate(sizeof(AllocTrackingData),
                                                reinterpret_cast<unsigned char**>(&data)))) {
    return;
  }
  memset(data, 0, sizeof(AllocTrackingData));
  // Get the extensions.
  jint n_ext = 0;
  jvmtiExtensionFunctionInfo* infos = nullptr;
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->GetExtensionFunctions(&n_ext, &infos))) {
    return;
  }
  for (jint i = 0; i < n_ext; i++) {
    jvmtiExtensionFunctionInfo* cur_info = &infos[i];
    if (strcmp("com.android.art.alloc.get_global_jvmti_allocation_stats", cur_info->id) == 0) {
      data->get_global_stats = reinterpret_cast<GetGlobalStats>(cur_info->func);
    } else if (strcmp("com.android.art.alloc.track_global_jvmti_allocations", cur_info->id) == 0) {
      data->track_global_stats = reinterpret_cast<TrackGlobalStats>(cur_info->func);
    }
    // Cleanup the cur_info
    DeallocParams(cur_info->params, cur_info->param_count);
    Dealloc(cur_info->id, cur_info->short_description, cur_info->params, cur_info->errors);
  }
  // Cleanup the array.
  Dealloc(infos);
  if (data->get_global_stats == nullptr || data->track_global_stats == nullptr) {
    ScopedLocalRef<jclass> rt_exception(env, env->FindClass("java/lang/RuntimeException"));
    env->ThrowNew(rt_exception.get(), "Unable to find memory tracking extensions.");
    return;
  }
  JvmtiErrorToException(env, jvmti_env, jvmti_env->SetEnvironmentLocalStorage(data));
  return;
}

}  // namespace Test1900TrackAlloc
}  // namespace art
