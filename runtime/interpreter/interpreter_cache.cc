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

#include "interpreter_cache.h"
#include "thread-inl.h"

namespace art {

std::array<std::atomic<InterpreterCache::Entry>,
           InterpreterCache::kSharedSize> InterpreterCache::shared_array_;

InterpreterCache::InterpreterCache(Thread* owning_thread) : owning_thread_(owning_thread) {
  // We can not use the ClearThreadLocal() method since the constructor will not
  // be called from the owning thread.
  thread_local_array_.fill(Entry{});
}

bool InterpreterCache::IsAccessibleFromCurrentThread() {
  return Thread::Current() == owning_thread_ || owning_thread_->IsSuspended();
}

}  // namespace art
