/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef ART_RUNTIME_MIRROR_OBJECT_POINTER_H_
#define ART_RUNTIME_MIRROR_OBJECT_POINTER_H_

#include "base/mutex.h"  // For Locks::mutator_lock_.
#include "globals.h"

namespace art {
namespace mirror {

class Object;

// Value type representing a pointer to a mirror::Object of type MirrorType
// Pass kPoison as a template boolean for testing in non-debug builds.
// Note that the function are not 100% thread safe and may have spurious check false positive check
// passes in these cases.
template<class MirrorType, bool kPoison = kIsDebugBuild>
class ObjPtr {
  static const size_t kCookieShift = (32 - kObjectAlignmentShift);

 public:
  ALWAYS_INLINE ObjPtr() REQUIRES_SHARED(Locks::mutator_lock_) : reference_(0u) {}

  ALWAYS_INLINE explicit ObjPtr(MirrorType* ptr) REQUIRES_SHARED(Locks::mutator_lock_)
      : reference_(Encode(ptr)) {}

  ALWAYS_INLINE explicit ObjPtr(const ObjPtr& other) REQUIRES_SHARED(Locks::mutator_lock_)
      = default;

  ALWAYS_INLINE ObjPtr& operator=(const ObjPtr& other) {
    reference_ = other.reference_;
  }

  ALWAYS_INLINE void Assign(MirrorType* ptr) REQUIRES_SHARED(Locks::mutator_lock_) {
    reference_ = Encode(ptr);
  }

  ALWAYS_INLINE MirrorType* operator->() const REQUIRES_SHARED(Locks::mutator_lock_) {
    return Get();
  }

  ALWAYS_INLINE MirrorType* Get() const REQUIRES_SHARED(Locks::mutator_lock_) {
    return Decode();
  }

  ALWAYS_INLINE bool IsNull() const {
    return reference_ == 0;
  }

  ALWAYS_INLINE bool IsValid() const REQUIRES_SHARED(Locks::mutator_lock_) {
    if (!kPoison || IsNull()) {
      return true;
    }
    return GetCookie() == TrimCookie(Thread::Current()->GetPoisonObjectCookie());
  }

  ALWAYS_INLINE void AssertValid() const REQUIRES_SHARED(Locks::mutator_lock_) {
    if (kPoison) {
      CHECK(IsValid()) << "Invalid cookie, expected "
          << TrimCookie(Thread::Current()->GetPoisonObjectCookie()) << " but got " << GetCookie();
    }
  }

 private:
  // Trim off high bits of thread local cookie.
  ALWAYS_INLINE static uintptr_t TrimCookie(uintptr_t cookie) {
    return (cookie << kCookieShift) >> kCookieShift;
  }

  ALWAYS_INLINE uintptr_t GetCookie() const {
    return reference_ >> kCookieShift;
  }

  static uintptr_t Encode(MirrorType* ptr) REQUIRES_SHARED(Locks::mutator_lock_) {
    uintptr_t ref = reinterpret_cast<uintptr_t>(ptr);
    if (kPoison && ref != 0) {
      ref >>= kObjectAlignmentShift;
      // Put cookie in high bits.
      Thread* self = Thread::Current();
      DCHECK(self != nullptr);
      ref |= self->GetPoisonObjectCookie() << kCookieShift;
    }
    return ref;
  }

  // Decode makes sure that the object pointer is valid.
  MirrorType* Decode() const REQUIRES_SHARED(Locks::mutator_lock_) {
    AssertValid();
    return reinterpret_cast<MirrorType*>(
        static_cast<uintptr_t>(static_cast<uint32_t>(reference_ << kObjectAlignmentShift)));
  }

  // The encoded reference and cookie.
  uintptr_t reference_;
};


}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_OBJECT_POINTER_H_
