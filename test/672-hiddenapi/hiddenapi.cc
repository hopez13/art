/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "class_linker.h"
#include "dex/dex_file_loader.h"
#include "jni.h"
#include "runtime.h"
#include "scoped_thread_state_change-inl.h"
#include "thread.h"
#include "ti-agent/scoped_utf_chars.h"

namespace art {
namespace Test672HiddenApi {

extern "C" JNIEXPORT void JNICALL Java_Main_appendToBootClassLoader(
    JNIEnv* env, jclass, jstring jpath) {
  ScopedUtfChars utf(env, jpath);
  const char *path = utf.c_str();
  if (path == nullptr) {
    return;
  }

  std::string error_msg;
  std::vector<std::unique_ptr<const DexFile>> dex_files;
  if (!DexFileLoader::Open(
        path, path, /* verify */ false, /* verify_checksum */ true, &error_msg, &dex_files)) {
    LOG(FATAL) << "Could not open " << path << " for boot classpath extension: " << error_msg;
    UNREACHABLE();
  }

  ScopedObjectAccess soa(Thread::Current());
  for (std::unique_ptr<const DexFile>& dex_file : dex_files) {
    Runtime::Current()->GetClassLinker()->AppendToBootClassPath(
        Thread::Current(), *dex_file.release());
  }
}

extern "C" JNIEXPORT jboolean JNICALL Java_JNI_canDiscoverField(
    JNIEnv* env, jclass, jclass klass, jstring name, jboolean is_static) {
  ScopedUtfChars utf_name(env, name);
  jfieldID field = is_static ? env->GetStaticFieldID(klass, utf_name.c_str(), "I")
                             : env->GetFieldID(klass, utf_name.c_str(), "I");
  if (field == NULL) {
    env->ExceptionClear();
    return JNI_FALSE;
  }

  return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL Java_JNI_canDiscoverMethod(
    JNIEnv* env, jclass, jclass klass, jstring name, jboolean is_static) {
  ScopedUtfChars utf_name(env, name);
  jmethodID method = is_static ? env->GetStaticMethodID(klass, utf_name.c_str(), "()I")
                               : env->GetMethodID(klass, utf_name.c_str(), "()I");
  if (method == NULL) {
    env->ExceptionClear();
    return JNI_FALSE;
  }

  return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL Java_JNI_canDiscoverConstructor(
    JNIEnv* env, jclass, jclass klass, jstring args) {
  ScopedUtfChars utf_args(env, args);
  jmethodID method = env->GetMethodID(klass, "<init>", utf_args.c_str());
  if (method == NULL) {
    env->ExceptionClear();
    return JNI_FALSE;
  }

  return JNI_TRUE;
}

extern "C" JNIEXPORT jint JNICALL Java_Reflection_getHiddenApiAccessFlags(JNIEnv*, jclass) {
  return static_cast<jint>(kAccHiddenBlacklist | kAccHiddenGreylist);
}

}  // namespace Test672HiddenApi
}  // namespace art
