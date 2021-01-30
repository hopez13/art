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

#ifndef ART_LIBARTBASE_BASE_FLAGS_H_
#define ART_LIBARTBASE_BASE_FLAGS_H_

#include <forward_list>
#include <optional>
#include <string>
#include <variant>

#pragma clang diagnostic push
#pragma clang diagnostic error "-Wconversion"

namespace art {

template <typename... T>
class FlagMetaBase {
 public:
  virtual ~FlagMetaBase() {}

  template <typename Builder>
  static void AddFlagsToCmdlineParser(Builder* builder) {
    for (auto* flag : ALL_FLAGS) {
      FlagValuePointer location = flag->GetLocation();
      auto cases = {[&]() {
        if (std::holds_alternative<std::optional<T>*>(location)) {
          builder = &builder->Define(flag->command_line_argument_name_.c_str())
                         .template WithType<T>()
                         .IntoLocation(std::get<std::optional<T>*>(location));
        }
      }...};
      for (auto c : cases) {
        c();
      }
    }
  }

 protected:
  using FlagValuePointer = std::variant<std::optional<T>*...>;
  static std::forward_list<FlagMetaBase<T...>*> ALL_FLAGS;

  std::string command_line_argument_name_;
  std::string system_property_name_;
  std::string server_setting_name_;

  virtual FlagValuePointer GetLocation() = 0;
};

using FlagBase = FlagMetaBase<bool>;

template <>
std::forward_list<FlagBase*> FlagBase::ALL_FLAGS;

template <typename Value>
class Flag : public FlagBase {
 public:
  Value operator()();

  explicit Flag(const std::string& name, Value default_value = {});
  virtual ~Flag() {}

  void Reload();

 protected:
  FlagValuePointer GetLocation() override { return &from_command_line_; }

 private:
  bool initialized_{false};
  const Value default_;
  std::optional<Value> from_command_line_;
  std::optional<Value> from_system_property_;
  std::optional<Value> from_server_setting_;
};

struct Flags {
  Flag<bool> WriteMetricsToLog{"metrics.write-to-log", false};
};

extern Flags gFlags;

}  // namespace art

#pragma clang diagnostic pop  // -Wconversion

#endif  // ART_LIBARTBASE_BASE_FLAGS_H_
