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
class LinearAlloc;
class ReflectiveValueVisitor;

template<typename K, typename V>
class DexCacheMap;

namespace mirror {

class CallSite;
class Class;
class ClassLoader;
class MethodType;
class String;

// C++ mirror of java.lang.DexCache.
class MANAGED DexCache final : public Object {
 public:
  MIRROR_CLASS("Ljava/lang/DexCache;");

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

  void Clear() REQUIRES_SHARED(Locks::mutator_lock_);

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

  ALWAYS_INLINE void SetResolvedMethodType(dex::ProtoIndex proto_idx, MethodType* resolved)
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

  ALWAYS_INLINE DexCacheMap<uint16_t, GcRoot<CallSite>> ResolvedCallSites();

  ALWAYS_INLINE DexCacheMap<uint16_t, ArtField*> ResolvedFields();

  ALWAYS_INLINE DexCacheMap<uint16_t, GcRoot<MethodType>> ResolvedMethodTypes();

  ALWAYS_INLINE DexCacheMap<uint16_t, ArtMethod*> ResolvedMethods();

  ALWAYS_INLINE DexCacheMap<uint16_t, GcRoot<Class>> ResolvedTypes();

  ALWAYS_INLINE DexCacheMap<uint32_t, GcRoot<String>> ResolvedStrings();

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
  uint64_t preresolved_strings_;  // UNUSED.
  uint64_t resolved_call_sites_;
  uint64_t resolved_fields_;
  uint64_t resolved_method_types_;
  uint64_t resolved_methods_;
  uint64_t resolved_types_;
  uint64_t strings_;

  uint32_t num_preresolved_strings_;  // UNUSED.
  uint32_t num_resolved_call_sites_;  // UNUSED.
  uint32_t num_resolved_fields_;  // UNUSED.
  uint32_t num_resolved_method_types_;  // UNUSED.
  uint32_t num_resolved_methods_;  // UNUSED.
  uint32_t num_resolved_types_;  // UNUSED.
  uint32_t num_strings_;  // UNUSED.

  friend struct art::DexCacheOffsets;  // for verifying offset information
  friend class linker::ImageWriter;
  friend class Object;  // For VisitReferences
  DISALLOW_IMPLICIT_CONSTRUCTORS(DexCache);
};

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_DEX_CACHE_H_
