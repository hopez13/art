/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef ART_RUNTIME_REFLECTION_CALLER_VISITOR_H_
#define ART_RUNTIME_REFLECTION_CALLER_VISITOR_H_

#include "art_method.h"
#include "base/mutex.h"
#include "stack.h"

namespace art {
class Thread;

// Walks up the stack 'n' callers, when used with Thread::WalkStack.
struct ReflectionCallerVisitor : public StackVisitor {
  explicit ReflectionCallerVisitor(Thread* thread)
      : StackVisitor(thread, nullptr, StackVisitor::StackWalkKind::kIncludeInlinedFrames),
        caller(nullptr) {}

  bool VisitFrame() REQUIRES_SHARED(Locks::mutator_lock_) {
    ArtMethod *m = GetMethod();
    if (m != nullptr && m->GetDeclaringClass()->IsClassClass()) {
      return true;
    } else {
      caller = m;
      return false;
    }
  }

  ArtMethod* caller;
};

}  // namespace art

#endif  // ART_RUNTIME_REFLECTION_CALLER_VISITOR_H_
