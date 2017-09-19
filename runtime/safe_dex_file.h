/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef ART_RUNTIME_SAFE_DEX_FILE_H_
#define ART_RUNTIME_SAFE_DEX_FILE_H_

#include <string>

#include "base/safe_copy.h"
#include "dex/dex_file.h"

namespace art {

class DexFile;

class SafeDexFile {
 public:
  static std::string String(const DexFile* dex_file, dex::StringIndex string_index);
  static std::string Descriptor(const DexFile* dex_file, dex::TypeIndex type_index);
  static std::string PrettyDescriptor(const DexFile* dex_file, dex::TypeIndex type_index);
  static std::string PrettyField(
      const DexFile* dex_file, uint32_t field_index, bool with_type = true);
  static std::string PrettyMethod(
      const DexFile* dex_file, uint32_t method_index, bool with_signature = true);

  explicit SafeDexFile(const DexFile* dex_file);

  bool IsValid() const {
    return header_ != nullptr;
  }

  std::string String(dex::StringIndex string_index) const;
  std::string Descriptor(dex::TypeIndex type_index) const;
  std::string PrettyDescriptor(dex::TypeIndex type_index) const;
  std::string PrettyField(uint32_t field_index, bool with_type = true) const;
  std::string PrettyMethod(uint32_t method_index, bool with_signature = true) const;

 private:
  SafeRawData<DexFile::Header> header_data_;
  const uint8_t* begin_;
  const uint8_t* data_begin_;
  const DexFile::Header* header_;
};

}  // namespace art

#endif  // ART_RUNTIME_SAFE_DEX_FILE_H_
