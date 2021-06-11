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
#include <vector>

namespace art {
namespace tools {

/*
 * This class contains the canonical names of various ART related artifact directories.  By default
 * it will initialize itself using the environmental variables ANDROID_DATA and ANDROID_ROOT.  The
 * contained paths can be re-computed using a provided data and root directory paths using the
 * InitializeFromDataAndRoot method.
 */
class Paths {
 public:
  Paths();

  // Name of the environment variable that contains the asec mountpoint.
  static constexpr const char* ASEC_MOUNTPOINT_ENV_NAME = "ASEC_MOUNTPOINT";

  std::string AndroidAppDir;
  std::string AndroidAppEphemeralDir;
  std::string AndroidAppLibDir;
  std::string AndroidAppPrivateDir;
  std::string AndroidAsecDir;
  std::string AndroidDataDir;
  std::string AndroidMediaDir;
  std::string AndroidMntExpandDir;
  std::string AndroidProfilesDir;
  std::string AndroidRootDir;
  std::string AndroidStagingDir;

  std::vector<std::string> AndroidSystemDirs;

  bool InitializeFromDataAndRoot();
  bool InitializeFromDataAndRoot(const std::string& data, const std::string& root);
};

extern Paths paths;

}  // namespace tools
}  // namespace art
