/*
 * Copyright 2017 The Android Open Source Project
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

#include <iostream>
#include <pthread.h>
#include <stdio.h>
#include <vector>

#include "gc/heap.h"
#include "gc/space/image_space.h"
#include "gc/space/space-inl.h"
#include "image.h"
#include "jni.h"
#include "mirror/class.h"
#include "runtime.h"
#include "scoped_thread_state_change-inl.h"

namespace art {

namespace {

extern "C" JNIEXPORT jboolean JNICALL Java_Main_bootImageContains(JNIEnv*, jclass, jobject obj) {
  ScopedObjectAccess soa(Thread::Current());
  return Runtime::Current()->GetHeap()->ObjectIsInBootImageSpace(soa.Decode<mirror::Object>(obj))
      ? JNI_TRUE
      : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL Java_Main_appImageContains(JNIEnv*, jclass, jobject obj) {
  ScopedObjectAccess soa(Thread::Current());
  gc::space::ContinuousSpace* space = Runtime::Current()->GetHeap()->FindContinuousSpaceFromObject(
      soa.Decode<mirror::Object>(obj),
      true);
  return (space != nullptr &&
      space->IsImageSpace() &&
      space->AsImageSpace()->GetImageHeader().IsAppImage()) ? JNI_TRUE : JNI_FALSE;
}

}  // namespace

}  // namespace art
