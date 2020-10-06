/*
 * Copyright (C) 2013 The Android Open Source Project
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

#ifndef ART_RUNTIME_MIRROR_DEX_CACHE_INL_H_
#define ART_RUNTIME_MIRROR_DEX_CACHE_INL_H_

#include "dex_cache.h"

#include <android-base/logging.h>

#include "art_field.h"
#include "art_method.h"
#include "base/casts.h"
#include "base/enums.h"
#include "class_linker.h"
#include "dex/dex_file.h"
#include "gc_root-inl.h"
#include "mirror/call_site.h"
#include "mirror/class.h"
#include "mirror/method_type.h"
#include "obj_ptr.h"
#include "object-inl.h"
#include "runtime.h"
#include "write_barrier-inl.h"

#include <atomic>

namespace art {
namespace mirror {

template<typename K, typename V>
void DexCacheMap<K, V>::Resize(size_t new_size) {
  uint64_t old_packed, new_packed;
  {
    MutexLock mu(Thread::Current(), *Locks::dex_cache_lock_);
    old_packed = GetPackedField();
    if (DexCacheArray<K, V>(old_packed).size() >= new_size) {
      return;  // Already resized by other thread.
    }
    new_packed = DexCacheArray<K, V>::Allocate(new_size);
    SetPackedField(new_packed);
  }

  DexCacheArray<K, V> old_map(old_packed);
  DexCacheArray<K, V> new_map(new_packed);
  size_t used = 0, index = 0;
  for (auto& entry : old_map) {
    K key = entry.key.load(std::memory_order_relaxed);
    if (key != DexCacheArray<K, V>::InvalidKey(index)) {
      V value = entry.value.load(std::memory_order_relaxed);
      new_map.Set(key, value);
      used++;
    }
    index++;
  }

  if (new_size >= 4096) {
    LOG(VERBOSE) << "DexCache " << object_ << "+" << offset_
        << " resized to " << new_size << " entries"
        << " (" << (used * 100) / index << "% full).";
  }
}

inline uint32_t DexCache::ClassSize(PointerSize pointer_size) {
  const uint32_t vtable_entries = Object::kVTableLength;
  return Class::ComputeClassSize(true, vtable_entries, 0, 0, 0, 0, 0, pointer_size);
}

inline String* DexCache::GetResolvedString(dex::StringIndex string_idx) {
  return ResolvedStrings().Get(string_idx.index_).Read();
}

inline void DexCache::SetResolvedString(dex::StringIndex string_idx, ObjPtr<String> resolved) {
  DCHECK(resolved != nullptr);
  ResolvedStrings().Set(string_idx.index_, GcRoot<String>(resolved));
  Runtime* const runtime = Runtime::Current();
  if (UNLIKELY(runtime->IsActiveTransaction())) {
    DCHECK(runtime->IsAotCompiler());
    runtime->RecordResolveString(this, string_idx);
  }
  // TODO: Fine-grained marking, so that we don't need to go through all arrays in full.
  WriteBarrier::ForEveryFieldWrite(this);
}

inline void DexCache::ClearString(dex::StringIndex string_idx) {
  DCHECK(Runtime::Current()->IsAotCompiler());
  ResolvedStrings().Set(string_idx.index_, GcRoot<String>());
}

inline Class* DexCache::GetResolvedType(dex::TypeIndex type_idx) {
  return ResolvedTypes().Get(type_idx.index_).Read();
}

inline void DexCache::SetResolvedType(dex::TypeIndex type_idx, ObjPtr<Class> resolved) {
  DCHECK(resolved != nullptr);
  DCHECK(resolved->IsResolved()) << resolved->GetStatus();
  // TODO default transaction support.
  // Use a release store for SetResolvedType. This is done to prevent other threads from seeing a
  // class but not necessarily seeing the loaded members like the static fields array.
  // See b/32075261.
  ResolvedTypes().Set(type_idx.index_, GcRoot<Class>(resolved));
  // TODO: Fine-grained marking, so that we don't need to go through all arrays in full.
  WriteBarrier::ForEveryFieldWrite(this);
}

inline void DexCache::ClearResolvedType(dex::TypeIndex type_idx) {
  DCHECK(Runtime::Current()->IsAotCompiler());
  ResolvedTypes().Set(type_idx.index_, GcRoot<Class>());
}

inline MethodType* DexCache::GetResolvedMethodType(dex::ProtoIndex proto_idx) {
  return ResolvedMethodTypes().Get(proto_idx.index_).Read();
}

inline void DexCache::SetResolvedMethodType(dex::ProtoIndex proto_idx, MethodType* resolved) {
  DCHECK(resolved != nullptr);
  ResolvedMethodTypes().Set(proto_idx.index_, GcRoot<MethodType>(resolved));
  // TODO: Fine-grained marking, so that we don't need to go through all arrays in full.
  WriteBarrier::ForEveryFieldWrite(this);
}

inline CallSite* DexCache::GetResolvedCallSite(uint32_t call_site_idx) {
  DCHECK(Runtime::Current()->IsMethodHandlesEnabled());
  DCHECK_LT(call_site_idx, GetDexFile()->NumCallSiteIds());
  std::atomic<GcRoot<mirror::CallSite>>& traget = GetResolvedCallSites()[call_site_idx];
  return traget.load(std::memory_order_seq_cst).Read();
}

inline ObjPtr<CallSite> DexCache::SetResolvedCallSite(uint32_t call_site_idx,
                                                      ObjPtr<CallSite> call_site) {
  DCHECK(Runtime::Current()->IsMethodHandlesEnabled());
  DCHECK_LT(call_site_idx, GetDexFile()->NumCallSiteIds());
  GcRoot<mirror::CallSite> seen_call_site(nullptr);
  GcRoot<mirror::CallSite> candidate(call_site);
  std::atomic<GcRoot<mirror::CallSite>>& target = GetResolvedCallSites()[call_site_idx];

  // The first assignment for a given call site wins.
  if (target.compare_exchange_strong(seen_call_site, candidate)) {
    // TODO: Fine-grained marking, so that we don't need to go through all arrays in full.
    WriteBarrier::ForEveryFieldWrite(this);
    return call_site;
  } else {
    return seen_call_site.Read();
  }
}

inline ArtField* DexCache::GetResolvedField(uint32_t field_idx) {
  return ResolvedFields().Get(field_idx);
}

inline void DexCache::SetResolvedField(uint32_t field_idx, ArtField* field) {
  ResolvedFields().Set(field_idx, field);
}

inline ArtMethod* DexCache::GetResolvedMethod(uint32_t method_idx) {
  return ResolvedMethods().Get(method_idx);
}

inline void DexCache::SetResolvedMethod(uint32_t method_idx, ArtMethod* method) {
  ResolvedMethods().Set(method_idx, method);
}

template <bool kVisitNativeRoots,
          VerifyObjectFlags kVerifyFlags,
          ReadBarrierOption kReadBarrierOption,
          typename Visitor>
inline void DexCache::VisitReferences(ObjPtr<Class> klass, const Visitor& visitor) {
  // Visit instance fields first.
  VisitInstanceFieldsReferences<kVerifyFlags, kReadBarrierOption>(klass, visitor);
  // Visit arrays after.
  if (kVisitNativeRoots) {
    ResolvedStrings().VisitDexCachePairs<kReadBarrierOption, Visitor>(visitor);
    ResolvedTypes().VisitDexCachePairs<kReadBarrierOption, Visitor>(visitor);
    ResolvedMethodTypes().VisitDexCachePairs<kReadBarrierOption, Visitor>(visitor);
    auto resolved_call_sites = GetResolvedCallSites();
    if (resolved_call_sites != nullptr) {
      for (size_t i = 0; i < GetDexFile()->NumCallSiteIds(); i++) {
        VisitAtomicGcRoot<kReadBarrierOption>(visitor, resolved_call_sites[i]);
      }
    }
  }
}

inline ObjPtr<String> DexCache::GetLocation() {
  return GetFieldObject<String>(OFFSET_OF_OBJECT_MEMBER(DexCache, location_));
}

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_DEX_CACHE_INL_H_
