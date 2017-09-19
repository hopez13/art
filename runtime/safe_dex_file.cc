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

#include <string.h>

#include "safe_dex_file.h"

#include "base/safe_copy.h"
#include "dex_file.h"
#include "leb128.h"
#include "utf-inl.h"

namespace art {

SafeDexFile::SafeDexFile(const DexFile* dex_file)
    : begin_(nullptr),
      header_(nullptr) {
  SafeRawData<DexFile> raw_dex_file;
  const DexFile* df = raw_dex_file.Copy(dex_file);
  if (df != nullptr) {
    begin_ = df->Begin();
    header_ = header_data_.Copy(df->Begin());
  }
}

std::string SafeDexFile::ReadString(dex::StringIndex string_index) const {
  if (!string_index.IsValid()) {
    return "";
  }
  if (!IsValid()) {
    return "StringInUnreadableDexFile";
  }
  if (string_index.index_ >= header_->string_ids_size_) {
    return "StringOOB#" +
        std::to_string(string_index.index_) + "/" + std::to_string(header_->string_ids_size_);
  }
  SafeRawData<DexFile::StringId> string_id_data;
  const DexFile::StringId* string_id =
      string_id_data.CopyArrayElement(begin_ + header_->string_ids_off_, string_index.index_);
  if (string_id == nullptr) {
    return "StringWithUnreadableId#" + std::to_string(string_index.index_);
  }
  constexpr size_t kBufferSize = 256;
  uint8_t buffer[kBufferSize] = {};
  size_t buffer_length = SafeCopy(buffer, begin_ + string_id->string_data_off_, kBufferSize);
  const uint8_t* ptr = buffer;
  uint32_t length = DecodeUnsignedLeb128(&ptr);  // Shall not go over the end of `buffer`.
  if (static_cast<size_t>(ptr - buffer) > buffer_length) {  // Did it go over the available data?
    return "StringWithUnreadableLength#" + std::to_string(string_index.index_);
  }
  const uint8_t* string_data_start = ptr;
  const char* string_data = reinterpret_cast<const char*>(string_data_start);
  constexpr size_t kMaxCodePointLength = 4;
  const char* suffix = "";
  size_t validated_chars = 0u;
  while (true) {
    if (validated_chars == length) {
      if (static_cast<size_t>(ptr - buffer) == buffer_length) {
        if (buffer_length != kBufferSize) {
          suffix = "[UNREADABLE-NUL]";
        }  // else we didn't read the next character because of `buffer` size; assume it's OK.
      } else if (*ptr != '\0') {
        suffix = "[MISSING-NUL]";
      }  // else properly terminated string, keep empty suffix.
      break;
    }
    if (static_cast<size_t>(kBufferSize - (ptr - buffer)) < kMaxCodePointLength) {
      suffix = "[...]";  // Unsafe to read further with GetUtf16FromUtf8().
      break;
    }
    // Note: Not fully verifying Modified-UTF8.
    const char* utf = reinterpret_cast<const char*>(ptr);
    uint32_t code_point = GetUtf16FromUtf8(&utf);
    const uint8_t* new_ptr = reinterpret_cast<const uint8_t*>(utf);
    DCHECK_LE(static_cast<size_t>(new_ptr - ptr), kMaxCodePointLength);
    if (static_cast<size_t>(new_ptr - buffer) > buffer_length) {
      suffix = "[...]";
      break;
    }
    if (GetTrailingUtf16Char(code_point) != 0u) {
      ++validated_chars;
      if (validated_chars == length) {
        suffix = "[BROKEN]";
        break;
      }
    }
    ptr = new_ptr;
    ++validated_chars;
  }
  return std::string(string_data, ptr - string_data_start) + suffix;
}

std::string SafeDexFile::ReadDescriptor(dex::TypeIndex type_index) const {
  if (!IsValid()) {
    return "TypeInUnreadableDexFile";
  }
  if (type_index.index_ >= header_->type_ids_size_) {
    return "TypeOOB#" +
        std::to_string(type_index.index_) + "/" + std::to_string(header_->type_ids_size_);
  }
  SafeRawData<DexFile::TypeId> type_id_data;
  const DexFile::TypeId* type_id =
      type_id_data.CopyArrayElement(begin_ + header_->type_ids_off_, type_index.index_);
  if (type_id == nullptr) {
    return "TypeWithUnreadableId#" + std::to_string(type_index.index_);
  }
  return ReadString(type_id->descriptor_idx_);
}

std::string SafeDexFile::PrettyDescriptor(dex::TypeIndex type_index) const {
  std::string result = ReadDescriptor(type_index);
  if (!result.empty() && result.front() == 'L') {
    result = result.substr(1u, result.size() - (result.back() == ';' ? 2u : 1u));
    std::replace(result.begin(), result.end(), '/', '.');
  }
  return result;
}

std::string SafeDexFile::String(const DexFile* dex_file, dex::StringIndex string_index) {
  SafeDexFile sdf(dex_file);
  return sdf.ReadString(string_index);
}

std::string SafeDexFile::Descriptor(const DexFile* dex_file, dex::TypeIndex type_index) {
  SafeDexFile sdf(dex_file);
  return sdf.ReadDescriptor(type_index);
}

std::string SafeDexFile::PrettyDescriptor(const DexFile* dex_file, dex::TypeIndex type_index) {
  SafeDexFile sdf(dex_file);
  return sdf.PrettyDescriptor(type_index);
}

}  // namespace art
