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

#include "dex.h"

#include <android-base/strings.h>
#include <arch/instruction_set.h>
#include <base/file_utils.h>
#include <base/logging.h>
#include <base/strlcpy.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <string>

#include "paths.h"
#include "utils.h"

using android::base::EndsWith;

namespace art {
namespace tools {
namespace dex {

/*
 * File Local Functions
 */

bool calculate_oat_file_path(char path[PKG_PATH_MAX],
                             const std::string& oat_dir,
                             const std::string& apk_path,
                             const std::string& instruction_set) {
  if (!IsAbsoluteLocation(oat_dir))
    return false;
  if (!IsAbsoluteLocation(apk_path))
    return false;
  if (GetInstructionSetFromString(instruction_set.c_str()) == InstructionSet::kNone)
    return false;

  std::string::size_type end = apk_path.rfind('.');
  std::string::size_type start = apk_path.rfind('/', end);
  if (end == std::string::npos || start == std::string::npos) {
    LOG(ERROR) << "Invalid apk_path " << apk_path;
    return false;
  }

  std::string res =
      oat_dir + '/' + instruction_set + '/' + apk_path.substr(start + 1, end - start - 1) + ".odex";

  if (res.length() >= PKG_PATH_MAX) {
    LOG(ERROR) << "Result too large";
    return false;
  } else {
    strlcpy(path, res.c_str(), PKG_PATH_MAX);
    return true;
  }
}

bool create_cache_path(char path[PKG_PATH_MAX],
                       const std::string& src,
                       const std::string& instruction_set) {
  if (!IsAbsoluteLocation(src))
    return false;
  if (GetInstructionSetFromString(instruction_set.c_str()) == InstructionSet::kNone)
    return false;

  std::string dalvik_cache_key = src;
  std::replace(dalvik_cache_key.begin() + 1, dalvik_cache_key.end(), '/', '@');

  std::string res = paths.AndroidDataDir + DALVIK_CACHE + '/' + instruction_set + dalvik_cache_key +
                    DALVIK_CACHE_POSTFIX;

  if (res.length() >= PKG_PATH_MAX) {
    LOG(ERROR) << "Result too large";
    return false;
  } else {
    strlcpy(path, res.c_str(), PKG_PATH_MAX);
    return true;
  }
}

// Best-effort check whether we can fit the path into our buffers.
// Note: the cache path will require an additional 5 bytes for ".swap", but we'll try to run
// without a swap file, if necessary. Reference profiles file also add an extra ".prof"
// extension to the cache path (5 bytes).
// TODO(calin): move away from char* buffers and PKG_PATH_MAX.
static bool validate_dex_path_size(const std::string& dex_path) {
  if (dex_path.size() >= (PKG_PATH_MAX - 8)) {
    LOG(ERROR) << "dex_path too long: " << dex_path;
    return false;
  }
  return true;
}

static bool create_oat_out_path(const std::string& apk_path,
                                const std::string& instruction_set,
                                const std::optional<const std::string>& oat_dir,
                                bool is_secondary_dex,
                                /*out*/ char* out_oat_path) {
  if (!validate_dex_path_size(apk_path)) {
    return false;
  }

  if (oat_dir) {
    // Oat dirs for secondary dex files are already validated.
    if (!is_secondary_dex && utils::ValidateApkPath(oat_dir.value())) {
      LOG(ERROR) << "cannot validate apk path with oat_dir " << oat_dir.value();
      return false;
    }
    if (!calculate_oat_file_path(out_oat_path, oat_dir.value(), apk_path, instruction_set)) {
      return false;
    }
  } else {
    // If no OAT directory was provided the artifacts are located in the Dalvik cache.
    if (!create_cache_path(out_oat_path, apk_path, instruction_set)) {
      return false;
    }
  }
  return true;
}

/*
 * Exported Functions
 */

int64_t DeleteOdex(const std::string& apk_path,
                   const std::string& instruction_set,
                   const std::optional<const std::string>& oat_dir) {
  // Delete the oat/odex file.
  char out_path[PKG_PATH_MAX];
  if (!create_oat_out_path(apk_path,
                           instruction_set,
                           oat_dir,
                           /*is_secondary_dex*/ false,
                           out_path)) {
    LOG(ERROR) << "Cannot create apk path for " << apk_path;
    return -1;
  }

  // In case of a permission failure report the issue. Otherwise just print a warning.
  auto unlink_and_check = [](const char* path) -> int64_t {
    struct stat file_stat;
    if (stat(path, &file_stat) != 0) {
      if (errno != ENOENT) {
        PLOG(ERROR) << "Could not stat " << path;
        return -1;
      }
      return 0;
    }

    if (unlink(path) != 0) {
      if (errno != ENOENT) {
        PLOG(ERROR) << "Could not unlink " << path;
        return -1;
      }
    }
    return static_cast<int64_t>(file_stat.st_size);
  };

  // Delete the oat/odex file.
  int64_t return_value_oat = unlink_and_check(out_path);

  // Derive and delete the app image.
  int64_t return_value_art = unlink_and_check(GetAppImageFilename(out_path).c_str());

  // Derive and delete the vdex file.
  int64_t return_value_vdex = unlink_and_check(GetVdexFilename(out_path).c_str());

  // Report result.
  if (return_value_oat == -1 || return_value_art == -1 || return_value_vdex == -1) {
    return -1;
  } else {
    return return_value_oat + return_value_art + return_value_vdex;
  }
}

}  // namespace dex
}  // namespace tools
}  // namespace art
