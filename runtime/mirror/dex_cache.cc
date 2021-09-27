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

void DexCache::Initialize(const DexFile* dex_file, ObjPtr<ClassLoader> class_loader) {
  DCHECK_EQ(dex_file_, 0u);
  DCHECK_EQ(resolved_call_sites_, 0u);
  DCHECK_EQ(resolved_fields_, 0u);
  DCHECK_EQ(resolved_method_types_, 0u);
  DCHECK_EQ(resolved_methods_, 0u);
  DCHECK_EQ(resolved_types_, 0u);
  DCHECK_EQ(strings_, 0u);

  SetDexFile(dex_file);
  SetClassLoader(class_loader);
}

void DexCache::VisitReflectiveTargets(ReflectiveValueVisitor* visitor) {
  bool wrote = false;
  ResolvedFields().ForEachEntry([&](uint32_t key, std::atomic<ArtField*>* value)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    ArtField* old_val = value->load(std::memory_order_relaxed);
    if (old_val == nullptr) {
      return;
    }
    ArtField* new_val = visitor->VisitField(
        old_val, DexCacheSourceInfo(kSourceDexCacheResolvedField, key, this));
    if (UNLIKELY(new_val != old_val)) {
      value->store(new_val, std::memory_order_release);
      wrote = true;
    }
  });
  ResolvedMethods().ForEachEntry([&](uint32_t key, std::atomic<ArtMethod*>* value)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    ArtMethod* old_val = value->load(std::memory_order_relaxed);
    if (old_val == nullptr) {
      return;
    }
    ArtMethod* new_val = visitor->VisitMethod(
        old_val, DexCacheSourceInfo(kSourceDexCacheResolvedMethod, key, this));
    if (UNLIKELY(new_val != old_val)) {
      value->store(new_val, std::memory_order_release);
      wrote = true;
    }
  });
  if (wrote) {
    WriteBarrier::ForEveryFieldWrite(this);
  }
}

void DexCache::Clear() {
  DCHECK(GetDexFile() != nullptr);
  ResolvedCallSites().Clear();
  ResolvedFields().Clear();
  ResolvedMethods().Clear();
  ResolvedMethodTypes().Clear();
  ResolvedTypes().Clear();
  ResolvedStrings().Clear();
}

void DexCache::ResetNativeArrays() {
  preresolved_strings_ = 0;
  resolved_call_sites_ = 0;
  resolved_fields_ = 0;
  resolved_method_types_ = 0;
  resolved_methods_ = 0;
  resolved_types_ = 0;
  strings_ = 0;
}

void DexCache::SetLocation(ObjPtr<mirror::String> location) {
  SetFieldObject<false>(OFFSET_OF_OBJECT_MEMBER(DexCache, location_), location);
}

void DexCache::SetClassLoader(ObjPtr<ClassLoader> class_loader) {
  SetFieldObject<false>(OFFSET_OF_OBJECT_MEMBER(DexCache, class_loader_), class_loader);
}

ObjPtr<ClassLoader> DexCache::GetClassLoader() {
  return GetFieldObject<ClassLoader>(OFFSET_OF_OBJECT_MEMBER(DexCache, class_loader_));
}

}  // namespace mirror
}  // namespace art
