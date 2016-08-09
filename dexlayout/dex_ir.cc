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
 * Implementation file of the dex file intermediate representation.
 *
 * Utilities for reading dex files into an internal representation,
 * manipulating them, and writing them out.
 */

#include "dex_ir.h"

#include <vector>
#include <map>
#include "dex_file.h"
#include "dex_file-inl.h"
#include "utils.h"

namespace art {
namespace dex_ir {

ClassDefItem::ClassDefItem(const DexFile::ClassDef& disk_class_def, Builder& builder) {
  class_ = builder.types_.Lookup(disk_class_def.class_idx_);
  access_flags_ = disk_class_def.access_flags_;
  superclass_ = builder.types_.Lookup(disk_class_def.superclass_idx_);

  const DexFile::TypeList* type_list = builder.dex_file_.GetInterfacesList(disk_class_def);
  for (uint32_t index = 0; index < type_list->Size(); ++index) {
    interfaces_.push_back(builder.types_.Lookup(type_list->GetTypeItem(index).type_idx_));
  }

  source_file_ = builder.strings_.Lookup(disk_class_def.source_file_idx_);

  // Read the fields and methods defined by the class, resolving the circular reference from those
  // to classes by setting class at the same time.
  const uint8_t* encoded_data = builder.dex_file_.GetClassData(disk_class_def);
  ClassDataItemIterator cdii(builder.dex_file_, encoded_data);
  // Static fields.
  // const uint8_t* static_data = builder.dex_file_.GetEncodedStaticFieldValuesArray(disk_class_def);
  // const uint32_t static_count = static_data == nullptr ? 0 : DecodeUnsignedLeb128(&static_data);
  for (uint32_t i = 0; cdii.HasNextStaticField(); i++, cdii.Next()) {
    const FieldIdItem* field_item = builder.fields_.Lookup(cdii.GetMemberIndex());
    const_cast<FieldIdItem*>(field_item)->setClass(this);
    uint32_t access_flags = cdii.GetRawMemberAccessFlags();
    // TODO(sehr): get data initializer.
    // std::vector<const EncodedArrayItem*> static_values_;
    class_data_.static_fields_.emplace_back(access_flags, field_item);
  }
  // Instance fields.
  for (uint32_t i = 0; cdii.HasNextInstanceField(); i++, cdii.Next()) {
    const FieldIdItem* field_item = builder.fields_.Lookup(cdii.GetMemberIndex());
    const_cast<FieldIdItem*>(field_item)->setClass(this);
    uint32_t access_flags = cdii.GetRawMemberAccessFlags();
    class_data_.instance_fields_.emplace_back(access_flags, field_item);
  }
  // Direct methods.
  for (uint32_t i = 0; cdii.HasNextDirectMethod(); i++, cdii.Next()) {
    const MethodIdItem* method_item = builder.methods_.Lookup(cdii.GetMemberIndex());
    const_cast<MethodIdItem*>(method_item)->setClass(this);
    uint32_t access_flags = cdii.GetRawMemberAccessFlags();
    // const DexFile::CodeItem* disk_code_item = cdii.GetMethodCodeItem();
    class_data_.direct_methods_.emplace_back(access_flags, method_item, nullptr);
  }
  // Virtual methods.
  for (uint32_t i = 0; cdii.HasNextVirtualMethod(); i++, cdii.Next()) {
    const MethodIdItem* method_item = builder.methods_.Lookup(cdii.GetMemberIndex());
    const_cast<MethodIdItem*>(method_item)->setClass(this);
    uint32_t access_flags = cdii.GetRawMemberAccessFlags();
    // const DexFile::CodeItem* disk_code_item = cdii.GetMethodCodeItem();
    class_data_.virtual_methods_.emplace_back(access_flags, method_item, nullptr);
  }

  // TODO(sehr): add these.
  // const AnnotationsDirectoryItem* annotations_

}

}  // namespace dex_ir
}  // namespace art
