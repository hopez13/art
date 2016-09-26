/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef ART_RUNTIME_OPENJDKJVMTI_ART_METHOD_DETOURS_H_
#define ART_RUNTIME_OPENJDKJVMTI_ART_METHOD_DETOURS_H_

#include <memory>

#include <jni.h>

#include "base/casts.h"
#include "events.h"
#include "java_vm_ext.h"
#include "jni_env_ext.h"
#include "method_detours.h"

namespace openjdkjvmti {

extern const DetoursInterface gDetoursInterface;

struct ArtDetoursEnv : public detoursEnv {
  art::JavaVMExt* art_vm;

  explicit ArtDetoursEnv(art::JavaVMExt* runtime) : art_vm(runtime) {
    functions = &gDetoursInterface;
  }
};

static inline JNIEnv* GetJniEnv(detoursEnv* env) {
  JNIEnv* ret_value = nullptr;
  jint res = static_cast<ArtDetoursEnv*>(env)->art_vm->GetEnv(
      reinterpret_cast<void**>(&ret_value), JNI_VERSION_1_1);
  if (res != JNI_OK) {
    return nullptr;
  }
  return ret_value;
}

}  // namespace openjdkjvmti

#endif  // ART_RUNTIME_OPENJDKJVMTI_ART_METHOD_DETOURS_H_
