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
#include <base/logging.h>
#include <base/strlcpy.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <string>

#include "globals.h"
#include "utils.h"

using android::base::EndsWith;

namespace art {
namespace tools {
namespace dex {

/*
 * File Local Functions
 */

static bool is_absolute_path(const std::string& path) {
  if (path.find('/') != 0 || path.find("..") != std::string::npos) {
    LOG(ERROR) << "Invalid absolute path " << path;
    return false;
  } else {
    return true;
  }
}

static bool is_valid_instruction_set(const std::string& instruction_set) {
  // TODO: add explicit allowlisting of instruction sets
  if (instruction_set.find('/') != std::string::npos) {
    LOG(ERROR) << "Invalid instruction set " << instruction_set;
    return false;
  } else {
    return true;
  }
}

bool calculate_oat_file_path(char path[PKG_PATH_MAX],
                             const std::string& oat_dir,
                             const std::string& apk_path,
                             const std::string& instruction_set) {
  if (!is_absolute_path(oat_dir))
    return false;
  if (!is_absolute_path(apk_path))
    return false;
  if (!is_valid_instruction_set(instruction_set))
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
  if (!is_absolute_path(src))
    return false;
  if (!is_valid_instruction_set(instruction_set))
    return false;

  std::string dalvik_cache_key = src;
  std::replace(dalvik_cache_key.begin() + 1, dalvik_cache_key.end(), '/', '@');

  std::string res = android_data_dir + DALVIK_CACHE + '/' + instruction_set + dalvik_cache_key +
                    DALVIK_CACHE_POSTFIX;

  if (res.length() >= PKG_PATH_MAX) {
    LOG(ERROR) << "Result too large";
    return false;
  } else {
    strlcpy(path, res.c_str(), PKG_PATH_MAX);
    return true;
  }
}

static std::string replace_file_extension(const std::string& oat_path, const std::string& new_ext) {
  // A standard dalvik-cache entry. Replace ".dex" with `new_ext`.
  if (EndsWith(oat_path, ".dex")) {
    std::string new_path = oat_path;
    new_path.replace(new_path.length() - strlen(".dex"), strlen(".dex"), new_ext);
    CHECK(EndsWith(new_path, new_ext));
    return new_path;
  }

  // An odex entry. Not that this may not be an extension, e.g., in the OTA
  // case (where the base name will have an extension for the B artifact).
  size_t odex_pos = oat_path.rfind(".odex");
  if (odex_pos != std::string::npos) {
    std::string new_path = oat_path;
    new_path.replace(odex_pos, strlen(".odex"), new_ext);
    CHECK_NE(new_path.find(new_ext), std::string::npos);
    return new_path;
  }

  // Don't know how to handle this.
  return "";
}

// Translate the given oat path to an art (app image) path. An empty string
// denotes an error.
static std::string create_image_filename(const std::string& oat_path) {
  return replace_file_extension(oat_path, ".art");
}

// Translate the given oat path to a vdex path. An empty string denotes an error.
static std::string create_vdex_filename(const std::string& oat_path) {
  return replace_file_extension(oat_path, ".vdex");
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
    if (!is_secondary_dex && utils::validate_apk_path(oat_dir.value())) {
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

int64_t delete_odex(const std::string& apk_path,
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

  LOG(INFO) << "OAT/ODEX File Path: " << out_path;

  // Delete the oat/odex file.
  int64_t return_value_oat = unlink_and_check(out_path);

  // Derive and delete the app image.
  int64_t return_value_art = unlink_and_check(create_image_filename(out_path).c_str());

  // Derive and delete the vdex file.
  int64_t return_value_vdex = unlink_and_check(create_vdex_filename(out_path).c_str());

  // Report result.
  if (return_value_oat == -1 || return_value_art == -1 || return_value_vdex == -1) {
    return -1;
  } else {
    LOG(INFO) << "OAT size freed: " << return_value_oat;
    LOG(INFO) << "ART size freed: " << return_value_art;
    LOG(INFO) << "VDEX size freed: " << return_value_vdex;

    return return_value_oat + return_value_art + return_value_vdex;
  }
}

}  // namespace dex
}  // namespace tools
}  // namespace art
