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

#include "dex_debugger_interface.h"

#include <inttypes.h>
#include <mutex>

namespace art {

extern "C" {
  struct DexFileDebugEntry {
    DexFileDebugEntry* next_;
    DexFileDebugEntry* prev_;
    const void* dexfile_;
  };

  // Linked-list of all registered dex-files.
  DexFileDebugEntry* __art_debug_dexfiles = nullptr;

  // Incremented whenever __art_debug_dexfiles is modified.
  uint32_t __art_debug_dexfiles_timestamp = 0;
}

static std::mutex __art_debug_dexfiles_lock;

DexFileDebugEntry* RegisterDexFileForNative(const void* dexfile) {
  std::lock_guard<std::mutex> guard(__art_debug_dexfiles_lock);
  DexFileDebugEntry* entry = new DexFileDebugEntry();
  entry->dexfile_ = dexfile;
  entry->prev_ = nullptr;
  entry->next_ = __art_debug_dexfiles;
  if (entry->next_ != nullptr) {
    entry->next_->prev_ = entry;
  }
  __art_debug_dexfiles = entry;
  __art_debug_dexfiles_timestamp++;
  return entry;
}

void DeregisterDexFileForNative(DexFileDebugEntry* entry) {
  if (entry == nullptr) {
    return;
  }
  std::lock_guard<std::mutex> guard(__art_debug_dexfiles_lock);
  if (entry->prev_ != nullptr) {
    entry->prev_->next_ = entry->next_;
  } else {
    __art_debug_dexfiles = entry->next_;
  }
  if (entry->next_ != nullptr) {
    entry->next_->prev_ = entry->prev_;
  }
  __art_debug_dexfiles_timestamp++;
  delete entry;
}

}  // namespace art
