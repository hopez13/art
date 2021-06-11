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

/*
 * Exported Functions
 */

int64_t DeleteOdex(const std::string& apk_path,
                   const std::string& instruction_set,
                   const std::optional<const std::string>& oat_dir) {
  // Validate input
  CHECK(IsAbsoluteLocation(apk_path));
  if (oat_dir.has_value()) {
    CHECK(IsAbsoluteLocation(oat_dir.value()));
    CHECK(utils::ValidateApkPath(oat_dir.value()));
  }
  CHECK_NE(GetInstructionSetFromString(instruction_set.c_str()), InstructionSet::kNone);

  const std::string& oat_path =
      GetDexArtifactPath(oat_dir, apk_path, instruction_set, oat_dir.has_value() ? "odex" : "dex");

  LOG(VERBOSE) << "Oat file path: " << oat_path;

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
  int64_t return_value_oat = unlink_and_check(oat_path.c_str());

  // Derive and delete the vdex file.
  int64_t return_value_vdex = unlink_and_check(GetVdexFilename(oat_path).c_str());

  // Derive and delete the app image.
  int64_t return_value_art = unlink_and_check(GetAppImageFilename(oat_path).c_str());

  // Report result.
  if (return_value_oat == -1 || return_value_vdex == -1 || return_value_art == -1) {
    return -1;
  } else {
    LOG(VERBOSE) << "OAT bytes freed: " << return_value_oat;
    LOG(VERBOSE) << "VDEX bytes freed: " << return_value_vdex;
    LOG(VERBOSE) << "AppImage bytes freed: " << return_value_art;

    return return_value_oat + return_value_vdex + return_value_art;
  }
}

// TODO (b/177273468): Reconcile with the API present in file_utils.h and move there.
///     (e.g. GetDalvikCacheFilename)
std::string GetDalvikCacheDexArtifactPath(const std::string& apk_path,
                                          const std::string& instruction_set,
                                          const std::string& type) {
  std::string dalvik_cache_key = apk_path;
  std::replace(dalvik_cache_key.begin() + 1, dalvik_cache_key.end(), '/', '@');

  // The / between the runtime ISA and cache key is provided by the leading / in the dex
  // path.
  return ::art::tools::paths.AndroidDataDir + DALVIK_CACHE + '/' + instruction_set +
         dalvik_cache_key + "@classes." + type;
}

std::string GetDexArtifactPath(const std::optional<std::string>& oat_dir,
                               const std::string& apk_path,
                               const std::string& instruction_set,
                               const std::string& type) {
  if (oat_dir == std::nullopt) {
    return GetDalvikCacheDexArtifactPath(apk_path, instruction_set, type);
  } else {
    return GetPrimaryDexArtifactPath(oat_dir.value(), apk_path, instruction_set, type);
  }
}

std::string GetPrimaryDexArtifactPath(const std::string& oat_dir,
                                      const std::string& apk_path,
                                      const std::string& instruction_set,
                                      const std::string& type) {
  std::string::size_type name_end = apk_path.rfind('.');
  std::string::size_type name_start = apk_path.rfind('/');
  return std::string(oat_dir) + '/' + instruction_set + '/' +
         apk_path.substr(name_start + 1, name_end - name_start) + type;
}

}  // namespace dex
}  // namespace tools
}  // namespace art
