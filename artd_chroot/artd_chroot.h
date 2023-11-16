/*
 * Copyright (C) 2023 The Android Open Source Project
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

#ifndef ART_ARTD_CHROOT_ARTD_CHROOT_H_
#define ART_ARTD_CHROOT_ARTD_CHROOT_H_

#include "aidl/com/android/server/art/BnArtdChroot.h"
#include "android-base/result.h"
#include "android-base/thread_annotations.h"

namespace art {
namespace artd_chroot {

class ArtdChroot : public aidl::com::android::server::art::BnArtdChroot {
 public:
  ndk::ScopedAStatus setUp() override;

  ndk::ScopedAStatus tearDown() override;

  android::base::Result<void> Start();

 private:
  android::base::Result<void> SetUpChroot() const REQUIRES(mu_);

  android::base::Result<void> TearDownChroot() const REQUIRES(mu_);

  std::mutex mu_;
};

}  // namespace artd_chroot
}  // namespace art

#endif  // ART_ARTD_CHROOT_ARTD_CHROOT_H_
