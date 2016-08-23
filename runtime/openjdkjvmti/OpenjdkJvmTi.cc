/* Copyright (C) 2016 The Android Open Source Project
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This file implements interfaces from the file jvmti.h. This implementation
 * is licensed under the same terms as the file jvmti.h.  The
 * copyright and license information for the file jvmti.h follows.
 *
 * Copyright (c) 2003, 2011, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#include <vector>

#include <string.h>
#include <jni.h>
#include "openjdkjvmti/jvmti.h"

#include "utils/dex_cache_arrays_layout-inl.h"
#include "base/enums.h"
#include "linear_alloc.h"
#include "gc_root-inl.h"
#include "globals.h"
#include "jni_env_ext-inl.h"
#include "scoped_thread_state_change.h"
#include "thread_list.h"
#include "mirror/array.h"
#include "mirror/class-inl.h"
#include "mirror/class_loader-inl.h"
#include "mirror/string-inl.h"
#include "class_linker.h"
#include "utf.h"

// TODO Remove this at some point by annotating all the methods. It was put in to make the skeleton
// easier to create.
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"

namespace openjdkjvmti {

extern const jvmtiInterface_1 gJvmtiInterface;

// A structure that is a jvmtiEnv with additional information for the runtime.
struct ArtJvmTiEnv : public jvmtiEnv {
  art::JavaVMExt* art_vm;
  void* local_data;
  jvmtiCapabilities capabilities;

  explicit ArtJvmTiEnv(art::JavaVMExt* runtime)
      : art_vm(runtime), local_data(nullptr), capabilities() {
    functions = &gJvmtiInterface;
    // Just zero out the capabilities.
    memset(reinterpret_cast<void*>(&capabilities), 0, sizeof(jvmtiCapabilities));
  }
};

static jvmtiCapabilities potentialCapabilities = {
    1,  // 0, TODO IMPLEMENT // can_tag_objects
    0,                       // can_generate_field_modification_events
    0,                       // can_generate_field_access_events
    0,                       // can_get_bytecodes
    0,                       // can_get_synthetic_attribute
    0,                       // can_get_owned_monitor_info
    0,                       // can_get_current_contended_monitor
    0,                       // can_get_monitor_info
    0,                       // can_pop_frame
    0,                       // can_redefine_classes
    0,                       // can_signal_thread
    0,                       // can_get_source_file_name
    0,                       // can_get_line_numbers
    0,                       // can_get_source_debug_extension
    0,                       // can_access_local_variables
    0,                       // can_maintain_original_method_order
    0,                       // can_generate_single_step_events
    0,                       // can_generate_exception_events
    0,                       // can_generate_frame_pop_events
    0,                       // can_generate_breakpoint_events
    0,                       // can_suspend
    0,                       // can_redefine_any_class
    0,                       // can_get_current_thread_cpu_time
    0,                       // can_get_thread_cpu_time
    0,                       // can_generate_method_entry_events
    0,                       // can_generate_method_exit_events
    1,  // 0, TODO IMPLEMENT // can_generate_all_class_hook_events
    0,                       // can_generate_compiled_method_load_events
    0,                       // can_generate_monitor_events
    1,  // 0, TODO IMPLEMENT // can_generate_vm_object_alloc_events
    0,                       // can_generate_native_method_bind_events
    1,  // 0, TODO IMPLEMENT // can_generate_garbage_collection_events
    1,  // 0, TODO IMPLEMENT // can_generate_object_free_events
    0,                       // can_force_early_return
    0,                       // can_get_owned_monitor_stack_depth_info
    0,                       // can_get_constant_pool
    0,                       // can_set_native_method_prefix
    1,  // 0, TODO IMPLEMENT // can_retransform_classes
    1,  // 0, TODO IMPLEMENT // can_retransform_any_class
    0,                       // can_generate_resource_exhaustion_heap_events
    0,                       // can_generate_resource_exhaustion_threads_events
};

// Macro and constexpr to make error values less annoying to write.
#define ERR(e) JVMTI_ERROR_ ## e
static constexpr jvmtiError OK = JVMTI_ERROR_NONE;

// Special error code for unimplemented functions in JVMTI
static constexpr jvmtiError ERR(NOT_IMPLEMENTED) = ERR(NOT_AVAILABLE);

class JvmtiFunctions {
 private:
  static bool IsValidEnv(jvmtiEnv* env) {
    return env != nullptr;
  }

 public:
  static jvmtiError Allocate(jvmtiEnv* env, jlong size, unsigned char** mem_ptr) {
    if (!IsValidEnv(env)) {
      return ERR(INVALID_ENVIRONMENT);
    }
    if (mem_ptr == nullptr) {
      return ERR(NULL_POINTER);
    }
    if (size < 0) {
      return ERR(ILLEGAL_ARGUMENT);
    } else if (size == 0) {
      *mem_ptr = nullptr;
      return OK;
    }
    *mem_ptr = static_cast<unsigned char*>(malloc(size));
    return (*mem_ptr != nullptr) ? OK : ERR(OUT_OF_MEMORY);
  }

  static jvmtiError Deallocate(jvmtiEnv* env, unsigned char* mem) {
    if (!IsValidEnv(env)) {
      return ERR(INVALID_ENVIRONMENT);
    }
    if (mem != nullptr) {
      free(mem);
    }
    return OK;
  }

  static jvmtiError GetThreadState(jvmtiEnv* env, jthread thread, jint* thread_state_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetCurrentThread(jvmtiEnv* env, jthread* thread_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetAllThreads(jvmtiEnv* env, jint* threads_count_ptr, jthread** threads_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError SuspendThread(jvmtiEnv* env, jthread thread) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError SuspendThreadList(jvmtiEnv* env,
                                      jint request_count,
                                      const jthread* request_list,
                                      jvmtiError* results) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError ResumeThread(jvmtiEnv* env, jthread thread) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError ResumeThreadList(jvmtiEnv* env,
                                     jint request_count,
                                     const jthread* request_list,
                                     jvmtiError* results) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError StopThread(jvmtiEnv* env, jthread thread, jobject exception) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError InterruptThread(jvmtiEnv* env, jthread thread) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetThreadInfo(jvmtiEnv* env, jthread thread, jvmtiThreadInfo* info_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetOwnedMonitorInfo(jvmtiEnv* env,
                                        jthread thread,
                                        jint* owned_monitor_count_ptr,
                                        jobject** owned_monitors_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetOwnedMonitorStackDepthInfo(jvmtiEnv* env,
                                                  jthread thread,
                                                  jint* monitor_info_count_ptr,
                                                  jvmtiMonitorStackDepthInfo** monitor_info_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetCurrentContendedMonitor(jvmtiEnv* env,
                                               jthread thread,
                                               jobject* monitor_ptr) {
  return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError RunAgentThread(jvmtiEnv* env,
                                   jthread thread,
                                   jvmtiStartFunction proc,
                                   const void* arg,
                                   jint priority) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError SetThreadLocalStorage(jvmtiEnv* env, jthread thread, const void* data) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetThreadLocalStorage(jvmtiEnv* env, jthread thread, void** data_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetTopThreadGroups(jvmtiEnv* env,
                                       jint* group_count_ptr,
                                       jthreadGroup** groups_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetThreadGroupInfo(jvmtiEnv* env,
                                       jthreadGroup group,
                                       jvmtiThreadGroupInfo* info_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetThreadGroupChildren(jvmtiEnv* env,
                                           jthreadGroup group,
                                           jint* thread_count_ptr,
                                           jthread** threads_ptr,
                                           jint* group_count_ptr,
                                           jthreadGroup** groups_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetStackTrace(jvmtiEnv* env,
                                  jthread thread,
                                  jint start_depth,
                                  jint max_frame_count,
                                  jvmtiFrameInfo* frame_buffer,
                                  jint* count_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetAllStackTraces(jvmtiEnv* env,
                                      jint max_frame_count,
                                      jvmtiStackInfo** stack_info_ptr,
                                      jint* thread_count_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetThreadListStackTraces(jvmtiEnv* env,
                                             jint thread_count,
                                             const jthread* thread_list,
                                             jint max_frame_count,
                                             jvmtiStackInfo** stack_info_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetFrameCount(jvmtiEnv* env, jthread thread, jint* count_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError PopFrame(jvmtiEnv* env, jthread thread) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetFrameLocation(jvmtiEnv* env,
                                     jthread thread,
                                     jint depth,
                                     jmethodID* method_ptr,
                                     jlocation* location_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError NotifyFramePop(jvmtiEnv* env, jthread thread, jint depth) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError ForceEarlyReturnObject(jvmtiEnv* env, jthread thread, jobject value) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError ForceEarlyReturnInt(jvmtiEnv* env, jthread thread, jint value) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError ForceEarlyReturnLong(jvmtiEnv* env, jthread thread, jlong value) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError ForceEarlyReturnFloat(jvmtiEnv* env, jthread thread, jfloat value) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError ForceEarlyReturnDouble(jvmtiEnv* env, jthread thread, jdouble value) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError ForceEarlyReturnVoid(jvmtiEnv* env, jthread thread) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError FollowReferences(jvmtiEnv* env,
                                     jint heap_filter,
                                     jclass klass,
                                     jobject initial_object,
                                     const jvmtiHeapCallbacks* callbacks,
                                     const void* user_data) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError IterateThroughHeap(jvmtiEnv* env,
                                       jint heap_filter,
                                       jclass klass,
                                       const jvmtiHeapCallbacks* callbacks,
                                       const void* user_data) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetTag(jvmtiEnv* env, jobject object, jlong* tag_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError SetTag(jvmtiEnv* env, jobject object, jlong tag) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetObjectsWithTags(jvmtiEnv* env,
                                       jint tag_count,
                                       const jlong* tags,
                                       jint* count_ptr,
                                       jobject** object_result_ptr,
                                       jlong** tag_result_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError ForceGarbageCollection(jvmtiEnv* env) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError IterateOverObjectsReachableFromObject(
      jvmtiEnv* env,
      jobject object,
      jvmtiObjectReferenceCallback object_reference_callback,
      const void* user_data) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError IterateOverReachableObjects(jvmtiEnv* env,
                                                jvmtiHeapRootCallback heap_root_callback,
                                                jvmtiStackReferenceCallback stack_ref_callback,
                                                jvmtiObjectReferenceCallback object_ref_callback,
                                                const void* user_data) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError IterateOverHeap(jvmtiEnv* env,
                                    jvmtiHeapObjectFilter object_filter,
                                    jvmtiHeapObjectCallback heap_object_callback,
                                    const void* user_data) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError IterateOverInstancesOfClass(jvmtiEnv* env,
                                                jclass klass,
                                                jvmtiHeapObjectFilter object_filter,
                                                jvmtiHeapObjectCallback heap_object_callback,
                                                const void* user_data) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetLocalObject(jvmtiEnv* env,
                                   jthread thread,
                                   jint depth,
                                   jint slot,
                                   jobject* value_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetLocalInstance(jvmtiEnv* env,
                                     jthread thread,
                                     jint depth,
                                     jobject* value_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetLocalInt(jvmtiEnv* env,
                                jthread thread,
                                jint depth,
                                jint slot,
                                jint* value_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetLocalLong(jvmtiEnv* env,
                                 jthread thread,
                                 jint depth,
                                 jint slot,
                                 jlong* value_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetLocalFloat(jvmtiEnv* env,
                                  jthread thread,
                                  jint depth,
                                  jint slot,
                                  jfloat* value_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetLocalDouble(jvmtiEnv* env,
                                   jthread thread,
                                   jint depth,
                                   jint slot,
                                   jdouble* value_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError SetLocalObject(jvmtiEnv* env,
                                   jthread thread,
                                   jint depth,
                                   jint slot,
                                   jobject value) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError SetLocalInt(jvmtiEnv* env,
                                jthread thread,
                                jint depth,
                                jint slot,
                                jint value) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError SetLocalLong(jvmtiEnv* env,
                                 jthread thread,
                                 jint depth,
                                 jint slot,
                                 jlong value) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError SetLocalFloat(jvmtiEnv* env,
                                  jthread thread,
                                  jint depth,
                                  jint slot,
                                  jfloat value) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError SetLocalDouble(jvmtiEnv* env,
                                   jthread thread,
                                   jint depth,
                                   jint slot,
                                   jdouble value) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError SetBreakpoint(jvmtiEnv* env, jmethodID method, jlocation location) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError ClearBreakpoint(jvmtiEnv* env, jmethodID method, jlocation location) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError SetFieldAccessWatch(jvmtiEnv* env, jclass klass, jfieldID field) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError ClearFieldAccessWatch(jvmtiEnv* env, jclass klass, jfieldID field) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError SetFieldModificationWatch(jvmtiEnv* env, jclass klass, jfieldID field) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError ClearFieldModificationWatch(jvmtiEnv* env, jclass klass, jfieldID field) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetLoadedClasses(jvmtiEnv* env, jint* class_count_ptr, jclass** classes_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetClassLoaderClasses(jvmtiEnv* env,
                                          jobject initiating_loader,
                                          jint* class_count_ptr,
                                          jclass** classes_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetClassSignature(jvmtiEnv* env,
                                      jclass klass,
                                      char** signature_ptr,
                                      char** generic_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetClassStatus(jvmtiEnv* env, jclass klass, jint* status_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetSourceFileName(jvmtiEnv* env, jclass klass, char** source_name_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetClassModifiers(jvmtiEnv* env, jclass klass, jint* modifiers_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetClassMethods(jvmtiEnv* env,
                                    jclass klass,
                                    jint* method_count_ptr,
                                    jmethodID** methods_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetClassFields(jvmtiEnv* env,
                                   jclass klass,
                                   jint* field_count_ptr,
                                   jfieldID** fields_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetImplementedInterfaces(jvmtiEnv* env,
                                             jclass klass,
                                             jint* interface_count_ptr,
                                             jclass** interfaces_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetClassVersionNumbers(jvmtiEnv* env,
                                           jclass klass,
                                           jint* minor_version_ptr,
                                           jint* major_version_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetConstantPool(jvmtiEnv* env,
                                    jclass klass,
                                    jint* constant_pool_count_ptr,
                                    jint* constant_pool_byte_count_ptr,
                                    unsigned char** constant_pool_bytes_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError IsInterface(jvmtiEnv* env, jclass klass, jboolean* is_interface_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError IsArrayClass(jvmtiEnv* env,
                                 jclass klass,
                                 jboolean* is_array_class_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError IsModifiableClass(jvmtiEnv* env,
                                      jclass klass,
                                      jboolean* is_modifiable_class_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetClassLoader(jvmtiEnv* env, jclass klass, jobject* classloader_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetSourceDebugExtension(jvmtiEnv* env,
                                            jclass klass,
                                            char** source_debug_extension_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError RetransformClasses(jvmtiEnv* env, jint class_count, const jclass* classes) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError RedefineClasses(jvmtiEnv* env,
                                    jint class_count,
                                    const jvmtiClassDefinition* class_definitions) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetObjectSize(jvmtiEnv* env, jobject object, jlong* size_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetObjectHashCode(jvmtiEnv* env, jobject object, jint* hash_code_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetObjectMonitorUsage(jvmtiEnv* env,
                                          jobject object,
                                          jvmtiMonitorUsage* info_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetFieldName(jvmtiEnv* env,
                                 jclass klass,
                                 jfieldID field,
                                 char** name_ptr,
                                 char** signature_ptr,
                                 char** generic_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetFieldDeclaringClass(jvmtiEnv* env,
                                           jclass klass,
                                           jfieldID field,
                                           jclass* declaring_class_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetFieldModifiers(jvmtiEnv* env,
                                      jclass klass,
                                      jfieldID field,
                                      jint* modifiers_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError IsFieldSynthetic(jvmtiEnv* env,
                                     jclass klass,
                                     jfieldID field,
                                     jboolean* is_synthetic_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetMethodName(jvmtiEnv* env,
                                  jmethodID method,
                                  char** name_ptr,
                                  char** signature_ptr,
                                  char** generic_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetMethodDeclaringClass(jvmtiEnv* env,
                                            jmethodID method,
                                            jclass* declaring_class_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetMethodModifiers(jvmtiEnv* env,
                                       jmethodID method,
                                       jint* modifiers_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetMaxLocals(jvmtiEnv* env,
                                 jmethodID method,
                                 jint* max_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetArgumentsSize(jvmtiEnv* env,
                                     jmethodID method,
                                     jint* size_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetLineNumberTable(jvmtiEnv* env,
                                       jmethodID method,
                                       jint* entry_count_ptr,
                                       jvmtiLineNumberEntry** table_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetMethodLocation(jvmtiEnv* env,
                                      jmethodID method,
                                      jlocation* start_location_ptr,
                                      jlocation* end_location_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetLocalVariableTable(jvmtiEnv* env,
                                          jmethodID method,
                                          jint* entry_count_ptr,
                                          jvmtiLocalVariableEntry** table_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetBytecodes(jvmtiEnv* env,
                                 jmethodID method,
                                 jint* bytecode_count_ptr,
                                 unsigned char** bytecodes_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError IsMethodNative(jvmtiEnv* env, jmethodID method, jboolean* is_native_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError IsMethodSynthetic(jvmtiEnv* env, jmethodID method, jboolean* is_synthetic_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError IsMethodObsolete(jvmtiEnv* env, jmethodID method, jboolean* is_obsolete_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError SetNativeMethodPrefix(jvmtiEnv* env, const char* prefix) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError SetNativeMethodPrefixes(jvmtiEnv* env, jint prefix_count, char** prefixes) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError CreateRawMonitor(jvmtiEnv* env, const char* name, jrawMonitorID* monitor_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError DestroyRawMonitor(jvmtiEnv* env, jrawMonitorID monitor) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError RawMonitorEnter(jvmtiEnv* env, jrawMonitorID monitor) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError RawMonitorExit(jvmtiEnv* env, jrawMonitorID monitor) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError RawMonitorWait(jvmtiEnv* env, jrawMonitorID monitor, jlong millis) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError RawMonitorNotify(jvmtiEnv* env, jrawMonitorID monitor) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError RawMonitorNotifyAll(jvmtiEnv* env, jrawMonitorID monitor) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError SetJNIFunctionTable(jvmtiEnv* env, const jniNativeInterface* function_table) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetJNIFunctionTable(jvmtiEnv* env, jniNativeInterface** function_table) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError SetEventCallbacks(jvmtiEnv* env,
                                      const jvmtiEventCallbacks* callbacks,
                                      jint size_of_callbacks) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError SetEventNotificationMode(jvmtiEnv* env,
                                             jvmtiEventMode mode,
                                             jvmtiEvent event_type,
                                             jthread event_thread,
                                             ...) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GenerateEvents(jvmtiEnv* env, jvmtiEvent event_type) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetExtensionFunctions(jvmtiEnv* env,
                                          jint* extension_count_ptr,
                                          jvmtiExtensionFunctionInfo** extensions) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetExtensionEvents(jvmtiEnv* env,
                                       jint* extension_count_ptr,
                                       jvmtiExtensionEventInfo** extensions) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError SetExtensionEventCallback(jvmtiEnv* env,
                                              jint extension_event_index,
                                              jvmtiExtensionEvent callback) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetPotentialCapabilities(jvmtiEnv* env, jvmtiCapabilities* capabilities_ptr) {
    if (!IsValidEnv(env)) {
      return ERR(INVALID_ENVIRONMENT);
    }
    if (capabilities_ptr == nullptr) {
      return ERR(NULL_POINTER);
    }
    memcpy(reinterpret_cast<void*>(capabilities_ptr),
           reinterpret_cast<void*>(&potentialCapabilities),
           sizeof(jvmtiCapabilities));
    return OK;
  }

  static jvmtiError AddCapabilities(jvmtiEnv* env, const jvmtiCapabilities* capabilities_ptr) {
    if (!IsValidEnv(env)) {
      return ERR(INVALID_ENVIRONMENT);
    }
    if (capabilities_ptr == nullptr) {
      return ERR(NULL_POINTER);
    }
    ArtJvmTiEnv* art_env = reinterpret_cast<ArtJvmTiEnv*>(env);
    jvmtiError ret = OK;
#define ADD_CAPABILITY(e) \
    do { \
      if (capabilities_ptr->e == 1) { \
        if (potentialCapabilities.e == 1) { \
          art_env->capabilities.e = 1;\
        } else { \
          ret = ERR(NOT_AVAILABLE); \
        } \
      } \
    } while (false)

    ADD_CAPABILITY(can_tag_objects);
    ADD_CAPABILITY(can_generate_field_modification_events);
    ADD_CAPABILITY(can_generate_field_access_events);
    ADD_CAPABILITY(can_get_bytecodes);
    ADD_CAPABILITY(can_get_synthetic_attribute);
    ADD_CAPABILITY(can_get_owned_monitor_info);
    ADD_CAPABILITY(can_get_current_contended_monitor);
    ADD_CAPABILITY(can_get_monitor_info);
    ADD_CAPABILITY(can_pop_frame);
    ADD_CAPABILITY(can_redefine_classes);
    ADD_CAPABILITY(can_signal_thread);
    ADD_CAPABILITY(can_get_source_file_name);
    ADD_CAPABILITY(can_get_line_numbers);
    ADD_CAPABILITY(can_get_source_debug_extension);
    ADD_CAPABILITY(can_access_local_variables);
    ADD_CAPABILITY(can_maintain_original_method_order);
    ADD_CAPABILITY(can_generate_single_step_events);
    ADD_CAPABILITY(can_generate_exception_events);
    ADD_CAPABILITY(can_generate_frame_pop_events);
    ADD_CAPABILITY(can_generate_breakpoint_events);
    ADD_CAPABILITY(can_suspend);
    ADD_CAPABILITY(can_redefine_any_class);
    ADD_CAPABILITY(can_get_current_thread_cpu_time);
    ADD_CAPABILITY(can_get_thread_cpu_time);
    ADD_CAPABILITY(can_generate_method_entry_events);
    ADD_CAPABILITY(can_generate_method_exit_events);
    ADD_CAPABILITY(can_generate_all_class_hook_events);
    ADD_CAPABILITY(can_generate_compiled_method_load_events);
    ADD_CAPABILITY(can_generate_monitor_events);
    ADD_CAPABILITY(can_generate_vm_object_alloc_events);
    ADD_CAPABILITY(can_generate_native_method_bind_events);
    ADD_CAPABILITY(can_generate_garbage_collection_events);
    ADD_CAPABILITY(can_generate_object_free_events);
    ADD_CAPABILITY(can_force_early_return);
    ADD_CAPABILITY(can_get_owned_monitor_stack_depth_info);
    ADD_CAPABILITY(can_get_constant_pool);
    ADD_CAPABILITY(can_set_native_method_prefix);
    ADD_CAPABILITY(can_retransform_classes);
    ADD_CAPABILITY(can_retransform_any_class);
    ADD_CAPABILITY(can_generate_resource_exhaustion_heap_events);
    ADD_CAPABILITY(can_generate_resource_exhaustion_threads_events);
#undef ADD_CAPABILITY
    return ret;
  }

  static jvmtiError RelinquishCapabilities(jvmtiEnv* env,
                                           const jvmtiCapabilities* capabilities_ptr) {
    if (!IsValidEnv(env)) {
      return ERR(INVALID_ENVIRONMENT);
    }
    if (capabilities_ptr == nullptr) {
      return ERR(NULL_POINTER);
    }
    ArtJvmTiEnv* art_env = reinterpret_cast<ArtJvmTiEnv*>(env);
#define DEL_CAPABILITY(e) \
    do { \
      if (capabilities_ptr->e == 1) { \
        art_env->capabilities.e = 0;\
      } \
    } while (false)

    DEL_CAPABILITY(can_tag_objects);
    DEL_CAPABILITY(can_generate_field_modification_events);
    DEL_CAPABILITY(can_generate_field_access_events);
    DEL_CAPABILITY(can_get_bytecodes);
    DEL_CAPABILITY(can_get_synthetic_attribute);
    DEL_CAPABILITY(can_get_owned_monitor_info);
    DEL_CAPABILITY(can_get_current_contended_monitor);
    DEL_CAPABILITY(can_get_monitor_info);
    DEL_CAPABILITY(can_pop_frame);
    DEL_CAPABILITY(can_redefine_classes);
    DEL_CAPABILITY(can_signal_thread);
    DEL_CAPABILITY(can_get_source_file_name);
    DEL_CAPABILITY(can_get_line_numbers);
    DEL_CAPABILITY(can_get_source_debug_extension);
    DEL_CAPABILITY(can_access_local_variables);
    DEL_CAPABILITY(can_maintain_original_method_order);
    DEL_CAPABILITY(can_generate_single_step_events);
    DEL_CAPABILITY(can_generate_exception_events);
    DEL_CAPABILITY(can_generate_frame_pop_events);
    DEL_CAPABILITY(can_generate_breakpoint_events);
    DEL_CAPABILITY(can_suspend);
    DEL_CAPABILITY(can_redefine_any_class);
    DEL_CAPABILITY(can_get_current_thread_cpu_time);
    DEL_CAPABILITY(can_get_thread_cpu_time);
    DEL_CAPABILITY(can_generate_method_entry_events);
    DEL_CAPABILITY(can_generate_method_exit_events);
    DEL_CAPABILITY(can_generate_all_class_hook_events);
    DEL_CAPABILITY(can_generate_compiled_method_load_events);
    DEL_CAPABILITY(can_generate_monitor_events);
    DEL_CAPABILITY(can_generate_vm_object_alloc_events);
    DEL_CAPABILITY(can_generate_native_method_bind_events);
    DEL_CAPABILITY(can_generate_garbage_collection_events);
    DEL_CAPABILITY(can_generate_object_free_events);
    DEL_CAPABILITY(can_force_early_return);
    DEL_CAPABILITY(can_get_owned_monitor_stack_depth_info);
    DEL_CAPABILITY(can_get_constant_pool);
    DEL_CAPABILITY(can_set_native_method_prefix);
    DEL_CAPABILITY(can_retransform_classes);
    DEL_CAPABILITY(can_retransform_any_class);
    DEL_CAPABILITY(can_generate_resource_exhaustion_heap_events);
    DEL_CAPABILITY(can_generate_resource_exhaustion_threads_events);
#undef DEL_CAPABILITY
    return OK;
  }

  static jvmtiError GetCapabilities(jvmtiEnv* env, jvmtiCapabilities* capabilities_ptr) {
    if (!IsValidEnv(env)) {
      return ERR(INVALID_ENVIRONMENT);
    }
    if (capabilities_ptr == nullptr) {
      return ERR(NULL_POINTER);
    }
    ArtJvmTiEnv* art_env = reinterpret_cast<ArtJvmTiEnv*>(env);
    memcpy(reinterpret_cast<void*>(capabilities_ptr),
           reinterpret_cast<void*>(&(art_env->capabilities)),
           sizeof(jvmtiCapabilities));
    return OK;
  }

  static jvmtiError GetCurrentThreadCpuTimerInfo(jvmtiEnv* env, jvmtiTimerInfo* info_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetCurrentThreadCpuTime(jvmtiEnv* env, jlong* nanos_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetThreadCpuTimerInfo(jvmtiEnv* env, jvmtiTimerInfo* info_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetThreadCpuTime(jvmtiEnv* env, jthread thread, jlong* nanos_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetTimerInfo(jvmtiEnv* env, jvmtiTimerInfo* info_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetTime(jvmtiEnv* env, jlong* nanos_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetAvailableProcessors(jvmtiEnv* env, jint* processor_count_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError AddToBootstrapClassLoaderSearch(jvmtiEnv* env, const char* segment) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError AddToSystemClassLoaderSearch(jvmtiEnv* env, const char* segment) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetSystemProperties(jvmtiEnv* env, jint* count_ptr, char*** property_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetSystemProperty(jvmtiEnv* env, const char* property, char** value_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError SetSystemProperty(jvmtiEnv* env, const char* property, const char* value) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetPhase(jvmtiEnv* env, jvmtiPhase* phase_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError DisposeEnvironment(jvmtiEnv* env) {
    if (!IsValidEnv(env)) {
      return ERR(INVALID_ENVIRONMENT);
    }
    delete env;
    return OK;
  }

  static jvmtiError SetEnvironmentLocalStorage(jvmtiEnv* env, const void* data) {
    if (!IsValidEnv(env)) {
      return ERR(INVALID_ENVIRONMENT);
    }
    reinterpret_cast<ArtJvmTiEnv*>(env)->local_data = const_cast<void*>(data);
    return OK;
  }

  static jvmtiError GetEnvironmentLocalStorage(jvmtiEnv* env, void** data_ptr) {
    if (!IsValidEnv(env)) {
      return ERR(INVALID_ENVIRONMENT);
    }
    *data_ptr = reinterpret_cast<ArtJvmTiEnv*>(env)->local_data;
    return OK;
  }

  static jvmtiError GetVersionNumber(jvmtiEnv* env, jint* version_ptr) {
    if (!IsValidEnv(env)) {
      return ERR(INVALID_ENVIRONMENT);
    }
    *version_ptr = JVMTI_VERSION;
    return OK;
  }

  static jvmtiError GetErrorName(jvmtiEnv* env, jvmtiError error,  char** name_ptr) {
    if (!IsValidEnv(env)) {
      return ERR(INVALID_ENVIRONMENT);
    }
    if (name_ptr == nullptr) {
      return ERR(NULL_POINTER);
    }
    switch (error) {
#define ERROR_CASE(e) case (JVMTI_ERROR_ ## e) : do { \
          *name_ptr = const_cast<char*>("JVMTI_ERROR_"#e); \
          return OK; \
        } while (false)
      ERROR_CASE(NONE);
      ERROR_CASE(INVALID_THREAD);
      ERROR_CASE(INVALID_THREAD_GROUP);
      ERROR_CASE(INVALID_PRIORITY);
      ERROR_CASE(THREAD_NOT_SUSPENDED);
      ERROR_CASE(THREAD_NOT_ALIVE);
      ERROR_CASE(INVALID_OBJECT);
      ERROR_CASE(INVALID_CLASS);
      ERROR_CASE(CLASS_NOT_PREPARED);
      ERROR_CASE(INVALID_METHODID);
      ERROR_CASE(INVALID_LOCATION);
      ERROR_CASE(INVALID_FIELDID);
      ERROR_CASE(NO_MORE_FRAMES);
      ERROR_CASE(OPAQUE_FRAME);
      ERROR_CASE(TYPE_MISMATCH);
      ERROR_CASE(INVALID_SLOT);
      ERROR_CASE(DUPLICATE);
      ERROR_CASE(NOT_FOUND);
      ERROR_CASE(INVALID_MONITOR);
      ERROR_CASE(NOT_MONITOR_OWNER);
      ERROR_CASE(INTERRUPT);
      ERROR_CASE(INVALID_CLASS_FORMAT);
      ERROR_CASE(CIRCULAR_CLASS_DEFINITION);
      ERROR_CASE(FAILS_VERIFICATION);
      ERROR_CASE(UNSUPPORTED_REDEFINITION_METHOD_ADDED);
      ERROR_CASE(UNSUPPORTED_REDEFINITION_SCHEMA_CHANGED);
      ERROR_CASE(INVALID_TYPESTATE);
      ERROR_CASE(UNSUPPORTED_REDEFINITION_HIERARCHY_CHANGED);
      ERROR_CASE(UNSUPPORTED_REDEFINITION_METHOD_DELETED);
      ERROR_CASE(UNSUPPORTED_VERSION);
      ERROR_CASE(NAMES_DONT_MATCH);
      ERROR_CASE(UNSUPPORTED_REDEFINITION_CLASS_MODIFIERS_CHANGED);
      ERROR_CASE(UNSUPPORTED_REDEFINITION_METHOD_MODIFIERS_CHANGED);
      ERROR_CASE(UNMODIFIABLE_CLASS);
      ERROR_CASE(NOT_AVAILABLE);
      ERROR_CASE(MUST_POSSESS_CAPABILITY);
      ERROR_CASE(NULL_POINTER);
      ERROR_CASE(ABSENT_INFORMATION);
      ERROR_CASE(INVALID_EVENT_TYPE);
      ERROR_CASE(ILLEGAL_ARGUMENT);
      ERROR_CASE(NATIVE_METHOD);
      ERROR_CASE(CLASS_LOADER_UNSUPPORTED);
      ERROR_CASE(OUT_OF_MEMORY);
      ERROR_CASE(ACCESS_DENIED);
      ERROR_CASE(WRONG_PHASE);
      ERROR_CASE(INTERNAL);
      ERROR_CASE(UNATTACHED_THREAD);
      ERROR_CASE(INVALID_ENVIRONMENT);
#undef ERROR_CASE
      default: {
        *name_ptr = const_cast<char*>("JVMTI_ERROR_UNKNOWN");
        return ERR(ILLEGAL_ARGUMENT);
      }
    }
  }

  static jvmtiError SetVerboseFlag(jvmtiEnv* env, jvmtiVerboseFlag flag, jboolean value) {
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetJLocationFormat(jvmtiEnv* env, jvmtiJlocationFormat* format_ptr) {
    return ERR(NOT_IMPLEMENTED);
  }

  // TODO Remove this once events are working.
  static jvmtiError RetransformClassWithHook(jvmtiEnv* env,
                                             jclass klass,
                                             jvmtiEventClassFileLoadHook hook) {
    std::vector<jclass> classes;
    classes.push_back(klass);
    return RetransformClassesWithHook(reinterpret_cast<ArtJvmTiEnv*>(env), classes, hook);
  }

  // TODO This will be called by the event handler for the art::ti Event Load Event
  static jvmtiError RetransformClassesWithHook(ArtJvmTiEnv* env,
                                               std::vector<jclass> classes,
                                               jvmtiEventClassFileLoadHook hook) {
    if (!IsValidEnv(env)) {
      return ERR(INVALID_ENVIRONMENT);
    }
    for (jclass klass : classes) {
      JNIEnv* jni_env = nullptr;
      jobject loader = nullptr;
      std::string name;
      jobject protection_domain = nullptr;
      jint data_len = 0;
      unsigned char* dex_data = nullptr;
      jvmtiError ret = OK;
      std::string location;
      if ((ret = GetTransformationData(env,
                                       klass,
                                       /*out*/&location,
                                       /*out*/&jni_env,
                                       /*out*/&loader,
                                       /*out*/&name,
                                       /*out*/&protection_domain,
                                       /*out*/&data_len,
                                       /*out*/&dex_data)) != OK) {
        // TODO Do something more here?
        return ret;
      }
      jint new_data_len = 0;
      unsigned char* new_dex_data = nullptr;
      hook(env, jni_env, klass, loader, name.c_str(), protection_domain, data_len, dex_data,
           /*out*/&new_data_len, /*out*/&new_dex_data);
      if ((new_data_len != 0 || new_dex_data != nullptr) && new_dex_data != dex_data) {
        MoveTransformedFileIntoRuntime(env,
                                       jni_env,
                                       klass,
                                       std::move(location),
                                       new_data_len,
                                       new_dex_data);
        env->Deallocate(new_dex_data);
      }
      env->Deallocate(dex_data);
    }
    return OK;
  }

 private:
  static jvmtiError GetTransformationData(ArtJvmTiEnv* env,
                                          jclass klass,
                                          /*out*/std::string* location,
                                          /*out*/JNIEnv** jni_env_ptr,
                                          /*out*/jobject* loader,
                                          /*out*/std::string* name,
                                          /*out*/jobject* protection_domain,
                                          /*out*/jint* data_len,
                                          /*out*/unsigned char** dex_data) {
    // TODO Check for error here.
    jint ret = env->art_vm->GetEnv(reinterpret_cast<void**>(jni_env_ptr), JNI_VERSION_1_1);
    if (ret != JNI_OK) {
      // TODO Different error might be better?
      return ERR(INTERNAL);
    }
    JNIEnv* jni_env = *jni_env_ptr;
    art::ScopedObjectAccess soa(jni_env);
    art::StackHandleScope<3> hs(art::Thread::Current());
    art::Handle<art::mirror::Class> hs_klass(hs.NewHandle(soa.Decode<art::mirror::Class*>(klass)));
    *loader = soa.AddLocalReference<jobject>(hs_klass->GetClassLoader());
    *name = art::mirror::Class::ComputeName(hs_klass)->ToModifiedUtf8();
    // TODO is this always null?
    *protection_domain = nullptr;
    const art::DexFile& dex = hs_klass->GetDexFile();
    *location = dex.GetLocation();
    *data_len = static_cast<jint>(dex.Size());
    // TODO We should maybe change allocate to allow us to mprotect this memory and stop writes.
    jvmtiError alloc_error = env->Allocate(*data_len, dex_data);
    if (alloc_error != OK) {
      return alloc_error;
    }
    // Copy the data into a temporary buffer.
    memcpy(reinterpret_cast<void*>(*dex_data),
           reinterpret_cast<const void*>(dex.Begin()),
           *data_len);
    return OK;
  }

  static bool ReadChecksum(jint data_len, const unsigned char* dex, /*out*/uint32_t* res) {
    if (data_len < static_cast<jint>(sizeof(art::DexFile::Header))) {
      return false;
    }
    *res = reinterpret_cast<const art::DexFile::Header*>(dex)->checksum_;
    return true;
  }

  static art::MemMap* MoveDataToMemMap(const std::string& original_location,
                                       jint data_len,
                                       unsigned char* dex_data) {
    // TODO Move to a memmap
    std::string error_msg;
    art::MemMap* map =
        art::MemMap::MapAnonymous(
            art::StringPrintf("%s-transformed", original_location.c_str()).c_str(),
            nullptr,
            data_len,
            PROT_READ|PROT_WRITE,
            /*low_4gb*/false,
            /*reuse*/false,
            &error_msg);
    CHECK(map != nullptr);
    memcpy(map->Begin(), dex_data, data_len);
    map->Protect(PROT_READ);
    return map;
  }

  // Make the runtime actually load the dex file
  static jvmtiError MoveTransformedFileIntoRuntime(ArtJvmTiEnv* env,
                                                   JNIEnv* jni_env,
                                                   jclass jklass,
                                                   std::string original_location,
                                                   jint data_len,
                                                   unsigned char* dex_data) {
    const char* dex_file_name = "Ldalvik/system/DexFile;";
    art::Thread* self = art::Thread::Current();
    art::Runtime* runtime = art::Runtime::Current();
    art::ThreadList* threads = runtime->GetThreadList();
    art::ClassLinker* class_linker = runtime->GetClassLinker();
    uint32_t checksum = 0;
    if (!ReadChecksum(data_len, dex_data, &checksum)) {
      return ERR(INVALID_CLASS_FORMAT);
    }

    art::MemMap* map = MoveDataToMemMap(original_location, data_len, dex_data);
    if (map == nullptr) {
      return ERR(INTERNAL);
    }
    std::string error_msg;
    // Load the new dex_data in memory (mmap it, etc)
    std::unique_ptr<const art::DexFile> new_dex_file = art::DexFile::OpenMemory(map->GetName(),
                                                                                checksum,
                                                                                map,
                                                                                &error_msg);
    CHECK(new_dex_file.get() != nullptr) << "Unable to load dex file! " << error_msg;

    // Get mutator lock. We need the lifetimes of these variables to be longer then current lock
    // (since there isn't upgrading of the lock) so we don't use soa.
    art::ThreadState old_state = self->TransitionFromSuspendedToRunnable();
    {
      art::StackHandleScope<10> hs(self);
      art::Handle<art::mirror::ClassLoader> null_loader =
          art::ScopedNullHandle<art::mirror::ClassLoader>();
      art::ArtField* dex_file_cookie_field = class_linker->FindClass(self, dex_file_name, null_loader)
            ->FindDeclaredInstanceField("mCookie", "Ljava/lang/Object;");
      art::ArtField* dex_file_internal_cookie_field =
          class_linker->FindClass(self, dex_file_name, null_loader)
            ->FindDeclaredInstanceField("mInternalCookie", "Ljava/lang/Object;");
      CHECK(dex_file_cookie_field != nullptr);
      art::Handle<art::mirror::Class> klass(
          hs.NewHandle(art::down_cast<art::mirror::Class*>(self->DecodeJObject(jklass))));
      // Find dalvik.system.DexFile that represents the dex file we are changing.
      art::Handle<art::mirror::Object> dex_file_obj(
          hs.NewHandle<art::mirror::Object>(FindDalvikSystemDexFileForClass(jni_env, klass)));
      if (dex_file_obj.Get() == nullptr) {
        self->TransitionFromRunnableToSuspended(old_state);
        LOG(art::ERROR) << "Could not find DexFile.";
        return ERR(INTERNAL);
      }
      art::Handle<art::mirror::LongArray> art_dex_array(
          hs.NewHandle<art::mirror::LongArray>(
              dex_file_cookie_field->GetObject(dex_file_obj.Get())->AsLongArray()));
      art::Handle<art::mirror::LongArray> new_art_dex_array(
          hs.NewHandle<art::mirror::LongArray>(
              InsertDexFileIntoArray(self, new_dex_file.get(), art_dex_array)));
      art::Handle<art::mirror::DexCache> cache(
          hs.NewHandle(AllocateDexCache(self, *new_dex_file.get(), runtime->GetLinearAlloc())));
      self->TransitionFromRunnableToSuspended(old_state);

      threads->SuspendAll("moving dex file into runtime", /*long_suspend*/true);
      // Change the mCookie field. Old value will be GC'd as normal.
      dex_file_cookie_field->SetObject<false>(dex_file_obj.Get(), new_art_dex_array.Get());
      dex_file_internal_cookie_field->SetObject<false>(dex_file_obj.Get(), new_art_dex_array.Get());
      // Invalidate existing methods.
      InvalidateExistingMethods(self, klass, cache, new_dex_file.release());
      // TODO Put the new art::DexFile into the array of native dex files in dalvik.system.DexFile
      //      at the second (2) element of the array (first is the oat file if it exists).
      // TODO Null out the oat file from the dalvik.system.DexFile (maybe not sure if needed).
      // TODO Replace references to old dex file in art::mirror::Class and art::ArtMethod's
      // TODO Resume.
      // TODO Invalidate all the functions
      //
      // TODO (2) do error checks
      // TODO (2) Refactor this into a more clean architecture.

      // TODO This is needed to make sure that the HandleScope dies with mutator_lock_.
    }
    threads->ResumeAll();
    return OK;
  }

  // TODO Dedup with ClassLinker::AllocDexCache
  static art::mirror::DexCache* AllocateDexCache(art::Thread* self,
                                                 const art::DexFile& dex_file,
                                                 art::LinearAlloc* linear_alloc)
      SHARED_REQUIRES(art::Locks::mutator_lock_) {
    auto* runtime = art::Runtime::Current();
    auto* class_linker = runtime->GetClassLinker();
    art::PointerSize image_pointer_size = class_linker->GetImagePointerSize();
    art::StackHandleScope<6> hs(self);
    auto dex_cache(hs.NewHandle(art::down_cast<art::mirror::DexCache*>(
        class_linker->GetClassRoot(art::ClassLinker::kJavaLangDexCache)->AllocObject(self))));
    if (dex_cache.Get() == nullptr) {
      self->AssertPendingOOMException();
      return nullptr;
    }
    auto location(
        hs.NewHandle(runtime->GetInternTable()->InternStrong(dex_file.GetLocation().c_str())));
    if (location.Get() == nullptr) {
      self->AssertPendingOOMException();
      return nullptr;
    }
    art::DexCacheArraysLayout layout(image_pointer_size, &dex_file);
    uint8_t* raw_arrays = nullptr;
    if (dex_file.GetOatDexFile() != nullptr &&
        dex_file.GetOatDexFile()->GetDexCacheArrays() != nullptr) {
      raw_arrays = dex_file.GetOatDexFile()->GetDexCacheArrays();
    } else if (dex_file.NumStringIds() != 0u || dex_file.NumTypeIds() != 0u ||
        dex_file.NumMethodIds() != 0u || dex_file.NumFieldIds() != 0u) {
      // Zero-initialized.
      raw_arrays = reinterpret_cast<uint8_t*>(linear_alloc->Alloc(self, layout.Size()));
    }
    art::GcRoot<art::mirror::String>* strings = (dex_file.NumStringIds() == 0u) ? nullptr :
        reinterpret_cast<art::GcRoot<art::mirror::String>*>(raw_arrays + layout.StringsOffset());
    art::GcRoot<art::mirror::Class>* types = (dex_file.NumTypeIds() == 0u) ? nullptr :
        reinterpret_cast<art::GcRoot<art::mirror::Class>*>(raw_arrays + layout.TypesOffset());
    art::ArtMethod** methods = (dex_file.NumMethodIds() == 0u) ? nullptr :
        reinterpret_cast<art::ArtMethod**>(raw_arrays + layout.MethodsOffset());
    art::ArtField** fields = (dex_file.NumFieldIds() == 0u) ? nullptr :
        reinterpret_cast<art::ArtField**>(raw_arrays + layout.FieldsOffset());
    dex_cache->Init(&dex_file,
                    location.Get(),
                    strings,
                    dex_file.NumStringIds(),
                    types,
                    dex_file.NumTypeIds(),
                    methods,
                    dex_file.NumMethodIds(),
                    fields,
                    dex_file.NumFieldIds(),
                    image_pointer_size);
    return dex_cache.Get();
  }

  // TODO Create new DexCache with new DexFile.
  // TODO reset dex_class_def_idx_
  // TODO for each method reset entry_point_from_quick_compiled_code_ to bridge
  // TODO for each method reset dex_code_item_offset_
  // TODO for each method reset dex_method_index_
  // TODO for each method set dex_cache_resolved_methods_ to new DexCache
  // TODO for each method set dex_cache_resolved_types_ to new DexCache
  static void InvalidateExistingMethods(art::Thread* self,
                                        art::Handle<art::mirror::Class>& klass,
                                        art::Handle<art::mirror::DexCache>& cache,
                                        const art::DexFile* dex_file)
      SHARED_REQUIRES(art::Locks::mutator_lock_) {
    auto* runtime = art::Runtime::Current();
    std::string descriptor_storage;
    const char* descriptor = klass->GetDescriptor(&descriptor_storage);
    // Get the new class def
    const art::DexFile::ClassDef* class_def = dex_file->FindClassDef(
        descriptor, art::ComputeModifiedUtf8Hash(descriptor));
    CHECK(class_def != nullptr);
    const art::DexFile::TypeId& declaring_class_id = dex_file->GetTypeId(class_def->class_idx_);
    art::PointerSize image_pointer_size = runtime->GetClassLinker()->GetImagePointerSize();
    art::StackHandleScope<6> hs(self);
    const art::DexFile& old_dex_file = klass->GetDexFile();
    // Need to
    for (art::ArtMethod& method : klass->GetMethods(image_pointer_size)) {
      // TODO Find the code_item!
      // TODO Find the dex method index and dex_code_item_offset to set
      const art::DexFile::StringId* new_name_id = dex_file->FindStringId(method.GetName());
      uint16_t method_return_idx =
          dex_file->GetIndexForTypeId(*dex_file->FindTypeId(method.GetReturnTypeDescriptor()));
      const auto* old_type_list = method.GetParameterTypeList();
      std::vector<uint16_t> new_type_list;
      for (uint32_t i = 0; old_type_list != nullptr && i < old_type_list->Size(); i++) {
        new_type_list.push_back(
            dex_file->GetIndexForTypeId(
                *dex_file->FindTypeId(
                    old_dex_file.GetTypeDescriptor(
                        old_dex_file.GetTypeId(
                            old_type_list->GetTypeItem(i).type_idx_)))));
      }
      const art::DexFile::ProtoId* proto_id = dex_file->FindProtoId(method_return_idx,
                                                                    new_type_list);
      CHECK(proto_id != nullptr || old_type_list == nullptr);
      const art::DexFile::MethodId* method_id = dex_file->FindMethodId(declaring_class_id,
                                                                       *new_name_id,
                                                                       *proto_id);
      CHECK(method_id != nullptr);
      uint32_t dex_method_idx = dex_file->GetIndexForMethodId(*method_id);
      method.SetDexMethodIndex(dex_method_idx);
      method.SetEntryPointFromQuickCompiledCode(
          runtime->GetClassLinker()->GetClassLinkerQuickToInterpreterBridge());
      const uint8_t* class_data = dex_file->GetClassData(*class_def);
      CHECK(class_data != nullptr);
      art::ClassDataItemIterator it(*dex_file, class_data);
      // Skip fields
      while (it.HasNextStaticField()) {
        it.Next();
      }
      while (it.HasNextInstanceField()) {
        it.Next();
      }
      bool found_method = false;
      while (it.HasNextDirectMethod()) {
        if (it.GetMemberIndex() == dex_method_idx) {
          method.SetCodeItemOffset(it.GetMethodCodeItemOffset());
          found_method = true;
          break;
        }
        it.Next();
      }
      while (!found_method && it.HasNextVirtualMethod()) {
        if (it.GetMemberIndex() == dex_method_idx) {
          method.SetCodeItemOffset(it.GetMethodCodeItemOffset());
          found_method = true;
          break;
        }
        it.Next();
      }
      CHECK(found_method);

      // TODO Set these.
      method.SetDexCacheResolvedMethods(cache->GetResolvedMethods(), image_pointer_size);
      method.SetDexCacheResolvedTypes(cache->GetResolvedTypes(), image_pointer_size);
    }

    // Update the class fields.
    // Need to update class last since the ArtMethod gets its DexFile from the class.
    klass->SetDexCache(cache.Get());
    klass->SetDexClassDefIndex(dex_file->GetIndexForClassDef(*class_def));
    klass->SetDexCacheStrings(cache->GetStrings());
    klass->SetDexTypeIndex(dex_file->GetIndexForTypeId(*dex_file->FindTypeId(descriptor)));
  }

  // Adds the dex file.
  static art::mirror::LongArray* InsertDexFileIntoArray(art::Thread* self,
                                                        const art::DexFile* dex,
                                                        art::Handle<art::mirror::LongArray>& orig)
      SHARED_REQUIRES(art::Locks::mutator_lock_) {
    art::StackHandleScope<1> hs(self);
    CHECK_GE(orig->GetLength(), 1);
    art::Handle<art::mirror::LongArray> ret(
        hs.NewHandle(art::mirror::LongArray::Alloc(self, orig->GetLength() + 1)));
    CHECK(ret.Get() != nullptr);
    // Copy the oat-dex.
    // TODO Should I clear this element?
    ret->SetWithoutChecks<false>(0, orig->GetWithoutChecks(0));
    ret->SetWithoutChecks<false>(1, static_cast<int64_t>(reinterpret_cast<intptr_t>(dex)));
    ret->Memcpy(2, orig.Get(), 1, orig->GetLength() - 1);
    return ret.Get();
  }

  // TODO Handle all types of class loaders.
  // TODO Rename this functions
  // TODO Make it actually add the stuff.
  static art::mirror::Object* FindDalvikSystemDexFileForClass(
      JNIEnv* jni_env,
      art::Handle<art::mirror::Class> klass) SHARED_REQUIRES(art::Locks::mutator_lock_) {
    const char* dex_path_list_element_array_name = "[Ldalvik/system/DexPathList$Element;";
    const char* dex_path_list_element_name = "Ldalvik/system/DexPathList$Element;";
    const char* dex_file_name = "Ldalvik/system/DexFile;";
    const char* dex_path_list_name = "Ldalvik/system/DexPathList;";
    const char* dex_class_loader_name = "Ldalvik/system/BaseDexClassLoader;";
    art::Handle<art::mirror::ClassLoader> null_loader =
        art::ScopedNullHandle<art::mirror::ClassLoader>();

    const char* base_loader_name = "Ldalvik/system/BaseDexClassLoader;";
    art::Thread* self = art::Thread::Current();
    CHECK(!self->IsExceptionPending());
    art::StackHandleScope<9> hs(self);
    art::ClassLinker* class_linker = art::Runtime::Current()->GetClassLinker();

    art::Handle<art::mirror::Class> base_dex_loader_class(hs.NewHandle(class_linker->FindClass(
        self, dex_class_loader_name, null_loader)));

    // TODO Rewrite all fo the BaseDexClassLoader stuff with mirror maybe
    art::ArtField* path_list_field = base_dex_loader_class->FindDeclaredInstanceField(
        "pathList", dex_path_list_name);
    CHECK(path_list_field != nullptr);

    art::ArtField* dex_path_list_element_field =
        class_linker->FindClass(self, dex_path_list_name, null_loader)
          ->FindDeclaredInstanceField("dexElements", dex_path_list_element_array_name);
    CHECK(dex_path_list_element_field != nullptr);

    art::ArtField* element_dex_file_field =
        class_linker->FindClass(self, dex_path_list_element_name, null_loader)
          ->FindDeclaredInstanceField("dexFile", dex_file_name);
    CHECK(element_dex_file_field != nullptr);

    art::Handle<art::mirror::ClassLoader> loader(hs.NewHandle(klass->GetClassLoader()));
    art::Handle<art::mirror::Class> loader_class(hs.NewHandle(loader->GetClass()));
    // Check if loader is a BaseDexClassLoader
    if (!loader_class->IsSubClass(base_dex_loader_class.Get())) {
      LOG(art::ERROR) << "The classloader is not a BaseDexClassLoader!";
      // TODO Print something or some other error?
      return nullptr;
    }
    art::Handle<art::mirror::Object> path_list(
        hs.NewHandle(path_list_field->GetObject(loader.Get())));
    CHECK(path_list.Get() != nullptr);
    CHECK(!self->IsExceptionPending());
    art::Handle<art::mirror::ObjectArray<art::mirror::Object>> dex_elements_list(
        hs.NewHandle(art::down_cast<art::mirror::ObjectArray<art::mirror::Object>*>(
            dex_path_list_element_field->GetObject(path_list.Get()))));
    CHECK(!self->IsExceptionPending());
    CHECK(dex_elements_list.Get() != nullptr);
    size_t num_elements = dex_elements_list->GetLength();
    art::MutableHandle<art::mirror::Object> current_element(
        hs.NewHandle<art::mirror::Object>(nullptr));
    art::MutableHandle<art::mirror::Object> first_dex_file(
        hs.NewHandle<art::mirror::Object>(nullptr));
    // TODO Use the code for FindClassInPathClassLoader to do the iteration and find the DexFile
    // TODO Fast way to do it:
    // TODO Get the first element (with a dex file).
    // TODO   Get the first dalvik.system.DexFile
    for (size_t i = 0; i < num_elements; i++) {
      current_element.Assign(dex_elements_list->Get(i));
      CHECK(current_element.Get() != nullptr);
      CHECK(!self->IsExceptionPending());
      CHECK(dex_elements_list.Get() != nullptr);
      CHECK_EQ(current_element->GetClass(), class_linker->FindClass(self,
                                                                    dex_path_list_element_name,
                                                                    null_loader));
      // TODO Really should probably put it into the used dex file instead.
      first_dex_file.Assign(element_dex_file_field->GetObject(current_element.Get()));
      if (first_dex_file.Get() != nullptr) {
        return first_dex_file.Get();
      }
    }
    return nullptr;
  }
};

static bool IsJvmtiVersion(jint version) {
  return version ==  JVMTI_VERSION_1 ||
         version == JVMTI_VERSION_1_0 ||
         version == JVMTI_VERSION_1_1 ||
         version == JVMTI_VERSION_1_2 ||
         version == JVMTI_VERSION;
}

// Creates a jvmtiEnv and returns it with the art::ti::Env that is associated with it. new_art_ti
// is a pointer to the uninitialized memory for an art::ti::Env.
static void CreateArtJvmTiEnv(art::JavaVMExt* vm, /*out*/void** new_jvmtiEnv) {
  struct ArtJvmTiEnv* env = new ArtJvmTiEnv(vm);
  *new_jvmtiEnv = env;
}

// A hook that the runtime uses to allow plugins to handle GetEnv calls. It returns true and
// places the return value in 'env' if this library can handle the GetEnv request. Otherwise
// returns false and does not modify the 'env' pointer.
static jint GetEnvHandler(art::JavaVMExt* vm, /*out*/void** env, jint version) {
  if (IsJvmtiVersion(version)) {
    CreateArtJvmTiEnv(vm, env);
    return JNI_OK;
  } else {
    printf("version 0x%x is not valid!", version);
    return JNI_EVERSION;
  }
}

// The plugin initialization function. This adds the jvmti environment.
extern "C" bool ArtPlugin_Initialize() {
  art::Runtime::Current()->GetJavaVM()->AddEnvironmentHook(GetEnvHandler);
  return true;
}

// The actual struct holding all of the entrypoints into the jvmti interface.
const jvmtiInterface_1 gJvmtiInterface = {
  // SPECIAL FUNCTION: RetransformClassesWithHook Is normally reserved1
  // TODO Remove once we have events working.
  reinterpret_cast<void*>(JvmtiFunctions::RetransformClassWithHook),
  // nullptr,  // reserved1
  JvmtiFunctions::SetEventNotificationMode,
  nullptr,  // reserved3
  JvmtiFunctions::GetAllThreads,
  JvmtiFunctions::SuspendThread,
  JvmtiFunctions::ResumeThread,
  JvmtiFunctions::StopThread,
  JvmtiFunctions::InterruptThread,
  JvmtiFunctions::GetThreadInfo,
  JvmtiFunctions::GetOwnedMonitorInfo,  // 10
  JvmtiFunctions::GetCurrentContendedMonitor,
  JvmtiFunctions::RunAgentThread,
  JvmtiFunctions::GetTopThreadGroups,
  JvmtiFunctions::GetThreadGroupInfo,
  JvmtiFunctions::GetThreadGroupChildren,
  JvmtiFunctions::GetFrameCount,
  JvmtiFunctions::GetThreadState,
  JvmtiFunctions::GetCurrentThread,
  JvmtiFunctions::GetFrameLocation,
  JvmtiFunctions::NotifyFramePop,  // 20
  JvmtiFunctions::GetLocalObject,
  JvmtiFunctions::GetLocalInt,
  JvmtiFunctions::GetLocalLong,
  JvmtiFunctions::GetLocalFloat,
  JvmtiFunctions::GetLocalDouble,
  JvmtiFunctions::SetLocalObject,
  JvmtiFunctions::SetLocalInt,
  JvmtiFunctions::SetLocalLong,
  JvmtiFunctions::SetLocalFloat,
  JvmtiFunctions::SetLocalDouble,  // 30
  JvmtiFunctions::CreateRawMonitor,
  JvmtiFunctions::DestroyRawMonitor,
  JvmtiFunctions::RawMonitorEnter,
  JvmtiFunctions::RawMonitorExit,
  JvmtiFunctions::RawMonitorWait,
  JvmtiFunctions::RawMonitorNotify,
  JvmtiFunctions::RawMonitorNotifyAll,
  JvmtiFunctions::SetBreakpoint,
  JvmtiFunctions::ClearBreakpoint,
  nullptr,  // reserved40
  JvmtiFunctions::SetFieldAccessWatch,
  JvmtiFunctions::ClearFieldAccessWatch,
  JvmtiFunctions::SetFieldModificationWatch,
  JvmtiFunctions::ClearFieldModificationWatch,
  JvmtiFunctions::IsModifiableClass,
  JvmtiFunctions::Allocate,
  JvmtiFunctions::Deallocate,
  JvmtiFunctions::GetClassSignature,
  JvmtiFunctions::GetClassStatus,
  JvmtiFunctions::GetSourceFileName,  // 50
  JvmtiFunctions::GetClassModifiers,
  JvmtiFunctions::GetClassMethods,
  JvmtiFunctions::GetClassFields,
  JvmtiFunctions::GetImplementedInterfaces,
  JvmtiFunctions::IsInterface,
  JvmtiFunctions::IsArrayClass,
  JvmtiFunctions::GetClassLoader,
  JvmtiFunctions::GetObjectHashCode,
  JvmtiFunctions::GetObjectMonitorUsage,
  JvmtiFunctions::GetFieldName,  // 60
  JvmtiFunctions::GetFieldDeclaringClass,
  JvmtiFunctions::GetFieldModifiers,
  JvmtiFunctions::IsFieldSynthetic,
  JvmtiFunctions::GetMethodName,
  JvmtiFunctions::GetMethodDeclaringClass,
  JvmtiFunctions::GetMethodModifiers,
  nullptr,  // reserved67
  JvmtiFunctions::GetMaxLocals,
  JvmtiFunctions::GetArgumentsSize,
  JvmtiFunctions::GetLineNumberTable,  // 70
  JvmtiFunctions::GetMethodLocation,
  JvmtiFunctions::GetLocalVariableTable,
  JvmtiFunctions::SetNativeMethodPrefix,
  JvmtiFunctions::SetNativeMethodPrefixes,
  JvmtiFunctions::GetBytecodes,
  JvmtiFunctions::IsMethodNative,
  JvmtiFunctions::IsMethodSynthetic,
  JvmtiFunctions::GetLoadedClasses,
  JvmtiFunctions::GetClassLoaderClasses,
  JvmtiFunctions::PopFrame,  // 80
  JvmtiFunctions::ForceEarlyReturnObject,
  JvmtiFunctions::ForceEarlyReturnInt,
  JvmtiFunctions::ForceEarlyReturnLong,
  JvmtiFunctions::ForceEarlyReturnFloat,
  JvmtiFunctions::ForceEarlyReturnDouble,
  JvmtiFunctions::ForceEarlyReturnVoid,
  JvmtiFunctions::RedefineClasses,
  JvmtiFunctions::GetVersionNumber,
  JvmtiFunctions::GetCapabilities,
  JvmtiFunctions::GetSourceDebugExtension,  // 90
  JvmtiFunctions::IsMethodObsolete,
  JvmtiFunctions::SuspendThreadList,
  JvmtiFunctions::ResumeThreadList,
  nullptr,  // reserved94
  nullptr,  // reserved95
  nullptr,  // reserved96
  nullptr,  // reserved97
  nullptr,  // reserved98
  nullptr,  // reserved99
  JvmtiFunctions::GetAllStackTraces,  // 100
  JvmtiFunctions::GetThreadListStackTraces,
  JvmtiFunctions::GetThreadLocalStorage,
  JvmtiFunctions::SetThreadLocalStorage,
  JvmtiFunctions::GetStackTrace,
  nullptr,  // reserved105
  JvmtiFunctions::GetTag,
  JvmtiFunctions::SetTag,
  JvmtiFunctions::ForceGarbageCollection,
  JvmtiFunctions::IterateOverObjectsReachableFromObject,
  JvmtiFunctions::IterateOverReachableObjects,  // 110
  JvmtiFunctions::IterateOverHeap,
  JvmtiFunctions::IterateOverInstancesOfClass,
  nullptr,  // reserved113
  JvmtiFunctions::GetObjectsWithTags,
  JvmtiFunctions::FollowReferences,
  JvmtiFunctions::IterateThroughHeap,
  nullptr,  // reserved117
  nullptr,  // reserved118
  nullptr,  // reserved119
  JvmtiFunctions::SetJNIFunctionTable,  // 120
  JvmtiFunctions::GetJNIFunctionTable,
  JvmtiFunctions::SetEventCallbacks,
  JvmtiFunctions::GenerateEvents,
  JvmtiFunctions::GetExtensionFunctions,
  JvmtiFunctions::GetExtensionEvents,
  JvmtiFunctions::SetExtensionEventCallback,
  JvmtiFunctions::DisposeEnvironment,
  JvmtiFunctions::GetErrorName,
  JvmtiFunctions::GetJLocationFormat,
  JvmtiFunctions::GetSystemProperties,  // 130
  JvmtiFunctions::GetSystemProperty,
  JvmtiFunctions::SetSystemProperty,
  JvmtiFunctions::GetPhase,
  JvmtiFunctions::GetCurrentThreadCpuTimerInfo,
  JvmtiFunctions::GetCurrentThreadCpuTime,
  JvmtiFunctions::GetThreadCpuTimerInfo,
  JvmtiFunctions::GetThreadCpuTime,
  JvmtiFunctions::GetTimerInfo,
  JvmtiFunctions::GetTime,
  JvmtiFunctions::GetPotentialCapabilities,  // 140
  nullptr,  // reserved141
  JvmtiFunctions::AddCapabilities,
  JvmtiFunctions::RelinquishCapabilities,
  JvmtiFunctions::GetAvailableProcessors,
  JvmtiFunctions::GetClassVersionNumbers,
  JvmtiFunctions::GetConstantPool,
  JvmtiFunctions::GetEnvironmentLocalStorage,
  JvmtiFunctions::SetEnvironmentLocalStorage,
  JvmtiFunctions::AddToBootstrapClassLoaderSearch,
  JvmtiFunctions::SetVerboseFlag,  // 150
  JvmtiFunctions::AddToSystemClassLoaderSearch,
  JvmtiFunctions::RetransformClasses,
  JvmtiFunctions::GetOwnedMonitorStackDepthInfo,
  JvmtiFunctions::GetObjectSize,
  JvmtiFunctions::GetLocalInstance,
};

};  // namespace openjdkjvmti
