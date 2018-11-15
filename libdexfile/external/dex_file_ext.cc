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

#include <inttypes.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <android-base/logging.h>
#include <android-base/macros.h>
#include <android-base/mapped_file.h>
#include <android-base/stringprintf.h>

#include <dex/class_accessor-inl.h>
#include <dex/code_item_accessors-inl.h>
#include <dex/dex_file-inl.h>
#include <dex/dex_file_loader.h>

#include "art_api/dex_file.h"

namespace art {
namespace {

struct MethodCacheEntry {
  int32_t offset;  // Offset relative to the start of the dex file header.
  int32_t len;
  int32_t index;     // Method index.
  std::string name;  // Method name. Not filled in for all cache entries.
};

// Wraps DexFile to add the caching needed by the external interface. This is
// what gets passed over as ExtDexFile*.
class DexFileWrapper {
  // Method cache for GetMethodInfoForOffset. This is populated as we iterate
  // sequentially through the class defs. MethodCacheEntry.name is only set for
  // methods returned by GetMethodInfoForOffset.
  std::map<int32_t, MethodCacheEntry> method_cache_;

  // Index of first class def for which method_cache_ isn't complete.
  uint32_t class_def_index_ = 0;

 public:
  std::unique_ptr<const DexFile> dex_file_;

  explicit DexFileWrapper(std::unique_ptr<const DexFile>&& dex_file)
      : dex_file_(std::move(dex_file)) {}

  MethodCacheEntry* GetMethodCacheEntryForOffset(int64_t dex_offset) {
    // First look in the method cache.
    auto it = method_cache_.upper_bound(dex_offset);
    if (it != method_cache_.end() && dex_offset >= it->second.offset) {
      return &it->second;
    }

    for (; class_def_index_ < dex_file_->NumClassDefs(); class_def_index_++) {
      ClassAccessor accessor(*dex_file_, class_def_index_);

      for (const ClassAccessor::Method& method : accessor.GetMethods()) {
        CodeItemInstructionAccessor code = method.GetInstructions();
        if (!code.HasCodeItem()) {
          continue;
        }

        int32_t offset = reinterpret_cast<const uint8_t*>(code.Insns()) - dex_file_->Begin();
        int32_t len = code.InsnsSizeInBytes();
        int32_t index = method.GetIndex();
        auto res = method_cache_.emplace(
            std::make_pair(offset + len, MethodCacheEntry{offset, len, index, ""}));
        if (!res.second) {
          // If dex_offset is within this method we'd have found it in the cache.
          continue;
        }
        if (offset <= dex_offset && dex_offset < offset + len) {
          return &res.first->second;
        }
      }
    }

    return nullptr;
  }

  const std::string& GetMethodName(MethodCacheEntry& entry) {
    if (entry.name.empty()) {
      entry.name = dex_file_->PrettyMethod(entry.index, false);
    }
    return entry.name;
  }
};

class MappedFileContainer : public DexFileContainer {
 public:
  explicit MappedFileContainer(std::unique_ptr<android::base::MappedFile>&& map)
      : map_(std::move(map)) {}
  ~MappedFileContainer() {}
  int GetPermissions() { return 0; }
  bool IsReadOnly() { return true; }
  bool EnableWrite() { return false; }
  bool DisableWrite() { return false; }

 private:
  std::unique_ptr<android::base::MappedFile> map_;
  DISALLOW_COPY_AND_ASSIGN(MappedFileContainer);
};

const ExtDexFileString* MakeExtDexFileString(const std::string& str) {
  return static_cast<const ExtDexFileString*>(new const std::string(str));
}

const ExtDexFileString* MakeExtDexFileString(const std::string&& str) {
  return static_cast<const ExtDexFileString*>(new const std::string(std::move(str)));
}

}  // namespace
}  // namespace art

extern "C" {

const char* ExtDexFileGetString(const ExtDexFileString* ext_string, size_t* size) {
  auto* str = static_cast<const std::string*>(ext_string);
  *size = str->size();
  return str->data();
}

void ExtDexFileFreeString(const ExtDexFileString* ext_string) {
  auto* str = static_cast<const std::string*>(ext_string);
  delete (str);
}

bool ExtDexFileOpenFromMemory(ExtDexFile** ext_dex_file, const void* addr, size_t* size,
                              const char* location, const ExtDexFileString** ext_error_msg) {
  if (*size < sizeof(art::DexFile::Header)) {
    *size = sizeof(art::DexFile::Header);
    *ext_error_msg = nullptr;
    return false;
  }

  const art::DexFile::Header* header = reinterpret_cast<const art::DexFile::Header*>(addr);
  uint32_t file_size = header->file_size_;
  if (art::CompactDexFile::IsMagicValid(header->magic_)) {
    // Compact dex file store data section separately so that it can be shared.
    // Therefore we need to extend the read memory range to include it.
    // TODO: This might be wasteful as we might read data in between as well.
    //       In practice, this should be fine, as such sharing only happens on disk.
    uint32_t computed_file_size;
    if (__builtin_add_overflow(header->data_off_, header->data_size_, &computed_file_size)) {
      *ext_error_msg = art::MakeExtDexFileString(
          android::base::StringPrintf("Corrupt CompactDexFile header in '%s'", location));
      return false;
    }
    if (computed_file_size > file_size) {
      file_size = computed_file_size;
    }
  } else if (!art::StandardDexFile::IsMagicValid(header->magic_)) {
    *ext_error_msg = art::MakeExtDexFileString(
        android::base::StringPrintf("Unrecognized dex file header in '%s'", location));
    return false;
  }

  if (*size < file_size) {
    *size = file_size;
    *ext_error_msg = nullptr;
    return false;
  }

  std::string loc_str = std::string(location);
  art::DexFileLoader loader;
  std::string error_msg;
  auto dex_file = loader.Open(static_cast<const uint8_t*>(addr), *size, loc_str, header->checksum_,
                              /* oat_dex_file= */ nullptr, /* verify= */ false,
                              /* verify_checksum= */ false, &error_msg);
  if (!dex_file) {
    *ext_error_msg = art::MakeExtDexFileString(std::move(error_msg));
    return false;
  }

  *ext_dex_file = static_cast<ExtDexFile*>(new art::DexFileWrapper(std::move(dex_file)));
  return true;
}

bool ExtDexFileOpenFromFd(ExtDexFile** ext_dex_file, int fd, off_t offset, const char* location,
                          const ExtDexFileString** ext_error_msg) {
  size_t length;
  {
    struct stat sbuf;
    std::memset(&sbuf, 0, sizeof(sbuf));
    if (fstat(fd, &sbuf) == -1) {
      *ext_error_msg = art::MakeExtDexFileString(
          android::base::StringPrintf("fstat '%s' failed: %s", location, std::strerror(errno)));
      return false;
    }
    if (S_ISDIR(sbuf.st_mode)) {
      *ext_error_msg = art::MakeExtDexFileString(
          android::base::StringPrintf("Attempt to mmap directory '%s'", location));
      return false;
    }
    length = sbuf.st_size;
  }

  if (length < offset + sizeof(art::DexFile::Header)) {
    *ext_error_msg = art::MakeExtDexFileString(android::base::StringPrintf(
        "Offset %" PRId64 " too large for '%s' of size %zu", int64_t{offset}, location, length));
    return false;
  }

  // Cannot use MemMap in libartbase here, because it pulls in dlopen which we
  // can't have when being compiled statically.
  std::unique_ptr<android::base::MappedFile> map =
      android::base::MappedFile::FromFd(fd, offset, length, PROT_READ);
  if (map == nullptr) {
    *ext_error_msg = art::MakeExtDexFileString(
        android::base::StringPrintf("mmap '%s' failed: %s", location, std::strerror(errno)));
    return false;
  }

  const art::DexFile::Header* header = reinterpret_cast<const art::DexFile::Header*>(map->data());
  uint32_t file_size;
  if (__builtin_add_overflow(offset, header->file_size_, &file_size)) {
    *ext_error_msg =
        art::MakeExtDexFileString(android::base::StringPrintf("Corrupt header in '%s'", location));
    return false;
  }
  if (length < file_size) {
    *ext_error_msg = art::MakeExtDexFileString(
        android::base::StringPrintf("Dex file '%s' too short: expected %" PRIu32 ", got %" PRIu64,
                                    location, file_size, uint64_t{length}));
    return false;
  }

  void* addr = map->data();
  size_t size = map->size();
  auto container =
      std::unique_ptr<art::MappedFileContainer>(new art::MappedFileContainer(std::move(map)));

  std::string loc_str = std::string(location);
  std::string error_msg;
  art::DexFileLoader loader;
  auto dex_file = loader.Open(reinterpret_cast<const uint8_t*>(addr), size, loc_str,
                              header->checksum_, /* oat_dex_file= */ nullptr, /* verify= */ false,
                              /* verify_checksum= */ false, &error_msg, std::move(container));
  if (!dex_file) {
    *ext_error_msg = art::MakeExtDexFileString(std::move(error_msg));
    return false;
  }
  *ext_dex_file = static_cast<ExtDexFile*>(new art::DexFileWrapper(std::move(dex_file)));
  return true;
}

bool ExtDexFileGetMethodInfoForOffset(ExtDexFile* ext_dex_file, int64_t dex_offset,
                                      ExtDexFileMethodInfo* method_info) {
  auto& dex_file = *static_cast<art::DexFileWrapper*>(ext_dex_file);

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

  art::MethodCacheEntry* entry = dex_file.GetMethodCacheEntryForOffset(dex_offset);
  if (entry) {
    method_info->offset = entry->offset;
    method_info->len = entry->len;
    method_info->name = art::MakeExtDexFileString(dex_file.GetMethodName(*entry));
    return true;
  }

  return false;
}

void ExtDexFileGetAllMethodInfos(ExtDexFile* ext_dex_file, ExtDexFileMethodInfoCb* method_info_cb,
                                 void* ctx) {
  auto& dex_file = *static_cast<art::DexFileWrapper*>(ext_dex_file);

  for (art::ClassAccessor accessor : dex_file.dex_file_->GetClasses()) {
    for (const art::ClassAccessor::Method& method : accessor.GetMethods()) {
      art::CodeItemInstructionAccessor code = method.GetInstructions();
      if (!code.HasCodeItem()) {
        continue;
      }

      ExtDexFileMethodInfo method_info;
      method_info.offset =
          reinterpret_cast<const uint8_t*>(code.Insns()) - dex_file.dex_file_->Begin();
      method_info.len = code.InsnsSizeInBytes();
      method_info.name =
          art::MakeExtDexFileString(dex_file.dex_file_->PrettyMethod(method.GetIndex()));
      method_info_cb(&method_info, ctx);
      ExtDexFileFreeString(method_info.name);
    }
  }
}

void ExtDexFileFree(void* ext_dex_file) {
  delete (static_cast<art::DexFileWrapper*>(ext_dex_file));
}

}  // extern "C"
