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

#include "call_stack_tracker.h"

#include <sstream>

#include "base/mutex.h"
#include "native_stack_dump.h"
#include "thread-inl.h"

namespace art {

void CallStackTracker::CallStack::Dump(std::ostream& os) const {
  os << name << "\n" << pretty_backtrace << "\n";
}

CallStackTracker::CallStackTracker()
    : lock_(new Mutex("Access lock", kCallStackTrackerLock)) {}

void CallStackTracker::Record(std::string&& name, size_t skip_frames) {
  Thread* const self = Thread::Current();
  CallStack stack;
  stack.name = std::move(name);
  stack.frames.Collect(skip_frames);
  // Since the last frames is likely to be in the oat file, remove it to reduce the number of
  // unique stack traces.
  if (stack.frames.NumFrames() > 0) {
    stack.frames.PopFrame();
  }
  MutexLock mu(self, *lock_);
  auto it = call_stacks_.find(stack);
  if (it == call_stacks_.end()) {
    std::ostringstream oss;
    DumpNativeStack(oss, self->GetTid(), nullptr, nullptr, nullptr, nullptr, skip_frames);
    stack.pretty_backtrace = oss.str();
    it = call_stacks_.emplace(std::move(stack), 0u).first;
  }
  ++it->second;
}

void CallStackTracker::Dump(std::ostream& os) {
  Thread* const self = Thread::Current();
  std::vector<std::pair<size_t, const CallStack*>> pairs;
  MutexLock mu(self, *lock_);
  for (const auto& pair : call_stacks_) {
    pairs.emplace_back(pair.second, &pair.first);
  }
  std::sort(pairs.rbegin(), pairs.rend());
  for (auto&& pair : pairs) {
    os << pair.first << ": ";
    pair.second->Dump(os);
  }
}

}  // namespace art
