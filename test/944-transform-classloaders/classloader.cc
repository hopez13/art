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

#include <stdio.h>

#include "base/macros.h"
#include "jni.h"
#include "mirror/class-inl.h"
#include "openjdkjvmti/jvmti.h"
#include "ScopedLocalRef.h"
#include "thread-inl.h"

#include "ti-agent/common_helper.h"
#include "ti-agent/common_load.h"

namespace art {
namespace Test912Classes {


extern "C" JNIEXPORT jlong JNICALL Java_Main_getDexFilePointerNative(
    JNIEnv* env, jclass Main_klass ATTRIBUTE_UNUSED, jclass klass) {
  Runtime* runtime = Runtime::Current();
  // We don't do anything for non-ART runtimes.
  if (runtime == nullptr) {
    return 0x0;
  }
  ScopedObjectAccess soa(env);
  return static_cast<jlong>(reinterpret_cast<intptr_t>(
      &soa.Decode<mirror::Class>(klass)->GetDexFile()));
}

}  // namespace Test912Classes
}  // namespace art
