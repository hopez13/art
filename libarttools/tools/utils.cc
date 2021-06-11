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

#include "utils.h"

#include <android-base/strings.h>
#include <base/logging.h>

#include <string>

#include "globals.h"

namespace art {
namespace tools {
namespace utils {

/*
 * File Local Functions
 */

/**
 * Validate that the path is valid in the context of the provided directory.
 * The path is allowed to have at most one subdirectory and no indirections
 * to top level directories (i.e. have "..").
 */
static int validate_path(const std::string& dir, const std::string& path, int maxSubdirs) {
  // Argument sanity checking
  if (dir.find('/') != 0 || dir.rfind('/') != dir.size() - 1 ||
      dir.find("..") != std::string::npos) {
    LOG(ERROR) << "Invalid directory " << dir;
    return -1;
  }
  if (path.find("..") != std::string::npos) {
    LOG(ERROR) << "Invalid path " << path;
    return -1;
  }

  if (path.compare(0, dir.size(), dir) != 0) {
    // Common case, path isn't under directory
    return -1;
  }

  // Count number of subdirectories
  auto pos = path.find('/', dir.size());
  int count = 0;
  while (pos != std::string::npos) {
    auto next = path.find('/', pos + 1);
    if (next > pos + 1) {
      count++;
    }
    pos = next;
  }

  if (count > maxSubdirs) {
    LOG(ERROR) << "Invalid path depth " << path << " when tested against " << dir;
    return -1;
  }

  return 0;
}

/**
 * Check whether path points to a valid path for an APK file. The path must
 * begin with a whitelisted prefix path and must be no deeper than |maxSubdirs| within
 * that path. Returns -1 when an invalid path is encountered and 0 when a valid path
 * is encountered.
 */
static int validate_apk_path_internal(const std::string& path, int maxSubdirs) {
  if (validate_path(android_app_dir, path, maxSubdirs) == 0) {
    return 0;
  } else if (validate_path(android_staging_dir, path, maxSubdirs) == 0) {
    return 0;
  } else if (validate_path(android_app_private_dir, path, maxSubdirs) == 0) {
    return 0;
  } else if (validate_path(android_app_ephemeral_dir, path, maxSubdirs) == 0) {
    return 0;
  } else if (validate_path(android_asec_dir, path, maxSubdirs) == 0) {
    return 0;
  } else if (android::base::StartsWith(path, android_mnt_expand_dir)) {
    // Rewrite the path as if it were on internal storage, and test that
    size_t end = path.find('/', android_mnt_expand_dir.size() + 1);
    if (end != std::string::npos) {
      auto modified = path;
      modified.replace(0, end + 1, android_data_dir);
      return validate_apk_path_internal(modified, maxSubdirs);
    }
  }
  return -1;
}

/*
 * Exported Functions
 */

int validate_apk_path(const std::string& path) {
  return validate_apk_path_internal(path, 2 /* maxSubdirs */);
}

}  // namespace utils
}  // namespace tools
}  // namespace art
