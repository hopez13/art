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

#include "debugger_interface.h"

#include <android-base/logging.h>

#include "base/array_ref.h"
#include "base/mutex.h"
#include "base/time_utils.h"
#include "thread-current-inl.h"
#include "thread.h"

#include <unordered_map>

//
// Debug interface for native tools (gdb, lldb, libunwind, simpleperf).
//
// See http://sourceware.org/gdb/onlinedocs/gdb/Declarations.html
//
// There are two ways for native tools to access the debug data safely:
//
// 1) Synchronously, by setting breakpoint in the __*_debug_register_code
//    method, which is called after every modification of the linked list.
//    GDB does this, but it is complex to set up and it stops the process.
//
// 2) Asynchronously, by monitoring the action_counter_, which is incremented
//    on every modification of the linked list and kept at ~0u during updates.
//    Therefore, if the tool process reads the counter both before and after
//    iterating over the linked list, and the counters match and are not ~0u,
//    the tool process can be sure the list was not modified during the read.
//    Obviously, it can also cache the data and use the counter to determine
//    if the cache is up to date, or to intelligently update it if needed.
//

namespace art {
extern "C" {
  typedef enum {
    JIT_NOACTION = 0,
    JIT_REGISTER_FN,
    JIT_UNREGISTER_FN
  } JITAction;

  struct JITCodeEntry {
    JITCodeEntry* next_;
    JITCodeEntry* prev_;
    const uint8_t* symfile_addr_;
    uint64_t symfile_size_;  // Beware of the offset (12 on x86; but 16 on ARM32).

    // Android-specific fields:
    uint64_t register_timestamp_;  // CLOCK_MONOTONIC time of entry registration.
  };

  struct JITDescriptor {
    uint32_t version_ = 1;                    // NB: GDB supports only version 1.
    uint32_t action_flag_ = JIT_NOACTION;     // One of the JITAction enum values.
    JITCodeEntry* relevant_entry_ = nullptr;  // The entry affected by the action.
    JITCodeEntry* first_entry_ = nullptr;     // Head of link list of all entries.

    // Android-specific fields:
    uint8_t magic_[8] = {'A', 'n', 'd', 'r', 'o', 'i', 'd', '1'};
    uint16_t sizeof_descriptor = sizeof(JITDescriptor);
    uint16_t sizeof_entry = sizeof(JITCodeEntry);
    std::atomic_uint32_t action_counter_;  // Number of actions, or ~0u if locked.
    uint64_t action_timestamp_ = 1;        // CLOCK_MONOTONIC time of last action.
  };

  // Check that std::atomic_uint32_t has the same layout as uint32_t.
  static_assert(alignof(std::atomic_uint32_t) == alignof(uint32_t), "Weird alignment");
  static_assert(sizeof(std::atomic_uint32_t) == sizeof(uint32_t), "Weird size");

  // GDB may set breakpoint here. We must ensure it is not removed or deduplicated.
  void __attribute__((noinline)) __jit_debug_register_code() {
    __asm__("");
  }

  // Alternatively, native tools may overwrite this field to execute custom handler.
  void (*__jit_debug_register_code_ptr)() = __jit_debug_register_code;

  // The root data structure describing of all JITed methods.
  JITDescriptor __jit_debug_descriptor {};

  // The following globals mirror the ones above, but are used to register dex files.
  void __attribute__((noinline)) __dex_debug_register_code() {
    __asm__("");
  }
  void (*__dex_debug_register_code_ptr)() = __dex_debug_register_code;
  JITDescriptor __dex_debug_descriptor {};
}

static JITCodeEntry* CreateJITCodeEntryInternal(
    JITDescriptor& descriptor,
    void (*register_code_ptr)(),
    const ArrayRef<const uint8_t>& symfile)
    REQUIRES(Locks::native_debug_interface_lock_) {
  // Mark the descriptor as locked, so native tools know the data is unstable.
  uint32_t action_counter_ = descriptor.action_counter_.exchange(~0u);

  JITCodeEntry* entry = new JITCodeEntry;
  CHECK(entry != nullptr);
  entry->symfile_addr_ = symfile.data();
  entry->symfile_size_ = symfile.size();
  entry->prev_ = nullptr;
  entry->next_ = descriptor.first_entry_;
  entry->register_timestamp_ = NanoTime();
  if (entry->next_ != nullptr) {
    entry->next_->prev_ = entry;
  }
  descriptor.first_entry_ = entry;
  descriptor.relevant_entry_ = entry;
  descriptor.action_flag_ = JIT_REGISTER_FN;
  descriptor.action_timestamp_ = entry->register_timestamp_;
  descriptor.action_counter_.store(action_counter_ + 1);  // Must be last.

  (*register_code_ptr)();
  return entry;
}

static void DeleteJITCodeEntryInternal(
    JITDescriptor& descriptor,
    void (*register_code_ptr)(),
    JITCodeEntry* entry)
    REQUIRES(Locks::native_debug_interface_lock_) {
  // Mark the descriptor as locked, so native tools know the data is unstable.
  uint32_t action_counter_ = descriptor.action_counter_.exchange(~0u);

  if (entry->prev_ != nullptr) {
    entry->prev_->next_ = entry->next_;
  } else {
    descriptor.first_entry_ = entry->next_;
  }
  if (entry->next_ != nullptr) {
    entry->next_->prev_ = entry->prev_;
  }

  descriptor.relevant_entry_ = entry;
  descriptor.action_flag_ = JIT_UNREGISTER_FN;
  descriptor.action_timestamp_ = NanoTime();
  descriptor.action_counter_.store(action_counter_ + 1);  // Must be last.

  (*register_code_ptr)();
  delete entry;
}

static std::unordered_map<const void*, JITCodeEntry*> __dex_debug_entries
    GUARDED_BY(Locks::native_debug_interface_lock_);

void AddNativeDebugInfoForDex(Thread* current_thread, ArrayRef<const uint8_t> dexfile) {
  MutexLock mu(current_thread, *Locks::native_debug_interface_lock_);
  if (__dex_debug_entries.count(dexfile.data()) == 0) {
    JITCodeEntry* entry = CreateJITCodeEntryInternal(__dex_debug_descriptor,
                                                     __dex_debug_register_code_ptr,
                                                     dexfile);
    __dex_debug_entries.emplace(dexfile.data(), entry);
  }
}

void RemoveNativeDebugInfoForDex(Thread* current_thread, ArrayRef<const uint8_t> dexfile) {
  MutexLock mu(current_thread, *Locks::native_debug_interface_lock_);
  auto it = __dex_debug_entries.find(dexfile.data());
  // We register dex files in the class linker and free them in DexFile_closeDexFile,
  // but might be cases where we load the dex file without using it in the class linker.
  if (it != __dex_debug_entries.end()) {
    DeleteJITCodeEntryInternal(__dex_debug_descriptor,
                               __dex_debug_register_code_ptr,
                               it->second);
    __dex_debug_entries.erase(it);
  }
}

static size_t __jit_debug_mem_usage
    GUARDED_BY(Locks::native_debug_interface_lock_) = 0;

// Mapping from code address to entry. Used to manage life-time of the entries.
static std::unordered_map<uintptr_t, JITCodeEntry*> __jit_debug_entries
    GUARDED_BY(Locks::native_debug_interface_lock_);

void AddNativeDebugInfoForJit(uintptr_t for_address, const std::vector<uint8_t>&& symfile) {
  DCHECK_NE(symfile.size(), 0u);
  CHECK_EQ(__jit_debug_entries.count(for_address), 0u);

  // Make a copy of the buffer to shrink it and to pass ownership to JITCodeEntry.
  uint8_t* copy = new uint8_t[symfile.size()];
  CHECK(copy != nullptr);
  memcpy(copy, symfile.data(), symfile.size());

  JITCodeEntry* entry = CreateJITCodeEntryInternal(__jit_debug_descriptor,
                                                   __jit_debug_register_code_ptr,
                                                   ArrayRef<const uint8_t>(copy, symfile.size()));
  __jit_debug_entries.emplace(for_address, entry);
  __jit_debug_mem_usage += sizeof(JITCodeEntry) + entry->symfile_size_;
}

void RemoveNativeDebugInfoForJit(uintptr_t for_address) {
  JITCodeEntry* entry = __jit_debug_entries[for_address];
  CHECK(entry != nullptr);
  const uint8_t* symfile_addr = entry->symfile_addr_;
  uint64_t symfile_size = entry->symfile_size_;
  DeleteJITCodeEntryInternal(__jit_debug_descriptor,
                             __jit_debug_register_code_ptr,
                             entry);
  __jit_debug_entries.erase(for_address);
  __jit_debug_mem_usage -= sizeof(JITCodeEntry) + symfile_size;
  delete[] symfile_addr;
}

size_t GetJitNativeDebugInfoMemUsage() {
  return __jit_debug_mem_usage + __jit_debug_entries.size() * 2 * sizeof(void*);
}

}  // namespace art
