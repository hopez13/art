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

#include "java_lang_Detour.h"

#include "jni_internal.h"
#include "reflection.h"
#include "scoped_fast_native_object_access.h"

namespace art {

static jobject Detour_invoke(JNIEnv* env,
                            jobject javaMethod,
                            jobject javaReceiver,
                            jobject javaArgs) {
  ScopedFastNativeObjectAccess soa(env);
  return InvokeDetour(soa, javaMethod, javaReceiver, javaArgs);
}

static JNINativeMethod gMethods[] = {
  NATIVE_METHOD(Detour, invoke, "!(Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;"),
};

void register_java_lang_Detour(JNIEnv* env) {
  REGISTER_NATIVE_METHODS("java/lang/Detour");
}

}  // namespace art
