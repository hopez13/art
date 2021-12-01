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

template<typename T>
ALWAYS_INLINE static T AtomicStructLoad(std::atomic<T>* target) {
#if defined(__x86_64__)
  if (sizeof(T) == 16) {
    const void* first;
    uint64_t second;
    __asm__ __volatile__(
        "lock cmpxchg16b (%2)"
        : "=&a"(first), "=&d"(second)
        : "r"(target), "a"(0), "d"(0), "b"(0), "c"(0)
        : "cc");
    return T{first, second};
  }
#else
  static_assert(std::atomic<T>::is_always_lock_free);
#endif
  return target->load(std::memory_order_relaxed);
}

template<typename T>
ALWAYS_INLINE static void AtomicStructStore(std::atomic<T>* target, T value) {
#if defined(__x86_64__)
  if (sizeof(T) == 16) {
    const void* first;
    uint64_t second;
    __asm__ __volatile__ (
        "movq (%2), %%rax\n\t"
        "movq 8(%2), %%rdx\n\t"
        "1:\n\t"
        "lock cmpxchg16b (%2)\n\t"
        "jnz 1b"
        : "=&a"(first), "=&d"(second)
        : "r"(target), "b"(value.dex_pc_ptr), "c"(value.value)
        : "cc");
    return;
  }
#else
  static_assert(std::atomic<T>::is_always_lock_free);
#endif
  target->store(value, std::memory_order_release);
}

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
  struct Entry {
    const void* dex_pc_ptr;
    size_t value;
  } ALIGNED(2 * sizeof(size_t));

  static constexpr size_t kThreadLocalSize = 256;   // Value of 256 has around 75% cache hit rate.
  static constexpr size_t kSharedSize = 16 * 1024;  // Value of 16k has around 90% cache hit rate.

  InterpreterCache(Thread* owning_thread) : owning_thread_(owning_thread) {
    // We can not use the Clear() method since the constructor will not
    // be called from the owning thread.
    thread_local_array_.fill(Entry{});
  }

  void ClearThreadLocal() {
    DCHECK(IsCalledFromOwningThread());
    thread_local_array_.fill(Entry{});
  }

  static void ClearShared() {
    for (std::atomic<Entry>& entry : shared_array_) {
      AtomicStructStore(&entry, Entry{});
    }
  }

  template<bool kSkipThreadLocal = false>
  ALWAYS_INLINE bool Get(const void* dex_pc_ptr, /* out */ size_t* value) {
    DCHECK(IsCalledFromOwningThread());
    Entry& small_entry = thread_local_array_[IndexOf<kThreadLocalSize>(dex_pc_ptr)];
    if (kSkipThreadLocal) {
      DCHECK_NE(small_entry.dex_pc_ptr, dex_pc_ptr);
    } else {
      if (LIKELY(small_entry.dex_pc_ptr == dex_pc_ptr)) {
        *value = small_entry.value;
        return true;
      }
    }
    Entry large_entry = AtomicStructLoad(&shared_array_[IndexOf<kSharedSize>(dex_pc_ptr)]);
    if (LIKELY(large_entry.dex_pc_ptr == dex_pc_ptr)) {
      small_entry = large_entry;  // Copy to smaller array as well.
      *value = large_entry.value;
      return true;
    }
    return false;
  }

  ALWAYS_INLINE void Set(const void* dex_pc_ptr, size_t value) {
    DCHECK(IsCalledFromOwningThread());
    thread_local_array_[IndexOf<kThreadLocalSize>(dex_pc_ptr)] = Entry{dex_pc_ptr, value};
    AtomicStructStore(&shared_array_[IndexOf<kSharedSize>(dex_pc_ptr)], Entry{dex_pc_ptr, value});
  }

  template<typename Callback>
  void ForEachTheadLocalEntry(Callback&& callback) {
    // DCHECK(IsCalledFromOwningThread()); TODO
    for (Entry& entry : thread_local_array_) {
      callback(entry.dex_pc_ptr, entry.value);
    }
  }

  template<typename Callback>
  static void ForEachSharedEntry(Callback&& callback) {
    for (std::atomic<Entry>& atomic_entry : shared_array_) {
      Entry entry = AtomicStructLoad(&atomic_entry);
      callback(entry.dex_pc_ptr, entry.value);
      AtomicStructStore(&atomic_entry, entry);
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
  std::array<Entry, kThreadLocalSize> thread_local_array_;

  // Larger cache which is allocated only for the main thread.
  // It is used as next cache level if lookup in the small array fails.
  // This boosts main thread without paying the memory cost on all threads.
  static std::array<std::atomic<Entry>, kSharedSize> shared_array_;

  Thread* owning_thread_;
};

}  // namespace art

#endif  // ART_RUNTIME_INTERPRETER_INTERPRETER_CACHE_H_
