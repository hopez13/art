/*
 * Copyright (C) 2019 The Android Open Source Project
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

#ifndef ART_LIBARTBASE_BASE_STRING_VIEW_CPP20_H_
#define ART_LIBARTBASE_BASE_STRING_VIEW_CPP20_H_

namespace art {

// Replacement functions for std::string_view::starts_with(), ends_with()
// which shall be available in C++20.
// TODO: Remove when C++20 is supported.

inline bool StartsWith(const std::string_view& sv, const std::string_view& prefix) {
  return sv.substr(0u, prefix.size()) == prefix;
}

inline bool EndsWith(const std::string_view& sv, const std::string_view& prefix) {
  return sv.size() >= prefix.size() && sv.substr(sv.size() - prefix.size()) == prefix;
}

}  // namespace art

#endif  // ART_LIBARTBASE_BASE_STRING_VIEW_CPP20_H_
