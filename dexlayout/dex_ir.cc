/*
 * Copyright (C) 2016 The Android Open Source Project
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
 *
 * Implementation file of the dexlayout utility.
 *
 * This is a tool to read dex files into an internal representation,
 * reorganize the representation, and emit dex files with a better
 * file layout.
 */

#include "dex_ir.h"
#include "dex_ir_builder.h"

namespace art {
namespace dex_ir {

void Collections::CreateStringId(const DexFile& dex_file, uint32_t i) {
  const DexFile::StringId& disk_string_id = dex_file.GetStringId(i);
  StringData* string_data = new StringData(dex_file.GetStringData(disk_string_id));
  string_datas_.AddItem(string_data, disk_string_id.string_data_off_);

  StringId* string_id = new StringId(string_data);
  string_ids_.AddIndexedItem(string_id, StringIdsOffset() + i * StringId::ItemSize(), i);
}

void Collections::CreateTypeId(const DexFile& dex_file, uint32_t i) {
  const DexFile::TypeId& disk_type_id = dex_file.GetTypeId(i);
  TypeId* type_id = new TypeId(GetStringId(disk_type_id.descriptor_idx_));
  type_ids_.AddIndexedItem(type_id, TypeIdsOffset() + i * TypeId::ItemSize(), i);
}

void Collections::CreateProtoId(const DexFile& dex_file, uint32_t i) {
  const DexFile::ProtoId& disk_proto_id = dex_file.GetProtoId(i);
  const DexFile::TypeList* type_list = dex_file.GetProtoParameters(disk_proto_id);
  TypeList* parameter_type_list = CreateTypeList(type_list, disk_proto_id.parameters_off_, true);

  ProtoId* proto_id = new ProtoId(GetStringId(disk_proto_id.shorty_idx_),
                                  GetTypeId(disk_proto_id.return_type_idx_),
                                  parameter_type_list);
  proto_ids_.AddIndexedItem(proto_id, ProtoIdsOffset() + i * ProtoId::ItemSize(), i);
}

void Collections::CreateFieldId(const DexFile& dex_file, uint32_t i) {
  const DexFile::FieldId& disk_field_id = dex_file.GetFieldId(i);
  FieldId* field_id = new FieldId(GetTypeId(disk_field_id.class_idx_),
                                  GetTypeId(disk_field_id.type_idx_),
                                  GetStringId(disk_field_id.name_idx_));
  field_ids_.AddIndexedItem(field_id, FieldIdsOffset() + i * FieldId::ItemSize(), i);
}

void Collections::CreateMethodId(const DexFile& dex_file, uint32_t i) {
  const DexFile::MethodId& disk_method_id = dex_file.GetMethodId(i);
  MethodId* method_id = new MethodId(GetTypeId(disk_method_id.class_idx_),
                                     GetProtoId(disk_method_id.proto_idx_),
                                     GetStringId(disk_method_id.name_idx_));
  method_ids_.AddIndexedItem(method_id, MethodIdsOffset() + i * MethodId::ItemSize(), i);
}

void Collections::CreateClassDef(const DexFile& dex_file, Header* header, uint32_t i) {
  const DexFile::ClassDef& disk_class_def = dex_file.GetClassDef(i);
  ClassDef* class_def = ReadClassDef(dex_file, disk_class_def, *header);
  class_defs_.AddIndexedItem(class_def, ClassDefsOffset() + i * ClassDef::ItemSize(), i);
}

TypeList* Collections::CreateTypeList(
    const DexFile::TypeList* dex_type_list, uint32_t offset, bool allow_empty) {
  if (dex_type_list == nullptr && !allow_empty) {
    return nullptr;
  }

  // TODO: Create more efficient lookup for existing type lists.
  for (std::unique_ptr<TypeList>& type_list : TypeLists()) {
    if (type_list->GetOffset() == offset) {
      return type_list.get();
    }
  }

  TypeIdVector* type_vector = new TypeIdVector();
  uint32_t size = dex_type_list == nullptr ? 0 : dex_type_list->Size();
  for (uint32_t index = 0; index < size; ++index) {
    type_vector->push_back(GetTypeId(dex_type_list->GetTypeItem(index).type_idx_));
  }
  TypeList* new_type_list = new TypeList(type_vector);
  type_lists_.AddItem(new_type_list, offset);
  return new_type_list;
}

}  // namespace dex_ir
}  // namespace art
