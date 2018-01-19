/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef ART_RUNTIME_HIDDEN_API_H_
#define ART_RUNTIME_HIDDEN_API_H_

#include "hidden_api_access_flags.h"
#include "runtime.h"

namespace art {

class HiddenApi {
 public:
  static ALWAYS_INLINE bool IsMemberHidden(bool caller_in_boot, uint32_t access_flags) {
    if (caller_in_boot || !Runtime::Current()->AreHiddenApiChecksEnabled()) {
      return false;
    }

    switch (HiddenApiAccessFlags::DecodeFromRuntime(access_flags)) {
      case HiddenApiAccessFlags::kWhitelist:
      case HiddenApiAccessFlags::kLightGreylist:
      case HiddenApiAccessFlags::kDarkGreylist:
        return false;
      case HiddenApiAccessFlags::kBlacklist:
        return true;
    }
  }
};

}  // namespace art


#endif  // ART_RUNTIME_HIDDEN_API_H_
