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

namespace unwindstack {
namespace hooks {

// Note: Unit tests for these functions are in /system/core/libunwindstack.

int64_t DexFileFromMemory(const DexFileImpl** dex_file_impl, const uint8_t* data, size_t size) {
  if (size < sizeof(art::DexFile::Header)) {
    return sizeof(art::DexFile::Header);
  }

  const art::DexFile::Header* header = reinterpret_cast<const art::DexFile::Header*>(data);
  uint32_t file_size = header->file_size_;
  if (art::CompactDexFile::IsMagicValid(header->magic_)) {
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
  } else if (!art::StandardDexFile::IsMagicValid(header->magic_)) {
    return -1;
  }

  if (size < file_size) {
    return file_size;
  }

  header = reinterpret_cast<const art::DexFile::Header*>(data);
  CHECK_EQ(size, header->file_size_);

  art::DexFileLoader loader;
  std::string error_msg;
  auto dex = loader.Open(data,
                         file_size,
                         /* location= */ "",
                         /* location_checksum= */ 0,
                         /* oat_dex_file= */ nullptr,
                         /* verify= */ false,
                         /* verify_checksum= */ false,
                         &error_msg);
  if (!dex) {
    return -1;
  }
  *dex_file_impl = static_cast<const DexFileImpl*>(dex.release());
  return 0;
}

bool DexFileFromFile(const DexFileImpl** dex_file_impl,
                     uint64_t dex_file_offset_in_file, const std::string& name) {
  android::base::unique_fd fd(TEMP_FAILURE_RETRY(open(name.c_str(), O_RDONLY | O_CLOEXEC)));
  if (fd == -1) {
    return false;
  }

  art::MemMap::Init();
  art::ArtDexFileLoader loader;
  std::string error_msg;
  auto dex = loader.OpenDex(fd,
                            /* offset= */ dex_file_offset_in_file,
                            /* location= */ name,
                            /* verify= */ false,
                            /* verify_checksum= */ false,
                            /* mmap_shared= */ false,
                            &error_msg);
  if (!dex) {
    return false;
  }
  *dex_file_impl = static_cast<const DexFileImpl*>(dex.release());
  return true;
}

bool GetMethodInformation(const DexFileImpl* dex_file_impl, uint64_t dex_offset,
                          std::string* method_name, uint64_t* method_offset_start,
                          uint64_t* method_offset_end) {
  auto dex_file = static_cast<const art::DexFile*>(dex_file_impl);
  if (!dex_file->IsInDataSection(dex_file->Begin() + dex_offset)) {
    return false;  // The DEX offset is not within the bytecode of this dex file.
  }

  if (dex_file->IsCompactDexFile()) {
    // The data section of compact dex files might be shared.
    // Check the subrange unique to this compact dex.
    const auto& cdex_header = dex_file->AsCompactDexFile()->GetHeader();
    uint32_t begin = cdex_header.data_off_ + cdex_header.OwnedDataBegin();
    uint32_t end = cdex_header.data_off_ + cdex_header.OwnedDataEnd();
    if (dex_offset < begin || dex_offset >= end) {
      return false;  // The DEX offset is not within the bytecode of this dex file.
    }
  }

  for (uint32_t class_def_index = 0; class_def_index < dex_file->NumClassDefs(); class_def_index++) {
    art::ClassAccessor accessor(*dex_file, class_def_index);

    for (const art::ClassAccessor::Method& method : accessor.GetMethods()) {
      art::CodeItemInstructionAccessor code = method.GetInstructions();
      if (!code.HasCodeItem()) {
        continue;
      }
      uint32_t offset_start = reinterpret_cast<const uint8_t*>(code.Insns()) - dex_file->Begin();
      uint32_t offset_end = offset_start + code.InsnsSizeInBytes();
      if (offset_start <= dex_offset && dex_offset < offset_end) {
        if (method_name != nullptr) {
          *method_name = dex_file->PrettyMethod(method.GetIndex(), false);
        }
        if (method_offset_start != nullptr) {
          *method_offset_start = offset_start;
        }
        if (method_offset_end != nullptr) {
          *method_offset_end = offset_end;
        }
        return true;
      }
    }
  }
  return false;
}

void FreeDexFile(const DexFileImpl* dex_file_impl) {
  delete(static_cast<const art::DexFile*>(dex_file_impl));
}

}  // namespace hooks
}  // namespace unwindstack
