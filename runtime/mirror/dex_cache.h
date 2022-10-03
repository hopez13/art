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

#include <atomic>
#include "array.h"
#include "base/array_ref.h"
#include "base/bit_utils.h"
#include "base/locks.h"
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

template <typename T> struct PACKED(8) DexCachePair {
  using Type = T;

  GcRoot<T> object;
  uint32_t index;
  // The array is initially [ {0,0}, {0,0}, {0,0} ... ]
  // We maintain the invariant that once a dex cache entry is populated,
  // the pointer is always non-0
  // Any given entry would thus be:
  // {non-0, non-0} OR {0,0}
  //
  // It's generally sufficiently enough then to check if the
  // lookup index matches the stored index (for a >0 lookup index)
  // because if it's true the pointer is also non-null.
  //
  // For the 0th entry which is a special case, the value is either
  // {0,0} (initial state) or {non-0, 0} which indicates
  // that a valid object is stored at that index for a dex section id of 0.
  //
  // As an optimization, we want to avoid branching on the object pointer since
  // it's always non-null if the id branch succeeds (except for the 0th id).
  // Set the initial state for the 0th entry to be {0,1} which is guaranteed to fail
  // the lookup id == stored id branch.
  DexCachePair(ObjPtr<T> object, uint32_t index);
  DexCachePair() : index(0) {}
  DexCachePair(const DexCachePair<T>&) = default;
  DexCachePair& operator=(const DexCachePair<T>&) = default;

  static void Initialize(std::atomic<DexCachePair<T>>* dex_cache);

  static uint32_t InvalidIndexForSlot(uint32_t slot) {
    // Since the cache size is a power of two, 0 will always map to slot 0.
    // Use 1 for slot 0 and 0 for all other slots.
    return (slot == 0) ? 1u : 0u;
  }

  static DexCachePair Load(std::atomic<DexCachePair>* array, size_t idx) {
    return array[idx].load(std::memory_order_relaxed);
  }

  static void Store(std::atomic<DexCachePair>* array, size_t idx, DexCachePair pair) {
    array[idx].store(pair, std::memory_order_relaxed);
  }

  T* GetObjectForIndex(uint32_t idx) REQUIRES_SHARED(Locks::mutator_lock_);
};

template <typename T> struct PACKED(2 * __SIZEOF_POINTER__) NativeDexCachePair {
  using Type = T;

  T* object;
  size_t index;
  // This is similar to DexCachePair except that we're storing a native pointer
  // instead of a GC root. See DexCachePair for the details.
  NativeDexCachePair(T* object, uint32_t index)
      : object(object),
        index(index) {}
  NativeDexCachePair() : object(nullptr), index(0u) { }
  NativeDexCachePair(const NativeDexCachePair<T>&) = default;
  NativeDexCachePair& operator=(const NativeDexCachePair<T>&) = default;

  static void Initialize(std::atomic<NativeDexCachePair<T>>* dex_cache);

  static uint32_t InvalidIndexForSlot(uint32_t slot) {
    // Since the cache size is a power of two, 0 will always map to slot 0.
    // Use 1 for slot 0 and 0 for all other slots.
    return (slot == 0) ? 1u : 0u;
  }

  static NativeDexCachePair Load(std::atomic<NativeDexCachePair>* array, size_t idx);

  static void Store(std::atomic<NativeDexCachePair>* array, size_t idx, NativeDexCachePair pair);

  T* GetObjectForIndex(uint32_t idx) REQUIRES_SHARED(Locks::mutator_lock_) {
    if (idx != index) {
      return nullptr;
    }
    DCHECK(object != nullptr);
    return object;
  }
};

using TypeDexCachePair = DexCachePair<Class>;
using TypeDexCacheType = std::atomic<TypeDexCachePair>;

using StringDexCachePair = DexCachePair<String>;
using StringDexCacheType = std::atomic<StringDexCachePair>;

using CallSiteDexCachePair = DexCachePair<CallSite>;
using CallSiteDexCacheType = std::atomic<CallSiteDexCachePair>;

using FieldDexCachePair = NativeDexCachePair<ArtField>;
using FieldDexCacheType = std::atomic<FieldDexCachePair>;

using MethodDexCachePair = NativeDexCachePair<ArtMethod>;
using MethodDexCacheType = std::atomic<MethodDexCachePair>;

using MethodTypeDexCachePair = DexCachePair<MethodType>;
using MethodTypeDexCacheType = std::atomic<MethodTypeDexCachePair>;

// C++ mirror of java.lang.DexCache.
class MANAGED DexCache final : public Object {
 public:
  MIRROR_CLASS("Ljava/lang/DexCache;");

  static constexpr size_t kDefaultCacheSize = 1024;

  template<typename Pair, size_t kCacheSize = kDefaultCacheSize>
  class Array {
   public:
    Array(DexCache* dex_cache, size_t data_offset, size_t size_offset):
      dex_cache_(dex_cache),
      data_offset_(data_offset),
      size_offset_(size_offset) {}

    template<bool kAllocateIfNull = false>
    ALWAYS_INLINE std::atomic<Pair>* Data() REQUIRES_SHARED(Locks::mutator_lock_) {
      auto data = dex_cache_->GetFieldPtr<std::atomic<Pair>*>(data_offset_);
      if (kAllocateIfNull && UNLIKELY(data == nullptr)) {
        data = dex_cache_->AllocArray<std::atomic<Pair>>(data_offset_, size_offset_);
      }
      return data;
    }

    ALWAYS_INLINE size_t Size() REQUIRES_SHARED(Locks::mutator_lock_) {
      return dex_cache_->GetField32<kDefaultVerifyFlags>(size_offset_);
    }

    ALWAYS_INLINE typename Pair::Type* Get(uint32_t index) REQUIRES_SHARED(Locks::mutator_lock_) {
      auto data = Data();
      if (UNLIKELY(data == nullptr)) {
        return nullptr;
      }
      return Pair::Load(data, Slot(index)).GetObjectForIndex(index);
    }

    template<typename T>
    ALWAYS_INLINE void Set(uint32_t index, T value) REQUIRES_SHARED(Locks::mutator_lock_) {
      DCHECK(value != nullptr);
      auto data = Data</*kAllocateIfNull=*/true>();
      Pair::Store(data, Slot(index), Pair(value, index));
    }

    ALWAYS_INLINE void Clear(uint32_t index) REQUIRES_SHARED(Locks::mutator_lock_) {
      auto data = Data();
      if (UNLIKELY(data == nullptr)) {
        return;
      }
      // This is racy but should only be called from the transactional interpreter.
      uint32_t slot_idx = Slot(index);
      Pair pair = Pair::Load(data, slot_idx);
      if (pair.index == index) {
        Pair::Store(data, slot_idx, Pair(nullptr, Pair::InvalidIndexForSlot(slot_idx)));
      }
    }

    ALWAYS_INLINE void Reset(std::atomic<Pair>* data = nullptr, size_t size = 0)
        REQUIRES_SHARED(Locks::mutator_lock_) {
      dex_cache_->SetFieldPtr<false>(data_offset_, data);
      dex_cache_->SetField32<false>(size_offset_, std::max(size, kCacheSize));
    }

   private:
    ALWAYS_INLINE size_t Slot(uint32_t index) REQUIRES_SHARED(Locks::mutator_lock_) {
      size_t slot = index % kCacheSize;
      DCHECK_LT(slot, Size());
      return slot;
    }

    DexCache* dex_cache_;
    MemberOffset data_offset_;
    MemberOffset size_offset_;
  };

  // Size of java.lang.DexCache.class.
  static uint32_t ClassSize(PointerSize pointer_size);

  // Size of an instance of java.lang.DexCache not including referenced values.
  static constexpr uint32_t InstanceSize() {
    return sizeof(DexCache);
  }

  void Initialize(const DexFile* dex_file, ObjPtr<ClassLoader> class_loader)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(Locks::dex_lock_);

  // Zero all array references.
  // WARNING: This does not free the memory since it is in LinearAlloc.
  void ResetNativeArrays() REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  ObjPtr<String> GetLocation() REQUIRES_SHARED(Locks::mutator_lock_);

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

  ALWAYS_INLINE void SetResolvedMethodType(dex::ProtoIndex proto_idx, ObjPtr<MethodType> resolved)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Clear a method type for proto_idx, used to undo method type resolution
  // in aborted transactions to make sure the method type isn't kept live.
  void ClearMethodType(dex::ProtoIndex proto_idx) REQUIRES_SHARED(Locks::mutator_lock_);

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

  // Always give callsites unlimited size since we don't want to evict them.
  ALWAYS_INLINE Array<CallSiteDexCachePair, (1u << 31)> ResolvedCallSites() {
    return {this, offsetof(DexCache, resolved_call_sites_), offsetof(DexCache, num_resolved_call_sites_)};
  }

  ALWAYS_INLINE Array<FieldDexCachePair> ResolvedFields() {
    return {this, offsetof(DexCache, resolved_fields_), offsetof(DexCache, num_resolved_fields_)};
  }

  ALWAYS_INLINE Array<MethodTypeDexCachePair> ResolvedMethodTypes() {
    return {this, offsetof(DexCache, resolved_method_types_), offsetof(DexCache, num_resolved_method_types_)};
  }

  ALWAYS_INLINE Array<MethodDexCachePair> ResolvedMethods() {
    return {this, offsetof(DexCache, resolved_methods_), offsetof(DexCache, num_resolved_methods_)};
  }

  ALWAYS_INLINE Array<TypeDexCachePair> ResolvedTypes() {
    return {this, offsetof(DexCache, resolved_types_), offsetof(DexCache, num_resolved_types_)};
  }

  ALWAYS_INLINE Array<StringDexCachePair> ResolvedStrings() {
    return {this, offsetof(DexCache, preresolved_strings_), offsetof(DexCache, num_preresolved_strings_)};
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

  ObjPtr<ClassLoader> GetClassLoader() REQUIRES_SHARED(Locks::mutator_lock_);

  template <VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
            ReadBarrierOption kReadBarrierOption = kWithReadBarrier,
            typename Visitor>
  void VisitNativeRoots(const Visitor& visitor)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(Locks::heap_bitmap_lock_);

 private:
  // Allocate new array in linear alloc and save it in the given fields.
  template<typename T>
  T* AllocArray(MemberOffset obj_offset, MemberOffset num_offset)
     REQUIRES_SHARED(Locks::mutator_lock_);

  // Visit instance fields of the dex cache as well as its associated arrays.
  template <bool kVisitNativeRoots,
            VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
            ReadBarrierOption kReadBarrierOption = kWithReadBarrier,
            typename Visitor>
  void VisitReferences(ObjPtr<Class> klass, const Visitor& visitor)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(Locks::heap_bitmap_lock_);

  HeapReference<ClassLoader> class_loader_;
  HeapReference<String> location_;

  uint64_t dex_file_;                // const DexFile*

  // Note that all arrays are allocated lazily on first use.
  uint64_t preresolved_strings_;     // GcRoot<mirror::String*> array with num_preresolved_strings
                                     // elements.
  uint64_t resolved_call_sites_;     // GcRoot<CallSite>* array with num_resolved_call_sites_
                                     // elements.
  uint64_t resolved_fields_;         // std::atomic<FieldDexCachePair>*, array with
                                     // num_resolved_fields_ elements.
  uint64_t resolved_method_types_;   // std::atomic<MethodTypeDexCachePair>* array with
                                     // num_resolved_method_types_ elements.
  uint64_t resolved_methods_;        // ArtMethod*, array with num_resolved_methods_ elements.
  uint64_t resolved_types_;          // TypeDexCacheType*, array with num_resolved_types_ elements.
  uint64_t strings_;                 // std::atomic<StringDexCachePair>*, array with num_strings_
                                     // elements.

  // Number of elements allocated or to be allocated in the arrays above.
  // This is valid even if the array was not allocated yet (nullptr).
  uint32_t num_preresolved_strings_;    // Number of elements in the preresolved_strings_ array.
  uint32_t num_resolved_call_sites_;    // Number of elements in the call_sites_ array.
  uint32_t num_resolved_fields_;        // Number of elements in the resolved_fields_ array.
  uint32_t num_resolved_method_types_;  // Number of elements in the resolved_method_types_ array.
  uint32_t num_resolved_methods_;       // Number of elements in the resolved_methods_ array.
  uint32_t num_resolved_types_;         // Number of elements in the resolved_types_ array.
  uint32_t num_strings_;                // Number of elements in the strings_ array.

  friend struct art::DexCacheOffsets;  // for verifying offset information
  friend class linker::ImageWriter;
  friend class Object;  // For VisitReferences
  DISALLOW_IMPLICIT_CONSTRUCTORS(DexCache);
};

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_DEX_CACHE_H_
