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

#define LOG_TAG "artd"

#include <unistd.h>

#include <binder/BinderService.h>

#include "android/os/BnArtd.h"
#include "base/logging.h"
#include "base/macros.h"
#include "tools/tools.h"

namespace android {
namespace artd {
class Artd : public BinderService<Artd>, public os::BnArtd {
 public:
  Artd(ATTRIBUTE_UNUSED const int argc, ATTRIBUTE_UNUSED char* argv[]) {}

  /*
   * Binder API
   */

  binder::Status buildTest() {
    return binder::Status::ok();
  }

  /*
   * Server API
   */

  NO_RETURN
  void Run() {
    LOG(DEBUG) << "Starting artd";

    while (true) {
      // This is a scaffolding CL.  This sleep is intended to keep the process
      // alive for testing without it using too many system resources.  This
      // will be removed and replaced with a server loop in a followup CL.
      sleep(5);
    }
  }
};

}  // namespace artd
}  // namespace android

int main(const int argc, char* argv[]) {
  setenv("ANDROID_LOG_TAGS", "*:v", 1);
  android::base::InitLogging(argv);

  android::artd::Artd artd(argc, argv);

  artd.Run();
}
