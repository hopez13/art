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

#include "base/utils.h"
#include "jni.h"

namespace art {

typedef jstring InternOrNew(JNIEnv*, const char*);

extern "C" JNIEXPORT jstring JNICALL Java_Main_runInternOrNew(JNIEnv* env,
                                                              jclass klass ATTRIBUTE_UNUSED,
                                                              jlong fn_ptr) {
  InternOrNew* internOrNew = reinterpret_cast<InternOrNew*>(fn_ptr);
  return internOrNew(env, "abcdEF42");
}

}  // namespace art
