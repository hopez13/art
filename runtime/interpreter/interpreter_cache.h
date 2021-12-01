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

#include <array>
#include <atomic>

#include "base/bit_utils.h"
#include "base/macros.h"

namespace art {

class Thread;

// Small fast thread-local cache for the interpreter.
//
// The key is absolute pointer to a dex instruction.
//
// The value depends on the opcode of the dex instruction.
// Presence of entry might imply some pre-conditions.
//
// All operations must be done from the owning thread,
// or at a point when the owning thread is suspended.
//
// The key-value pairs stored in the cache currently are:
//   iget/iput: The field offset. The field must be non-volatile.
//   sget/sput: The ArtField* pointer. The field must be non-volitile.
//   invoke: The ArtMethod* pointer (before vtable indirection, etc).
//
// We ensure consistency of the cache by clearing it
// whenever any dex file is unloaded.
//
// Aligned to 16-bytes to make it easier to get the address of the cache
// from assembly (it ensures that the offset is valid immediate value).
class ALIGNED(16) InterpreterCache {
 public:
  // Aligned since we load the whole entry in single assembly instruction.
  typedef std::pair<const void*, size_t> Entry ALIGNED(2 * sizeof(size_t));

  static constexpr size_t kSmallSize = 256;   // Value of 256 has around 75% cache hit rate.
  static constexpr size_t kLargeSize = 4096;

  InterpreterCache(Thread* owning_thread) : owning_thread_(owning_thread) {
    // We can not use the Clear() method since the constructor will not
    // be called from the owning thread.
    small_array_.fill(Entry{});
    large_array_ = nullptr;
  }

  void Grow() {
    DCHECK(IsCalledFromOwningThread());
    if (large_array_ == nullptr) {
      std::unique_ptr<std::array<Entry, kLargeSize>> array(new std::array<Entry, kLargeSize>());
      array->fill(Entry{});
      large_array_ = std::move(array);
    }
  }

  void Clear() {
    DCHECK(IsCalledFromOwningThread());
    small_array_.fill(Entry{});
    if (large_array_ != nullptr) {
      large_array_->fill(Entry{});
    }
  }

  ALWAYS_INLINE bool Get(const void* dex_pc_ptr, /* out */ size_t* value) {
    DCHECK(IsCalledFromOwningThread());
    Entry& small_entry = small_array_[IndexOf<kSmallSize>(dex_pc_ptr)];
    if (LIKELY(small_entry.first == dex_pc_ptr)) {
      *value = small_entry.second;
      return true;
    }
    if (large_array_ != nullptr) {
      Entry& large_entry = (*large_array_)[IndexOf<kLargeSize>(dex_pc_ptr)];
      if (LIKELY(large_entry.first == dex_pc_ptr)) {
        small_entry = large_entry;  // Copy to smaller array as well.
        *value = large_entry.second;
        return true;
      }
    }
    return false;
  }

  ALWAYS_INLINE void Set(const void* dex_pc_ptr, size_t value) {
    DCHECK(IsCalledFromOwningThread());
    small_array_[IndexOf<kSmallSize>(dex_pc_ptr)] = Entry{dex_pc_ptr, value};
    if (large_array_ != nullptr) {
      (*large_array_)[IndexOf<kLargeSize>(dex_pc_ptr)] = Entry{dex_pc_ptr, value};
    }
  }

  template<typename Callback>
  void ForEachEntry(Callback&& callback) {
    // DCHECK(IsCalledFromOwningThread()); TODO
    for (Entry& entry : small_array_) {
      callback(entry);
    }
    if (large_array_ != nullptr) {
      for (Entry& entry : *large_array_) {
        callback(entry);
      }
    }
  }

 private:
  bool IsCalledFromOwningThread();

  template<size_t kSize>
  static ALWAYS_INLINE size_t IndexOf(const void* dex_pc_ptr) {
    static_assert(IsPowerOfTwo(kSize), "Size must be power of two");
    size_t index = (reinterpret_cast<uintptr_t>(dex_pc_ptr) >> 2) & (kSize - 1);
    DCHECK_LT(index, kSize);
    return index;
  }

  // Small cache of fixed size which is always present for every thread.
  // It is stored directly (without indrection) inside the Thread object.
  // This makes is as fast as possible to access from assembly fast-path.
  std::array<Entry, kSmallSize> small_array_;

  // Larger cache which is allocated only for the main thread.
  // It is used as next cache level if lookup in the small array fails.
  // This boosts main thread without paying the memory cost on all threads.
  std::unique_ptr<std::array<Entry, kLargeSize>> large_array_;

  Thread* owning_thread_;
};

}  // namespace art

#endif  // ART_RUNTIME_INTERPRETER_INTERPRETER_CACHE_H_
