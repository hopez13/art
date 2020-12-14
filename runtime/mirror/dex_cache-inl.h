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
// The array data pointer and size is packed into single field,
// which allows us to load both without any synchronization.
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
// means that we keep many of the memory pages zeroed (clean).
// This means resize does not immediately increase memory use.
//
// The first (index 0) collision entry can not be referenced so
// it is repurposed to store the number of used collision entries.
//
// The key is initialized to 0 which is reasonable invalid marker
// since the key 0 will never be stored in most entries.  It will
// be stored in the first entry (so we initialize that one to 1).
//
template<typename K, typename V>
class DexCacheArray {
 public:
  using PackedPointerAndSize = uint64_t;
  static constexpr size_t kNoNextEntry = 0;
  static constexpr size_t kNumSizeBits = 6;
  static constexpr size_t kAlignment = 1 << kNumSizeBits;

  // Key-value pair.
  // After the key is set, it can never be modified or removed.
  // This constraint is used to get very cheap synchronization.
  struct ALIGNED(8) Entry {
    std::atomic<K> key;
    std::atomic<K> next;  // Index in the supplemental collision table.
    std::atomic<V> value;
  };

  explicit DexCacheArray(PackedPointerAndSize packed_data_and_size)
      : data_(reinterpret_cast<Entry*>(BitFieldClear(packed_data_and_size, 0, kNumSizeBits))),
        size_(1u << BitFieldExtract(packed_data_and_size, 0, kNumSizeBits)) {
    DCHECK(data_ != nullptr);
  }

  Entry* data() { return data_; }
  size_t size() { return size_; }
  Entry* begin() { return data_; }
  Entry* end() { return begin() + size() + size(); }  // Including collision entries.
  std::atomic<K>& NumCollisions() { return data_[size()].next; }
  size_t IndexOf(K key) { return key & (size() - 1); }
  static K InvalidKey(size_t index) { return index == 0 ? 1 : 0; }

  // Load value from the table.
  // Synchronization is achieved by not allowing deletion.
  ALWAYS_INLINE V Get(uint32_t key) {
    DCHECK_LE(key, std::numeric_limits<K>::max());
    size_t index = IndexOf(key);
    DCHECK_LT(index, size());
    while(true) {
      Entry* entry = &data_[index];
      DCHECK_LT(entry, end());
      if (LIKELY(entry->key.load(std::memory_order_relaxed) == key)) {
        return entry->value.load(std::memory_order_relaxed);
      }
      K next = entry->next.load(std::memory_order_relaxed);
      if (UNLIKELY(next == kNoNextEntry)) {
        return V();
      }
      index = size() + next;
    }
  }

  // Store new value in the table.
  // It will store the value in the primary table or allocate collision entry.
  // Given key will be always added only once, even in the case of a race.
  // The 'resize' argument is set (exactly once) when the table is filled.
  ALWAYS_INLINE bool Set(uint32_t key, V value, /*out*/ bool* resize) {
    DCHECK_LE(key, std::numeric_limits<K>::max());
    size_t index = IndexOf(key);
    DCHECK_LT(index, size());
    DCHECK_NE(key, InvalidKey(index));
    while(true) {
      Entry* entry = &data_[index];
      DCHECK_LT(entry, end());
      K seen_key = InvalidKey(index);
      if (entry->key.compare_exchange_strong(seen_key, key) || seen_key == key) {
        entry->value.store(value);
        return true;  // Value was set.
      }
      // Allocate next entry from the collision table if needed.
      // (we might allocate unnecessarily in case of a race).
      K next = entry->next.load();
      if (next == kNoNextEntry) {
        K collision = NumCollisions().load(std::memory_order_relaxed);  // Get free entry.
        DCHECK_LT(collision, std::numeric_limits<K>::max());  // Can be incremented.
        do {
          if (collision == size()) {
            return false;  // No available space.
          }
        } while (!NumCollisions().compare_exchange_strong(collision, collision + 1));
        *resize = (collision + 1) == size();
        entry->next.compare_exchange_strong(next, collision);
        next = entry->next.load();
      }
      index = size() + next;
    };
  }

  ALWAYS_INLINE static PackedPointerAndSize Allocate(size_t size = 16) {
    DCHECK_NE(size, 0u);
    DCHECK_LE(size - 1, std::numeric_limits<K>::max());
    DCHECK(IsPowerOfTwo(size));
    size_t alloc_size = RoundUp(sizeof(Entry) * 2 * size, kAlignment);
    Entry* data = reinterpret_cast<Entry*>(aligned_alloc(kAlignment, alloc_size));
    CHECK(data != nullptr);
    CHECK(IsAlignedParam(data, kAlignment));
    ZeroAndReleasePages(data, alloc_size);
    PackedPointerAndSize packed = BitFieldInsert(reinterpret_cast<size_t>(data),
                                                 WhichPowerOf2(size), 0, kNumSizeBits);
    DexCacheArray map(packed);
    DCHECK(map.data_ == data);
    DCHECK(map.size_ == size);
    map.data_[0].key.store(InvalidKey(0), std::memory_order_relaxed);
    for (Entry& entry : map) {
      size_t index = &entry - map.begin();
      DCHECK_EQ(entry.key.load(), InvalidKey(index)) << index;
      DCHECK_EQ(entry.next.load(), kNoNextEntry) << index;
    }
    map.NumCollisions().store(1, std::memory_order_relaxed);  // Reserve first entry.
    return packed;
  }

 private:
  Entry* data_;
  size_t size_;
};

void FreeDexCacheArray(void*);

template<typename K, typename V>
class DexCacheMap {
 public:
  DexCacheMap(Object* object, size_t offset) : object_(object), offset_(MemberOffset(offset)) {}

  void Initialize()
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES_SHARED(Locks::dex_lock_) {
    CHECK_EQ(GetPackedField(), 0u);
    SetPackedField(DexCacheArray<K, V>::Allocate());
  }

  void Clear() REQUIRES_SHARED(Locks::mutator_lock_) {
    SetPackedField(DexCacheArray<K, V>::Allocate());
  }

  ALWAYS_INLINE V Get(uint32_t key) REQUIRES_SHARED(Locks::mutator_lock_) {
    return DexCacheArray<K, V>(GetPackedField()).Get(key);
  }

  ALWAYS_INLINE void Set(uint32_t key, V value) REQUIRES_SHARED(Locks::mutator_lock_) {
    DexCacheArray<K, V> map(GetPackedField());
    bool resize = false;
    if (map.Set(key, value, &resize)) {
      WriteBarrier::ForEveryFieldWrite(object_);
    }
    // Resize happens when the last collision entry is filled (and the Set still succeeds).
    // By definition, this happens only once, so only one thread will try to do the resize.
    if (resize) {
      Resize(map.size() * 2);
    }
  }

  template <ReadBarrierOption kReadBarrierOption, typename Visitor>
  inline void VisitDexCachePairs(const Visitor& visitor)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(Locks::heap_bitmap_lock_) {
    uint64_t packed = GetPackedField();
    if (packed != 0) {
      DexCacheArray<K, V> map(packed);
      for (auto& entry : map) {
        VisitAtomicGcRoot<kReadBarrierOption>(visitor, entry.value);
      }
    }
  }

 private:
  void Resize(size_t new_size) REQUIRES_SHARED(Locks::mutator_lock_) {
    uint64_t packed = DexCacheArray<K, V>::Allocate(new_size);
    DexCacheArray<K, V> old_map(GetPackedField());
    DexCacheArray<K, V> new_map(packed);
    DCHECK_GT(new_map.size(), old_map.size());

    size_t used = 0, count = 0;
    for (auto& entry : old_map) {
      K key = entry.key.load(std::memory_order_relaxed);
      if (key != DexCacheArray<K, V>::InvalidKey(count)) {
        V value = entry.value.load(std::memory_order_relaxed);
        bool resize = false;
        new_map.Set(key, value, &resize);
        DCHECK(!resize);
        used++;
      }
      count++;
    }
    SetPackedField(packed);

    if (new_size >= 4096) {
      LOG(VERBOSE) << "DexCache " << object_ << "+" << offset_
          << " resized to " << new_size << " entries"
          << " (was " << (used * 100) / count << "% full).";
    }
  }

  ALWAYS_INLINE uint64_t GetPackedField() REQUIRES_SHARED(Locks::mutator_lock_) {
    return object_->GetField64(offset_);
  }

  ALWAYS_INLINE void SetPackedField(uint64_t new_value)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    uint64_t old_value = GetPackedField();
    bool ok = object_->CasFieldStrongSequentiallyConsistent64<false, false>(offset_,
                                                                            old_value,
                                                                            new_value);
    DCHECK(ok) << "Race when changing DexCache";
    WriteBarrier::ForEveryFieldWrite(object_);
    if (old_value != 0) {
      FreeDexCacheArray(DexCacheArray<K, V>(old_value).data());
    }
  }

  Object* object_;
  MemberOffset offset_;
};

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
}

inline void DexCache::ClearString(dex::StringIndex string_idx) {
  DCHECK(Runtime::Current()->IsAotCompiler());
  ResolvedStrings().Set(string_idx.index_, GcRoot<String>());
}

inline Class* DexCache::GetResolvedType(dex::TypeIndex type_idx) {
  return ResolvedTypes().Get(type_idx.index_).Read();
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

inline void DexCache::SetResolvedType(dex::TypeIndex type_idx, ObjPtr<Class> resolved) {
  DCHECK(resolved != nullptr);
  DCHECK(resolved->IsResolved()) << resolved->GetStatus();
  // TODO default transaction support.
  ResolvedTypes().Set(type_idx.index_, GcRoot<Class>(resolved));
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
