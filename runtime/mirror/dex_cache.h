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

#ifndef ART_RUNTIME_MIRROR_DEX_CACHE_H_
#define ART_RUNTIME_MIRROR_DEX_CACHE_H_

#include <cstdlib>
#include <map>

#include "array.h"
#include "base/bit_utils.h"
#include "base/locks.h"
#include "base/mem_map.h"
#include "dex/dex_file_types.h"
#include "gc_root.h"  // Note: must not use -inl here to avoid circular dependency.
#include "object.h"
#include "object_array.h"

namespace art {

namespace linker {
class ImageWriter;
}  // namespace linker

class ArtField;
class ArtMethod;
struct DexCacheOffsets;
class DexFile;
union JValue;
class LinearAlloc;
class ReflectiveValueVisitor;
class Thread;

namespace mirror {

class CallSite;
class Class;
class ClassLoader;
class MethodType;
class String;

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
  static constexpr size_t kNoNextEntry = 0;
  static constexpr size_t kNumSizeBits = 6;
  static constexpr size_t kAlignment = 1 << kNumSizeBits;

  // Key-value pair.
  // All fields can only be written once after initialization.
  // This constraint is used to get very cheap synchronization.
  struct ALIGNED(8) Entry {
    std::atomic<K> key;
    std::atomic<K> next;  // Index in the supplemental collision table.
    std::atomic<V> value;
  };

  explicit DexCacheArray(uint64_t packed_data_and_size)
      : data_(reinterpret_cast<Entry*>(BitFieldClear(packed_data_and_size, 0, kNumSizeBits))),
        size_(1u << BitFieldExtract(packed_data_and_size, 0, kNumSizeBits)) {
    DCHECK(data_ != nullptr);
  }

  Entry* begin() { return data_; }
  size_t size() { return size_; }
  Entry* end() { return begin() + size() + size(); }
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
  ALWAYS_INLINE bool Set(uint32_t key, V value) {
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
        do {
          if (collision == size()) {
            return false;  // No available space.
          }
        } while (!NumCollisions().compare_exchange_strong(collision, collision + 1));
        entry->next.compare_exchange_strong(next, collision);
        next = entry->next.load();
      }
      index = size() + next;
    };
  }

  ALWAYS_INLINE static uint64_t Allocate(size_t size = 16) {
    DCHECK_NE(size, 0u);
    DCHECK_LE(size - 1, std::numeric_limits<K>::max());
    DCHECK(IsPowerOfTwo(size));
    size_t alloc_size = RoundUp(sizeof(Entry) * 2 * size, kAlignment);
    Entry* data = reinterpret_cast<Entry*>(aligned_alloc(kAlignment, alloc_size));
    CHECK(data != nullptr);
    CHECK(IsAlignedParam(data, kAlignment));
    ZeroAndReleasePages(data, alloc_size);
    uint64_t packed = BitFieldInsert(reinterpret_cast<size_t>(data),
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

template<typename K, typename V>
class DexCacheMap {
 public:
  DexCacheMap(Object* object, size_t offset) : object_(object), offset_(MemberOffset(offset)) {}

  void EnsureInitialized()
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES_SHARED(Locks::dex_cache_lock_) {
    if (GetPackedField() == 0) {
      SetPackedField(DexCacheArray<K, V>::Allocate());
    }
  }

  void Clear() REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES_SHARED(Locks::dex_cache_lock_) {
    SetPackedField(DexCacheArray<K, V>::Allocate());
  }

  void Reset() REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES_SHARED(Locks::dex_cache_lock_) {
    SetPackedField(0);
  }

  ALWAYS_INLINE V Get(uint32_t key) REQUIRES_SHARED(Locks::mutator_lock_) {
    return DexCacheArray<K, V>(GetPackedField()).Get(key);
  }

  ALWAYS_INLINE void Set(uint32_t key, V value) REQUIRES_SHARED(Locks::mutator_lock_) {
    DexCacheArray<K, V> map(GetPackedField());
    if (!map.Set(key, value)) {
      Resize(map.size() * 2);
      DexCacheArray<K, V>(GetPackedField()).Set(key, value);
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
  void Resize(size_t new_size) REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

  ALWAYS_INLINE uint64_t GetPackedField() REQUIRES_SHARED(Locks::mutator_lock_) {
    return object_->GetField64(offset_);
  }

  ALWAYS_INLINE void SetPackedField(uint64_t packed)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES_SHARED(Locks::dex_cache_lock_) {
    object_->SetField64</*kTransactionActive=*/false,
                        /*kCheckTransaction=*/false>(offset_, packed);
    // TODO: free old data.
    // TODO: write barrier?
  }

  Object* object_;
  MemberOffset offset_;
};

// C++ mirror of java.lang.DexCache.
class MANAGED DexCache final : public Object {
 public:
  // Size of java.lang.DexCache.class.
  static uint32_t ClassSize(PointerSize pointer_size);

  // Size of an instance of java.lang.DexCache not including referenced values.
  static constexpr uint32_t InstanceSize() {
    return sizeof(DexCache);
  }

  void EnsureInitialized()
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(Locks::dex_lock_);

  void ResetNativeFileds() REQUIRES_SHARED(Locks::mutator_lock_);

  void Clear() REQUIRES_SHARED(Locks::mutator_lock_);

  ObjPtr<String> GetLocation() REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE DexCacheMap<uint16_t, ArtField*> ResolvedFields() {
    return DexCacheMap<uint16_t, ArtField*>(this, offsetof(DexCache, resolved_fields_));
  }

  ALWAYS_INLINE DexCacheMap<uint16_t, GcRoot<MethodType>> ResolvedMethodTypes() {
    return DexCacheMap<uint16_t, GcRoot<MethodType>>(this, offsetof(DexCache, resolved_method_types_));
  }

  ALWAYS_INLINE DexCacheMap<uint16_t, ArtMethod*> ResolvedMethods() {
    return DexCacheMap<uint16_t, ArtMethod*>(this, offsetof(DexCache, resolved_methods_));
  }

  ALWAYS_INLINE DexCacheMap<uint16_t, GcRoot<Class>> ResolvedTypes() {
    return DexCacheMap<uint16_t, GcRoot<Class>>(this, offsetof(DexCache, resolved_types_));
  }

  ALWAYS_INLINE DexCacheMap<uint32_t, GcRoot<String>> ResolvedStrings() {
    return DexCacheMap<uint32_t, GcRoot<String>>(this, offsetof(DexCache, resolved_strings_));
  }

  ALWAYS_INLINE String* GetResolvedString(dex::StringIndex string_idx)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE void SetResolvedString(dex::StringIndex string_idx, ObjPtr<mirror::String> resolved)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Clear a string for a string_idx, used to undo string intern transactions to make sure
  // the string isn't kept live.
  ALWAYS_INLINE void ClearString(dex::StringIndex string_idx)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE Class* GetResolvedType(dex::TypeIndex type_idx)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE void SetResolvedType(dex::TypeIndex type_idx, ObjPtr<Class> resolved)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE void ClearResolvedType(dex::TypeIndex type_idx)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE ArtMethod* GetResolvedMethod(uint32_t method_idx)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE void SetResolvedMethod(uint32_t method_idx, ArtMethod* resolved)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE ArtField* GetResolvedField(uint32_t idx)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE void SetResolvedField(uint32_t idx, ArtField* field)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE MethodType* GetResolvedMethodType(dex::ProtoIndex proto_idx)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE void SetResolvedMethodType(dex::ProtoIndex proto_idx, MethodType* resolved)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE CallSite* GetResolvedCallSite(uint32_t call_site_idx)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Attempts to bind |call_site_idx| to the call site |resolved|. The
  // caller must use the return value in place of |resolved|. This is
  // because multiple threads can invoke the bootstrap method each
  // producing a call site, but the method handle invocation on the
  // call site must be on a common agreed value.
  ALWAYS_INLINE ObjPtr<CallSite> SetResolvedCallSite(uint32_t call_site_idx,
                                                     ObjPtr<CallSite> resolved)
      REQUIRES_SHARED(Locks::mutator_lock_) WARN_UNUSED;

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE std::atomic<GcRoot<CallSite>>* GetResolvedCallSites()
      REQUIRES_SHARED(Locks::mutator_lock_) {
    return reinterpret_cast<std::atomic<GcRoot<CallSite>>*>(resolved_call_sites_);
  }

  ALWAYS_INLINE const DexFile* GetDexFile() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetFieldPtr<const DexFile*>(OFFSET_OF_OBJECT_MEMBER(DexCache, dex_file_));
  }

  void SetDexFile(const DexFile* dex_file) REQUIRES_SHARED(Locks::mutator_lock_) {
    SetFieldPtr<false>(OFFSET_OF_OBJECT_MEMBER(DexCache, dex_file_), dex_file);
  }

  void SetLocation(ObjPtr<String> location) REQUIRES_SHARED(Locks::mutator_lock_);

  void VisitReflectiveTargets(ReflectiveValueVisitor* visitor) REQUIRES(Locks::mutator_lock_);

  void SetClassLoader(ObjPtr<ClassLoader> class_loader) REQUIRES_SHARED(Locks::mutator_lock_);

 private:
  // Visit instance fields of the dex cache as well as its associated arrays.
  template <bool kVisitNativeRoots,
            VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
            ReadBarrierOption kReadBarrierOption = kWithReadBarrier,
            typename Visitor>
  void VisitReferences(ObjPtr<Class> klass, const Visitor& visitor)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(Locks::heap_bitmap_lock_);

  HeapReference<ClassLoader> class_loader_;
  HeapReference<String> location_;

  uint64_t dex_file_;  // const DexFile*
  uint64_t preresolved_strings_; // UNUSED.
  uint64_t resolved_call_sites_;
  uint64_t resolved_fields_;
  uint64_t resolved_method_types_;
  uint64_t resolved_methods_;
  uint64_t resolved_types_;
  uint64_t resolved_strings_;

  uint32_t num_preresolved_strings_; // UNUSED.
  uint32_t num_resolved_call_sites_; // UNUSED.
  uint32_t num_resolved_fields_; // UNUSED.
  uint32_t num_resolved_method_types_; // UNUSED.
  uint32_t num_resolved_methods_; // UNUSED.
  uint32_t num_resolved_types_; // UNUSED.
  uint32_t num_strings_; // UNUSED.

  friend struct art::DexCacheOffsets;  // for verifying offset information
  friend class linker::ImageWriter;
  friend class Object;  // For VisitReferences
  DISALLOW_IMPLICIT_CONSTRUCTORS(DexCache);
};

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_DEX_CACHE_H_
