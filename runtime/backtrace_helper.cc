/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "backtrace_helper.h"

#include <unwind.h>

namespace art {

static _Unwind_Reason_Code CollectCallback(_Unwind_Context* context, void* arg) {
  auto* const state = reinterpret_cast<BacktraceCollector*>(arg);
  const uintptr_t ip = _Unwind_GetIP(context);
  // The first stack frame is get_backtrace itself. Skip it.
  if (ip != 0 && state->skip_count_ > 0) {
    --state->skip_count_;
    return _URC_NO_REASON;
  }
  // ip may be off for ARM but it shouldn't matter since we only use it for hashing.
  state->out_frames_[state->num_frames_] = ip;
  state->num_frames_++;
  return state->num_frames_ >= state->max_depth_ ? _URC_END_OF_STACK : _URC_NO_REASON;
}

void BacktraceCollector::Collect() {
  _Unwind_Backtrace(&CollectCallback, this);
}

}  // namespace art
