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

#ifndef ART_RUNTIME_GC_SCOPED_GC_PREVENTED_SECTION_H_
#define ART_RUNTIME_GC_SCOPED_GC_PREVENTED_SECTION_H_

#include "base/mutex.h"
#include "collector_type.h"
#include "gc_cause.h"

namespace art {

class Thread;

namespace gc {

// Try to prevent the GC from running in the section. Call IsGcBlocked to determine if GC has been
// blocked. Will not wait for a GC in progress to finish. Use ScopedGCCriticalSection if that is
// required.
class ScopedGCPreventedSection {
 public:
  ScopedGCPreventedSection(Thread* self, GcCause cause, CollectorType collector_type)
      ACQUIRE(Roles::uninterruptible_);

  ~ScopedGCPreventedSection() RELEASE(Roles::uninterruptible_);

  bool IsGcBlocked() const {
    return successful_;
  }

 private:
  Thread* const self_;
  bool successful_;
  const char* old_cause_;
};

}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_SCOPED_GC_PREVENTED_SECTION_H_
