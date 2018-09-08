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

#ifndef ART_RUNTIME_INTERPRETER_INTERPRETER_CACHE_H_
#define ART_RUNTIME_INTERPRETER_INTERPRETER_CACHE_H_

#include <atomic>

#include "base/bit_utils.h"
#include "base/macros.h"

namespace art {

class Instruction;

// Small fast thread-local cache for the interpreter.
// The key for the cache is the dex instruction pointer.
// The interpretation of the value depends on the opcode.
// Presence of entry might imply some preformance pre-conditions.
// Most of the of operations must be done from the owning thread.
class InterpreterCache {
  typedef std::pair<const Instruction*, size_t> Entry ALIGNED(2 * sizeof(size_t));
  static constexpr size_t kSize = kPageSize / sizeof(Entry);

 public:
  InterpreterCache() : data_(shared_empty_data_) { }

  // Clear cache. This is the only method that is safe to call form different thread.
  // Release the memory only if all threads can not be using the cache at this point.
  ALWAYS_INLINE void Clear(bool release_memory = false) {
    data_.store(shared_empty_data_);
    if (release_memory) {
      local_data_.reset();
    }
  }

  // Look-up entry in the cache. It is fine to read from cache that is not writable,
  // the data will point to the shared always-zeroed table and the lookup will fail.
  ALWAYS_INLINE bool Get(const Instruction* key, /* out */ size_t* value) {
    Entry* data = data_.load(std::memory_order_relaxed);
    Entry& entry = data[IndexOf(key)];
    if (LIKELY(entry.first == key)) {
      *value = entry.second;
      return true;
    }
    return false;
  }

  // Add entry to the cache. Must be called from the thread that owns this cache.
  // It may silently fail if the cache was just cleared from other thread.
  ALWAYS_INLINE void Set(const Instruction* key, size_t value) {
    Entry* data = data_.load(std::memory_order_relaxed);
    Entry& entry = data[IndexOf(key)];
    if (LIKELY(data == local_data_.get())) {
      entry.first = key;
      entry.second = value;
    }
  }

  // The data might point to the valid, but read-only, shared section.
  ALWAYS_INLINE bool IsWritable() {
    return data_.load(std::memory_order_relaxed) == local_data_.get();
  }

  // Must be called from the thread that owns this cache.
  // This is separate method so that setting can be as simple as posible.
  void EnsureWritable() {
    if (!IsWritable()) {
      local_data_.reset(new Entry[kSize]);
      std::fill_n(local_data_.get(), kSize, Entry{});
      data_.store(local_data_.get());
    }
  }

 private:
  static ALWAYS_INLINE size_t IndexOf(const Instruction* key) {
    static_assert(IsPowerOfTwo(kSize), "Size must be power of two");
    size_t index = (reinterpret_cast<uintptr_t>(key) >> 2) & (kSize - 1);
    DCHECK_LT(index, kSize);
    return index;
  }

  std::atomic<Entry*> data_;  // Used for reading, points to one of the fields below.
  std::unique_ptr<Entry[]> local_data_;  // The actual thread-local data of this cache.
  static Entry shared_empty_data_[kSize];  // Always empty. Used for thread-safe clear.
};

}  // namespace art

#endif  // ART_RUNTIME_INTERPRETER_INTERPRETER_CACHE_H_
