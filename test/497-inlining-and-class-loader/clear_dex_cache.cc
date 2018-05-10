/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include "art_method.h"
#include "base/enums.h"
#include "jni.h"
#include "mirror/array-inl.h"
#include "mirror/class-inl.h"
#include "mirror/dex_cache-inl.h"
#include "scoped_thread_state_change-inl.h"
#include "stack.h"
#include "thread.h"

namespace art {

namespace {

extern "C" JNIEXPORT jobject JNICALL Java_Main_cloneResolvedMethods(JNIEnv* env,
                                                                    jclass,
                                                                    jclass cls) {
  ScopedObjectAccess soa(Thread::Current());
  ObjPtr<mirror::DexCache> dex_cache = soa.Decode<mirror::Class>(cls)->GetDexCache();
  size_t num_methods = dex_cache->NumResolvedMethods();
  mirror::MethodDexCacheType* methods = dex_cache->GetResolvedMethods();
  CHECK_EQ(num_methods != 0u, methods != nullptr);
  if (num_methods == 0u) {
    return nullptr;
  }
  jarray array;
  if (sizeof(void*) == 4) {
    array = env->NewIntArray(2u * num_methods);
  } else {
    array = env->NewLongArray(2u * num_methods);
  }
  CHECK(array != nullptr);
  ObjPtr<mirror::Array> decoded_array = soa.Decode<mirror::Array>(array);
  for (size_t i = 0; i != num_methods; ++i) {
    auto pair = mirror::DexCache::GetNativePair(methods, i);
    uint32_t index = pair.index;
    ArtMethod* method = pair.object;
    if (sizeof(void*) == 4) {
      ObjPtr<mirror::IntArray> int_array = ObjPtr<mirror::IntArray>::DownCast(decoded_array);
      int_array->Set(2u * i, index);
      int_array->Set(2u * i + 1u, reinterpret_cast32<jint>(method));
    } else {
      ObjPtr<mirror::LongArray> long_array = ObjPtr<mirror::LongArray>::DownCast(decoded_array);
      long_array->Set(2u * i, index);
      long_array->Set(2u * i + 1u, reinterpret_cast64<jlong>(method));
    }
  }
  return array;
}

extern "C" JNIEXPORT void JNICALL Java_Main_restoreResolvedMethods(
    JNIEnv*, jclass, jclass cls, jobject old_cache) {
  ScopedObjectAccess soa(Thread::Current());
  ObjPtr<mirror::DexCache> dex_cache = soa.Decode<mirror::Class>(cls)->GetDexCache();
  size_t num_methods = dex_cache->NumResolvedMethods();
  mirror::MethodDexCacheType* methods = dex_cache->GetResolvedMethods();
  CHECK_EQ(num_methods != 0u, methods != nullptr);
  ObjPtr<mirror::Array> old = soa.Decode<mirror::Array>(old_cache);
  CHECK_EQ(methods != nullptr, old != nullptr);
  CHECK_EQ(num_methods, static_cast<size_t>(old->GetLength()));
  for (size_t i = 0; i != num_methods; ++i) {
    uint32_t index;
    ArtMethod* method;
    if (sizeof(void*) == 4) {
      ObjPtr<mirror::IntArray> int_array = ObjPtr<mirror::IntArray>::DownCast(old);
      index = static_cast<uint32_t>(int_array->Get(2u * i));
      method = reinterpret_cast32<ArtMethod*>(int_array->Get(2u * i + 1u));
    } else {
      ObjPtr<mirror::LongArray> long_array = ObjPtr<mirror::LongArray>::DownCast(old);
      index = dchecked_integral_cast<uint32_t>(long_array->Get(2u * i));
      method = reinterpret_cast64<ArtMethod*>(long_array->Get(2u * i + 1u));
    }
    mirror::MethodDexCachePair pair(method, index);
    mirror::DexCache::SetNativePair(methods, i, pair);
  }
}

constexpr size_t histogramIndex(uint64_t value) {
  if (value < 2) {
    return value;
  }
  size_t lead_digit = 63 - CLZ(value);
  return 2 * lead_digit + ((value >> (lead_digit - 1u)) & 1u);
}

extern "C" JNIEXPORT void JNICALL Java_Main_benchmarkSuspend(
    JNIEnv*, jobject m, jobject t) {
  Thread* self = Thread::Current();
  Thread* other;
  ScopedObjectAccess soa(Thread::Current());
  {
    MutexLock mu(self, *Locks::thread_list_lock_);
    other = Thread::FromManagedThread(soa, t);
  }
  CHECK(self != other);
  CHECK(other != nullptr);
  ArtField* counter =
      soa.Decode<mirror::Object>(m)->GetClass()->FindDeclaredInstanceField("counter", "I");
  CHECK(counter != nullptr) << soa.Decode<mirror::Object>(m)->GetClass()->PrettyDescriptor();
  DCHECK(counter->IsVolatile());
  MemberOffset counter_offset = counter->GetOffset();

  struct Checkpoint : Closure {
    Checkpoint(uint64_t* hit_time, std::atomic<bool>* cont)
        : hit_time_(hit_time),
          cont_(cont) { }
    void Run(Thread* t ATTRIBUTE_UNUSED) override {
      *hit_time_ = NanoTime();
      cont_->store(!cont_->load(std::memory_order_relaxed), std::memory_order_release);
    }
    uint64_t* const hit_time_;
    std::atomic<bool>* const cont_;
  };

  constexpr size_t histogram_size = histogramIndex(std::numeric_limits<uint64_t>::max()) + 1u;
  uint32_t hit_time_histogram[histogram_size];
  std::fill_n(hit_time_histogram, histogram_size, 0u);
  uint32_t end_time_histogram[histogram_size];
  std::fill_n(end_time_histogram, histogram_size, 0u);
  uint64_t hit_time;
  std::atomic<bool> cont(false);
  Checkpoint checkpoint(&hit_time, &cont);
  uint64_t start_time = NanoTime();
  uint64_t end_time;
  uint64_t total_hit_time = 0u;
  uint64_t total_end_time = 0u;
  uint32_t total_count = 0u;
  do {
    bool old_cont = cont.load(std::memory_order_acquire);
    uint64_t request_time = NanoTime();
    {
      MutexLock mu(self, *Locks::thread_suspend_count_lock_);
      other->RequestCheckpoint(&checkpoint);
    }
    // Wait until we hit the checkpoint.
    while (cont.load(std::memory_order_acquire) == old_cont) {}
    // Wait until we get back to compiled code and start changing the "counter".
    int32_t old_counter = soa.Decode<mirror::Object>(m)->GetField32Volatile(counter_offset);
    while (old_counter == soa.Decode<mirror::Object>(m)->GetField32Volatile(counter_offset)) { }
    // Update the statistics.
    end_time = NanoTime();
    hit_time_histogram[histogramIndex(hit_time - request_time)] += 1u;
    end_time_histogram[histogramIndex(end_time - request_time)] += 1u;
    total_hit_time += hit_time - request_time;
    total_end_time += end_time - request_time;
    total_count += 1u;
  } while (end_time - start_time < UINT64_C(10000000000));

  std::ostringstream hoss;
  for (uint32_t v : hit_time_histogram) {
    hoss << " " << v;
  }
  LOG(ERROR) << "hit_time histogram:" << hoss.str();
  std::ostringstream eoss;
  for (uint32_t v : end_time_histogram) {
    eoss << " " << v;
  }
  LOG(ERROR) << "end_time histogram:" << eoss.str();
  LOG(ERROR) << "Average hit_time: " << (total_hit_time / total_count);
  LOG(ERROR) << "Average end_time: " << (total_end_time / total_count);
}

}  // namespace

}  // namespace art
