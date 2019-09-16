/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include "reflective_handle_scope-inl.h"

#include "base/locks.h"
#include "base/mutex-inl.h"
#include "mirror/class.h"
#include "mirror/object-inl.h"

namespace art {

void HeapReflectiveSourceInfo::Describe(std::ostream& os) const {
  Locks::mutator_lock_->AssertExclusiveHeld(Thread::Current());
  ReflectionSourceInfo::Describe(os);
  os << " Type=" << src_->GetClass()->PrettyClass();
}

}  // namespace art