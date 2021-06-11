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

#include <android-base/logging.h>
#include <sys/stat.h>

#include <fstream>
#include <random>
#include <string>

namespace art {
namespace tools {
namespace tests {

#if defined(__arm__)
const std::string kRuntimeIsa = "arm";
#elif defined(__aarch64__)
const std::string kRuntimeIsa = "arm64";
#elif defined(__i386__)
const std::string kRuntimeIsa = "x86";
#elif defined(__x86_64__)
const std::string kRuntimeIsa = "x86_64";
#else
const std::string kRuntimeIsa = "none";
#endif

int MkDir(const std::string& path, uid_t owner, gid_t group, mode_t mode) {
  int ret = ::mkdir(path.c_str(), mode);
  if (ret != 0) {
    return ret;
  }
  ret = ::chown(path.c_str(), owner, group);
  if (ret != 0) {
    return ret;
  }
  return ::chmod(path.c_str(), mode);
}

void RunCmd(const std::string& cmd) {
  if (system(cmd.c_str()) == -1) {
    LOG(ERROR) << "Error executing system command: " << strerror(errno);
  }
}

void WriteRandomData(const std::string& path, size_t num_bytes) {
  std::random_device rd;
  std::uniform_int_distribution<uint8_t> dist(0, UINT8_MAX);
  std::ofstream file(path, std::ios::binary);

  while (num_bytes-- > 0) {
    file << dist(rd);
  }
}

/*
 * Cloned Functions
 *
 * These must be kept in sync with the implementations in libcutils/multiuser.cpp
 */

uid_t multiuser_get_uid(userid_t user_id, appid_t app_id) {
  return (user_id * AID_USER_OFFSET) + (app_id % AID_USER_OFFSET);
}

gid_t multiuser_get_shared_gid(userid_t, appid_t app_id) {
  if (app_id >= AID_APP_START && app_id <= AID_APP_END) {
    return (app_id - AID_APP_START) + AID_SHARED_GID_START;
  } else if (app_id >= AID_ROOT && app_id <= AID_APP_START) {
    return app_id;
  } else {
    return -1;
  }
}

}  // namespace tests
}  // namespace tools
}  // namespace art