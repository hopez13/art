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
#include "base/atomic_pair.h"
#include "base/casts.h"
#include "base/enums.h"
#include "class_linker.h"
#include "dex/dex_file.h"
#include "gc_root-inl.h"
#include "linear_alloc.h"
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

template<typename DexCachePair>
static void InitializeArray(std::atomic<DexCachePair>* array) {
  DexCachePair::Initialize(array);
}

template<typename T>
static void InitializeArray(GcRoot<T>*) {
  // No special initialization is needed.
}

template<typename T>
T* DexCache::AllocArray(MemberOffset obj_offset, MemberOffset num_offset) {
  mirror::DexCache* dex_cache = this;
  if (gUseReadBarrier && Thread::Current()->GetIsGcMarking()) {
    // Several code paths use DexCache without read-barrier for performance.
    // We have to check the "to-space" object here to avoid allocating twice.
    dex_cache = reinterpret_cast<DexCache*>(ReadBarrier::Mark(dex_cache));
  }
  Thread* self = Thread::Current();
  ClassLinker* linker = Runtime::Current()->GetClassLinker();
  LinearAlloc* alloc = linker->GetOrCreateAllocatorForClassLoader(GetClassLoader());
  MutexLock mu(self, *Locks::dex_cache_lock_);  // Avoid allocation by multiple threads.
  T* array = dex_cache->GetFieldPtr64<T*>(obj_offset);
  if (array != nullptr) {
    DCHECK(alloc->Contains(array));
    return array;  // Other thread just allocated the array.
  }
  size_t num = dex_cache->GetField32<kDefaultVerifyFlags>(num_offset);
  DCHECK_NE(num, 0u);
  array = reinterpret_cast<T*>(alloc->AllocAlign16(self, RoundUp(num * sizeof(T), 16)));
  InitializeArray(array);  // Ensure other threads see the array initialized.
  dex_cache->SetField64Volatile<false, false>(obj_offset, reinterpret_cast64<uint64_t>(array));
  return array;
}

template <typename T>
inline DexCachePair<T>::DexCachePair(ObjPtr<T> object, uint32_t index)
    : object(object), index(index) {}

template <typename T>
inline void DexCachePair<T>::Initialize(std::atomic<DexCachePair<T>>* dex_cache) {
  DexCachePair<T> first_elem;
  first_elem.object = GcRoot<T>(nullptr);
  first_elem.index = InvalidIndexForSlot(0);
  dex_cache[0].store(first_elem, std::memory_order_relaxed);
}

template <typename T>
inline T* DexCachePair<T>::GetObjectForIndex(uint32_t idx) {
  if (idx != index) {
    return nullptr;
  }
  DCHECK(!object.IsNull());
  return object.Read();
}

template <typename T>
inline void NativeDexCachePair<T>::Initialize(std::atomic<NativeDexCachePair<T>>* array) {
  NativeDexCachePair<T> first_elem;
  first_elem.object = nullptr;
  first_elem.index = InvalidIndexForSlot(0);
  Store(array, 0, first_elem);
}

inline uint32_t DexCache::ClassSize(PointerSize pointer_size) {
  const uint32_t vtable_entries = Object::kVTableLength;
  return Class::ComputeClassSize(true, vtable_entries, 0, 0, 0, 0, 0, pointer_size);
}

inline String* DexCache::GetResolvedString(dex::StringIndex string_idx) {
  return ResolvedStrings().Get(string_idx.index_);
}

inline void DexCache::SetResolvedString(dex::StringIndex string_idx, ObjPtr<String> resolved) {
  DCHECK(resolved != nullptr);
  ResolvedStrings().Set(string_idx.index_, resolved);
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
  ResolvedStrings().Clear(string_idx.index_);
}

inline Class* DexCache::GetResolvedType(dex::TypeIndex type_idx) {
  return ResolvedTypes().Get(type_idx.index_);
}

inline void DexCache::SetResolvedType(dex::TypeIndex type_idx, ObjPtr<Class> resolved) {
  DCHECK(resolved != nullptr);
  DCHECK(resolved->IsResolved()) << resolved->GetStatus();
  // TODO default transaction support.
  ResolvedTypes().Set(type_idx.index_, resolved);
  // TODO: Fine-grained marking, so that we don't need to go through all arrays in full.
  WriteBarrier::ForEveryFieldWrite(this);
}

inline void DexCache::ClearResolvedType(dex::TypeIndex type_idx) {
  DCHECK(Runtime::Current()->IsAotCompiler());
  ResolvedTypes().Clear(type_idx.index_);
}

inline MethodType* DexCache::GetResolvedMethodType(dex::ProtoIndex proto_idx) {
  return ResolvedMethodTypes().Get(proto_idx.index_);
}

inline void DexCache::SetResolvedMethodType(dex::ProtoIndex proto_idx, ObjPtr<MethodType> resolved) {
  DCHECK(resolved != nullptr);
  ResolvedMethodTypes().Set(proto_idx.index_, resolved);
  Runtime* const runtime = Runtime::Current();
  if (UNLIKELY(runtime->IsActiveTransaction())) {
    DCHECK(runtime->IsAotCompiler());
    runtime->RecordResolveMethodType(this, proto_idx);
  }
  // TODO: Fine-grained marking, so that we don't need to go through all arrays in full.
  WriteBarrier::ForEveryFieldWrite(this);
}

inline void DexCache::ClearMethodType(dex::ProtoIndex proto_idx) {
  DCHECK(Runtime::Current()->IsAotCompiler());
  ResolvedMethodTypes().Clear(proto_idx.index_);
}

inline CallSite* DexCache::GetResolvedCallSite(uint32_t call_site_idx) {
  DCHECK(Runtime::Current()->IsMethodHandlesEnabled());
  return ResolvedCallSites().Get(call_site_idx);
}

inline ObjPtr<CallSite> DexCache::SetResolvedCallSite(uint32_t call_site_idx,
                                                      ObjPtr<CallSite> call_site) {
  DCHECK(Runtime::Current()->IsMethodHandlesEnabled());
  DCHECK_LT(call_site_idx, GetDexFile()->NumCallSiteIds());

  CallSiteDexCachePair old_pair(nullptr, CallSiteDexCachePair::InvalidIndexForSlot(call_site_idx));
  CallSiteDexCachePair new_pair(call_site, call_site_idx);
  CallSiteDexCacheType* call_sites = ResolvedCallSites().Data</*kAllocateIfNull*/true>();

  // The first assignment for a given call site wins.
  if (call_sites[call_site_idx].compare_exchange_strong(old_pair, new_pair)) {
    // TODO: Fine-grained marking, so that we don't need to go through all arrays in full.
    WriteBarrier::ForEveryFieldWrite(this);
    return new_pair.object.Read();
  } else {
    return old_pair.object.Read();
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

template <typename T>
NativeDexCachePair<T> NativeDexCachePair<T>::Load(std::atomic<NativeDexCachePair>* pair_array,
                                                  size_t idx) {
  auto* array = reinterpret_cast<std::atomic<AtomicPair<uintptr_t>>*>(pair_array);
  AtomicPair<uintptr_t> value = AtomicPairLoadAcquire(&array[idx]);
  return NativeDexCachePair<T>(reinterpret_cast<T*>(value.first), value.second);
}

template <typename T>
void NativeDexCachePair<T>::Store(std::atomic<NativeDexCachePair>* pair_array,
                                  size_t idx,
                                  NativeDexCachePair pair) {
  auto* array = reinterpret_cast<std::atomic<AtomicPair<uintptr_t>>*>(pair_array);
  AtomicPair<uintptr_t> v(reinterpret_cast<size_t>(pair.object), pair.index);
  AtomicPairStoreRelease(&array[idx], v);
}

template <typename T,
          ReadBarrierOption kReadBarrierOption,
          typename Visitor>
inline void VisitDexCachePairs(std::atomic<DexCachePair<T>>* pairs,
                               size_t num_pairs,
                               const Visitor& visitor)
    REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(Locks::heap_bitmap_lock_) {
  // Check both the data pointer and count since the array might be initialized
  // concurrently on other thread, and we might observe just one of the values.
  for (size_t i = 0; pairs != nullptr && i < num_pairs; ++i) {
    DexCachePair<T> source = pairs[i].load(std::memory_order_relaxed);
    // NOTE: We need the "template" keyword here to avoid a compilation
    // failure. GcRoot<T> is a template argument-dependent type and we need to
    // tell the compiler to treat "Read" as a template rather than a field or
    // function. Otherwise, on encountering the "<" token, the compiler would
    // treat "Read" as a field.
    T* const before = source.object.template Read<kReadBarrierOption>();
    visitor.VisitRootIfNonNull(source.object.AddressWithoutBarrier());
    if (source.object.template Read<kReadBarrierOption>() != before) {
      pairs[i].store(source, std::memory_order_relaxed);
    }
  }
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
    VisitNativeRoots<kVerifyFlags, kReadBarrierOption>(visitor);
  }
}

template <VerifyObjectFlags kVerifyFlags,
          ReadBarrierOption kReadBarrierOption,
          typename Visitor>
inline void DexCache::VisitNativeRoots(const Visitor& visitor) {
  VisitDexCachePairs<String, kReadBarrierOption, Visitor>(
      ResolvedStrings().Data(), ResolvedStrings().Size(), visitor);

  VisitDexCachePairs<Class, kReadBarrierOption, Visitor>(
      ResolvedTypes().Data(), ResolvedTypes().Size(), visitor);

  VisitDexCachePairs<MethodType, kReadBarrierOption, Visitor>(
      ResolvedMethodTypes().Data(), ResolvedMethodTypes().Size(), visitor);

  VisitDexCachePairs<CallSite, kReadBarrierOption, Visitor>(
      ResolvedCallSites().Data(), ResolvedCallSites().Size(), visitor);
}

template <VerifyObjectFlags kVerifyFlags, ReadBarrierOption kReadBarrierOption>
inline ObjPtr<String> DexCache::GetLocation() {
  return GetFieldObject<String, kVerifyFlags, kReadBarrierOption>(
      OFFSET_OF_OBJECT_MEMBER(DexCache, location_));
}

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_DEX_CACHE_INL_H_
