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
#include "reflection-inl.h"
#include "runtime.h"

namespace art {
namespace hiddenapi {

inline bool ShouldEnforceBasedOnCallerFromStackWalk(Thread* self, size_t num_frames)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  return Runtime::Current()->AreHiddenApiChecksEnabled() &&
         !IsCallerInBootClassPath(self, num_frames);
}

inline bool ShouldEnforceBasedOnCallerMethod(ArtMethod* caller)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  DCHECK(!caller->IsRuntimeMethod());
  return Runtime::Current()->AreHiddenApiChecksEnabled() &&
         !caller->GetDeclaringClass()->IsBootStrapClassLoaded();
}

inline bool IsMemberHidden(uint32_t access_flags) {
  switch (HiddenApiAccessFlags::DecodeFromRuntime(access_flags)) {
    case HiddenApiAccessFlags::kWhitelist:
    case HiddenApiAccessFlags::kLightGreylist:
    case HiddenApiAccessFlags::kDarkGreylist:
      return false;
    case HiddenApiAccessFlags::kBlacklist:
      return true;
  }
}

inline bool ShouldWarnAboutMember(uint32_t access_flags) {
  switch (HiddenApiAccessFlags::DecodeFromRuntime(access_flags)) {
    case HiddenApiAccessFlags::kWhitelist:
      return false;
    case HiddenApiAccessFlags::kLightGreylist:
    case HiddenApiAccessFlags::kDarkGreylist:
      return true;
    case HiddenApiAccessFlags::kBlacklist:
      LOG(FATAL) << "Should not be allowed to access member";
      UNREACHABLE();
  }
}

inline void MaybeWarnAboutFieldAccess(ArtField* field, bool should_enforce)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (should_enforce &&
      field != nullptr &&
      ShouldWarnAboutMember(field->GetAccessFlags())) {
    Runtime::Current()->SetPendingHiddenApiWarning(true);
    LOG(ERROR) << "Access to hidden field " << field->PrettyField();
  }
}

inline void MaybeWarnAboutMethodInvocation(ArtMethod* method, bool should_enforce)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (should_enforce &&
      method != nullptr &&
      ShouldWarnAboutMember(method->GetAccessFlags())) {
    Runtime::Current()->SetPendingHiddenApiWarning(true);
    LOG(ERROR) << "Access to hidden method " << method->PrettyMethod();
  }
}

}  // namespace hiddenapi
}  // namespace art

#endif  // ART_RUNTIME_HIDDEN_API_H_
