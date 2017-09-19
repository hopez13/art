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

#include "base/leb128.h"
#include "base/safe_copy.h"
#include "dex/dex_file.h"
#include "dex/descriptors_names.h"
#include "dex/utf-inl.h"

namespace art {

SafeDexFile::SafeDexFile(const DexFile* dex_file)
    : begin_(nullptr),
      header_(nullptr) {
  SafeRawData<DexFile> raw_dex_file;
  const DexFile* df = raw_dex_file.Copy(dex_file);
  if (df != nullptr) {
    begin_ = df->Begin();
    data_begin_ = df->DataBegin();
    header_ = header_data_.Copy(df->Begin());
  }
}

std::string SafeDexFile::String(dex::StringIndex string_index) const {
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
  size_t buffer_length = SafeCopy(buffer, data_begin_ + string_id->string_data_off_, kBufferSize);
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

std::string SafeDexFile::Descriptor(dex::TypeIndex type_index) const {
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
  return String(type_id->descriptor_idx_);
}

std::string SafeDexFile::PrettyDescriptor(dex::TypeIndex type_index) const {
  std::string result = Descriptor(type_index);
  size_t array_dim = result.find_first_not_of('[');
  if (array_dim == std::string::npos) {
    return result;  // The descriptor is broken (empty or consists solely of '[' characters).
  }
  if (result[array_dim] == 'L') {
    result = result.substr(array_dim + 1u,
                           result.size() - array_dim - (result.back() == ';' ? 2u : 1u));
    std::replace(result.begin(), result.end(), '/', '.');
    for (size_t i = 0; i != array_dim; ++i) {
      result += "[]";
    }
    return result;
  } else if (array_dim + 1u == result.size()) {
    // Probably a primitive type, art::PrettyDescriptor() can deal with that.
    return art::PrettyDescriptor(result.c_str());
  } else {
    return result;  // The descriptor is broken, return it as it is.
  }
}

std::string SafeDexFile::PrettyField(uint32_t field_index, bool with_type) const {
  if (!IsValid()) {
    return "FieldInUnreadableDexFile";
  }
  if (field_index >= header_->field_ids_size_) {
    return "FieldOOB#" +
        std::to_string(field_index) + "/" + std::to_string(header_->field_ids_size_);
  }
  SafeRawData<DexFile::FieldId> field_id_data;
  const DexFile::FieldId* field_id =
      field_id_data.CopyArrayElement(begin_ + header_->field_ids_off_, field_index);
  if (field_id == nullptr) {
    return "FieldWithUnreadableId#" + std::to_string(field_index);
  }
  std::string result;
  if (with_type) {
    result += PrettyDescriptor(field_id->type_idx_);
    result += " ";
  }
  result += PrettyDescriptor(field_id->class_idx_);
  result += '.';
  result += String(field_id->name_idx_);
  return result;
}

std::string SafeDexFile::PrettyMethod(uint32_t method_index, bool with_signature) const {
  if (!IsValid()) {
    return "MethodInUnreadableDexFile";
  }
  if (method_index >= header_->method_ids_size_) {
    return "MethodOOB#" +
        std::to_string(method_index) + "/" + std::to_string(header_->method_ids_size_);
  }
  SafeRawData<DexFile::MethodId> method_id_data;
  const DexFile::MethodId* method_id =
      method_id_data.CopyArrayElement(begin_ + header_->method_ids_off_, method_index);
  if (method_id == nullptr) {
    return "MethodWithUnreadableId#" + std::to_string(method_index);
  }
  std::string result;
  result += PrettyDescriptor(method_id->class_idx_);
  result += '.';
  result += String(method_id->name_idx_);
  if (with_signature) {
    if (method_id->proto_idx_.index_ >= header_->proto_ids_size_) {
      result += "/ProtoOOB#" + std::to_string(method_id->proto_idx_.index_) +
          "/" + std::to_string(header_->proto_ids_size_);
    } else {
      SafeRawData<DexFile::ProtoId> proto_id_data;
      const DexFile::ProtoId* proto_id =
          proto_id_data.CopyArrayElement(begin_ + header_->proto_ids_off_,
                                         method_id->proto_idx_.index_);
      if (proto_id == nullptr) {
        result+= "/UnreadableProtoId#" + std::to_string(method_id->proto_idx_.index_);
      } else {
        result = PrettyDescriptor(proto_id->return_type_idx_) + ' ' + result;
        if (proto_id->parameters_off_ != 0u) {
          uint32_t size;
          static_assert(DexFile::TypeList::GetHeaderSize() == sizeof(size), "Header size check.");
          if (!SafeCopy(&size, data_begin_ + proto_id->parameters_off_, sizeof(size))) {
            result += "/UnreadableParametersSize";
          } else {
            result += '(';
            for (uint32_t i = 0; i != size; ++i) {
              if (i != 0) {
                result += ", ";
              }
              constexpr uint32_t kMaxArgs = 10u;  // Do not read unlimited number of arguments.
              if (i == kMaxArgs) {
                result += "[...]";
                break;
              }
              SafeRawData<DexFile::TypeItem> type_item_data;
              const DexFile::TypeItem* type_item = type_item_data.CopyArrayElement(
                  data_begin_ + proto_id->parameters_off_ + sizeof(size), i);
              if (type_item == nullptr) {
                result += "[UNREADABLE]";
                break;
              }
              result += PrettyDescriptor(type_item->type_idx_);
            }
            result += ')';
          }
        }
      }
    }
  }
  return result;
}

std::string SafeDexFile::String(const DexFile* dex_file, dex::StringIndex string_index) {
  SafeDexFile sdf(dex_file);
  return sdf.String(string_index);
}

std::string SafeDexFile::Descriptor(const DexFile* dex_file, dex::TypeIndex type_index) {
  SafeDexFile sdf(dex_file);
  return sdf.Descriptor(type_index);
}

std::string SafeDexFile::PrettyDescriptor(const DexFile* dex_file, dex::TypeIndex type_index) {
  SafeDexFile sdf(dex_file);
  return sdf.PrettyDescriptor(type_index);
}

std::string SafeDexFile::PrettyField(
    const DexFile* dex_file, uint32_t field_index, bool with_type) {
  SafeDexFile sdf(dex_file);
  return sdf.PrettyField(field_index, with_type);
}

std::string SafeDexFile::PrettyMethod(
    const DexFile* dex_file, uint32_t method_index, bool with_signature) {
  SafeDexFile sdf(dex_file);
  return sdf.PrettyMethod(method_index, with_signature);
}

}  // namespace art
