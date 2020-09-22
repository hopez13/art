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

#include <jni.h>

#include <iostream>

#include "android-base/logging.h"

struct ThreadArgs {
  JavaVM* jvm;
  jobject consumer;
  JavaVMAttachArgs* attach_args;
  bool set_in_java;
};

void* ThreadMain(void* arg) {
  ThreadArgs* args = reinterpret_cast<ThreadArgs*>(arg);
  JNIEnv* env = nullptr;
  pthread_t self = pthread_self();

  int err = pthread_setname_np(self, "native-thread");
  CHECK_EQ(err, 0);

  args->jvm->AttachCurrentThread(&env, args->attach_args);

  jclass thread_class = env->FindClass("java/lang/Thread");
  jclass consumer_class = env->FindClass("java/util/function/BiConsumer");
  jmethodID currentThread =
      env->GetStaticMethodID(thread_class, "currentThread", "()Ljava/lang/Thread;");
  jmethodID accept =
      env->GetMethodID(consumer_class, "accept", "(Ljava/lang/Object;Ljava/lang/Object;)V");
  jobject curthr = env->CallStaticObjectMethod(thread_class, currentThread);
  if (args->set_in_java) {
    jmethodID setName = env->GetMethodID(thread_class, "setName", "(Ljava/lang/String;)V");
    jobject str_name = env->NewStringUTF("native-thread-set-java");
    env->CallVoidMethod(curthr, setName, str_name);
  }

  char name_chars[1024];
  err = pthread_getname_np(self, name_chars, sizeof(name_chars));
  CHECK_EQ(err, 0);
  jobject str_name = env->NewStringUTF(name_chars);

  env->CallVoidMethod(args->consumer, accept, str_name, curthr);

  args->jvm->DetachCurrentThread();

  return nullptr;
}

extern "C" JNIEXPORT void Java_Main_RunThreadTestWithName(JNIEnv* env,
                                                          jclass /*clazz*/,
                                                          jobject consumer) {
  jobject global_consumer = env->NewGlobalRef(consumer);
  JavaVMAttachArgs args;
  args.group = nullptr;
  args.name = "java-native-thread";
  args.version = JNI_VERSION_1_6;
  ThreadArgs ta {
    .jvm = nullptr, .consumer = global_consumer, .attach_args = &args, .set_in_java = false
  };
  env->GetJavaVM(&ta.jvm);
  pthread_t child;
  pthread_create(&child, nullptr, ThreadMain, &ta);
  void* ret;
  pthread_join(child, &ret);
  env->DeleteGlobalRef(ta.consumer);
}

extern "C" JNIEXPORT void Java_Main_RunThreadTest(JNIEnv* env, jclass /*clazz*/, jobject consumer) {
  jobject global_consumer = env->NewGlobalRef(consumer);
  ThreadArgs ta {
    .jvm = nullptr, .consumer = global_consumer, .attach_args = nullptr, .set_in_java = false
  };
  env->GetJavaVM(&ta.jvm);
  pthread_t child;
  pthread_create(&child, nullptr, ThreadMain, &ta);
  void* ret;
  pthread_join(child, &ret);
  env->DeleteGlobalRef(ta.consumer);
}

extern "C" JNIEXPORT void Java_Main_RunThreadTestSetJava(JNIEnv* env,
                                                         jclass /*clazz*/,
                                                         jobject consumer) {
  jobject global_consumer = env->NewGlobalRef(consumer);
  ThreadArgs ta {
    .jvm = nullptr, .consumer = global_consumer, .attach_args = nullptr, .set_in_java = true
  };
  env->GetJavaVM(&ta.jvm);
  pthread_t child;
  pthread_create(&child, nullptr, ThreadMain, &ta);
  void* ret;
  pthread_join(child, &ret);
  env->DeleteGlobalRef(ta.consumer);
}
