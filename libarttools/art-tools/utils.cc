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

#include "paths.h"

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
static bool ValidatePath(const std::string& dir, const std::string& path, int maxSubdirs) {
  // Argument validity checking
  if (dir.find('/') != 0 || dir.rfind('/') != dir.size() - 1 ||
      dir.find("..") != std::string::npos) {
    LOG(ERROR) << "Invalid directory " << dir;
    return false;
  }
  if (path.find("..") != std::string::npos) {
    LOG(ERROR) << "Invalid path " << path;
    return false;
  }

  if (path.compare(0, dir.size(), dir) != 0) {
    // Common case, path isn't under directory
    return false;
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
    return false;
  }

  return true;
}

/**
 * Check whether path points to a valid path for an APK file. The path must
 * begin with a allowlisted prefix path and must be no deeper than |maxSubdirs| within
 * that path.
 */
static bool ValidateApkPathInternal(const std::string& path, int maxSubdirs) {
  if (ValidatePath(paths.AndroidAppDir, path, maxSubdirs) == 0) {
    return true;
  } else if (ValidatePath(paths.AndroidStagingDir, path, maxSubdirs) == 0) {
    return true;
  } else if (ValidatePath(paths.AndroidAppPrivateDir, path, maxSubdirs) == 0) {
    return true;
  } else if (ValidatePath(paths.AndroidAppEphemeralDir, path, maxSubdirs) == 0) {
    return true;
  } else if (ValidatePath(paths.AndroidAsecDir, path, maxSubdirs) == 0) {
    return true;
  } else if (android::base::StartsWith(path, paths.AndroidMntExpandDir)) {
    // Rewrite the path as if it were on internal storage, and test that
    size_t end = path.find('/', paths.AndroidMntExpandDir.size() + 1);
    if (end != std::string::npos) {
      auto modified = path;
      modified.replace(0, end + 1, paths.AndroidDataDir);
      return ValidateApkPathInternal(modified, maxSubdirs);
    }
  }
  return false;
}

/*
 * Exported Functions
 */

bool ValidateApkPath(const std::string& path) {
  return ValidateApkPathInternal(path, 2 /* maxSubdirs */);
}

}  // namespace utils
}  // namespace tools
}  // namespace art
