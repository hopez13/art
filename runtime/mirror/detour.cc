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

#include "detour.h"

#include "art_method.h"
#include "gc_root-inl.h"
#include "mirror/class-inl.h"
#include "mirror/object-inl.h"

namespace art {
namespace mirror {

GcRoot<Class> Detour::static_class_;
GcRoot<Class> Detour::array_class_;

void Detour::SetClass(Class* klass) {
  CHECK(static_class_.IsNull()) << static_class_.Read() << " " << klass;
  CHECK(klass != nullptr);
  static_class_ = GcRoot<Class>(klass);
}

void Detour::ResetClass() {
  CHECK(!static_class_.IsNull());
  static_class_ = GcRoot<Class>(nullptr);
}

void Detour::SetArrayClass(Class* klass) {
  CHECK(array_class_.IsNull()) << array_class_.Read() << " " << klass;
  CHECK(klass != nullptr);
  array_class_ = GcRoot<Class>(klass);
}

void Detour::ResetArrayClass() {
  CHECK(!array_class_.IsNull());
  array_class_ = GcRoot<Class>(nullptr);
}

template <PointerSize kPointerSize, bool kTransactionActive>
Detour* Detour::CreateFromArtDetour(Thread* self, ArtDetour* detour) {
  auto* ret = down_cast<Detour*>(StaticClass()->AllocObject(self));
  if (LIKELY(ret != nullptr)) {
    ret->SetArtDetour<kTransactionActive>(detour);
  }
  return ret;
}

ArtDetour* Detour::GetArtDetour() {
  return reinterpret_cast<ArtDetour*>(GetField64(ArtDetourOffset()));
}

template <bool kTransactionActive>
void Detour::SetArtDetour(ArtDetour* detour) {
  SetField64<kTransactionActive>(ArtDetourOffset(), reinterpret_cast<uint64_t>(detour));
}

template void Detour::SetArtDetour<false>(ArtDetour*);
template void Detour::SetArtDetour<true>(ArtDetour*);

template Detour* Detour::CreateFromArtDetour<PointerSize::k32, false>(Thread* self,
                                                                      ArtDetour*);
template Detour* Detour::CreateFromArtDetour<PointerSize::k32, true>(Thread* self,
                                                                     ArtDetour*);
template Detour* Detour::CreateFromArtDetour<PointerSize::k64, false>(Thread* self,
                                                                      ArtDetour*);
template Detour* Detour::CreateFromArtDetour<PointerSize::k64, true>(Thread* self,
                                                                     ArtDetour*);

void Detour::VisitRoots(RootVisitor* visitor) {
  static_class_.VisitRootIfNonNull(visitor, RootInfo(kRootStickyClass));
  array_class_.VisitRootIfNonNull(visitor, RootInfo(kRootStickyClass));
}

}  // namespace mirror
}  // namespace art
