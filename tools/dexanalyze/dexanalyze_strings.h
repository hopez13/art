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
  ALWAYS_INLINE void GetOffset(uint32_t prefix_index, uint32_t* offset, uint32_t* length) const;

  uint32_t AddOffset(uint32_t offset, uint32_t length);

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
  size_t AddString(uint16_t prefix, const std::string& str);

  std::string GetString(uint32_t string_idx) const;

  ALWAYS_INLINE bool Equal(uint32_t string_idx, const uint8_t* data, size_t len) const;
 public:
  PrefixDictionary dictionary_;
  std::vector<uint8_t> chars_;
  std::vector<uint32_t> string_offsets_;
};

// Normal non prefix strings.
class NormalStrings {
 public:
  // Return the string index that was added.
  size_t AddString(const std::string& str);

  std::string GetString(uint32_t string_idx) const;

  ALWAYS_INLINE bool Equal(uint32_t string_idx, const uint8_t* data, size_t len) const;

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
