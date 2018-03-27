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

#include <log/log_event_list.h>

#include "hidden_api.h"

#include "base/dumpable.h"

namespace art {
namespace hiddenapi {

static inline std::ostream& operator<<(std::ostream& os, AccessMethod value) {
  switch (value) {
    case kReflection:
      os << "reflection";
      break;
    case kJNI:
      os << "JNI";
      break;
    case kLinking:
      os << "linking";
      break;
  }
  return os;
}

static constexpr bool EnumsEqual(EnforcementPolicy policy, HiddenApiAccessFlags::ApiList apiList) {
  return static_cast<int>(policy) == static_cast<int>(apiList);
}

// GetMemberAction-related static_asserts.
static_assert(
    EnumsEqual(EnforcementPolicy::kAllLists, HiddenApiAccessFlags::kLightGreylist) &&
    EnumsEqual(EnforcementPolicy::kDarkGreyAndBlackList, HiddenApiAccessFlags::kDarkGreylist) &&
    EnumsEqual(EnforcementPolicy::kBlacklistOnly, HiddenApiAccessFlags::kBlacklist),
    "Mismatch between EnforcementPolicy and ApiList enums");
static_assert(
    EnforcementPolicy::kAllLists < EnforcementPolicy::kDarkGreyAndBlackList &&
    EnforcementPolicy::kDarkGreyAndBlackList < EnforcementPolicy::kBlacklistOnly,
    "EnforcementPolicy values ordering not correct");

namespace detail {

// This is the ID of the event log event. It is duplicated from
// system/core/logcat/event.logtags
constexpr int EVENT_LOG_TAG_art_hidden_api_access = 20004;

MemberSignature::MemberSignature(ArtField* field) {
  class_name_ = field->GetDeclaringClass()->GetDescriptor(&tmp_);
  member_name_ = field->GetName();
  type_signature_ = field->GetTypeDescriptor();
  type_ = Field;
  signature_parts_ = { class_name_, "->", member_name_, ":", type_signature_ };
}

MemberSignature::MemberSignature(ArtMethod* method) {
  class_name_ = method->GetDeclaringClass()->GetDescriptor(&tmp_);
  member_name_ = method->GetName();
  type_signature_ = method->GetSignature().ToString();
  type_ = Method;
  signature_parts_ = { class_name_, "->", member_name_, type_signature_ };
}

std::string MemberSignature::GetTypeStr() const {
  return type_ == Field ? "field" : "method";
}

bool MemberSignature::DoesPrefixMatch(const std::string& prefix) const {
  size_t pos = 0;
  for (const std::string& part : signature_parts_) {
    size_t count = std::min(prefix.length() - pos, part.length());
    if (prefix.compare(pos, count, part, 0, count) == 0) {
      pos += count;
    } else {
      return false;
    }
  }
  // We have a complete match if all parts match (we exit the loop without
  // returning) AND we've matched the whole prefix.
  return pos == prefix.length();
}

bool MemberSignature::IsExempted(const std::vector<std::string>& exemptions) {
  for (const std::string& exemption : exemptions) {
    if (DoesPrefixMatch(exemption)) {
      return true;
    }
  }
  return false;
}

void MemberSignature::Dump(std::ostream& os) const {
  for (std::string part : signature_parts_) {
    os << part;
  }
}

void MemberSignature::WarnAboutAccess(AccessMethod access_method,
                                      HiddenApiAccessFlags::ApiList list) {
  LOG(WARNING) << "Accessing hidden " << GetTypeStr() << " " << Dumpable<MemberSignature>(*this)
               << " (" << list << ", " << access_method << ")";
}

void MemberSignature::LogAccessToEventLog(AccessMethod access_method, Action action_taken) {
  uint32_t flags = 0;
  if (action_taken == kDeny) {
    flags |= AccessDenied;
  }
  if (type_ == Field) {
    flags |= MemberIsField;
  }
  if (Runtime::Current()->IsAotCompiler()) {
    flags |= IsCompiling;
  }
  android_log_event_list ctx(EVENT_LOG_TAG_art_hidden_api_access);
  ctx << access_method;
  ctx << flags;
  ctx << class_name_;
  ctx << member_name_;
  ctx << type_signature_;
  ctx << LOG_ID_EVENTS;
}

template<typename T>
bool ShouldBlockAccessToMemberImpl(T* member, Action action, AccessMethod access_method) {
  // Get the signature, we need it later.
  MemberSignature member_signature(member);

  Runtime* runtime = Runtime::Current();

  if (action == kDeny) {
    // If we were about to deny, check for an exemption first.
    // Exempted APIs are treated as light grey list.
    if (member_signature.IsExempted(runtime->GetHiddenApiExemptions())) {
      action = kAllowButWarn;
      // Avoid re-examining the exemption list next time.
      // Note this results in the warning below showing "light greylist", which
      // seems like what one would expect. Exemptions effectively add new members to
      // the light greylist.
      member->SetAccessFlags(HiddenApiAccessFlags::EncodeForRuntime(
              member->GetAccessFlags(), HiddenApiAccessFlags::kLightGreylist));
    }
  }

  // Print a log message with information about this class member access.
  // We do this regardless of whether we block the access or not.
  member_signature.WarnAboutAccess(access_method,
      HiddenApiAccessFlags::DecodeFromRuntime(member->GetAccessFlags()));

  if (kIsTargetBuild) {
    uint32_t eventLogSampleRate = runtime->GetHiddenApiEventLogSampleRate();
    if (eventLogSampleRate != 0 && (static_cast<uint32_t>(rand()) & 0xffff < eventLogSampleRate)) {
      member_signature.LogAccessToEventLog(access_method, action);
    }
  }

  if (action == kDeny) {
    // Block access
    return true;
  }

  // Allow access to this member but print a warning.
  DCHECK(action == kAllowButWarn || action == kAllowButWarnAndToast);

  // Depending on a runtime flag, we might move the member into whitelist and
  // skip the warning the next time the member is accessed.
  if (runtime->ShouldDedupeHiddenApiWarnings()) {
    member->SetAccessFlags(HiddenApiAccessFlags::EncodeForRuntime(
        member->GetAccessFlags(), HiddenApiAccessFlags::kWhitelist));
  }

  // If this action requires a UI warning, set the appropriate flag.
  if (action == kAllowButWarnAndToast || runtime->ShouldAlwaysSetHiddenApiWarningFlag()) {
    runtime->SetPendingHiddenApiWarning(true);
  }

  return false;
}

// Need to instantiate this.
template bool ShouldBlockAccessToMemberImpl<ArtField>(ArtField* member,
                                                      Action action,
                                                      AccessMethod access_method);
template bool ShouldBlockAccessToMemberImpl<ArtMethod>(ArtMethod* member,
                                                       Action action,
                                                       AccessMethod access_method);

}  // namespace detail
}  // namespace hiddenapi
}  // namespace art
