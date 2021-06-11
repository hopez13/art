/*
** Copyright 2021, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#include <string>
#define LOG_TAG "artd"

#include <android-base/stringprintf.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <android/binder_status.h>
#include <base/logging.h>
#include <base/macros.h>
#include <unistd.h>
#include <utils/Errors.h>
#include <utils/String8.h>

#include <mutex>

#include "aidl/android/os/BnArtd.h"
#include "art-tools/dex.h"
#include "art-tools/utils.h"

using ::android::base::StringPrintf;
using ::ndk::ScopedAStatus;

#define ENFORCE_UID(uid)                    \
  {                                         \
    ScopedAStatus status = CheckUid((uid)); \
    if (!status.isOk()) {                   \
      return status;                        \
    }                                       \
  }

#define CHECK_ARGUMENT_PATH(path)                     \
  {                                                   \
    ScopedAStatus status = CheckArgumentPath((path)); \
    if (!status.isOk()) {                             \
      return status;                                  \
    }                                                 \
  }

namespace art {
namespace artd {

/*
 * Helper Functions
 */

static ScopedAStatus Exception(uint32_t code, const std::string& msg) {
  LOG(ERROR) << msg << " (" << code << ")";
  return ScopedAStatus::fromExceptionCodeWithMessage(code, msg.c_str());
}

static ScopedAStatus CheckUid(uid_t expectedUid) {
  uid_t uid = AIBinder_getCallingUid();
  if (uid == expectedUid || uid == AID_ROOT) {
    return ScopedAStatus::ok();
  } else {
    return Exception(EX_SECURITY, StringPrintf("UID %d is not expected UID %d", uid, expectedUid));
  }
}

/*
 * Checks to ensure that the path:
 * 1. Exists
 * 2. Is absolute
 * 3. Doesn't contain any `..` directories
 * 4. Doesn't contain any illegal characters
 */
static ScopedAStatus CheckArgumentPath(const std::string& path) {
  if (path.empty()) {
    return Exception(EX_ILLEGAL_ARGUMENT, "Missing path");
  }
  if (path[0] != '/') {
    return Exception(EX_ILLEGAL_ARGUMENT, StringPrintf("Path %s is relative", path.c_str()));
  }
  if ((path + '/').find("/../") != std::string::npos) {
    return Exception(EX_ILLEGAL_ARGUMENT, StringPrintf("Path %s is shady", path.c_str()));
  }
  for (const char& c : path) {
    if (c == '\0' || c == '\n') {
      return Exception(EX_ILLEGAL_ARGUMENT, StringPrintf("Path %s is malformed", path.c_str()));
    }
  }
  return ScopedAStatus::ok();
}

class Artd : public aidl::android::os::BnArtd {
  constexpr static const char* const SERVICE_NAME = "artd";

  std::recursive_mutex mLock;

 public:
  Artd() {}

  /*
   * Binder API
   */

  ScopedAStatus deleteOdex(const std::string& apkPath,
                           const std::string& instructionSet,
                           const std::optional<std::string>& outputPath,
                           int64_t* _aidl_return) {
    ENFORCE_UID(AID_SYSTEM);
    CHECK_ARGUMENT_PATH(apkPath);
    if (outputPath) {
      CHECK_ARGUMENT_PATH(outputPath.value());
    }

    // We need to acquire the lock to ensure that no operations are performed on the optimization
    // artifacts we are deleting.
    std::lock_guard<std::recursive_mutex> lock(mLock);

    *_aidl_return = art::tools::dex::DeleteOdex(apkPath, instructionSet, outputPath);
    return *_aidl_return == -1 ? ScopedAStatus::fromStatus(STATUS_UNKNOWN_ERROR) :
                                 ScopedAStatus::ok();
  }

  ScopedAStatus isAlive(bool* _aidl_return) {
    *_aidl_return = true;
    return ScopedAStatus::ok();
  }

  /*
   * Server API
   */

  ScopedAStatus Start() {
    LOG(INFO) << "Starting artd";

    android::status_t ret = AServiceManager_addService(this->asBinder().get(), SERVICE_NAME);
    if (ret != android::OK) {
      return ScopedAStatus::fromStatus(ret);
    }

    ABinderProcess_startThreadPool();

    return ScopedAStatus::ok();
  }
};

}  // namespace artd
}  // namespace art

int main(const int argc __attribute__((unused)), char* argv[]) {
  setenv("ANDROID_LOG_TAGS", "*:v", 1);
  android::base::InitLogging(argv);

  art::artd::Artd artd;

  if (auto ret = artd.Start(); !ret.isOk()) {
    LOG(ERROR) << "Unable to start artd: " << ret.getMessage();
    exit(1);
  }

  ABinderProcess_joinThreadPool();

  LOG(INFO) << "artd shutting down";

  return 0;
}
