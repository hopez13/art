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
#include "installd_constants.h"
#include "tools/tools.h"

namespace {
class Artd : public BinderService<Artd>, public os::BnArtd {
 public:
  Artd(ATTRIBUTE_UNUSED const int argc, ATTRIBUTE_UNUSED char* argv[]) {}

  NO_RETURN
  void Run() {
    LOG(DEBUG) << "Hello from ARTD";

    while (true) {
      sleep(5);
    }
  }
};

}  // namespace

int main(const int argc, char* argv[]) {
  setenv("ANDROID_LOG_TAGS", "*:v", 1);
  android::base::InitLogging(argv);

  Artd artd(argc, argv);

  artd.Run();
}
