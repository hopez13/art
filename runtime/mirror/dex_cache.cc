/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include "dex_cache-inl.h"

#include "art_method-inl.h"
#include "class_linker.h"
#include "gc/accounting/card_table-inl.h"
#include "gc/heap.h"
#include "linear_alloc.h"
#include "oat_file.h"
#include "object-inl.h"
#include "object.h"
#include "object_array-inl.h"
#include "reflective_value_visitor.h"
#include "runtime.h"
#include "runtime_globals.h"
#include "string.h"
#include "thread.h"
#include "write_barrier.h"

namespace art {
namespace mirror {

void DexCache::VisitReflectiveTargets(ReflectiveValueVisitor* visitor ATTRIBUTE_UNUSED) {
  MutexLock mu2(Thread::Current(), *Locks::dex_cache_lock_);
  ResolvedFields().Clear();
  ResolvedMethods().Clear();
  WriteBarrier::ForEveryFieldWrite(this);
}

void DexCache::EnsureInitialized() {
  MutexLock mu2(Thread::Current(), *Locks::dex_cache_lock_);
  DCHECK(GetDexFile() != nullptr);
  if (resolved_call_sites_ == 0) {
    auto* resolved_call_sites = new std::atomic<GcRoot<CallSite>>[GetDexFile()->NumCallSiteIds()];
    std::fill_n(resolved_call_sites, GetDexFile()->NumCallSiteIds(), GcRoot<CallSite>());
    resolved_call_sites_ = reinterpret_cast<uintptr_t>(resolved_call_sites);
  }
  ResolvedFields().EnsureInitialized();
  ResolvedMethods().EnsureInitialized();
  ResolvedMethodTypes().EnsureInitialized();
  ResolvedTypes().EnsureInitialized();
  ResolvedStrings().EnsureInitialized();
}

void DexCache::Clear() {
  MutexLock mu2(Thread::Current(), *Locks::dex_cache_lock_);
  DCHECK(GetDexFile() != nullptr);
  std::fill_n(GetResolvedCallSites(), GetDexFile()->NumCallSiteIds(), GcRoot<CallSite>());
  ResolvedFields().Clear();
  ResolvedMethods().Clear();
  ResolvedMethodTypes().Clear();
  ResolvedTypes().Clear();
  ResolvedStrings().Clear();
}

void DexCache::ResetNativeFileds() {
  MutexLock mu2(Thread::Current(), *Locks::dex_cache_lock_);
  dex_file_ = 0;
  resolved_call_sites_ = 0;
  ResolvedFields().Reset();
  ResolvedMethods().Reset();
  ResolvedMethodTypes().Reset();
  ResolvedTypes().Reset();
  ResolvedStrings().Reset();
}

void DexCache::SetLocation(ObjPtr<mirror::String> location) {
  SetFieldObject<false>(OFFSET_OF_OBJECT_MEMBER(DexCache, location_), location);
}

void DexCache::SetClassLoader(ObjPtr<ClassLoader> class_loader) {
  SetFieldObject<false>(OFFSET_OF_OBJECT_MEMBER(DexCache, class_loader_), class_loader);
}

}  // namespace mirror
}  // namespace art
