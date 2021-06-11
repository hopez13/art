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

#include <string>

namespace art {
namespace tools {
namespace utils {

bool ValidateApkPath(const std::string& path);

/*
 * Cloned Functions
 *
 * These must be kept in sync with the implementations in
 * system/core/libcutils/include/private/android_filesystem_config.h
 */

#define AID_ROOT 0 /* traditional unix root user */

#define AID_SYSTEM 1000 /* system server */

#define AID_APP       10000 /* TODO: switch users over to AID_APP_START */
#define AID_APP_START 10000 /* first app user */
#define AID_APP_END   19999 /* last app user */

#define AID_SHARED_GID_START 50000 /* start of gids for apps in each user to share */
#define AID_SHARED_GID_END   59999 /* end of gids for apps in each user to share */

#define AID_USER_OFFSET 100000 /* offset for uid ranges for each user */

}  // namespace utils
}  // namespace tools
}  // namespace art
