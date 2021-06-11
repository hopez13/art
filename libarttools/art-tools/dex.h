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

#pragma once

#include <optional>
#include <string>

#include "constants.h"

namespace art {
namespace tools {
namespace dex {

// Returns the total bytes that were freed, or -1 in the case of errors.
int64_t DeleteOdex(const std::string& apk_path,
                   const std::string& instruction_set,
                   const std::optional<const std::string>& output_path);

std::string GetDalvikCacheDexArtifactPath(const std::string& apk_path,
                                          const std::string& instruction_set,
                                          const std::string& type);

std::string GetDexArtifactPath(const std::optional<std::string>& oat_dir,
                               const std::string& apk_path,
                               const std::string& instruction_set,
                               const std::string& type);

std::string GetPrimaryDexArtifactPath(const std::string& oat_dir,
                                      const std::string& apk_path,
                                      const std::string& instruction_set,
                                      const std::string& type);

}  // namespace dex
}  // namespace tools
}  // namespace art
