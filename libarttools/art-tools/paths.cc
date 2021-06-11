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

#define LOG_TAG "libarttools"

#include "paths.h"

#include <android-base/logging.h>
#include <stdlib.h>
#include <string.h>

#include "constants.h"
#include "utils.h"

namespace art {
namespace tools {

/*
 * Sub-directories under ANDROID_DATA
 */

static constexpr const char* APP_SUBDIR = "app/";
static constexpr const char* PRIV_APP_SUBDIR = "priv-app/";
static constexpr const char* EPHEMERAL_APP_SUBDIR = "app-ephemeral/";
static constexpr const char* APP_LIB_SUBDIR = "app-lib/";
static constexpr const char* MEDIA_SUBDIR = "media/";
static constexpr const char* PROFILES_SUBDIR = "misc/profiles";
static constexpr const char* PRIVATE_APP_SUBDIR = "app-private/";
static constexpr const char* STAGING_SUBDIR = "app-staging/";

/*
 * Global Variables
 */

Paths paths;

/*
 * Function/Method Definitions
 */

static std::string EnsureTrailingSlash(const std::string& path) {
  if (path.rfind('/') != path.size() - 1) {
    return path + '/';
  } else {
    return path;
  }
}

Paths::Paths() {
  InitializeFromDataAndRoot();
}

bool Paths::InitializeFromDataAndRoot() {
  const char* data_path = getenv("ANDROID_DATA");
  if (data_path == nullptr) {
    LOG(ERROR) << "Could not find ANDROID_DATA";
    return false;
  }
  const char* root_path = getenv("ANDROID_ROOT");
  if (root_path == nullptr) {
    LOG(ERROR) << "Could not find ANDROID_ROOT";
    return false;
  }
  return InitializeFromDataAndRoot(data_path, root_path);
}

bool Paths::InitializeFromDataAndRoot(const std::string& data, const std::string& root) {
  // Get the android data directory.
  AndroidDataDir = EnsureTrailingSlash(data);

  // Get the android root directory.
  AndroidRootDir = EnsureTrailingSlash(root);

  // Get the android app directory.
  AndroidAppDir = AndroidDataDir + APP_SUBDIR;

  // Get the android protected app directory.
  AndroidAppPrivateDir = AndroidDataDir + PRIVATE_APP_SUBDIR;

  // Get the android ephemeral app directory.
  AndroidAppEphemeralDir = AndroidDataDir + EPHEMERAL_APP_SUBDIR;

  // Get the android app native library directory.
  AndroidAppLibDir = AndroidDataDir + APP_LIB_SUBDIR;

  // Get the sd-card ASEC mount point.
  AndroidAsecDir = EnsureTrailingSlash(getenv(ASEC_MOUNTPOINT_ENV_NAME));

  // Get the android media directory.
  AndroidMediaDir = AndroidDataDir + MEDIA_SUBDIR;

  // Get the android external app directory.
  AndroidMntExpandDir = "/mnt/expand/";

  // Get the android profiles directory.
  AndroidProfilesDir = AndroidDataDir + PROFILES_SUBDIR;

  // Get the android session staging directory.
  AndroidStagingDir = AndroidDataDir + STAGING_SUBDIR;

  // Take note of the system and vendor directories.
  AndroidSystemDirs.clear();
  AndroidSystemDirs.push_back(AndroidRootDir + APP_SUBDIR);
  AndroidSystemDirs.push_back(AndroidRootDir + PRIV_APP_SUBDIR);
  AndroidSystemDirs.push_back("/vendor/app/");
  AndroidSystemDirs.push_back("/oem/app/");

  return true;
}

}  // namespace tools
}  // namespace art
