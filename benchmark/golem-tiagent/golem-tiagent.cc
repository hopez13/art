/*
 * Copyright (C) 2022 The Android Open Source Project
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

#include "android-base/macros.h"
#include "android-base/logging.h"

#include "jni.h"
#include "jvmti.h"

namespace art {

static const jvmtiCapabilities standard_caps = {
  .can_tag_objects                                 = 1,
  .can_generate_field_modification_events          = 1,
  .can_generate_field_access_events                = 1,
  .can_get_bytecodes                               = 1,
  .can_get_synthetic_attribute                     = 1,
  .can_get_owned_monitor_info                      = 0,
  .can_get_current_contended_monitor               = 1,
  .can_get_monitor_info                            = 1,
  .can_pop_frame                                   = 0,
  .can_redefine_classes                            = 1,
  .can_signal_thread                               = 1,
  .can_get_source_file_name                        = 1,
  .can_get_line_numbers                            = 1,
  .can_get_source_debug_extension                  = 1,
  .can_access_local_variables                      = 0,
  .can_maintain_original_method_order              = 1,
  .can_generate_single_step_events                 = 1,
  .can_generate_exception_events                   = 0,
  .can_generate_frame_pop_events                   = 0,
  .can_generate_breakpoint_events                  = 1,
  .can_suspend                                     = 1,
  .can_redefine_any_class                          = 0,
  .can_get_current_thread_cpu_time                 = 0,
  .can_get_thread_cpu_time                         = 0,
  .can_generate_method_entry_events                = 1,
  .can_generate_method_exit_events                 = 1,
  .can_generate_all_class_hook_events              = 0,
  .can_generate_compiled_method_load_events        = 0,
  .can_generate_monitor_events                     = 0,
  .can_generate_vm_object_alloc_events             = 1,
  .can_generate_native_method_bind_events          = 1,
  .can_generate_garbage_collection_events          = 1,
  .can_generate_object_free_events                 = 1,
  .can_force_early_return                          = 0,
  .can_get_owned_monitor_stack_depth_info          = 0,
  .can_get_constant_pool                           = 0,
  .can_set_native_method_prefix                    = 0,
  .can_retransform_classes                         = 1,
  .can_retransform_any_class                       = 0,
  .can_generate_resource_exhaustion_heap_events    = 0,
  .can_generate_resource_exhaustion_threads_events = 0,
};

jvmtiEnv* jvmti_env = nullptr;

void CheckJvmtiError(jvmtiEnv* env, jvmtiError error) {
  if (error != JVMTI_ERROR_NONE) {
    char* error_name;
    jvmtiError name_error = env->GetErrorName(error, &error_name);
    if (name_error != JVMTI_ERROR_NONE) {
      LOG(FATAL) << "Unable to get error name for " << error;
    }
    LOG(FATAL) << "Unexpected error: " << error_name;
  }
}

bool JvmtiErrorToException(JNIEnv* env, jvmtiEnv* jvmtienv, jvmtiError error) {
  if (error == JVMTI_ERROR_NONE) {
    return false;
  }

  jclass rt_exception =  env->FindClass("java/lang/RuntimeException");
  if (rt_exception == nullptr) {
    return true;
  }

  char* err;
  CheckJvmtiError(jvmtienv, jvmtienv->GetErrorName(error, &err));
  env->ThrowNew(rt_exception, err);

  env->DeleteLocalRef(rt_exception);
  jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(err));
  return true;
}

extern "C" JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM* vm,
                                               char* options ATTRIBUTE_UNUSED,
                                               void* reserved ATTRIBUTE_UNUSED) {
  if (vm->GetEnv(reinterpret_cast<void**>(&jvmti_env), JVMTI_VERSION_1_0) != 0) {
    LOG(ERROR) << "Unable to get jvmti env!";
    return 1;
  }

  CheckJvmtiError(jvmti_env, jvmti_env->AddCapabilities(&standard_caps));
  return 0;
}

extern "C" JNIEXPORT jlong JNICALL
  Java_benchmarks_common_java_Breakpoint_getStartLocation(JNIEnv* env,
                                                          jclass k ATTRIBUTE_UNUSED,
                                                          jobject target) {
  jmethodID method = env->FromReflectedMethod(target);
  if (env->ExceptionCheck()) {
    return 0;
  }
  jlong start = 0;
  jlong end;
  JvmtiErrorToException(env, jvmti_env, jvmti_env->GetMethodLocation(method, &start, &end));
  return start;
}

extern "C" JNIEXPORT void JNICALL
  Java_benchmarks_common_java_Breakpoint_setBreakpoint(JNIEnv* env,
                                                       jclass k ATTRIBUTE_UNUSED,
                                                       jobject target,
                                                       jlocation location) {
  jmethodID method = env->FromReflectedMethod(target);
  if (env->ExceptionCheck()) {
    return;
  }
  JvmtiErrorToException(env, jvmti_env, jvmti_env->SetBreakpoint(method, location));
}

}  // namespace art
