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

//
// Concurrent append-only hash-map optimized for fast reads.
//
// Values can be modified, but keys can not be deleted once set.
// This allows us to do fast reads without any synchronization.
//
// The storage is hybrid between open and closed addressing.
// First half of the bucket array stores the entries in-place.
// Second half of the arrays is used for linked-list collisions.
// This generally allows the table to fill up to ~95% of capacity.
//
// The collisions are allocated linearly. For large tables this
// means that we can keep many of the memory pages zeroed (clean).
//
// Since key is usually 16-bit, we get the next pointer for free.
//
// The key is initialized to 0 which is reasonable invalid marker
// since the key 0 will never be stored in most entries.  It will
// be stored in the first entry (so we initialize that one to 1).
//
// We can not free allocated linear-alloc memory, therefore when
// we grow the map, we keep using the old smaller buckets as well.
//
template<typename K, typename V>
class DexCacheMap {
  static constexpr size_t kNoNextEntry = 0;

  struct ALIGNED(8) Entry {
    std::atomic<K> key;    // Can not be modified after it is set.
    std::atomic<K> next;   // Can not be modified after it is set.
    std::atomic<V> value;  // Always mutable.
  };

  class Buckets {
   public:
    size_t capacity() { return capacity_; }
    Entry* begin() { return data_; }
    Entry* end() { return data_ + capacity_; }

    Buckets* Next() { return next_; }
    size_t IndexOf(K key) { return key & mask_; }
    static K InvalidKey(size_t index) { return index == 0 ? 1 : 0; }

    // Internal method which returns the bucket for the given key.
    // If not found, it can optionally insert the key (i.e. allocate new entry).
    ALWAYS_INLINE std::optional<std::atomic<V>*> Lookup(uint32_t key, bool insert = false) {
      DCHECK_LE(key, std::numeric_limits<K>::max());
      DCHECK_NE(key, InvalidKey(IndexOf(key)));
      for (size_t index = IndexOf(key);;) {
        DCHECK_LT(index, capacity_);
        Entry* entry = &data_[index];
        if (LIKELY(entry->key.load(std::memory_order_relaxed) == key)) {
          return &entry->value;
        } else if (insert) {
          K seen_key = InvalidKey(index);
          if (entry->key.compare_exchange_strong(seen_key, key) || seen_key == key) {
            return &entry->value;
          }
        }
        K next = entry->next.load(std::memory_order_relaxed);
        if (UNLIKELY(next == kNoNextEntry)) {
          if (!insert) {
            return std::optional<std::atomic<V>*>();  // Not found.
          }
          std::lock_guard<std::mutex> guard(lock_);
          next = entry->next.load(std::memory_order_seq_cst);
          if (next == kNoNextEntry) {
            if (size_ == capacity_) {
              return std::optional<std::atomic<V>*>();  // No available space.
            }
            next = size_++;
            data_[next].key.store(key, std::memory_order_release);
            entry->next.store(next, std::memory_order_seq_cst);
            return &data_[next].value;
          }
        }
        index = next;
      }
    }

    Buckets(size_t capacity, Buckets* next)
       : mask_(capacity / 2 - 1),  // Maps keys to in-place entries in the first half of data.
         capacity_(capacity),
         size_(capacity / 2),  // Collision entries are located in the second half of the data.
         next_(next) {
      data_[0].key.store(InvalidKey(0));  // Only the first entry has non-zero key.
      for (size_t i = 0; i < capacity_; i++) {
        DCHECK_EQ(data_[i].key.load(), InvalidKey(i)) << i;
        DCHECK_EQ(data_[i].next.load(), kNoNextEntry) << i;
      }
    }

   private:
    size_t const mask_;              // Pre-calculated mask used for hashing.
    size_t const capacity_;          // Number of entries in data_.
    std::mutex lock_;                // Guards allocation of collision entries.
    size_t size_ GUARDED_BY(lock_);  // Index of next free entry for collisions.
    Buckets* const next_;            // Older smaller bucket (before we grew to this one).
    Entry data_[];
  };

 public:
  static constexpr size_t kInitialSize = 64;

  DexCacheMap(mirror::DexCache* cache, size_t offset)
      : cache_(cache), offset_(MemberOffset(offset)) {}

  size_t Capacity() REQUIRES_SHARED(Locks::mutator_lock_) {
    size_t capacity = 0;
    for (Buckets* buckets = GetBuckets(); buckets != nullptr; buckets = buckets->Next()) {
      capacity += buckets->capacity();
    }
    return capacity;
  }

  ALWAYS_INLINE V Get(uint32_t key) REQUIRES_SHARED(Locks::mutator_lock_) {
    Buckets* buckets = GetBuckets();
    if (LIKELY(buckets != nullptr)) {
      std::optional<std::atomic<V>*> v = buckets->Lookup(key);
      if (LIKELY(v.has_value())) {
        return v.value()->load(std::memory_order_relaxed);
      }
      buckets = buckets->Next();
    }
    for (; LIKELY(buckets != nullptr); buckets = buckets->Next()) {
      std::optional<std::atomic<V>*> v = buckets->Lookup(key);
      if (LIKELY(v.has_value())) {
        Set(key, v.value()->load(std::memory_order_relaxed));
        return v.value()->load(std::memory_order_relaxed);
      }
    }
    return V();
  }

  NO_INLINE COLD_ATTR void Set(uint32_t key, V value) REQUIRES_SHARED(Locks::mutator_lock_) {
    Insert(key)->store(value, std::memory_order_release);
    WriteBarrier::ForEveryFieldWrite(cache_);
  }

  void Clear(uint32_t key) REQUIRES_SHARED(Locks::mutator_lock_) {
    for (Buckets* buckets = GetBuckets(); buckets != nullptr; buckets = buckets->Next()) {
      std::optional<std::atomic<V>*> v = buckets->Lookup(key);
      if (v.has_value()) {
        v.value()->store(V(), std::memory_order_release);
      }
    }
    WriteBarrier::ForEveryFieldWrite(cache_);
  }

  // Clear all values (but preserves keys since we can not modify those).
  void Clear() REQUIRES_SHARED(Locks::mutator_lock_) {
    ForEachEntry([](K, std::atomic<V>* v) { v->store(V(), std::memory_order_release); });
    WriteBarrier::ForEveryFieldWrite(cache_);
  }

  template<typename EntryCallback /* Arguments: (K, std::atomic<V>*) */>
  void ForEachEntry(EntryCallback callback) REQUIRES_SHARED(Locks::mutator_lock_) {
    for (Buckets* buckets = GetBuckets(); buckets != nullptr; buckets = buckets->Next()) {
      for (auto& entry : *buckets) {
        callback(entry.key.load(std::memory_order_relaxed), &entry.value);
      }
    }
  }

  std::atomic<V>* Insert(uint32_t key) REQUIRES_SHARED(Locks::mutator_lock_) {
    Buckets* buckets = GetBuckets();
    if (buckets != nullptr) {
      std::optional<std::atomic<V>*> v = buckets->Lookup(key, /*insert=*/ true);
      if (v.has_value()) {
        return v.value();
      }
    }

    // There is no more room for the key anywhere, so we will need to allocated larger map.
    Thread* self = Thread::Current();
    ClassLinker* linker = Runtime::Current()->GetClassLinker();
    LinearAlloc* alloc = linker->GetOrCreateAllocatorForClassLoader(cache_->GetClassLoader());

    // We need to retry under a lock since other thread might have already allocated the map.
    MutexLock mu(self, *Locks::dex_cache_lock_);
    buckets = GetBuckets();
    if (buckets != nullptr) {
      std::optional<std::atomic<V>*> v = buckets->Lookup(key, /*insert=*/ true);
      if (v.has_value()) {
        return v.value();
      }
    }

    // Allocate larger map.
    size_t new_capacity = (buckets == nullptr) ? kInitialSize : (buckets->capacity() * 2);
    size_t alloc_size = sizeof(Buckets) + sizeof(Entry) * new_capacity;
    void* alloc_data = alloc->AllocAlign16(self, RoundUp(alloc_size, 16));
    CHECK(alloc_data != nullptr);
    ZeroAndReleasePages(alloc_data, alloc_size);
    buckets = new (alloc_data) Buckets(new_capacity, buckets);
    std::optional<std::atomic<V>*> v = buckets->Lookup(key, /*insert=*/ true);
    DCHECK(v.has_value());
    SetBuckets(buckets);
    return v.value();
  }

 private:
  ALWAYS_INLINE Buckets* GetBuckets() REQUIRES_SHARED(Locks::mutator_lock_) {
    return reinterpret_cast<Buckets*>(cache_->GetField64(offset_));
  }

  ALWAYS_INLINE void SetBuckets(Buckets* buckets) REQUIRES_SHARED(Locks::mutator_lock_) {
    if (Runtime::Current()->IsActiveTransaction()) {
      cache_->SetField64Volatile<true>(offset_, reinterpret_cast<uint64_t>(buckets));
    } else {
      cache_->SetField64Volatile<false>(offset_, reinterpret_cast<uint64_t>(buckets));
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
  return ResolvedCallSites().Insert(call_site_idx)->load(std::memory_order_seq_cst).Read();
}

inline ObjPtr<CallSite> DexCache::SetResolvedCallSite(uint32_t call_site_idx,
                                                      ObjPtr<CallSite> call_site) {
  DCHECK(Runtime::Current()->IsMethodHandlesEnabled());

  DCHECK_LT(call_site_idx, GetDexFile()->NumCallSiteIds());
  GcRoot<mirror::CallSite> seen_call_site(nullptr);
  GcRoot<mirror::CallSite> candidate(call_site);
  std::atomic<GcRoot<CallSite>>* target = ResolvedCallSites().Insert(call_site_idx);

  // The first assignment for a given call site wins.
  if (target->compare_exchange_strong(seen_call_site, candidate)) {
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

template <ReadBarrierOption kReadBarrierOption,
          typename Visitor,
          typename K,
          typename T>
inline void VisitDexCacheEntries(DexCacheMap<K, GcRoot<T>> map, const Visitor& visitor)
    REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(Locks::heap_bitmap_lock_) {
  map.ForEachEntry([&visitor](K, std::atomic<GcRoot<T>>* value)
      NO_THREAD_SAFETY_ANALYSIS {  // Different visitors require different locks.
    GcRoot<T> gcroot = value->load(std::memory_order_relaxed);
    T* const before = gcroot.template Read<kReadBarrierOption>();
    visitor.VisitRootIfNonNull(gcroot.AddressWithoutBarrier());
    T* const after = gcroot.template Read<kReadBarrierOption>();
    if (after != before) {
      value->store(gcroot, std::memory_order_release);
    }
  });
}

template <bool kVisitNativeRoots,
          VerifyObjectFlags kVerifyFlags,
          ReadBarrierOption kReadBarrierOption,
          typename Visitor>
inline void DexCache::VisitReferences(ObjPtr<Class> klass, const Visitor& visitor) {
  // Visit instance fields first.
  VisitInstanceFieldsReferences<kVerifyFlags, kReadBarrierOption>(klass, visitor);
  // Visit arrays after.
  // Note that DexFile might have been freed at this point, so we can not access it.
  if (kVisitNativeRoots) {
    VisitDexCacheEntries<kReadBarrierOption, Visitor>(ResolvedStrings(), visitor);
    VisitDexCacheEntries<kReadBarrierOption, Visitor>(ResolvedTypes(), visitor);
    VisitDexCacheEntries<kReadBarrierOption, Visitor>(ResolvedMethodTypes(), visitor);
    VisitDexCacheEntries<kReadBarrierOption, Visitor>(ResolvedCallSites(), visitor);
  }
}

inline ObjPtr<String> DexCache::GetLocation() {
  return GetFieldObject<String>(OFFSET_OF_OBJECT_MEMBER(DexCache, location_));
}

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_DEX_CACHE_INL_H_
