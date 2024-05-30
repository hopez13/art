/*
 * Copyright 2024 The Android Open Source Project
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

#ifndef ART_RUNTIME_JIT_JIT_CODE_CACHE_INL_H_
#define ART_RUNTIME_JIT_JIT_CODE_CACHE_INL_H_

#include "jit/jit_code_cache.h"

#include "base/macros.h"
#include "mirror/method_type.h"
#include "mirror/object.h"
#include "mirror/object_reference.h"
#include "read_barrier.h"
#include "thread.h"
#include "well_known_classes-inl.h"

namespace art HIDDEN {

class ArtMethod;

namespace jit {

template<typename RootVisitorType>
EXPORT void JitCodeCache::VisitRootTables(ArtMethod* method, RootVisitorType& visitor) {
  if (method->IsNative()) {
    return;
  }

  Thread* self = Thread::Current();
  ScopedDebugDisallowReadBarriers sddrb(self);
  MutexLock mu(self, *Locks::jit_lock_);

  auto it = method_types_.find(method);

  if (it == method_types_.end()) {
    return;
  }

  for (auto method_type : it->second) {
    auto ref = mirror::CompressedReference<mirror::Object>::FromMirrorPtr(method_type);
    visitor.VisitRoot(&ref);
  }
}

}  // namespace jit
}  // namespace art

#endif  // ART_RUNTIME_JIT_JIT_CODE_CACHE_INL_H_


