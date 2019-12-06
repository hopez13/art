/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef ART_RUNTIME_MIRROR_CLASS_LOADER_INL_H_
#define ART_RUNTIME_MIRROR_CLASS_LOADER_INL_H_

#include "class_loader.h"

#include "array.h"
#include "class_table-inl.h"
#include "jni/jni_internal.h"
#include "object-inl.h"
#include "object_array-inl.h"
#include "well_known_classes.h"

namespace art {
namespace mirror {

inline ObjPtr<ClassLoader> ClassLoader::GetParent() {
  return GetFieldObject<ClassLoader>(OFFSET_OF_OBJECT_MEMBER(ClassLoader, parent_));
}

template <bool kVisitClasses,
          VerifyObjectFlags kVerifyFlags,
          ReadBarrierOption kReadBarrierOption,
          typename Visitor>
inline void ClassLoader::VisitReferences(ObjPtr<mirror::Class> klass, const Visitor& visitor) {
  // Visit instance fields first.
  VisitInstanceFieldsReferences<kVerifyFlags, kReadBarrierOption>(klass, visitor);
  if (kVisitClasses) {
    // Visit classes loaded after.
    ClassTable* const class_table = GetClassTable<kVerifyFlags>();
    if (class_table != nullptr) {
      class_table->VisitRoots(visitor);
    }
  }
}

template<typename Visitor>
void ClassLoader::VisitDexFiles(Visitor vis) {
  // Get all the ArtFields so we can look in the BaseDexClassLoader
  art::ArtField* path_list_field = jni::DecodeArtField(
      WellKnownClasses::dalvik_system_BaseDexClassLoader_pathList);
  ObjPtr<Class> base_dex_class_loader(path_list_field->GetDeclaringClass());
  if (!InstanceOf(base_dex_class_loader)) {
    return;
  }
  ObjPtr<Object> path_list(path_list_field->GetObject(this));
  if (path_list.IsNull()) {
    // No dex-files yet.
    return;
  }
  art::ArtField* dex_path_list_element_field =
      jni::DecodeArtField(WellKnownClasses::dalvik_system_DexPathList_dexElements);
  ObjPtr<ObjectArray<Object>> dex_elements(dex_path_list_element_field->GetObject(path_list)->AsObjectArray<Object>());
  if (dex_elements.IsNull()) {
    // No dex-files yet.
    return;
  }
  art::ArtField* element_dex_file_field = art::jni::DecodeArtField(
      art::WellKnownClasses::dalvik_system_DexPathList__Element_dexFile);
  art::ArtField* dex_file_cookie_field = art::jni::DecodeArtField(
      art::WellKnownClasses::dalvik_system_DexFile_cookie);
  // Index 0 is the OatDex (if present) so skip it.
  constexpr int32_t kFirstDexFileIndex = 1;
  for (auto element : dex_elements->Iterate()) {
    ObjPtr<Object> dex_file_obj(element_dex_file_field->GetObject(element));
    if (!dex_file_obj.IsNull()) {
      ObjPtr<LongArray> cookie(dex_file_cookie_field->GetObject(dex_file_obj)->AsLongArray());
      for (int32_t i = kFirstDexFileIndex; i < cookie->GetLength(); i++) {
        // This must match the casts in
        // runtime/native/dalvik_system_DexFile.cc:ConvertDexFilesToJavaArray
        vis(reinterpret_cast<const DexFile*>(static_cast<intptr_t>(cookie->Get(i))));
      }
    }
  }
}

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_CLASS_LOADER_INL_H_
