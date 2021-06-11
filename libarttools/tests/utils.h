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

#include <art-tools/utils.h>

#include <string>

namespace art {
namespace tools {
namespace tests {

extern const std::string kRuntimeIsa;

int MkDir(const std::string& path, uid_t owner, gid_t group, mode_t mode);
void RunCmd(const std::string& cmd);
void WriteRandomData(const std::string& path, size_t num_bytes);

/*
 * Cloned Functions
 *
 * These must be kept in sync with the implementations in libcutils/multiuser.h
 */

typedef uid_t userid_t;
typedef uid_t appid_t;

uid_t multiuser_get_uid(userid_t user_id, appid_t app_id);
gid_t multiuser_get_shared_gid(userid_t, appid_t app_id);

}  // namespace tests
}  // namespace tools
}  // namespace art