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

#ifndef ART_TOOLS_DEXANALYZE_DEXANALYZE_STRINGS_H_
#define ART_TOOLS_DEXANALYZE_DEXANALYZE_STRINGS_H_

#include <array>
#include <map>
#include <vector>

#include "base/leb128.h"
#include "base/safe_map.h"
#include "dexanalyze_experiments.h"
#include "dex/code_item_accessors.h"
#include "dex/utf-inl.h"

namespace art {
namespace dexanalyze {

class PrefixDictionary {
 public:
  // Add prefix data and return the offset to the start of the added data.
  size_t AddPrefixData(const uint8_t* data, size_t len) {
    const size_t offset = prefix_data_.size();
    prefix_data_.insert(prefix_data_.end(), data, data + len);
    return offset;
  }

  static constexpr size_t kLengthBits = 8;
  static constexpr size_t kLengthMask = (1u << kLengthBits) - 1;

  // Return the prefix offset and length.
  ALWAYS_INLINE void GetOffset(uint32_t prefix_index, uint32_t* offset, uint32_t* length) const {
    CHECK_LT(prefix_index, offsets_.size());
    const uint32_t data = offsets_[prefix_index];
    *length = data & kLengthMask;
    *offset = data >> kLengthBits;
  }

  uint32_t AddOffset(uint32_t offset, uint32_t length) {
    CHECK_LE(length, kLengthMask);
    offsets_.push_back((offset << kLengthBits) | length);
    return offsets_.size() - 1;
  }

 public:
  std::vector<uint32_t> offsets_;
  std::vector<uint8_t> prefix_data_;
};

class PrefixStrings {
 public:
  class Builder {
   public:
    explicit Builder(PrefixStrings* output) : output_(output) {}
    void Build(const std::vector<std::string>& strings);

   private:
    PrefixStrings* const output_;
  };

  // Return the string index that was added.
  size_t AddString(uint16_t prefix, const std::string& str) {
    const size_t string_offset = chars_.size();
    chars_.push_back(static_cast<uint8_t>(prefix >> 8));
    chars_.push_back(static_cast<uint8_t>(prefix >> 0));
    EncodeUnsignedLeb128(&chars_, str.length());
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&str[0]);
    chars_.insert(chars_.end(), ptr, ptr + str.length());
    string_offsets_.push_back(string_offset);
    return string_offsets_.size() - 1;
  }

  std::string GetString(uint32_t string_idx) const {
    const size_t offset = string_offsets_[string_idx];
    const uint8_t* suffix_data = &chars_[offset];
    uint16_t prefix_idx = (static_cast<uint16_t>(suffix_data[0]) << 8) +
        suffix_data[1];
    suffix_data += 2;
    uint32_t prefix_offset;
    uint32_t prefix_len;
    dictionary_.GetOffset(prefix_idx, &prefix_offset, &prefix_len);
    const uint8_t* prefix_data = &dictionary_.prefix_data_[prefix_offset];
    std::string ret(prefix_data, prefix_data + prefix_len);
    uint32_t suffix_len = DecodeUnsignedLeb128(&suffix_data);
    ret.insert(ret.end(), suffix_data, suffix_data + suffix_len);
    return ret;
  }

  bool Equal(uint32_t string_idx, const uint8_t* data, size_t len) const {
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

 public:
  PrefixDictionary dictionary_;
  std::vector<uint8_t> chars_;
  std::vector<uint32_t> string_offsets_;
};

// Normal non prefix strings.
class NormalStrings {
 public:
  // Return the string index that was added.
  size_t AddString(const std::string& str) {
    const size_t string_offset = chars_.size();
    EncodeUnsignedLeb128(&chars_, str.length());
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&str[0]);
    chars_.insert(chars_.end(), ptr, ptr + str.length());
    string_offsets_.push_back(string_offset);
    return string_offsets_.size() - 1;
  }

  std::string GetString(uint32_t string_idx) const {
    const size_t offset = string_offsets_[string_idx];
    const uint8_t* data = &chars_[offset];
    uint32_t len = DecodeUnsignedLeb128(&data);
    return std::string(data, data + len);
  }

  bool Equal(uint32_t string_idx, const uint8_t* data, size_t len) const {
    const size_t offset = string_offsets_[string_idx];
    const uint8_t* str_data = &chars_[offset];
    uint32_t str_len = DecodeUnsignedLeb128(&str_data);
    if (str_len != len) {
      return false;
    }
    return memcmp(data, str_data, len) == 0u;
  }

 public:
  std::vector<uint8_t> chars_;
  std::vector<uint32_t> string_offsets_;
};

class StringTimings {
 public:
  void Dump(std::ostream& os) const;

  uint64_t time_equal_comparisons_ = 0u;
  uint64_t time_non_equal_comparisons_ = 0u;
  uint64_t num_comparisons_ = 0u;
};

// Analyze string data and strings accessed from code.
class AnalyzeStrings : public Experiment {
 public:
  void ProcessDexFiles(const std::vector<std::unique_ptr<const DexFile>>& dex_files) override;
  void Dump(std::ostream& os, uint64_t total_size) const override;

 private:
  void ProcessStrings(const std::vector<std::string>& strings);
  template <typename Strings> void Benchmark(const Strings& strings,
                                             const std::vector<std::string>& reference,
                                             StringTimings* timings);

  StringTimings prefix_timings_;
  StringTimings normal_timings_;
  int64_t wide_string_bytes_ = 0u;
  int64_t ascii_string_bytes_ = 0u;
  int64_t string_data_bytes_ = 0u;
  int64_t total_unique_string_data_bytes_ = 0u;
  int64_t total_shared_prefix_bytes_ = 0u;
  int64_t total_prefix_savings_ = 0u;
  int64_t total_prefix_dict_ = 0u;
  int64_t total_prefix_table_ = 0u;
  int64_t total_prefix_index_cost_ = 0u;
  int64_t total_num_prefixes_ = 0u;
  int64_t strings_used_prefixed_ = 0u;
  int64_t short_strings_ = 0u;
  int64_t long_strings_ = 0u;
};

}  // namespace dexanalyze
}  // namespace art

#endif  // ART_TOOLS_DEXANALYZE_DEXANALYZE_STRINGS_H_
