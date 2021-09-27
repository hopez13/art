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

template <ReadBarrierOption kReadBarrierOption, typename Visitor, typename T>
inline void VisitAtomicGcRoot(const Visitor& visitor, std::atomic<GcRoot<T>>& entry)
    REQUIRES_SHARED(Locks::mutator_lock_)
    REQUIRES(Locks::heap_bitmap_lock_) {
  auto gcroot = entry.load(std::memory_order_relaxed);
  auto before = gcroot.template Read<kReadBarrierOption>();
  visitor.VisitRootIfNonNull(gcroot.AddressWithoutBarrier());
  auto after = gcroot.template Read<kReadBarrierOption>();
  if (before != after) {
    entry.store(gcroot, std::memory_order_relaxed);
  }
}

//
// Helper class for hash-map, which accesses continuous array of Entries.
//
// The layout consists of two consecutive tables (both of length 'size'):
//   Entry[size]: Table of "primary" entries (accessed by hash).
//   Entry[size]: Table of "collision" entries (accessed by next field).
//
// Values can be modified, but keys can not be deleted once set.
// This allows us to access the data without any synchronization.
//
// The storage is hybrid between open and closed addressing.
// The first entry is stored in-place and collisions are linked.
// The key is usually 16-bit which gives us the next pointer for free.
// This generally allows the table to fill up to ~95% before resize.
//
// The collisions are allocated linearly. For large tables this
// means that we can keep many of the memory pages zeroed (clean).
//
// The key is initialized to 0 which is reasonable invalid marker
// since the key 0 will never be stored in most entries.  It will
// be stored in the first entry (so we initialize that one to 1).
//
template<typename K, typename V>
class DexCacheMap {
  // After the key is set, it can never be modified or removed.
  // This constraint is used to get very cheap synchronization.
  struct ALIGNED(8) Entry {
    std::atomic<K> key;    // Can not be modified after it is set.
    std::atomic<K> next;   // Can not be modified after it is set.
    std::atomic<V> value;  // Always mutable.
  };

  class Buckets {
   public:
    static constexpr size_t kNoNextEntry = 0;

    Entry* data() { return data_; }
    size_t size() { return size_; }  // Excluding collision entries.
    Entry* begin() { return data_; }
    Entry* end() { return begin() + size() + size(); }  // Including collision entries.
    size_t IndexOf(K key) { return key & (size() - 1); }
    static K InvalidKey(size_t index) { return index == 0 ? 1 : 0; }

    Buckets* Next() { return next_; }

    ALWAYS_INLINE std::optional<V> Get(uint32_t key) {
      DCHECK_LE(key, std::numeric_limits<K>::max());
      size_t index = IndexOf(key);
      DCHECK_LT(index, size());
      while (true) {
        Entry* entry = &data_[index];
        DCHECK_LT(entry, end());
        if (LIKELY(entry->key.load(std::memory_order_relaxed) == key)) {
          return entry->value.load(std::memory_order_relaxed);
        }
        K next = entry->next.load(std::memory_order_relaxed);
        if (UNLIKELY(next == kNoNextEntry)) {
          return std::optional<V>();
        }
        index = next;
      }
    }

    // Store new value in the table.
    // It will store the value in the primary table or allocate collision entry.
    // Given key will be always added only once, even in the case of a race.
    ALWAYS_INLINE std::optional<std::atomic<V>*> GetLValue(uint32_t key) {
      DCHECK_LE(key, std::numeric_limits<K>::max());
      size_t index = IndexOf(key);
      DCHECK_LT(index, size());
      DCHECK_NE(key, InvalidKey(index));
      while (true) {
        Entry* entry = &data_[index];
        DCHECK_LT(entry, end());
        K seen_key = InvalidKey(index);
        if (entry->key.compare_exchange_strong(seen_key, key) || seen_key == key) {
          return &entry->value;
        }
        K next = entry->next.load(std::memory_order_relaxed);
        if (UNLIKELY(next == kNoNextEntry)) {
          std::lock_guard<std::mutex> guard(lock_);
          next = entry->next.load(std::memory_order_seq_cst);
          if (next == kNoNextEntry) {
            if (begin() + next_collision_ == end()) {
              return std::optional<std::atomic<V>*>();  // No available space.
            }
            next = entry->next = next_collision_++;
          }
        }
        index = next;
      }
    }

    Buckets(size_t size, Buckets* next): size_(size), next_collision_(size), next_(next) {
      data_[0].key.store(InvalidKey(0));
      for (Entry& entry : *this) {
        size_t index = &entry - this->begin();
        DCHECK_EQ(entry.key.load(), InvalidKey(index)) << index;
        DCHECK_EQ(entry.next.load(), kNoNextEntry) << index;
      }
    }

   private:
    size_t size_;
    std::mutex lock_;
    size_t next_collision_ GUARDED_BY(lock_);
    Buckets* next_;
    Entry data_[];
  };

 public:
  static constexpr size_t kInitialSize = 16;

  DexCacheMap(mirror::DexCache* cache, size_t offset)
      : cache_(cache), offset_(MemberOffset(offset)) {}

  void Clear() REQUIRES_SHARED(Locks::mutator_lock_) {
    for (Buckets* buckets = GetBuckets(); buckets != nullptr; buckets = buckets->Next()) {
      for (auto& entry : *buckets) {
        entry.value.store(V());
      }
    }
  }

  // Read cache entry.
  // This is fairly fast since it requires no explicit synchronization.
  ALWAYS_INLINE V Get(uint32_t key) REQUIRES_SHARED(Locks::mutator_lock_) {
    for (Buckets* buckets = GetBuckets(); buckets != nullptr; buckets = buckets->Next()) {
      std::optional<V> v = buckets->Get(key);
      if (v.has_value()) {
        return v.value();
      }
    }
    return V();
  }

  // Write cache entry.
  // Some sets might be ignored while the array is being resized.
  std::atomic<V>* GetLValue(uint32_t key) REQUIRES_SHARED(Locks::mutator_lock_) {
    std::optional<std::atomic<V>*> v;
    Buckets* buckets = GetBuckets();
    if (LIKELY(buckets != nullptr)) {
      v = buckets->GetLValue(key);
    }
    if (!v.has_value()) {
      Thread* self = Thread::Current();
      ClassLinker* linker = Runtime::Current()->GetClassLinker();
      LinearAlloc* alloc = linker->GetOrCreateAllocatorForClassLoader(cache_->GetClassLoader());
      MutexLock mu(self, *Locks::dex_cache_lock_);
      size_t new_size = (buckets == nullptr) ? kInitialSize : (buckets->size() * 2);
      size_t alloc_size = sizeof(Buckets) + sizeof(Entry) * new_size * 2;
      void* alloc_data = alloc->AllocAlign16(self, alloc_size);
      CHECK(alloc_data != nullptr);
      ZeroAndReleasePages(alloc_data, alloc_size);
      Buckets* new_buckets = new (alloc_data) Buckets(new_size, buckets);
      v = new_buckets->GetLValue(key);
      DCHECK(v.has_value());
      SetBuckets(new_buckets);
    }
    return v.value();
  }

  ALWAYS_INLINE void Set(uint32_t key, V value) REQUIRES_SHARED(Locks::mutator_lock_) {
    GetLValue(key)->store(value, std::memory_order_release);
    WriteBarrier::ForEveryFieldWrite(cache_);
  }

  ALWAYS_INLINE void Clear(uint32_t key) REQUIRES_SHARED(Locks::mutator_lock_) {
    for (Buckets* buckets = GetBuckets(); buckets != nullptr; buckets = buckets->Next()) {
      std::optional<std::atomic<V>*> v = buckets->GetLValue(key);
      if (v.has_value()) {
        v.value()->store(V(), std::memory_order_release);
      }
    }
    WriteBarrier::ForEveryFieldWrite(cache_);
  }

  template <ReadBarrierOption kReadBarrierOption, typename Visitor>
  inline void VisitDexCachePairs(const Visitor& visitor)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(Locks::heap_bitmap_lock_) {
    for (Buckets* buckets = GetBuckets(); buckets != nullptr; buckets = buckets->Next()) {
      for (auto& entry : *buckets) {
        VisitAtomicGcRoot<kReadBarrierOption>(visitor, entry.value);
      }
    }
  }

 private:
  ALWAYS_INLINE Buckets* GetBuckets() REQUIRES_SHARED(Locks::mutator_lock_) {
    return reinterpret_cast<Buckets*>(cache_->GetField64(offset_));
  }

  ALWAYS_INLINE void SetBuckets(Buckets* buckets) REQUIRES_SHARED(Locks::mutator_lock_) {
    if (Runtime::Current()->IsActiveTransaction()) {
      cache_->SetField64<true>(offset_, reinterpret_cast<uint64_t>(buckets));
    } else {
      cache_->SetField64<false>(offset_, reinterpret_cast<uint64_t>(buckets));
    }
    WriteBarrier::ForEveryFieldWrite(cache_);
  }

  // We would like to just store the data here, but since DexCache is a heap object,
  // have to use indirection, so DexCacheMap is just temporary access helper.
  mirror::DexCache* cache_;
  MemberOffset offset_;
};

namespace mirror {

inline uint32_t DexCache::ClassSize(PointerSize pointer_size) {
  const uint32_t vtable_entries = Object::kVTableLength;
  return Class::ComputeClassSize(true, vtable_entries, 0, 0, 0, 0, 0, pointer_size);
}

inline DexCacheMap<uint16_t, GcRoot<CallSite>> DexCache::ResolvedCallSites() {
  return DexCacheMap<uint16_t, GcRoot<CallSite>>(this, offsetof(DexCache, resolved_call_sites_));
}

inline DexCacheMap<uint16_t, ArtField*> DexCache::ResolvedFields() {
  return DexCacheMap<uint16_t, ArtField*>(this, offsetof(DexCache, resolved_fields_));
}

inline DexCacheMap<uint16_t, GcRoot<MethodType>> DexCache::ResolvedMethodTypes() {
  return DexCacheMap<uint16_t, GcRoot<MethodType>>(this, offsetof(DexCache, resolved_method_types_));
}

inline DexCacheMap<uint16_t, ArtMethod*> DexCache::ResolvedMethods() {
  return DexCacheMap<uint16_t, ArtMethod*>(this, offsetof(DexCache, resolved_methods_));
}

inline DexCacheMap<uint16_t, GcRoot<Class>> DexCache::ResolvedTypes() {
  return DexCacheMap<uint16_t, GcRoot<Class>>(this, offsetof(DexCache, resolved_types_));
}

inline DexCacheMap<uint32_t, GcRoot<String>> DexCache::ResolvedStrings() {
  return DexCacheMap<uint32_t, GcRoot<String>>(this, offsetof(DexCache, strings_));
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
}

inline void DexCache::ClearString(dex::StringIndex string_idx) {
  DCHECK(Runtime::Current()->IsAotCompiler());
  ResolvedStrings().Clear(string_idx.index_);
}

inline Class* DexCache::GetResolvedType(dex::TypeIndex type_idx) {
  return ResolvedTypes().Get(type_idx.index_).Read();
}

inline void DexCache::SetResolvedType(dex::TypeIndex type_idx, ObjPtr<Class> resolved) {
  DCHECK(resolved != nullptr);
  DCHECK(resolved->IsResolved()) << resolved->GetStatus();
  // TODO default transaction support.
  ResolvedTypes().Set(type_idx.index_, GcRoot<Class>(resolved));
}

inline void DexCache::ClearResolvedType(dex::TypeIndex type_idx) {
  DCHECK(Runtime::Current()->IsAotCompiler());
  ResolvedTypes().Clear(type_idx.index_);
}

inline MethodType* DexCache::GetResolvedMethodType(dex::ProtoIndex proto_idx) {
  return ResolvedMethodTypes().Get(proto_idx.index_).Read();
}

inline void DexCache::SetResolvedMethodType(dex::ProtoIndex proto_idx, MethodType* resolved) {
  DCHECK(resolved != nullptr);
  ResolvedMethodTypes().Set(proto_idx.index_, GcRoot<MethodType>(resolved));
  Runtime* const runtime = Runtime::Current();
  if (UNLIKELY(runtime->IsActiveTransaction())) {
    DCHECK(runtime->IsAotCompiler());
    runtime->RecordResolveMethodType(this, proto_idx);
  }
}

inline void DexCache::ClearMethodType(dex::ProtoIndex proto_idx) {
  DCHECK(Runtime::Current()->IsAotCompiler());
  ResolvedMethodTypes().Clear(proto_idx.index_);
}

inline CallSite* DexCache::GetResolvedCallSite(uint32_t call_site_idx) {
  DCHECK(Runtime::Current()->IsMethodHandlesEnabled());
  return ResolvedCallSites().GetLValue(call_site_idx)->load(std::memory_order_seq_cst).Read();
}

inline ObjPtr<CallSite> DexCache::SetResolvedCallSite(uint32_t call_site_idx,
                                                      ObjPtr<CallSite> call_site) {
  DCHECK(Runtime::Current()->IsMethodHandlesEnabled());

  DCHECK_LT(call_site_idx, GetDexFile()->NumCallSiteIds());
  GcRoot<mirror::CallSite> seen_call_site(nullptr);
  GcRoot<mirror::CallSite> candidate(call_site);

  //
  // TODO: This is not guaranteed to be unique
  //
  std::atomic<GcRoot<CallSite>>* lvalue = ResolvedCallSites().GetLValue(call_site_idx);

  // The first assignment for a given call site wins.
  if (lvalue->compare_exchange_strong(seen_call_site, candidate)) {
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
  // Note that DexFile might be freed at this point, so we can not access it.
  if (kVisitNativeRoots) {
    ResolvedCallSites().VisitDexCachePairs<kReadBarrierOption, Visitor>(visitor);
    ResolvedStrings().VisitDexCachePairs<kReadBarrierOption, Visitor>(visitor);
    ResolvedTypes().VisitDexCachePairs<kReadBarrierOption, Visitor>(visitor);
    ResolvedMethodTypes().VisitDexCachePairs<kReadBarrierOption, Visitor>(visitor);
  }
}

inline ObjPtr<String> DexCache::GetLocation() {
  return GetFieldObject<String>(OFFSET_OF_OBJECT_MEMBER(DexCache, location_));
}

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_DEX_CACHE_INL_H_
