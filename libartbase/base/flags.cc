/*
 * Copyright (C) 2021 The Android Open Source Project
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

#include "flags.h"

#include "android-base/parsebool.h"
#include "android-base/properties.h"
//#include "server_configurable_flags/get_flags.h"

#pragma clang diagnostic push
#pragma clang diagnostic error "-Wconversion"

namespace {
// constexpr const char* kPhenotypeNamespace = "runtime_native_boot";
const std::string kUndefinedValue = "UNSET";

// The various ParseValue functions store the parsed value into *destination and return true if
// successful. Otherwise, ParseValue returns false and makes no changes to *destination.

bool ParseValue(const std::string_view value, std::optional<bool>* destination) {
  switch (::android::base::ParseBool(value)) {
    case ::android::base::ParseBoolResult::kError:
      return false;
    case ::android::base::ParseBoolResult::kTrue:
      *destination = true;
      return true;
    case ::android::base::ParseBoolResult::kFalse:
      *destination = false;
      return true;
  }
}

}  // namespace

namespace art {

template <>
std::forward_list<FlagBase*> FlagBase::ALL_FLAGS{};

// gFlags must be defined after FlagBase::ALL_FLAGS so the constructors run in the right order.
Flags gFlags;

template <typename Value>
Flag<Value>::Flag(Value default_value,
                  std::string command_line_argument_name,
                  std::string system_property_name,
                  std::string server_setting_name)

    : default_{default_value} {
  command_line_argument_name_ = command_line_argument_name;
  system_property_name_ = "dalvik.vm." + system_property_name;
  server_setting_name_ = server_setting_name;

  ALL_FLAGS.push_front(this);

  // Check system properties and server configured value.
  std::string sysprop =
      kUndefinedValue;  // ::android::base::GetProperty(system_property_name_, kUndefinedValue);
  if (sysprop != kUndefinedValue) {
    ParseValue(sysprop, &from_system_property_);
  }
}

template <typename Value>
Value Flag<Value>::operator()() const {
  if (from_system_property_.has_value()) {
    return from_system_property_.value();
  }
  return default_;
}

template class Flag<bool>;

}  // namespace art

#pragma clang diagnostic pop  // -Wconversion
