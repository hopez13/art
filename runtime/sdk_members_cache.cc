/*
 * Copyright (C) 2022 The Android Open Source Project
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

#include "sdk_members_cache.h"

#include "art_field-inl.h"
#include "art_method.h"
#include "dex/dex_file-inl.h"
#include "dex/utf.h"
#include "mirror/class.h"

namespace art {

static size_t ComputeSdkFieldHash(const DexFile& dex_file,
                                  const dex::FieldId& field_id,
                                  const dex::TypeId& type_id) {
  size_t hash = ComputeModifiedUtf8Hash(dex_file.GetFieldNameView(field_id));
  hash = UpdateModifiedUtf8Hash(hash, dex_file.GetFieldTypeDescriptorView(field_id));
  hash = UpdateModifiedUtf8Hash(hash, dex_file.GetTypeDescriptorView(type_id));
  return hash;
}

static size_t ComputeSdkMethodHash(const DexFile& dex_file,
                                   const dex::MethodId& method_id,
                                   const dex::TypeId& type_id) {
  return ComputeModifiedUtf8Hash(dex_file.GetMethodNameView(method_id)) +
      ComputeModifiedUtf8Hash(dex_file.GetTypeDescriptorView(type_id));
}

size_t SdkMembersCache::ComputeSdkFieldHash(ArtField* field) {
  const DexFile& sdk_dex_file = *field->GetDexFile();
  const dex::FieldId& sdk_field_id = sdk_dex_file.GetFieldId(field->GetDexFieldIndex());
  const dex::TypeId& sdk_type_id = sdk_dex_file.GetTypeId(field->GetDeclaringClass()->GetDexTypeIndex());
  return art::ComputeSdkFieldHash(sdk_dex_file, sdk_field_id, sdk_type_id);
}

size_t SdkMembersCache::ComputeSdkMethodHash(ArtMethod* method) {
  const DexFile& sdk_dex_file = *method->GetDexFile();
  const dex::MethodId& sdk_method_id = sdk_dex_file.GetMethodId(method->GetDexMethodIndex());
  const dex::TypeId& sdk_type_id = sdk_dex_file.GetTypeId(method->GetDeclaringClass()->GetDexTypeIndex());
  return art::ComputeSdkMethodHash(sdk_dex_file, sdk_method_id, sdk_type_id);
}

ArtField* SdkMembersCache::FindField(DexFileReference ref, uint32_t hash) const {
  auto it = sdk_fields_set_.FindWithHash(ref, hash);
  return it == sdk_fields_set_.end() ? nullptr : *it;
}

ArtMethod* SdkMembersCache::FindMethod(MethodReference ref, uint32_t hash) const {
  auto it = sdk_methods_set_.FindWithHash(ref, hash);
  return it == sdk_methods_set_.end() ? nullptr : *it;
}

bool SdkMembersCache::SdkFieldEqual::operator()(ArtField* field, DexFileReference reference) const {
  const DexFile& dex_file = *reference.dex_file;
  const dex::FieldId& field_id = dex_file.GetFieldId(reference.index);
  const dex::TypeId& type_id = dex_file.GetTypeId(field_id.class_idx_);

  const DexFile& sdk_dex_file = *field->GetDexFile();
  const dex::FieldId& sdk_field_id = sdk_dex_file.GetFieldId(field->GetDexFieldIndex());
  const dex::TypeId& sdk_type_id = sdk_dex_file.GetTypeId(field->GetDeclaringClass()->GetDexTypeIndex());

  return dex_file.GetFieldNameView(field_id) == sdk_dex_file.GetFieldNameView(sdk_field_id) &&
      dex_file.GetFieldTypeDescriptorView(field_id) == sdk_dex_file.GetFieldTypeDescriptorView(sdk_field_id) &&
      dex_file.GetTypeDescriptorView(type_id) == sdk_dex_file.GetTypeDescriptorView(sdk_type_id);
}

bool SdkMembersCache::SdkMethodEqual::operator()(ArtMethod* method, MethodReference reference) const {
  const DexFile& dex_file = *reference.dex_file;
  const dex::MethodId& method_id = dex_file.GetMethodId(reference.index);
  const dex::TypeId& type_id = dex_file.GetTypeId(method_id.class_idx_);

  const DexFile& sdk_dex_file = *method->GetDexFile();
  const dex::MethodId& sdk_method_id = sdk_dex_file.GetMethodId(method->GetDexMethodIndex());
  const dex::TypeId& sdk_type_id = sdk_dex_file.GetTypeId(method->GetDeclaringClass()->GetDexTypeIndex());

  return dex_file.GetMethodNameView(method_id) == sdk_dex_file.GetMethodNameView(sdk_method_id) &&
      dex_file.GetMethodSignature(method_id) == sdk_dex_file.GetMethodSignature(sdk_method_id) &&
      dex_file.GetTypeDescriptorView(type_id) == sdk_dex_file.GetTypeDescriptorView(sdk_type_id);
}

size_t SdkMembersCache::SdkFieldHash::operator()(DexFileReference reference) const {
  const DexFile& dex_file = *reference.dex_file;
  const dex::FieldId& field_id = dex_file.GetFieldId(reference.index);
  const dex::TypeId& type_id = dex_file.GetTypeId(field_id.class_idx_);
  return art::ComputeSdkFieldHash(dex_file, field_id, type_id);
}

size_t SdkMembersCache::SdkMethodHash::operator()(MethodReference reference) const {
  const DexFile& dex_file = *reference.dex_file;
  const dex::MethodId& method_id = dex_file.GetMethodId(reference.index);
  const dex::TypeId& type_id = dex_file.GetTypeId(method_id.class_idx_);
  return art::ComputeSdkMethodHash(dex_file, method_id, type_id);
}

}
