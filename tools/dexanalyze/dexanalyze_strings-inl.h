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

#ifndef ART_TOOLS_DEXANALYZE_DEXANALYZE_STRINGS_INL_H_
#define ART_TOOLS_DEXANALYZE_DEXANALYZE_STRINGS_INL_H_


#include "dexanalyze_strings.h"

namespace art {
namespace dexanalyze {

// Return the prefix offset and length.
inline void PrefixDictionary::GetOffset(uint32_t prefix_index, uint32_t* offset, uint32_t* length) const {
  CHECK_LT(prefix_index, offsets_.size());
  const uint32_t data = offsets_[prefix_index];
  *length = data & kLengthMask;
  *offset = data >> kLengthBits;
}

inline bool PrefixStrings::Equal(uint32_t string_idx, const uint8_t* data, size_t len) const {
  const size_t offset = string_offsets_[string_idx];
  const uint8_t* suffix_data = &chars_[offset];
  uint16_t prefix_idx = (static_cast<uint16_t>(suffix_data[0]) << 8) +
      suffix_data[1];
  suffix_data += 2;
  uint32_t prefix_offset;
  uint32_t prefix_len;
  dictionary_.GetOffset(prefix_idx, &prefix_offset, &prefix_len);
  uint32_t suffix_len = DecodeUnsignedLeb128(&suffix_data);
  if (prefix_len + suffix_len != len) {
    return false;
  }
  const uint8_t* prefix_data = &dictionary_.prefix_data_[prefix_offset];
  return memcmp(prefix_data, data, prefix_len) == 0u &&
      memcmp(suffix_data, data + prefix_len, len - prefix_len) == 0u;
}

inline bool NormalStrings::Equal(uint32_t string_idx, const uint8_t* data, size_t len) const {
  const size_t offset = string_offsets_[string_idx];
  const uint8_t* str_data = &chars_[offset];
  uint32_t str_len = DecodeUnsignedLeb128(&str_data);
  if (str_len != len) {
    return false;
  }
  return memcmp(data, str_data, len) == 0u;
}

}  // namespace dexanalyze
}  // namespace art

#endif  // ART_TOOLS_DEXANALYZE_DEXANALYZE_STRINGS_INL_H_
