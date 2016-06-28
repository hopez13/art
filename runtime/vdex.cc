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

#include "vdex.h"

#include "base/unix_file/fd_file.h"
#include "os.h"

#include <algorithm>
#include <iostream>

namespace art {

constexpr uint8_t VdexHeader::kVdexMagic[4];
constexpr uint8_t VdexHeader::kVdexVersion[4];

VdexFile* VdexFile::Create(File* file,
                           const std::string& location ATTRIBUTE_UNUSED,
                           const std::vector<const DexFile*>& dex_files,
                           std::string* error_msg) {
  DCHECK_EQ(file->GetLength(), 0);
  DCHECK_EQ(lseek(file->Fd(), 0, SEEK_CUR), 0);

  *error_msg = "Success";
  return new VdexFile(file, new verifier::VerifierMetadata(dex_files));
}

VdexFile* VdexFile::Open(File* file,
                         const std::string& location ATTRIBUTE_UNUSED,
                         const std::vector<const DexFile*>& dex_files,
                         std::string* error_msg) {
  DCHECK_EQ(lseek(file->Fd(), 0, SEEK_CUR), 0);

  verifier::VerifierMetadata* metadata(verifier::VerifierMetadata::ReadFromFile(file, dex_files));
  if (metadata == nullptr) {
    *error_msg = "Failed parsing metadata";
    return nullptr;
  }
  DCHECK(metadata->IsSuccessfullyLoadedFromFile());

  *error_msg = "Success";
  return new VdexFile(file, metadata);
}

bool VdexFile::WriteToFile() {
  if (!metadata_->IsSuccessfullyLoadedFromFile()) {
    if (!file_->MoveToOffset(kVdexMetadataOffset) || !metadata_->WriteToFile(file_)) {
      return false;
    }
  }

  return true;
}

}  // namespace art
