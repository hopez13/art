/*
 * Copyright (C) 2012 The Android Open Source Project
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

#include "art_method-inl.h"
#include "base/callee_save_type.h"
#include "callee_save_frame.h"
#include "class_linker-inl.h"
#include "class_table-inl.h"
#include "dex/dex_file-inl.h"
#include "dex/dex_file_types.h"
#include "entrypoints/entrypoint_utils-inl.h"
#include "gc/heap.h"
#include "mirror/class-inl.h"
#include "mirror/class_loader.h"
#include "mirror/object-inl.h"
#include "mirror/object_array-inl.h"
#include "oat_file.h"
#include "runtime.h"

namespace art {

static void StoreObjectInBss(ArtMethod* caller,
                             const OatFile* oat_file,
                             size_t bss_offset,
                             ObjPtr<mirror::Object> object) REQUIRES_SHARED(Locks::mutator_lock_) {
  // Used for storing Class or String in .bss GC roots.
  static_assert(sizeof(GcRoot<mirror::Class>) == sizeof(GcRoot<mirror::Object>), "Size check.");
  static_assert(sizeof(GcRoot<mirror::String>) == sizeof(GcRoot<mirror::Object>), "Size check.");
  DCHECK_NE(bss_offset, IndexBssMappingLookup::npos);
  DCHECK_ALIGNED(bss_offset, sizeof(GcRoot<mirror::Object>));
  DCHECK_NE(oat_file, nullptr);
  if (UNLIKELY(!oat_file->IsExecutable())) {
    // There are situations where we execute bytecode tied to an oat file opened
    // as non-executable (i.e. the AOT-compiled code cannot be executed) and we
    // can JIT that bytecode and get here without the .bss being mmapped.
    return;
  }
  GcRoot<mirror::Object>* slot = reinterpret_cast<GcRoot<mirror::Object>*>(
      const_cast<uint8_t*>(oat_file->BssBegin() + bss_offset));
  DCHECK_GE(slot, oat_file->GetBssGcRoots().data());
  DCHECK_LT(slot, oat_file->GetBssGcRoots().data() + oat_file->GetBssGcRoots().size());
  if (slot->IsNull()) {
    // This may race with another thread trying to store the very same value but that's OK.
    std::atomic<GcRoot<mirror::Object>>* atomic_slot =
        reinterpret_cast<std::atomic<GcRoot<mirror::Object>>*>(slot);
    static_assert(sizeof(*slot) == sizeof(*atomic_slot), "Size check");
    atomic_slot->store(GcRoot<mirror::Object>(object), std::memory_order_release);
    // We need a write barrier for the class loader that holds the GC roots in the .bss.
    ObjPtr<mirror::ClassLoader> class_loader = caller->GetClassLoader();
    Runtime* runtime = Runtime::Current();
    if (kIsDebugBuild) {
      ClassTable* class_table = runtime->GetClassLinker()->ClassTableForClassLoader(class_loader);
      CHECK(class_table != nullptr && !class_table->InsertOatFile(oat_file))
          << "Oat file with .bss GC roots was not registered in class table: "
          << oat_file->GetLocation();
    }
    if (class_loader != nullptr) {
      WriteBarrier::ForEveryFieldWrite(class_loader);
    } else {
      runtime->GetClassLinker()->WriteBarrierForBootOatFileBssRoots(oat_file);
    }
  } else {
    // Each slot serves to store exactly one Class or String.
    DCHECK_EQ(object, slot->Read());
  }
}

namespace {

// De-duplicates code from both branches of StoreTypeInBss.
static inline void StoreTypeInBssHelper(ArtMethod* caller,
                                        dex::TypeIndex type_idx,
                                        ObjPtr<mirror::Class> resolved_type,
                                        const IndexBssMapping* type_mapping,
                                        const IndexBssMapping* public_type_mapping,
                                        const IndexBssMapping* package_type_mapping,
                                        const OatFile* oat_file)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  const DexFile* dex_file = caller->GetDexFile();
  auto store = [=](const IndexBssMapping* mapping) REQUIRES_SHARED(Locks::mutator_lock_) {
    size_t bss_offset = IndexBssMappingLookup::GetBssOffset(
        mapping, type_idx.index_, dex_file->NumTypeIds(), sizeof(GcRoot<mirror::Class>));
    if (bss_offset != IndexBssMappingLookup::npos) {
      StoreObjectInBss(caller, oat_file, bss_offset, resolved_type);
    }
  };
  store(type_mapping);
  if (resolved_type->IsPublic()) {
    store(public_type_mapping);
  }
  if (resolved_type->IsPublic() || resolved_type->GetClassLoader() == caller->GetClassLoader()) {
    store(package_type_mapping);
  }
}

// De-duplicates code from both branches of StoreStringInBss.
static inline void StoreStringInBssHelper(ArtMethod* caller,
                                          dex::StringIndex string_idx,
                                          ObjPtr<mirror::String> resolved_string,
                                          const IndexBssMapping* string_mapping,
                                          const OatFile* oat_file)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  const DexFile* dex_file = caller->GetDexFile();
  size_t bss_offset = IndexBssMappingLookup::GetBssOffset(
      string_mapping, string_idx.index_, dex_file->NumStringIds(), sizeof(GcRoot<mirror::Class>));
  if (bss_offset != IndexBssMappingLookup::npos) {
    StoreObjectInBss(caller, oat_file, bss_offset, resolved_string);
  }
}

}  // anonymous namespace

static inline void StoreTypeInBss(ArtMethod* caller,
                                  dex::TypeIndex type_idx,
                                  ObjPtr<mirror::Class> resolved_type,
                                  ArtMethod* outer_method) REQUIRES_SHARED(Locks::mutator_lock_) {
  const DexFile* dex_file = caller->GetDexFile();
  DCHECK_NE(dex_file, nullptr);
  // BCP DexFiles case.
  if (caller->GetDeclaringClass()->IsBootStrapClassLoaded()) {
    if (outer_method->GetDexFile()->GetOatDexFile() == nullptr) {
      return;
    }
    const OatFile* oat_file = outer_method->GetDexFile()->GetOatDexFile()->GetOatFile();
    if (oat_file == nullptr) {
      return;
    }
    ArrayRef<const OatFile::BssMappingInfo> mapping_info_vector(oat_file->GetBcpBssInfo());
    ArrayRef<const DexFile* const> bcp_dexfiles(
        Runtime::Current()->GetClassLinker()->GetBootClassPath());
    auto it = std::find_if(bcp_dexfiles.begin(), bcp_dexfiles.end(), [dex_file](const DexFile* df) {
      return &*df == &*dex_file;
    });
    uint32_t dexfile_index = std::distance(bcp_dexfiles.begin(), it);
    // `dexfile_index` could be bigger than the vector size if:
    // A) We have an empty bcp_bss_info. This can happen if:
    //   1) We had no mappings at compile time, or
    //   2) We compiled with multi-image.
    // B) The BCP at runtime contains additional components that we did not compile with.
    // In all of these cases, we don't update the bss entry.
    if (dexfile_index >= mapping_info_vector.size()) {
      return;
    }
    const OatFile::BssMappingInfo mapping_info = mapping_info_vector[dexfile_index];
    StoreTypeInBssHelper(caller,
                         type_idx,
                         resolved_type,
                         mapping_info.type_bss_mapping,
                         mapping_info.public_type_bss_mapping,
                         mapping_info.package_type_bss_mapping,
                         oat_file);
    return;
  }

  // DexFiles compiled together to an oat file case.
  const OatDexFile* oat_dex_file = dex_file->GetOatDexFile();
  if (oat_dex_file != nullptr) {
    StoreTypeInBssHelper(caller,
                         type_idx,
                         resolved_type,
                         oat_dex_file->GetTypeBssMapping(),
                         oat_dex_file->GetPublicTypeBssMapping(),
                         oat_dex_file->GetPackageTypeBssMapping(),
                         oat_dex_file->GetOatFile());
  }
}

static inline void StoreStringInBss(ArtMethod* caller,
                                    dex::StringIndex string_idx,
                                    ObjPtr<mirror::String> resolved_string,
                                    ArtMethod* outer_method) REQUIRES_SHARED(Locks::mutator_lock_) {
  const DexFile* dex_file = caller->GetDexFile();
  DCHECK_NE(dex_file, nullptr);
  // BCP DexFiles case.
  if (caller->GetDeclaringClass()->IsBootStrapClassLoaded()) {
    if (outer_method->GetDexFile()->GetOatDexFile() == nullptr) {
      return;
    }
    const OatFile* oat_file = outer_method->GetDexFile()->GetOatDexFile()->GetOatFile();
    if (oat_file == nullptr) {
      return;
    }
    ArrayRef<const OatFile::BssMappingInfo> mapping_info_vector(oat_file->GetBcpBssInfo());
    ArrayRef<const DexFile* const> bcp_dexfiles(
        Runtime::Current()->GetClassLinker()->GetBootClassPath());
    auto it = std::find_if(bcp_dexfiles.begin(), bcp_dexfiles.end(), [dex_file](const DexFile* df) {
      return &*df == &*dex_file;
    });
    uint32_t dexfile_index = std::distance(bcp_dexfiles.begin(), it);
    // `dexfile_index` could be bigger than the vector size if:
    // A) We have an empty bcp_bss_info. This can happen if:
    //   1) We had no mappings at compile time, or
    //   2) We compiled with multi-image.
    // B) The BCP at runtime contains additional components that we did not compile with.
    // In all of these cases, we don't update the bss entry.
    if (dexfile_index >= mapping_info_vector.size()) {
      return;
    }
    const OatFile::BssMappingInfo mapping_info = mapping_info_vector[dexfile_index];
    StoreStringInBssHelper(
        caller, string_idx, resolved_string, mapping_info.string_bss_mapping, oat_file);
    return;
  }

  // DexFiles compiled together to an oat file case.
  const OatDexFile* oat_dex_file = dex_file->GetOatDexFile();
  if (oat_dex_file != nullptr) {
    StoreStringInBssHelper(caller,
                           string_idx,
                           resolved_string,
                           oat_dex_file->GetStringBssMapping(),
                           oat_dex_file->GetOatFile());
  }
}

static ALWAYS_INLINE bool CanReferenceBss(ArtMethod* outer_method, ArtMethod* caller)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  // .bss references are used only for AOT-compiled code. As we do not want to check if the call is
  // coming from AOT-compiled code (that could be expensive), we can simply check if the caller has
  // the same dex file.
  //
  // When we are JIT compiling, if the caller and outer method have the same dex file we may or may
  // not find a .bss slot to update; if we do, this can still benefit AOT-compiled code executed
  // later.
  const DexFile* outer_dex_file = outer_method->GetDexFile();
  const DexFile* caller_dex_file = caller->GetDexFile();
  if (outer_dex_file == caller_dex_file) {
    return true;
  }

  // We allow AOT-compiled code to reference .bss slots for all dex files compiled together to an
  // oat file, ...
  const bool compiled_together = caller_dex_file->GetOatDexFile() != nullptr &&
                                 outer_dex_file->GetOatDexFile() != nullptr &&
                                 caller_dex_file->GetOatDexFile()->GetOatFile() ==
                                     outer_dex_file->GetOatDexFile()->GetOatFile();

  // or if it is an inlined BCP DexFile.
  const bool bcp_inline = outer_dex_file->GetOatDexFile() != nullptr &&
                          outer_dex_file->GetOatDexFile()->GetOatFile() != nullptr &&
                          caller->GetDeclaringClass()->IsBootStrapClassLoaded();

  return compiled_together || bcp_inline;
}

extern "C" mirror::Class* artInitializeStaticStorageFromCode(mirror::Class* klass, Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  // Called to ensure static storage base is initialized for direct static field reads and writes.
  // A class may be accessing another class' fields when it doesn't have access, as access has been
  // given by inheritance.
  ScopedQuickEntrypointChecks sqec(self);
  DCHECK(klass != nullptr);
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  StackHandleScope<1> hs(self);
  Handle<mirror::Class> h_klass = hs.NewHandle(klass);
  bool success = class_linker->EnsureInitialized(
      self, h_klass, /* can_init_fields= */ true, /* can_init_parents= */ true);
  if (UNLIKELY(!success)) {
    return nullptr;
  }
  return h_klass.Get();
}

extern "C" mirror::Class* artResolveTypeFromCode(uint32_t type_idx, Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  // Called when the .bss slot was empty or for main-path runtime call.
  ScopedQuickEntrypointChecks sqec(self);
  auto caller_and_outer = GetCalleeSaveMethodCallerAndOuterMethod(
      self, CalleeSaveType::kSaveEverythingForClinit);
  ArtMethod* caller = caller_and_outer.caller;
  ObjPtr<mirror::Class> result = ResolveVerifyAndClinit(dex::TypeIndex(type_idx),
                                                        caller,
                                                        self,
                                                        /* can_run_clinit= */ false,
                                                        /* verify_access= */ false);
  ArtMethod* outer_method = caller_and_outer.caller;
  if (LIKELY(result != nullptr) && CanReferenceBss(outer_method, caller)) {
    StoreTypeInBss(caller, dex::TypeIndex(type_idx), result, outer_method);
  }
  return result.Ptr();
}

extern "C" mirror::Class* artResolveTypeAndVerifyAccessFromCode(uint32_t type_idx, Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  // Called when caller isn't guaranteed to have access to a type.
  ScopedQuickEntrypointChecks sqec(self);
  auto caller_and_outer = GetCalleeSaveMethodCallerAndOuterMethod(self,
                                                                  CalleeSaveType::kSaveEverything);
  ArtMethod* caller = caller_and_outer.caller;
  ObjPtr<mirror::Class> result = ResolveVerifyAndClinit(dex::TypeIndex(type_idx),
                                                        caller,
                                                        self,
                                                        /* can_run_clinit= */ false,
                                                        /* verify_access= */ true);
  ArtMethod* outer_method = caller_and_outer.caller;
  if (LIKELY(result != nullptr) && CanReferenceBss(outer_method, caller)) {
    StoreTypeInBss(caller, dex::TypeIndex(type_idx), result, outer_method);
  }
  return result.Ptr();
}

extern "C" mirror::MethodHandle* artResolveMethodHandleFromCode(uint32_t method_handle_idx,
                                                                Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ScopedQuickEntrypointChecks sqec(self);
  auto caller_and_outer =
      GetCalleeSaveMethodCallerAndOuterMethod(self, CalleeSaveType::kSaveEverything);
  ArtMethod* caller = caller_and_outer.caller;
  ObjPtr<mirror::MethodHandle> result = ResolveMethodHandleFromCode(caller, method_handle_idx);
  return result.Ptr();
}

extern "C" mirror::MethodType* artResolveMethodTypeFromCode(uint32_t proto_idx, Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ScopedQuickEntrypointChecks sqec(self);
  auto caller_and_outer = GetCalleeSaveMethodCallerAndOuterMethod(self,
                                                                  CalleeSaveType::kSaveEverything);
  ArtMethod* caller = caller_and_outer.caller;
  ObjPtr<mirror::MethodType> result = ResolveMethodTypeFromCode(caller, dex::ProtoIndex(proto_idx));
  return result.Ptr();
}

extern "C" mirror::String* artResolveStringFromCode(int32_t string_idx, Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ScopedQuickEntrypointChecks sqec(self);
  auto caller_and_outer = GetCalleeSaveMethodCallerAndOuterMethod(self,
                                                                  CalleeSaveType::kSaveEverything);
  ArtMethod* caller = caller_and_outer.caller;
  ObjPtr<mirror::String> result =
      Runtime::Current()->GetClassLinker()->ResolveString(dex::StringIndex(string_idx), caller);
  ArtMethod* outer_method = caller_and_outer.caller;
  if (LIKELY(result != nullptr) && CanReferenceBss(outer_method, caller)) {
    StoreStringInBss(caller, dex::StringIndex(string_idx), result, outer_method);
  }
  return result.Ptr();
}

}  // namespace art
