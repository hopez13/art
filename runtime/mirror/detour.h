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

#ifndef ART_RUNTIME_MIRROR_DETOUR_H_
#define ART_RUNTIME_MIRROR_DETOUR_H_

#include "accessible_object.h"
#include "gc_root.h"
#include "object.h"
#include "object_callbacks.h"
#include "read_barrier_option.h"

namespace art {

struct DetourOffsets;
struct ArtDetour;

namespace mirror {

// C++ mirror of dalvik.system.Detour
class MANAGED Detour : public Object {
 public:
  template <PointerSize kPointerSize, bool kTransactionActive>
  static Detour* CreateFromArtDetour(Thread* self, ArtDetour* detour)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_);

  ArtDetour* GetArtDetour() REQUIRES_SHARED(Locks::mutator_lock_);

  template <bool kTransactionActive = false>
  void SetArtDetour(ArtDetour* detour) REQUIRES_SHARED(Locks::mutator_lock_);

  static mirror::Class* StaticClass() REQUIRES_SHARED(Locks::mutator_lock_) {
    return static_class_.Read();
  }

  static void SetClass(Class* klass) REQUIRES_SHARED(Locks::mutator_lock_);

  static void ResetClass() REQUIRES_SHARED(Locks::mutator_lock_);

  static mirror::Class* ArrayClass() REQUIRES_SHARED(Locks::mutator_lock_) {
    return array_class_.Read();
  }

  static void SetArrayClass(Class* klass) REQUIRES_SHARED(Locks::mutator_lock_);

  static void ResetArrayClass() REQUIRES_SHARED(Locks::mutator_lock_);

  static void VisitRoots(RootVisitor* visitor) REQUIRES_SHARED(Locks::mutator_lock_);

 private:
  static MemberOffset ArtDetourOffset() {
    return MemberOffset(OFFSETOF_MEMBER(Detour, art_detour_));
  }

  uint64_t art_detour_;

 private:
  static GcRoot<Class> static_class_;   // dalvik.system.Detour.class
  static GcRoot<Class> array_class_;    // [dalvik.system.Detour.class

  friend struct art::DetourOffsets;     // for verifying offset information
  DISALLOW_COPY_AND_ASSIGN(Detour);
};

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_DETOUR_H_
