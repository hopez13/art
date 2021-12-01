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

#include "base/atomic_pair.h"
#include "base/bit_utils.h"
#include "base/macros.h"

namespace art {

class Instruction;
class Thread;

// Small fast thread-local cache for the interpreter.
//
// The key is an absolute pointer to a dex instruction.
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
  using Entry = AtomicPair<size_t>;

  static constexpr size_t kThreadLocalSize = 256;   // Value of 256 has around 75% cache hit rate.
  static constexpr size_t kSharedSize = 16 * 1024;  // Value of 16k has around 90% cache hit rate.

  InterpreterCache(Thread* owning_thread);

  void ClearThreadLocal() {
    DCHECK(IsCalledFromOwningThread());
    thread_local_array_.fill(Entry{});
  }

  static void ClearShared() {
    for (std::atomic<Entry>& entry : shared_array_) {
      AtomicPairStoreRelease(&entry, Entry{});
    }
  }

  template<bool kSkipThreadLocal = false>
  ALWAYS_INLINE bool Get(const void* dex_pc_ptr, /* out */ size_t* value) {
    DCHECK(IsCalledFromOwningThread());
    size_t dex_pc = reinterpret_cast<size_t>(dex_pc_ptr);
    Entry& local_entry = thread_local_array_[IndexOf<kThreadLocalSize>(dex_pc)];
    if (kSkipThreadLocal) {
      DCHECK_NE(local_entry.first, dex_pc) << "Expected cache miss";
    } else {
      if (LIKELY(local_entry.first == dex_pc)) {
        *value = local_entry.second;
        return true;
      }
    }
    Entry shared_entry = AtomicPairLoadRelaxed(&shared_array_[IndexOf<kSharedSize>(dex_pc)]);
    if (LIKELY(shared_entry.first == dex_pc)) {
      local_entry = shared_entry;  // Copy to local array to make future lookup faster.
      *value = shared_entry.second;
      return true;
    }
    return false;
  }

  ALWAYS_INLINE void Set(const void* dex_pc_ptr, size_t value) {
    DCHECK(IsCalledFromOwningThread());
    size_t dex_pc = reinterpret_cast<size_t>(dex_pc_ptr);
    thread_local_array_[IndexOf<kThreadLocalSize>(dex_pc)] = {dex_pc, value};
    AtomicPairStoreRelease(&shared_array_[IndexOf<kSharedSize>(dex_pc)], {dex_pc, value});
  }

  template<typename Callback>
  void ForEachTheadLocalEntry(Callback&& callback) {
    // DCHECK(IsCalledFromOwningThread()); TODO
    for (Entry& entry : thread_local_array_) {
      callback(reinterpret_cast<const Instruction*>(entry.first), entry.second);
    }
  }

  template<typename Callback>
  static void ForEachSharedEntry(Callback&& callback) {
    for (std::atomic<Entry>& atomic_entry : shared_array_) {
      Entry old_entry = AtomicPairLoadRelaxed(&atomic_entry);
      Entry new_entry = old_entry;
      callback(reinterpret_cast<const Instruction*>(new_entry.first), new_entry.second);
      if (old_entry.second != new_entry.second) {
        AtomicPairStoreRelease(&atomic_entry, new_entry);
      }
    }
  }

 private:
  bool IsCalledFromOwningThread();

  template<size_t kSize>
  static ALWAYS_INLINE size_t IndexOf(size_t dex_pc) {
    static_assert(IsPowerOfTwo(kSize), "Size must be power of two");
    size_t index = (dex_pc >> 2) & (kSize - 1);
    DCHECK_LT(index, kSize);
    return index;
  }

  // Small cache of fixed size which is always present for every thread.
  // It is stored directly (without indrection) inside the Thread object.
  // This makes it as fast as possible to access from assembly fast-path.
  std::array<Entry, kThreadLocalSize> thread_local_array_;

  // Larger cache which is shared by all threads.
  // It is used as next cache level if lookup in the local array fails.
  // It needs to be accessed using atomic operations, and is contended,
  // but the sharing allows it to be larger then the per-thread cache.
  static std::array<std::atomic<Entry>, kSharedSize> shared_array_;

  Thread* owning_thread_;
};

}  // namespace art

#endif  // ART_RUNTIME_INTERPRETER_INTERPRETER_CACHE_H_
