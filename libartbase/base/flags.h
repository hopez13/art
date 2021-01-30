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

#include <optional>
#include <string>
#include <vector>

#pragma clang diagnostic push
#pragma clang diagnostic error "-Wconversion"

namespace art {

class FlagBase {};

template <typename Value>
class Flag : public FlagBase {
 public:
  Value operator()() const;

 private:
  Flag(Value default_value,
                 std::string command_line_argument_name,
                 std::string system_property_name,
                 std::string server_setting_name);

  const Value default_;

  const std::string command_line_argument_name_;
  std::optional<Value> from_command_line_;

  const std::string system_property_name_;
  const std::string server_setting_name_;

  friend class Flags;
};

class Flags {
 public:
  Flag<bool> WriteMetricsToLog{
      false, "-Xwrite-metrics-to-log", "write-metrics-to-log", "metrics.write-to-log"};

 private:
  static std::vector<FlagBase*> ALL_FLAGS;

  template <typename Value>
  friend class Flag;
};

}  // namespace art

#pragma clang diagnostic pop  // -Wconversion

#endif  // LIBARTBASE_BASE_FLAGS_H_
