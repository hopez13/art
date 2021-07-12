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

#include "base/logging.h"
#include "jni.h"

namespace art {

extern "C" JNIEXPORT jclass JNICALL Java_Main_jniFindClass(JNIEnv* env, jclass, jstring class_name) {
  const char* name = env->GetStringUTFChars(class_name, nullptr);
  CHECK(name != nullptr);
  jclass clazz = env->FindClass(name);
  env->ReleaseStringUTFChars(class_name, name);
  return clazz;
}

}  // namespace art

