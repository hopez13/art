/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "vdex_file.h"

#include <sys/mman.h>  // For the PROT_* and MAP_* constants.

#include <memory>

#include <android-base/logging.h>

#include "base/bit_utils.h"
#include "base/stl_util.h"
#include "base/unix_file/fd_file.h"
#include "dex_file.h"
#include "dex_file_loader.h"
#include "dex_to_dex_decompiler.h"
#include "quicken_info.h"

namespace art {

constexpr uint8_t VdexFile::Header::kVdexInvalidMagic[4];
constexpr uint8_t VdexFile::Header::kVdexMagic[4];
constexpr uint8_t VdexFile::Header::kVdexVersion[4];

bool VdexFile::Header::IsMagicValid() const {
  return (memcmp(magic_, kVdexMagic, sizeof(kVdexMagic)) == 0);
}

bool VdexFile::Header::IsVersionValid() const {
  return (memcmp(version_, kVdexVersion, sizeof(kVdexVersion)) == 0);
}

VdexFile::Header::Header(uint32_t number_of_dex_files,
                         uint32_t dex_size,
                         uint32_t verifier_deps_size,
                         uint32_t quickening_info_size)
    : number_of_dex_files_(number_of_dex_files),
      dex_size_(dex_size),
      verifier_deps_size_(verifier_deps_size),
      quickening_info_size_(quickening_info_size) {
  memcpy(magic_, kVdexMagic, sizeof(kVdexMagic));
  memcpy(version_, kVdexVersion, sizeof(kVdexVersion));
  DCHECK(IsMagicValid());
  DCHECK(IsVersionValid());
}

std::unique_ptr<VdexFile> VdexFile::Open(const std::string& vdex_filename,
                                         bool writable,
                                         bool low_4gb,
                                         bool unquicken,
                                         std::string* error_msg) {
  if (!OS::FileExists(vdex_filename.c_str())) {
    *error_msg = "File " + vdex_filename + " does not exist.";
    return nullptr;
  }

  std::unique_ptr<File> vdex_file;
  if (writable) {
    vdex_file.reset(OS::OpenFileReadWrite(vdex_filename.c_str()));
  } else {
    vdex_file.reset(OS::OpenFileForReading(vdex_filename.c_str()));
  }
  if (vdex_file == nullptr) {
    *error_msg = "Could not open file " + vdex_filename +
                 (writable ? " for read/write" : "for reading");
    return nullptr;
  }

  int64_t vdex_length = vdex_file->GetLength();
  if (vdex_length == -1) {
    *error_msg = "Could not read the length of file " + vdex_filename;
    return nullptr;
  }

  return Open(vdex_file->Fd(), vdex_length, vdex_filename, writable, low_4gb, unquicken, error_msg);
}

std::unique_ptr<VdexFile> VdexFile::Open(int file_fd,
                                         size_t vdex_length,
                                         const std::string& vdex_filename,
                                         bool writable,
                                         bool low_4gb,
                                         bool unquicken,
                                         std::string* error_msg) {
  std::unique_ptr<MemMap> mmap(MemMap::MapFile(
      vdex_length,
      (writable || unquicken) ? PROT_READ | PROT_WRITE : PROT_READ,
      unquicken ? MAP_PRIVATE : MAP_SHARED,
      file_fd,
      0 /* start offset */,
      low_4gb,
      vdex_filename.c_str(),
      error_msg));
  if (mmap == nullptr) {
    *error_msg = "Failed to mmap file " + vdex_filename + " : " + *error_msg;
    return nullptr;
  }

  std::unique_ptr<VdexFile> vdex(new VdexFile(mmap.release()));
  if (!vdex->IsValid()) {
    *error_msg = "Vdex file is not valid";
    return nullptr;
  }

  if (unquicken) {
    std::vector<std::unique_ptr<const DexFile>> unique_ptr_dex_files;
    if (!vdex->OpenAllDexFiles(&unique_ptr_dex_files, error_msg)) {
      return nullptr;
    }
    Unquicken(MakeNonOwningPointerVector(unique_ptr_dex_files),
              vdex->GetQuickeningInfo(),
              /* decompile_return_instruction */ false);
    // Update the quickening info size to pretend there isn't any.
    reinterpret_cast<Header*>(vdex->mmap_->Begin())->quickening_info_size_ = 0;
  }

  *error_msg = "Success";
  return vdex;
}

const uint8_t* VdexFile::GetNextDexFileData(const uint8_t* cursor) const {
  DCHECK(cursor == nullptr || (cursor > Begin() && cursor <= End()));
  LOG(FATAL) << "UNSUPPORTED";
  if (cursor == nullptr) {
    // Beginning of the iteration, return the first dex file if there is one.
    return HasDexSection() ? DexBegin() + sizeof(uint32_t) : nullptr;
  } else {
    // Fetch the next dex file. Return null if there is none.
    const uint8_t* data = cursor + reinterpret_cast<const DexFile::Header*>(cursor)->file_size_;
    // Dex files are required to be 4 byte aligned. the OatWriter makes sure they are, see
    // OatWriter::SeekToDexFiles.
    data = AlignUp(data, 4);

    return (data == DexEnd()) ? nullptr : data + sizeof(uint32_t);
  }
}

bool VdexFile::OpenAllDexFiles(std::vector<std::unique_ptr<const DexFile>>* dex_files,
                               std::string* error_msg) {
  size_t i = 0;
  for (const uint8_t* dex_file_start = GetNextDexFileData(nullptr);
       dex_file_start != nullptr;
       dex_file_start = GetNextDexFileData(dex_file_start), ++i) {
    size_t size = reinterpret_cast<const DexFile::Header*>(dex_file_start)->file_size_;
    // TODO: Supply the location information for a vdex file.
    static constexpr char kVdexLocation[] = "";
    std::string location = DexFileLoader::GetMultiDexLocation(i, kVdexLocation);
    std::unique_ptr<const DexFile> dex(DexFileLoader::Open(dex_file_start,
                                                           size,
                                                           location,
                                                           GetLocationChecksum(i),
                                                           nullptr /*oat_dex_file*/,
                                                           false /*verify*/,
                                                           false /*verify_checksum*/,
                                                           error_msg));
    if (dex == nullptr) {
      return false;
    }
    dex_files->push_back(std::move(dex));
  }
  return true;
}

void VdexFile::Unquicken(const std::vector<const DexFile*>& dex_files,
                         ArrayRef<const uint8_t> quickening_info,
                         bool decompile_return_instruction) {
  for (uint32_t i = 0; i < dex_files.size(); ++i) {
    UnquickenDexFile(*dex_files[i], quickening_info, decompile_return_instruction);
  }
}

static QuickenInfoOffsetTableAccessor GetQuickenInfoOffsetTable(
    const DexFile& dex_file,
    const ArrayRef<const uint8_t>& quickening_info) {
  LOG(FATAL) << "UNIMPLEMENTED";
  const uint8_t* data = dex_file.Begin() + dex_file.Size();
  // Dex files are required to be 4 byte aligned. the OatWriter makes sure they are, see
  // OatWriter::SeekToDexFiles.
  data = AlignUp(data, 4);
  using OffsetType = uint32_t;
  const uint8_t* data_ptr = quickening_info.data() + *reinterpret_cast<const OffsetType*>(data);
  return QuickenInfoOffsetTableAccessor(data_ptr, dex_file.NumMethodIds());
}

static ArrayRef<const uint8_t> GetQuickeningInfoAt(const ArrayRef<const uint8_t>& quickening_info,
                                                   uint32_t quickening_offset) {
  return quickening_info.SubArray(
      quickening_offset,
      QuickenInfoTable::SizeInBytes(&quickening_info[quickening_offset]));
}

static uint32_t GetQuickeningInfoOffset(const DexFile& dex_file,
                                        uint32_t dex_method_index,
                                        const ArrayRef<const uint8_t>& quickening_info) {
  DCHECK(!quickening_info.empty());
  QuickenInfoOffsetTableAccessor table(GetQuickenInfoOffsetTable(dex_file,
                                                                 quickening_info));
  uint32_t remainder;
  uint32_t offset = table.ElementOffset(dex_method_index, &remainder);
  // Decode the sizes for the remainder offsets (not covered by the table).
  while (remainder != 0) {
    offset += GetQuickeningInfoAt(quickening_info, offset).size();
    --remainder;
  }
  return offset;
}

void VdexFile::UnquickenDexFile(const DexFile& target_dex_file,
                                ArrayRef<const uint8_t> quickening_info,
                                bool decompile_return_instruction) {
  if (quickening_info.size() == 0 && !decompile_return_instruction) {
    // Bail early if there is no quickening info and no need to decompile
    // RETURN_VOID_NO_BARRIER instructions to RETURN_VOID instructions.
    return;
  }
  for (uint32_t i = 0; i < target_dex_file.NumClassDefs(); ++i) {
    const DexFile::ClassDef& class_def = target_dex_file.GetClassDef(i);
    const uint8_t* class_data = target_dex_file.GetClassData(class_def);
    if (class_data != nullptr) {
      for (ClassDataItemIterator class_it(target_dex_file, class_data);
           class_it.HasNext();
           class_it.Next()) {
        if (class_it.IsAtMethod() && class_it.GetMethodCodeItem() != nullptr) {
          const DexFile::CodeItem* code_item = class_it.GetMethodCodeItem();
          const uint32_t quickening_offset = GetQuickeningInfoOffset(
              target_dex_file, class_it.GetMemberIndex(), quickening_info);
          optimizer::ArtDecompileDEX(
              target_dex_file,
              *code_item,
              GetQuickeningInfoAt(quickening_info, quickening_offset),
              decompile_return_instruction);
        }
      }
    }
  }
}

ArrayRef<const uint8_t> VdexFile::GetQuickenedInfoOf(const DexFile& dex_file,
                                                     uint32_t dex_method_idx) const {
  ArrayRef<const uint8_t> quickening_info = GetQuickeningInfo();
  if (quickening_info.empty()) {
    return ArrayRef<const uint8_t>();
  }
  const uint32_t quickening_offset = GetQuickeningInfoOffset(dex_file,
                                                             dex_method_idx,
                                                             quickening_info);
  return GetQuickeningInfoAt(quickening_info, quickening_offset);
}

}  // namespace art
