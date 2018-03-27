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

namespace art {
namespace hiddenapi {

constexpr int EVENT_LOG_TAG_art_hidden_api_access = 20004;

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

}  // namespace hiddenapi
}  // namespace art
