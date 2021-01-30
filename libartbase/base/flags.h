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

#ifndef LIBARTBASE_BASE_FLAGS_H_
#define LIBARTBASE_BASE_FLAGS_H_

#include <forward_list>
#include <optional>
#include <string>

#pragma clang diagnostic push
#pragma clang diagnostic error "-Wconversion"

namespace art {

class FlagBase {
 public:
 protected:
  static std::forward_list<FlagBase*> ALL_FLAGS;
};

template <typename Value>
class Flag : public FlagBase {
 public:
  Value operator()() const;

  Flag(Value default_value,
       std::string command_line_argument_name,
       std::string system_property_name,
       std::string server_setting_name);

 private:
  const Value default_;

  const std::string command_line_argument_name_;
  std::optional<Value> from_command_line_;
  std::optional<Value> from_system_property_;

  const std::string system_property_name_;
  const std::string server_setting_name_;

  friend class Flags;
};

}  // namespace art

#pragma clang diagnostic pop  // -Wconversion

#endif  // LIBARTBASE_BASE_FLAGS_H_
