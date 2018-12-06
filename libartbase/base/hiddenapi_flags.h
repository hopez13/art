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

#ifndef ART_LIBARTBASE_BASE_HIDDENAPI_FLAGS_H_
#define ART_LIBARTBASE_BASE_HIDDENAPI_FLAGS_H_

#include "sdk_version.h"

#include <sstream>

#include "android-base/logging.h"

namespace art {
namespace hiddenapi {

/*
 * This class represents the information whether a field/method is in
 * public API (whitelist) or if it isn't, apps targeting which SDK
 * versions are allowed to access it.
 */
class ApiList {
 private:
  using IntValueType = uint32_t;

  enum class Value : IntValueType {
    // Values independent of target SDK version of app
    kWhitelist =     0,
    kGreylist =      1,
    kBlacklist =     2,

    // Values dependent on target SDK version of app. Put these last as
    // their list will be extended in future releases.
    // The max release code implicitly includes all maintenance releases,
    // e.g. GreylistMaxO is accessible to targetSdkVersion <= 27 (O_MR1).
    kGreylistMaxO =  3,
    kGreylistMaxP =  4,

    // Special values
    kInvalid =       static_cast<uint32_t>(-1),
    kMinValue =      kWhitelist,
    kMaxValue =      kGreylistMaxP,
  };

  static constexpr const char* kNames[] = {
    "whitelist",
    "greylist",
    "blacklist",
    "greylist-max-o",
    "greylist-max-p",
  };

  static constexpr const char* kInvalidName = "invalid";

  static constexpr SdkVersion kMaxSdkVersions[] {
    /* whitelist */ SdkVersion::kMax,
    /* greylist */ SdkVersion::kMax,
    /* blacklist */ SdkVersion::kMin,
    /* greylist-max-o */ SdkVersion::kO_MR1,
    /* greylist-max-p */ SdkVersion::kP,
  };

  static ApiList MinValue() { return ApiList(Value::kMinValue); }
  static ApiList MaxValue() { return ApiList(Value::kMaxValue); }

  explicit ApiList(Value value) : value_(value) {}

  Value value_;

 public:
  static ApiList Whitelist() { return ApiList(Value::kWhitelist); }
  static ApiList Greylist() { return ApiList(Value::kGreylist); }
  static ApiList Blacklist() { return ApiList(Value::kBlacklist); }
  static ApiList GreylistMaxO() { return ApiList(Value::kGreylistMaxO); }
  static ApiList GreylistMaxP() { return ApiList(Value::kGreylistMaxP); }
  static ApiList Invalid() { return ApiList(Value::kInvalid); }

  // Decodes ApiList from dex hiddenapi flags.
  static ApiList FromDexFlags(uint32_t dex_flags) {
    dex_flags &= kDexFlagsMask;
    if (MinValue().GetIntValue() <= dex_flags && dex_flags <= MaxValue().GetIntValue()) {
      return ApiList(static_cast<Value>(dex_flags));
    }
    return Invalid();
  }

  // Decodes ApiList from its integer value.
  static ApiList FromIntValue(IntValueType int_value) {
    if (MinValue().GetIntValue() <= int_value && int_value <= MaxValue().GetIntValue()) {
      return ApiList(static_cast<Value>(int_value));
    }
    return Invalid();
  }

  // Returns the ApiList with a given name.
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

  uint32_t ToDexFlags() const {
    CHECK(IsValid());
    uint32_t dex_flags = GetIntValue();
    CHECK((dex_flags & kDexFlagsMask) == dex_flags);
    return dex_flags;
  }

  IntValueType GetIntValue() const {
    DCHECK(IsValid());
    return static_cast<IntValueType>(value_);
  }

  const char* GetName() const { return IsValid() ? kNames[GetIntValue()]: kInvalidName; }

  SdkVersion GetMaxAllowedSdkVersion() const { return kMaxSdkVersions[GetIntValue()]; }

  // Number of bits designated for the ApiList value in the dex file for each field/method.
  // Changing this value breaks compatibility with existing dex files.
  static constexpr uint32_t kDexFlagsNumBits = 4u;

  static constexpr uint32_t kValueCount = static_cast<IntValueType>(Value::kMaxValue) + 1;
  static constexpr uint32_t kDexFlagsValueCount = (1 << kDexFlagsNumBits);
  static constexpr uint32_t kDexFlagsMask = kDexFlagsValueCount - 1;
  static_assert(kValueCount <= kDexFlagsValueCount,
      "Not enough bits to store all ApiList values in dex");
};

inline std::ostream& operator<<(std::ostream& os, ApiList value) {
  os << value.GetName();
  return os;
}

/*
 * A bit vector with flags where each one denotes that a field/method is
 * a member of a specialized API which only some code is allowed to access.
 * The flags are intended to override the restrictions defined in ApiList,
 * effectively treating the field/method as whitelisted for certain callers.
 */
class SpecializedApiFlags {
 private:
  using FlagIndexType = uint32_t;

  enum class Flag : FlagIndexType {
    kCorePlatformApi = 0,

    kMaxIndex = kCorePlatformApi,
  };

  static constexpr const char* kNames[] = {
    "core-platform-api",
  };

  static constexpr const char* kInvalidName = "invalid";
  static constexpr uint32_t kEmptyBitVector = 0u;
  static constexpr uint32_t kInvalidBitVector = static_cast<uint32_t>(-1);

  explicit SpecializedApiFlags(uint32_t bit_vector) : bit_vector_(bit_vector) {}

  uint32_t bit_vector_;

 public:
  static SpecializedApiFlags CorePlatformApi() {
    return SpecializedApiFlags(1u << static_cast<FlagIndexType>(Flag::kCorePlatformApi));
  }

  static SpecializedApiFlags Empty() { return SpecializedApiFlags(kEmptyBitVector); }
  static SpecializedApiFlags Invalid() { return SpecializedApiFlags(kInvalidBitVector); }

  bool operator==(const SpecializedApiFlags other) const {
    return bit_vector_ == other.bit_vector_;
  }

  bool operator!=(const SpecializedApiFlags other) const { return !(*this == other); }

  SpecializedApiFlags operator|=(const SpecializedApiFlags other) {
    CHECK(IsValid());
    CHECK(other.IsValid());
    bit_vector_ |= other.bit_vector_;
    return *this;
  }

  bool IsValid() const { return *this != Invalid(); }

  uint32_t ToDexFlags() const {
    CHECK(IsValid());
    return bit_vector_ << ApiList::kDexFlagsNumBits;
  }

  static SpecializedApiFlags FromDexFlags(uint32_t dex_flags) {
    dex_flags >>= ApiList::kDexFlagsNumBits;
    constexpr uint32_t kMaxBitVector = (1u << kFlagsCount) - 1;
    return (dex_flags > kMaxBitVector) ? Invalid() : SpecializedApiFlags(dex_flags);
  }

  static SpecializedApiFlags FromName(const std::string& str) {
    for (FlagIndexType i = 0; i < kFlagsCount; ++i) {
      if (str == kNames[i]) {
        return SpecializedApiFlags(i);
      }
    }
    return Invalid();
  }

  std::string ToString() const {
    if (IsValid()) {
      std::stringstream ss;
      bool is_first = true;
      for (size_t i = 0; i < kFlagsCount; ++i) {
        const uint32_t flag_bit_value = 1u << i;
        if ((bit_vector_ & flag_bit_value) == flag_bit_value) {
          if (is_first) {
            is_first = false;
          } else {
            ss << ",";
          }
          ss << kNames[i];
        }
      }
      return ss.str();
    } else {
      return kInvalidName;
    }
  }

  bool Contains(const SpecializedApiFlags other) const {
    CHECK(IsValid());
    CHECK(other.IsValid());
    return ((other.bit_vector_ & bit_vector_) == other.bit_vector_);
  }

  static constexpr FlagIndexType kFlagsCount = static_cast<FlagIndexType>(Flag::kMaxIndex) + 1;
};

inline std::ostream& operator<<(std::ostream& os, SpecializedApiFlags value) {
  os << value.ToString();
  return os;
}

/*
 * Class combining all information about non-SDK API visibility stored in dex files.
 */
class ApiInfo {
 public:
  ApiInfo(ApiList api_list, SpecializedApiFlags specialized_api_flags)
      : api_list_(api_list), specialized_api_flags_(specialized_api_flags) {}

  bool IsValid() const { return api_list_.IsValid() && specialized_api_flags_.IsValid(); }

  ApiList& GetApiList() { return api_list_; }
  const ApiList& GetApiList() const { return api_list_; }

  SpecializedApiFlags& GetSpecializedApiFlags() { return specialized_api_flags_; }
  const SpecializedApiFlags& GetSpecializedApiFlags() const { return specialized_api_flags_; }

  uint32_t ToDexFlags() const {
    uint32_t dex_api_list = api_list_.ToDexFlags();
    uint32_t dex_specialized_api_flags = specialized_api_flags_.ToDexFlags();
    CHECK_EQ(dex_api_list & dex_specialized_api_flags, 0u);
    return dex_api_list | dex_specialized_api_flags;
  }

  static ApiInfo FromDexFlags(uint32_t dex_flags) {
    return ApiInfo(
        ApiList::FromDexFlags(dex_flags), SpecializedApiFlags::FromDexFlags(dex_flags));
  }

 private:
  ApiList api_list_;
  SpecializedApiFlags specialized_api_flags_;
};

inline bool AreValidDexFlags(uint32_t dex_flags) {
  return ApiInfo::FromDexFlags(dex_flags).IsValid();
}

}  // namespace hiddenapi
}  // namespace art


#endif  // ART_LIBARTBASE_BASE_HIDDENAPI_FLAGS_H_
