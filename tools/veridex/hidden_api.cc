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

#include "hidden_api.h"

#include <fstream>
#include <sstream>

#include "android-base/strings.h"
#include "dex/dex_file-inl.h"

namespace art {

HiddenApi::HiddenApi(const char* filename, bool sdk_uses_only) {
  CHECK(filename != nullptr);

  std::ifstream in(filename);
  for (std::string str; std::getline(api_file, str);) {
    std::vector<std::string> values = android::base::Split(str, ",");
    CHECK_EQ(values.size(), 2u) << "Currently only signature and one flag are supported";

    const std::string& signature = values[0];
    const std::string& flag_str = values[1];

    hiddenapi::ApiList membership = hiddenapi::ApiList::kNoList;
    for (size_t i = 0; i < hiddenapi::kApiListCount; ++i) {
      if (flag_str == hiddenapi::kApiListNames[i]) {
        membership = static_cast<hiddenapi::ApiList>(i);
      }
    }
    CHECK_NE(membership, hiddenapi::ApiList::kNoList) << "Unknown ApiList name: " << flag_str;

    CHECK(api_list_.find(signature) == api_list_.end()) << "Duplicate entry: " << signature;

    api_list_.emplace(signature, membership);
    size_t pos = signature.find("->");
    if (pos != std::string::npos) {
      // Add the class name.
      api_list_.emplace(signature.substr(0, pos), membership);
      pos = signature.find('(');
      if (pos != std::string::npos) {
        // Add the class->method name (so stripping the signature).
        api_list_.emplace(signature.substr(0, pos), membership);
      }
      pos = signature.find(':');
      if (pos != std::string::npos) {
        // Add the class->field name (so stripping the type).
        api_list_.emplace(signature.substr(0, pos), membership);
      }
    }
  }
}

std::string HiddenApi::GetApiMethodName(const DexFile& dex_file, uint32_t method_index) {
  std::stringstream ss;
  const DexFile::MethodId& method_id = dex_file.GetMethodId(method_index);
  ss << dex_file.StringByTypeIdx(method_id.class_idx_)
     << "->"
     << dex_file.GetMethodName(method_id)
     << dex_file.GetMethodSignature(method_id).ToString();
  return ss.str();
}

std::string HiddenApi::GetApiFieldName(const DexFile& dex_file, uint32_t field_index) {
  std::stringstream ss;
  const DexFile::FieldId& field_id = dex_file.GetFieldId(field_index);
  ss << dex_file.StringByTypeIdx(field_id.class_idx_)
     << "->"
     << dex_file.GetFieldName(field_id)
     << ":"
     << dex_file.GetFieldTypeDescriptor(field_id);
  return ss.str();
}

}  // namespace art
