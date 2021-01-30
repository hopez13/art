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

#pragma clang diagnostic push
#pragma clang diagnostic error "-Wconversion"

namespace art {

std::vector<FlagBase*> Flags::ALL_FLAGS;

template <typename Value>
Flag<Value>::Flag(Value default_value,
                            std::string command_line_argument_name,
                            std::string system_property_name,
                            std::string server_setting_name)

    : default_{default_value},
      command_line_argument_name_{command_line_argument_name},
      system_property_name_{system_property_name},
      server_setting_name_{server_setting_name} {
  Flags::ALL_FLAGS.push_back(this);
}

// template <typename Value>
// Value Flag<Value>::operator()() const {}

template class Flag<bool>;

}  // namespace art

#pragma clang diagnostic pop  // -Wconversion
