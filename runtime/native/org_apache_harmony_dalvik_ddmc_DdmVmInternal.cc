/*
 * Copyright (C) 2008 The Android Open Source Project
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

#include "org_apache_harmony_dalvik_ddmc_DdmVmInternal.h"

#include <android-base/logging.h>

#include "base/file_utils.h"
#include "base/mutex.h"
#include "base/endian_utils.h"
#include "debugger.h"
#include "gc/heap.h"
#include "jni/jni_internal.h"
#include "native_util.h"
#include "nativehelper/jni_macros.h"
#include "nativehelper/scoped_local_ref.h"
#include "nativehelper/scoped_primitive_array.h"
#include "scoped_fast_native_object_access-inl.h"
#include "thread_list.h"

namespace art {

static void DdmVmInternal_enableRecentAllocations(JNIEnv*, jclass, jboolean enable) {
  Dbg::SetAllocTrackingEnabled(enable);
}

static void DdmVmInternal_threadNotify(JNIEnv*, jclass, jboolean enable) {
  Dbg::DdmSetThreadNotification(enable);
}

static JNINativeMethod gMethods[] = {
  NATIVE_METHOD(DdmVmInternal, enableRecentAllocations, "(Z)V"),
  NATIVE_METHOD(DdmVmInternal, threadNotify, "(Z)V"),
};

void register_org_apache_harmony_dalvik_ddmc_DdmVmInternal(JNIEnv* env) {
  REGISTER_NATIVE_METHODS("org/apache/harmony/dalvik/ddmc/DdmVmInternal");
}

}  // namespace art
