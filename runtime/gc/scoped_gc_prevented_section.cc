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

#include "scoped_gc_prevented_section.h"

#include "gc/collector_type.h"
#include "gc/heap.h"
#include "runtime.h"
#include "thread-current-inl.h"

namespace art {
namespace gc {

ScopedGCPreventedSection::ScopedGCPreventedSection(Thread* self,
                                                   GcCause cause,
                                                   CollectorType collector_type)
    : self_(self) {
  successful_ = Runtime::Current()->GetHeap()->TryStartGC(self, cause, collector_type);
  if (self != nullptr) {
    old_cause_ = self->StartAssertNoThreadSuspension("ScopedGCCriticalSection");
  }
}
ScopedGCPreventedSection::~ScopedGCPreventedSection() {
  if (self_ != nullptr) {
    self_->EndAssertNoThreadSuspension(old_cause_);
  }
  if (successful_) {
    Runtime::Current()->GetHeap()->FinishGC(self_, collector::kGcTypeNone);
  }
}

}  // namespace gc
}  // namespace art
