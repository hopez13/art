/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include "jni.h"

#include "well_known_classes.h"

#define NDK_EXPORT(NDK_SUPPORT_METHOD) \
  extern "C" NDK_SUPPORT_METHOD __attribute__((visibility("default")))

// Declarations of symbols exports for NDK Support (export from ART SDK eventually).

NDK_EXPORT(jobject NdkNewFileDescriptor(C_JNIEnv* c_jnienv, int fd));
NDK_EXPORT(jint NdkGetFileDescriptorNativeFD(C_JNIEnv* c_jnienv, jobject jifd));
NDK_EXPORT(void NdkSetFileDescriptorNativeFD(C_JNIEnv* c_jnienv, jobject jifd, int fd));
NDK_EXPORT(jlong NdkGetFileDescriptorOwnerId(C_JNIEnv* c_jnienv, jobject jifd));
NDK_EXPORT(void NdkSetFileDescriptorOwnerId(C_JNIEnv* c_jnienv, jobject jifd, jlong ownerId));

NDK_EXPORT(jint NdkGetNioBufferPosition(C_JNIEnv* c_jnienv, jobject niob));
NDK_EXPORT(jint NdkGetNioBufferLimit(C_JNIEnv* c_jnienv, jobject niob));
NDK_EXPORT(jint NdkGetNioBufferElementSizeShift(C_JNIEnv* c_jnienv, jobject niob));
NDK_EXPORT(jarray NdkGetNioBufferArray(C_JNIEnv* c_jnienv, jobject niob));
NDK_EXPORT(jint NdkGetNioBufferArrayOffset(C_JNIEnv* c_jnienv, jobject niob));

// Helpers to check invariants, hard errors for getting these wrong.

namespace {

static void inline CheckFileDescriptor(JNIEnv* env, jobject jifd) {
  CHECK(jifd != nullptr);
  CHECK(env->IsInstanceOf(jifd, art::WellKnownClasses::java_io_FileDescriptor));
}

static void inline CheckNioBuffer(JNIEnv* env, jobject niob) {
  CHECK(niob != nullptr);
  CHECK(env->IsInstanceOf(niob, art::WellKnownClasses::java_nio_Buffer));
}

}  // namespace

// Implementation of symbols exported for NDK Support.

jobject NdkNewFileDescriptor(C_JNIEnv* c_jnienv, jint fd) {
  JNIEnv* env = reinterpret_cast<JNIEnv*>(c_jnienv);
  return env->NewObject(art::WellKnownClasses::java_io_FileDescriptor,
                        art::WellKnownClasses::java_io_FileDescriptor_init,
                        fd);
}

int NdkGetFileDescriptorNativeFD(C_JNIEnv* c_jnienv, jobject jifd) {
  JNIEnv* env = reinterpret_cast<JNIEnv*>(c_jnienv);
  CheckFileDescriptor(env, jifd);
  return env->GetIntField(jifd, art::WellKnownClasses::java_io_FileDescriptor_descriptor);
}

void NdkSetFileDescriptorNativeFD(C_JNIEnv* c_jnienv, jobject jifd, jint fd) {
  JNIEnv* env = reinterpret_cast<JNIEnv*>(c_jnienv);
  CheckFileDescriptor(env, jifd);
  env->SetIntField(jifd, art::WellKnownClasses::java_io_FileDescriptor_descriptor, fd);
}

jlong NdkGetFileDescriptorOwnerId(C_JNIEnv* c_jnienv, jobject jifd) {
  JNIEnv* env = reinterpret_cast<JNIEnv*>(c_jnienv);
  CheckFileDescriptor(env, jifd);
  return env->GetLongField(jifd, art::WellKnownClasses::java_io_FileDescriptor_ownerId);
}

void NdkSetFileDescriptorOwnerId(C_JNIEnv* c_jnienv, jobject jifd, jlong fd) {
  JNIEnv* env = reinterpret_cast<JNIEnv*>(c_jnienv);
  CheckFileDescriptor(env, jifd);
  env->SetLongField(jifd, art::WellKnownClasses::java_io_FileDescriptor_ownerId, fd);
}

jint NdkGetNioBufferPosition(C_JNIEnv* c_jnienv, jobject niob) {
  JNIEnv* env = reinterpret_cast<JNIEnv*>(c_jnienv);
  CheckNioBuffer(env, niob);
  return env->GetIntField(niob, art::WellKnownClasses::java_nio_Buffer_position);
}

jint NdkGetNioBufferLimit(C_JNIEnv* c_jnienv, jobject niob) {
  JNIEnv* env = reinterpret_cast<JNIEnv*>(c_jnienv);
  CheckNioBuffer(env, niob);
  return env->GetIntField(niob, art::WellKnownClasses::java_nio_Buffer_limit);
}

jint NdkGetNioBufferElementSizeShift(C_JNIEnv* c_jnienv, jobject niob) {
  JNIEnv* env = reinterpret_cast<JNIEnv*>(c_jnienv);
  CheckNioBuffer(env, niob);
  return env->GetIntField(niob, art::WellKnownClasses::java_nio_Buffer_elementSizeShift);
}

jarray NdkGetNioBufferArray(C_JNIEnv* c_jnienv, jobject niob) {
  JNIEnv* env = reinterpret_cast<JNIEnv*>(c_jnienv);
  CheckNioBuffer(env, niob);
  if (env->ExceptionCheck()) {
    return nullptr;
  }

  // Fast-path if |niob| is a ByteBuffer.
  if (env->IsInstanceOf(niob, art::WellKnownClasses::java_nio_ByteBuffer)) {
    if (env->GetBooleanField(niob, art::WellKnownClasses::java_nio_ByteBuffer_isReadOnly)) {
      return nullptr;
    }
    jobject hb = env->GetObjectField(niob, art::WellKnownClasses::java_nio_ByteBuffer_hb);
    return reinterpret_cast<jarray>(hb);
  }

  jobject array = env->CallObjectMethod(niob, art::WellKnownClasses::java_nio_Buffer_array);
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    return nullptr;
  }
  return reinterpret_cast<jarray>(array);
}

jint NdkGetNioBufferArrayOffset(C_JNIEnv* c_jnienv, jobject niob) {
  constexpr int kBadOffset = -1;

  JNIEnv* env = reinterpret_cast<JNIEnv*>(c_jnienv);
  CheckNioBuffer(env, niob);
  if (env->ExceptionCheck()) {
    return kBadOffset;
  }

  // Fast-path if |niob| is a ByteBuffer.
  if (env->IsInstanceOf(niob, art::WellKnownClasses::java_nio_ByteBuffer)) {
    if (env->GetBooleanField(niob, art::WellKnownClasses::java_nio_ByteBuffer_isReadOnly)) {
      return kBadOffset;
    }
    return env->GetIntField(niob, art::WellKnownClasses::java_nio_ByteBuffer_offset);
  }

  jint arrayOffset = env->CallIntMethod(niob,
                                        art::WellKnownClasses::java_nio_Buffer_arrayOffset);
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    return kBadOffset;
  }
  return arrayOffset;
}

