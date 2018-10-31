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

#ifndef ART_LIBDEXFILE_DEX_HIDDENAPI_FLAGS_H_
#define ART_LIBDEXFILE_DEX_HIDDENAPI_FLAGS_H_

#include "base/bit_utils.h"
#include "base/macros.h"
#include "dex/modifiers.h"

/* This class is used for encoding and decoding access flags of class members
 * from the boot class path. These access flags might contain additional two bits
 * of information on whether the given class member should be hidden from apps
 * and under what circumstances.
 *
 * Two bits are encoded for each class member in the HiddenapiClassData item,
 * stored in a stream of uleb128-encoded values for each ClassDef item.
 * The two bits correspond to values in the ApiList enum below.
 *
 * At runtime, two bits are set aside in the uint32_t access flags in the
 * intrinsics ordinal space (thus intrinsics need to be special-cased). These are
 * two consecutive bits and they are directly used to store the integer value of
 * the ApiList enum values.
 *
 */

namespace art {
namespace hiddenapi {

class ApiList {
 private:
  using IntValueType = uint32_t;

  enum class Value : IntValueType {
    // Values independent of target SDK version of app
    kWhitelist =     0,
    kGreylist =      1,
    kBlacklist =     2,

    // Values dependent on target SDK version of app.
    // List will be extended in future releases.
    kBlacklistMaxO = 3,

    // Special values
    kInvalid =       static_cast<uint32_t>(-1),
    kMinValue =      kWhitelist,
    kMaxValue =      kBlacklistMaxO,
  };

  static constexpr const char* kNames[] = {
    "whitelist",
    "greylist",
    "blacklist",
    "blacklist-max-o",
  };

  enum class SdkCodes : int32_t {
    kVersionNone      = std::numeric_limits<int32_t>::min(),
    kVersionUnlimited = std::numeric_limits<int32_t>::max(),
    kVersionO         = 24,
  };

  static constexpr SdkCodes kMaxSdkVersions[] {
    /* whitelist */ SdkCodes::kVersionUnlimited,
    /* greylist */ SdkCodes::kVersionUnlimited,
    /* blacklist */ SdkCodes::kVersionNone,
    /* blacklist-max-o */ SdkCodes::kVersionO,
  };

  static ApiList MinValue() { return ApiList(Value::kMinValue); }
  static ApiList MaxValue() { return ApiList(Value::kMaxValue); }

  explicit ApiList(Value value) : value_(value) {}

  const Value value_;

 public:
  static ApiList Whitelist() { return ApiList(Value::kWhitelist); }
  static ApiList Greylist() { return ApiList(Value::kGreylist); }
  static ApiList Blacklist() { return ApiList(Value::kBlacklist); }
  static ApiList BlacklistMaxO() { return ApiList(Value::kBlacklistMaxO); }
  static ApiList Invalid() { return ApiList(Value::kInvalid); }

  static ApiList FromFlags(uint32_t flags) {
    if (MinValue().GetIntValue() <= flags && flags <= MaxValue().GetIntValue()) {
      return ApiList(static_cast<Value>(flags));
    }
    return Invalid();
  }

  static ApiList FromName(const std::string& str) {
    for (IntValueType i = MinValue().GetIntValue(); i <= MaxValue().GetIntValue(); i++) {
      ApiList current = ApiList(static_cast<Value>(i));
      if (str == current.GetName()) {
        return current;
      }
    }
    return Invalid();
  }

  bool operator==(const ApiList other) const { return value_ == other.value_; }
  bool operator!=(const ApiList other) const { return !(*this == other); }

  bool IsValid() const { return *this != Invalid(); }

  IntValueType GetIntValue() const {
    DCHECK(IsValid());
    return static_cast<IntValueType>(value_);
  }

  const char* GetName() const { return kNames[GetIntValue()]; }

  int32_t GetMaxAllowedSdkVersion() const {
    return static_cast<int32_t>(kMaxSdkVersions[GetIntValue()]);
  }
};

inline std::ostream& operator<<(std::ostream& os, ApiList value) {
  os << value.GetName();
  return os;
}

inline bool AreValidFlags(uint32_t flags) {
  return ApiList::FromFlags(flags).IsValid();
}

}  // namespace hiddenapi
}  // namespace art


#endif  // ART_LIBDEXFILE_DEX_HIDDENAPI_FLAGS_H_
