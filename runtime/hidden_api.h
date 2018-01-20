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
#include "reflection.h"
#include "runtime.h"

namespace art {
namespace hiddenapi {

inline bool ShouldEnforceBasedOnCallerFromStackWalk(Thread* self, size_t num_frames)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (!Runtime::Current()->AreHiddenApiChecksEnabled()) {
    return false;
  }

  ObjPtr<mirror::Class> klass = GetCallingClass(self, num_frames);
  if (klass == nullptr) {
    // Unattached native thread. Assume that this is *not* boot class path.
    return true;
  }
  return !klass->IsBootStrapClassLoaded();
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

// Print a warning and set the warning flag if this field is greylisted.
// Note that this function will abort if the field is blacklisted because non-
// boot class path code should not be able to acquire a handle to the field.
inline void MaybeWarnAboutFieldAccess(ArtField* field, bool should_enforce)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (should_enforce &&
      field != nullptr &&
      ShouldWarnAboutMember(field->GetAccessFlags())) {
    Runtime::Current()->SetPendingHiddenApiWarning(true);
    LOG(WARNING) << "Access to hidden field " << field->PrettyField();
  }
}

// Print a warning and set the warning flag if this method is greylisted.
// Note that this function will abort if the method is blacklisted because non-
// boot class path code should not be able to acquire a handle to the method.
inline void MaybeWarnAboutMethodInvocation(ArtMethod* method, bool should_enforce)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (should_enforce &&
      method != nullptr &&
      ShouldWarnAboutMember(method->GetAccessFlags())) {
    Runtime::Current()->SetPendingHiddenApiWarning(true);
    LOG(WARNING) << "Access to hidden method " << method->PrettyMethod();
  }
}

}  // namespace hiddenapi
}  // namespace art

#endif  // ART_RUNTIME_HIDDEN_API_H_
