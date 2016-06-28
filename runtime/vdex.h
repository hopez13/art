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

#ifndef ART_RUNTIME_VDEX_H_
#define ART_RUNTIME_VDEX_H_

#include <vector>

#include "base/macros.h"
#include "dex_file.h"
#include "safe_map.h"
#include "verifier/verifier_metadata.h"

namespace art {

class VdexFile;

class PACKED(4) VdexHeader {
 public:
  static constexpr uint8_t kVdexMagic[] = { 'v', 'd', 'e', 'x' };
  static constexpr uint8_t kVdexVersion[] = { '0', '0', '0', '\0' };

 private:
  friend class VdexFile;
};

class VdexFile {
 public:
  static VdexFile* Create(File* file,
                          const std::string& location,
                          const std::vector<const DexFile*>& dex_files,
                          std::string* error_msg);
  static VdexFile* Open(File* file,
                        const std::string& location,
                        const std::vector<const DexFile*>& dex_files,
                        std::string* error_msg);

  const VdexHeader& GetHeader() const { return header_; }
  verifier::VerifierMetadata* GetMetadata() { return metadata_.get(); }

  bool WriteToFile();

 private:
  explicit VdexFile(File* file, verifier::VerifierMetadata* metadata)
      : file_(file),
        metadata_(metadata) {}

  static constexpr off_t kVdexMetadataOffset = 0;

  File* file_;
  VdexHeader header_;
  std::unique_ptr<verifier::VerifierMetadata> metadata_;
};

}  // namespace art

#endif  // ART_RUNTIME_VDEX_H_
