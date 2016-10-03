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

#include "art_method-inl.h"
#include "base/logging.h"
#include "entrypoints/entrypoint_utils.h"
#include "mirror/object-inl.h"
#include "scoped_thread_state_change.h"
#include "thread.h"

namespace art {

// Used by the JNI dlsym stub to find the native method to invoke if none is registered.
#if defined(__arm__) || defined(__aarch64__)
extern "C" void* artFindNativeMethod() {
  Thread* self = Thread::Current();
#else
extern "C" void* artFindNativeMethod(Thread* self) {
  DCHECK_EQ(self, Thread::Current());
#endif

  bool mutator_lock_is_shared_held;
  bool mutator_lock_is_exclusive_held;

  if (kIsDebugBuild) {
    // Check the status of the mutator lock before SOA, it will re-acquire lock if not held.
    mutator_lock_is_shared_held = Locks::mutator_lock_->IsSharedHeld(self);
    mutator_lock_is_exclusive_held = Locks::mutator_lock_->IsExclusiveHeld(self);
  }

  ScopedObjectAccess soa(self);

  ArtMethod* method = self->GetCurrentMethod(nullptr);
  DCHECK(method != nullptr);

  if (kIsDebugBuild) {
    bool is_fast = method->IsAnnotatedWithFastNative();
    bool is_critical = method->IsAnnotatedWithCriticalNative();

    if (!is_fast && !is_critical) {
      // Normal JNI releases mutator lock before calling down.
      CHECK_EQ(false, mutator_lock_is_shared_held) << PrettyMethod(method);
      CHECK_EQ(false, mutator_lock_is_exclusive_held) << PrettyMethod(method);
    } else {
      // @FastNative and @CriticalNative retain the shared mutator lock.
      CHECK(mutator_lock_is_shared_held) << PrettyMethod(method);
    }
  }

  // Lookup symbol address for method, on failure we'll return null with an exception set,
  // otherwise we return the address of the method we found.
  void* native_code = soa.Vm()->FindCodeForNativeMethod(method);
  if (native_code == nullptr) {
    DCHECK(self->IsExceptionPending());
    return nullptr;
  } else {
    // Register so that future calls don't come here
    method->RegisterNative(native_code, false);
    return native_code;
  }
}

}  // namespace art
