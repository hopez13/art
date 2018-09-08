/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef ART_RUNTIME_INTERPRETER_CACHE_H_
#define ART_RUNTIME_INTERPRETER_CACHE_H_

#include <atomic>

#include "base/bit_utils.h"
#include "base/macros.h"

namespace art {

class Instruction;

// Small fast thread-local cache for the interpreter.
// The key for the cache is the dex instruction pointer.
// The meaning of the value depends on the type of the instruction.
// Most of the of operations must be done from the owning thread.
class InterpreterCache {
  typedef std::pair<Instruction*, size_t> Entry ALIGNED(2 * sizeof(size_t));
  static constexpr size_t kSize = kPageSize / sizeof(Entry);
 public:

  InterpreterCache() : data_(shared_empty_data_) { }

  // This is the only method that is safe to call form different thread.
  // Release the memory only if the owning thread can not be using the cache at this point.
  ALWAYS_INLINE void Clear(bool release_memory = false) {
    // TODO: Call this method when dex file might be released.
    data_.store(shared_empty_data_);
    if (release_memory) {
      local_data_.release();
    }
  }

  // Look-up entry in the cache.  It is fine to read from "uninitialized" cache -
  // the data will point to the shared always-zeroed table and the lookup will fail.
  ALWAYS_INLINE bool Get(Instruction* key, /* out */ size_t* value) {
    Entry& data = Get(key);
    if (data.first == key) {
      *value = data.second;
      return true;
    }
    return false;
  }

  ALWAYS_INLINE void Set(Instruction* key, size_t value) {
    DCHECK(IsInitialized());
    Entry& data = Get(key);
    data.first = key;
    data.second = value;
  }

  ALWAYS_INLINE bool IsInitialized() {
    return data_.load(std::memory_order_relaxed) == local_data_.get();
  }

  // Must be called from the thread that owns this cache.
  void EnsureInitialized() {
    if (!IsInitialized()) {
      local_data_.reset(new Entry[kSize]);
      std::fill_n(local_data_.get(), kSize, Entry{});
      data_.store(local_data_.get());
    }
  }

 private:
  ALWAYS_INLINE Entry& Get(Instruction* key) {
    static_assert(IsPowerOfTwo(kSize), "Size must be power of two");
    size_t index = (reinterpret_cast<uintptr_t>(key) >> 2) & (kSize - 1);
    DCHECK_LT(index, kSize);
    return data_.load(std::memory_order_relaxed)[index];
  }

  std::atomic<Entry*> data_;  // Used for reading, points to one of the fields below.
  std::unique_ptr<Entry[]> local_data_;  // The actual thread-local data of this cache.
  static Entry shared_empty_data_[kSize];  // Always empty. Used for thread-safe clear.
};

}  // namespace art

#endif  // ART_RUNTIME_INTERPRETER_CACHE_H_
