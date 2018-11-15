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

#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <vector>

#include <android-base/unique_fd.h>
#include <base/mem_map.h>
#include <unwindstack/DexFileHooks.h>

#include "art_dex_file_loader.h"
#include "class_accessor-inl.h"
#include "code_item_accessors-inl.h"
#include "dex_file-inl.h"
#include "dex_file_loader.h"
#include "unwindstack_hooks.h"

namespace art {
namespace {

// Note: Unit tests for these functions are in /system/core/libunwindstack.

struct MethodCacheEntry {
  uint32_t offset_start;  // Offset from beginning of dex file to method instruction start.
  uint32_t index;         // Method index.
  std::string name;       // Method name. Not filled in for all cache entries.
};

// Wraps DexFile to add the caching needed by this interface.
class DexFileWrapper {
 public:
  explicit DexFileWrapper(std::unique_ptr<const DexFile>&& dex_file)
      : dex_file_(std::move(dex_file)) {}

  std::unique_ptr<const DexFile> dex_file_;

  // Method cache for GetMethodInformation. This is populated as we iterate
  // sequentially through the class defs. MethodCacheEntry.name is only set for
  // methods returned by GetMethodInformation.
  std::map<uint32_t, MethodCacheEntry> method_cache_;

  // Index of first class def for which method_cache_ isn't complete.
  uint32_t class_def_index_ = 0;
};

int64_t DexFileFromMemory(DexFileImpl** dex_file_impl, const uint8_t* data, size_t size) {
  if (size < sizeof(DexFile::Header)) {
    return sizeof(DexFile::Header);
  }

  const DexFile::Header* header = reinterpret_cast<const DexFile::Header*>(data);
  uint32_t file_size = header->file_size_;
  if (CompactDexFile::IsMagicValid(header->magic_)) {
    // Compact dex file store data section separately so that it can be shared.
    // Therefore we need to extend the read memory range to include it.
    // TODO: This might be wasteful as we might read data in between as well.
    //       In practice, this should be fine, as such sharing only happens on disk.
    uint32_t computed_file_size;
    if (__builtin_add_overflow(header->data_off_, header->data_size_, &computed_file_size)) {
      return -1;
    }
    if (computed_file_size > file_size) {
      file_size = computed_file_size;
    }
  } else if (!StandardDexFile::IsMagicValid(header->magic_)) {
    return -1;
  }

  if (size < file_size) {
    return file_size;
  }

  DexFileLoader loader;
  std::string error_msg;
  auto dex = loader.Open(data, file_size,
                         /* location= */ "",
                         /* location_checksum= */ 0,
                         /* oat_dex_file= */ nullptr,
                         /* verify= */ false,
                         /* verify_checksum= */ false, &error_msg);
  if (!dex) {
    return -1;
  }
  *dex_file_impl = reinterpret_cast<DexFileImpl*>(new DexFileWrapper(std::move(dex)));
  return 0;
}

bool DexFileFromFile(DexFileImpl** dex_file_impl, uint64_t dex_file_offset_in_file,
                     const char* name) {
  android::base::unique_fd fd(TEMP_FAILURE_RETRY(open(name, O_RDONLY | O_CLOEXEC)));
  if (fd == -1) {
    return false;
  }

  MemMap::Init();
  ArtDexFileLoader loader;
  std::string error_msg;
  auto dex = loader.OpenDex(fd,
                            /* offset= */ dex_file_offset_in_file,
                            /* location= */ std::string(name),
                            /* verify= */ false,
                            /* verify_checksum= */ false,
                            /* mmap_shared= */ false, &error_msg);
  if (!dex) {
    return false;
  }
  *dex_file_impl = reinterpret_cast<DexFileImpl*>(new DexFileWrapper(std::move(dex)));
  return true;
}

bool GetMethodInformation(DexFileImpl* dex_file_impl, uint64_t dex_offset, const char** method_name,
                          uint64_t* method_offset) {
  auto& dex_file = *reinterpret_cast<DexFileWrapper*>(dex_file_impl);

  if (!dex_file.dex_file_->IsInDataSection(dex_file.dex_file_->Begin() + dex_offset)) {
    return false;  // The DEX offset is not within the bytecode of this dex file.
  }

  if (dex_file.dex_file_->IsCompactDexFile()) {
    // The data section of compact dex files might be shared.
    // Check the subrange unique to this compact dex.
    const auto& cdex_header = dex_file.dex_file_->AsCompactDexFile()->GetHeader();
    uint32_t begin = cdex_header.data_off_ + cdex_header.OwnedDataBegin();
    uint32_t end = cdex_header.data_off_ + cdex_header.OwnedDataEnd();
    if (dex_offset < begin || dex_offset >= end) {
      return false;  // The DEX offset is not within the bytecode of this dex file.
    }
  }

  // First look in the method cache.
  auto it = dex_file.method_cache_.upper_bound(dex_offset);
  if (it != dex_file.method_cache_.end()) {
    MethodCacheEntry& entry = it->second;
    if (dex_offset >= entry.offset_start) {
      if (entry.name.empty()) {
        entry.name = dex_file.dex_file_->PrettyMethod(entry.index, false);
      }
      *method_name = entry.name.c_str();
      *method_offset = dex_offset - entry.offset_start;
      return true;
    }
  }

  for (; dex_file.class_def_index_ < dex_file.dex_file_->NumClassDefs();
       dex_file.class_def_index_++) {
    ClassAccessor accessor(*dex_file.dex_file_, dex_file.class_def_index_);

    for (const ClassAccessor::Method& method : accessor.GetMethods()) {
      CodeItemInstructionAccessor code = method.GetInstructions();
      if (!code.HasCodeItem()) {
        continue;
      }

      uint32_t offset_start =
          reinterpret_cast<const uint8_t*>(code.Insns()) - dex_file.dex_file_->Begin();
      uint32_t offset_end = offset_start + code.InsnsSizeInBytes();
      uint32_t member_index = method.GetIndex();
      auto res = dex_file.method_cache_.emplace(
          std::make_pair(offset_end, MethodCacheEntry{offset_start, member_index, ""}));
      if (!res.second) {
        // If dex_offset is within this method we'd have found it in the cache.
        continue;
      }
      if (offset_start <= dex_offset && dex_offset < offset_end) {
        res.first->second.name = dex_file.dex_file_->PrettyMethod(member_index, false);
        *method_name = res.first->second.name.c_str();
        *method_offset = dex_offset - offset_start;
        return true;
      }
    }
  }
  return false;
}

void FreeDexFile(DexFileImpl* dex_file_impl) {
  delete (reinterpret_cast<DexFileWrapper*>(dex_file_impl));
}

constexpr DexFileHooks g_dex_file_hooks = {
    DexFileFromMemory,
    DexFileFromFile,
    GetMethodInformation,
    FreeDexFile,
};

}  // namespace
}  // namespace art

extern "C" {

const DexFileHooks* GetDexFileHooks() { return &art::g_dex_file_hooks; }

}  // extern "C"
