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

namespace {
static uint64_t readVarWidth(const uint8_t** data, uint8_t length, bool sign_extend) {
  uint64_t value = 0;
  for (uint32_t i = 0; i <= length; i++) {
    value |= static_cast<uint64_t>(*(*data)++) << (i * 8);
  }
  if (sign_extend) {
    int shift = (7 - length) * 8;
    return (static_cast<int64_t>(value) << shift) >> shift;
  }
  return value;
}
}  // namespace

EncodedArrayItem::EncodedArrayItem(Builder& builder, const uint8_t** data) {
  const uint8_t encoded_value = *(*data)++;
  type_ = encoded_value & 0x1f;
  uint32_t length = encoded_value >> 5;
  switch (type_) {
    case DexFile::kDexAnnotationByte:
      item_.byte_val_ = static_cast<int8_t>(readVarWidth(data, length, false));
      break;
    case DexFile::kDexAnnotationShort:
      item_.short_val_ = static_cast<int16_t>(readVarWidth(data, length, true));
      break;
    case DexFile::kDexAnnotationChar:
      item_.char_val_ = static_cast<uint16_t>(readVarWidth(data, length, false));
      break;
    case DexFile::kDexAnnotationInt:
      item_.int_val_ = static_cast<int32_t>(readVarWidth(data, length, true));
      break;
    case DexFile::kDexAnnotationLong:
      item_.long_val_ = static_cast<int64_t>(readVarWidth(data, length, true));
      break;
    case DexFile::kDexAnnotationFloat: {
      // Fill on right.
      union {
        float f;
        uint32_t data;
      } conv;
      conv.data = static_cast<uint32_t>(readVarWidth(data, length, false)) << (3 - length) * 8;
      item_.float_val_ = conv.f;
      break;
    }
    case DexFile::kDexAnnotationDouble: {
      // Fill on right.
      union {
        double d;
        uint64_t data;
      } conv;
      conv.data = readVarWidth(data, length, false) << (7 - length) * 8;
      item_.double_val_ = conv.d;
      break;
    }
    case DexFile::kDexAnnotationString:
    case DexFile::kDexAnnotationType: {
      const uint32_t string_index = static_cast<uint32_t>(readVarWidth(data, length, false));
      item_.string_val_ = builder.strings_.Lookup(string_index);
      break;
    }
    case DexFile::kDexAnnotationField:
    case DexFile::kDexAnnotationEnum: {
      const uint32_t field_index = static_cast<uint32_t>(readVarWidth(data, length, false));
      item_.field_val_ = builder.fields_.Lookup(field_index);
      break;
    }
    case DexFile::kDexAnnotationMethod: {
      const uint32_t method_index = static_cast<uint32_t>(readVarWidth(data, length, false));
      item_.method_val_ = builder.methods_.Lookup(method_index);
      break;
    }
    case DexFile::kDexAnnotationArray: {
      item_.annotation_array_val_ = new std::vector<const EncodedArrayItem*>();
      // Decode all elements.
      const uint32_t size = DecodeUnsignedLeb128(data);
      for (uint32_t i = 0; i < size; i++) {
        item_.annotation_array_val_->push_back(new EncodedArrayItem(builder, data));
      }
      break;
    }
    case DexFile::kDexAnnotationAnnotation: {
      const uint32_t string_index = DecodeUnsignedLeb128(data);
      item_.annotation_annotation_val_.string_ = builder.strings_.Lookup(string_index);
      item_.annotation_annotation_val_.array_ = new std::vector<const NameValuePair*>();
      // Decode all name=value pairs.
      const uint32_t size = DecodeUnsignedLeb128(data);
      for (uint32_t i = 0; i < size; i++) {
        const uint32_t name_index = DecodeUnsignedLeb128(data);
        NameValuePair* tvp = new NameValuePair(builder.strings_.Lookup(name_index),
                                               new EncodedArrayItem(builder, data));
        item_.annotation_annotation_val_.array_->push_back(tvp);
      }
      break;
    }
    case DexFile::kDexAnnotationNull:
      break;
    case DexFile::kDexAnnotationBoolean:
      item_.bool_val_ = (length != 0);
      break;
    default:
      break;
  }
}

ClassDefItem::ClassDefItem(const DexFile::ClassDef& disk_class_def, Builder& builder) {
  class_type_ = builder.types_.Lookup(disk_class_def.class_idx_);
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
  const uint8_t* static_data = builder.dex_file_.GetEncodedStaticFieldValuesArray(disk_class_def);
  uint32_t static_value_count = static_data == nullptr ? 0 : DecodeUnsignedLeb128(&static_data);
  if (static_value_count > 0) {
    static_values_ = new std::vector<const EncodedArrayItem*>();
    for (uint32_t i = 0; i < static_value_count; ++i) {
      static_values_->push_back(new EncodedArrayItem(builder, &static_data));
    }
  }

  for (uint32_t i = 0; cdii.HasNextStaticField(); i++, cdii.Next()) {
    const FieldIdItem* field_item = builder.fields_.Lookup(cdii.GetMemberIndex());
    const_cast<FieldIdItem*>(field_item)->setClass(this);
    uint32_t access_flags = cdii.GetRawMemberAccessFlags();
    class_data_.static_fields_.push_back(new EncodedFieldItem(access_flags, field_item));
  }
  // Instance fields.
  for (uint32_t i = 0; cdii.HasNextInstanceField(); i++, cdii.Next()) {
    const FieldIdItem* field_item = builder.fields_.Lookup(cdii.GetMemberIndex());
    const_cast<FieldIdItem*>(field_item)->setClass(this);
    uint32_t access_flags = cdii.GetRawMemberAccessFlags();
    class_data_.instance_fields_.push_back(new EncodedFieldItem(access_flags, field_item));
  }
  // Direct methods.
  for (uint32_t i = 0; cdii.HasNextDirectMethod(); i++, cdii.Next()) {
    const MethodIdItem* method_item = builder.methods_.Lookup(cdii.GetMemberIndex());
    const_cast<MethodIdItem*>(method_item)->setClass(this);
    uint32_t access_flags = cdii.GetRawMemberAccessFlags();
    // const DexFile::CodeItem* disk_code_item = cdii.GetMethodCodeItem();
    class_data_.direct_methods_.push_back(
        new EncodedMethodItem(access_flags, method_item, nullptr));
  }
  // Virtual methods.
  for (uint32_t i = 0; cdii.HasNextVirtualMethod(); i++, cdii.Next()) {
    const MethodIdItem* method_item = builder.methods_.Lookup(cdii.GetMemberIndex());
    const_cast<MethodIdItem*>(method_item)->setClass(this);
    uint32_t access_flags = cdii.GetRawMemberAccessFlags();
    // const DexFile::CodeItem* disk_code_item = cdii.GetMethodCodeItem();
    class_data_.virtual_methods_.push_back(
        new EncodedMethodItem(access_flags, method_item, nullptr));
  }

  // TODO(sehr): read annotations.
  annotations_ = nullptr;
}

CodeItem::CodeItem(const DexFile::CodeItem& disk_code_item, Builder& builder) {
  registers_size_ = disk_code_item.registers_size_;
  ins_size_ = disk_code_item.ins_size_;
  outs_size_ = disk_code_item.outs_size_;
  tries_size_ = disk_code_item.tries_size_;
  // TODO(sehr): read debug info.
  debug_info_ = nullptr;
  insns_size_ = disk_code_item.insns_size_in_code_units_;
  insns_ = new uint16_t[insns_size_];
  memcpy(insns_, disk_code_item.insns_, insns_size_ * sizeof(*insns_));
  if (tries_size_ > 0) {
    tries_ = new std::vector<const TryItem*>();
    for (uint32_t i = 0; i < tries_size_; ++i) {
      const DexFile::TryItem* disk_try_item = builder.dex_file_.GetTryItems(disk_code_item, i);
      tries_->push_back(new TryItem(*disk_try_item, disk_code_item, builder));
    }
  } else {
    tries_ = nullptr;
  }
}

}  // namespace dex_ir
}  // namespace art
