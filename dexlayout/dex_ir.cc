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

#include <map>
#include <vector>

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

Header::Header(const DexFile& dex_file) : dex_file_(dex_file) {
  const DexFile::Header& disk_header = dex_file.GetHeader();
  memcpy(magic_, disk_header.magic_, sizeof(magic_));
  checksum_ = disk_header.checksum_;
  // TODO(sehr): clearly the signature will need to be recomputed before dumping.
  memcpy(signature_, disk_header.signature_, sizeof(signature_));
  endian_tag_ = disk_header.endian_tag_;
  // Walk the rest of the header fields.
  for (uint32_t i = 0; i < dex_file_.NumStringIds(); ++i) {
    string_ids_.push_back(new StringId(dex_file_.GetStringId(i), *this));
  }
  for (uint32_t i = 0; i < dex_file_.NumTypeIds(); ++i) {
    type_ids_.push_back(new TypeId(dex_file_.GetTypeId(i), *this));
  }
  for (uint32_t i = 0; i < dex_file_.NumProtoIds(); ++i) {
    proto_ids_.push_back(new ProtoId(dex_file_.GetProtoId(i), *this));
  }
  for (uint32_t i = 0; i < dex_file_.NumFieldIds(); ++i) {
    field_ids_.push_back(new FieldId(dex_file_.GetFieldId(i), *this));
  }
  for (uint32_t i = 0; i < dex_file_.NumMethodIds(); ++i) {
    method_ids_.push_back(new MethodId(dex_file_.GetMethodId(i), *this));
  }
  for (uint32_t i = 0; i < dex_file_.NumClassDefs(); ++i) {
    classes_.push_back(new ClassDef(dex_file_.GetClassDef(i), *this));
  }
}

ArrayItem::ArrayItem(Header& header, const uint8_t** data, uint8_t type, uint8_t length) {
  Read(header, data, type, length);
}

ArrayItem::ArrayItem(Header& header, const uint8_t** data) {
  const uint8_t encoded_value = *(*data)++;
  Read(header, data, encoded_value & 0x1f, encoded_value >> 5);
}

void ArrayItem::Read(Header& header, const uint8_t** data, uint8_t type, uint8_t length) {
  type_ = type;
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
      item_.string_val_ = (*header.string_ids())[string_index];
      break;
    }
    case DexFile::kDexAnnotationField:
    case DexFile::kDexAnnotationEnum: {
      const uint32_t field_index = static_cast<uint32_t>(readVarWidth(data, length, false));
      item_.field_val_ = (*header.field_ids())[field_index];
      break;
    }
    case DexFile::kDexAnnotationMethod: {
      const uint32_t method_index = static_cast<uint32_t>(readVarWidth(data, length, false));
      item_.method_val_ = (*header.method_ids())[method_index];
      break;
    }
    case DexFile::kDexAnnotationArray: {
      item_.annotation_array_val_ = new std::vector<const ArrayItem*>();
      // Decode all elements.
      const uint32_t size = DecodeUnsignedLeb128(data);
      for (uint32_t i = 0; i < size; i++) {
        item_.annotation_array_val_->push_back(new ArrayItem(header, data));
      }
      break;
    }
    case DexFile::kDexAnnotationAnnotation: {
      const uint32_t string_index = DecodeUnsignedLeb128(data);
      item_.annotation_annotation_val_.string_ = (*header.string_ids())[string_index];
      item_.annotation_annotation_val_.array_ = new std::vector<const NameValuePair*>();
      // Decode all name=value pairs.
      const uint32_t size = DecodeUnsignedLeb128(data);
      for (uint32_t i = 0; i < size; i++) {
        const uint32_t name_index = DecodeUnsignedLeb128(data);
        NameValuePair* tvp = new NameValuePair((*header.string_ids())[name_index],
                                               new ArrayItem(header, data));
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

ClassDef::ClassDef(const DexFile::ClassDef& disk_class_def, Header& header) {
  class_type_ = (*header.type_ids())[disk_class_def.class_idx_];
  access_flags_ = disk_class_def.access_flags_;
  superclass_ = (*header.type_ids())[disk_class_def.superclass_idx_];

  const DexFile::TypeList* type_list = header.dex_file().GetInterfacesList(disk_class_def);
  if (type_list != nullptr) {
    for (uint32_t index = 0; index < type_list->Size(); ++index) {
      interfaces_.push_back((*header.type_ids())[type_list->GetTypeItem(index).type_idx_]);
    }
  }
  if (disk_class_def.source_file_idx_ == DexFile::kDexNoIndex) {
    source_file_ = nullptr;
  } else {
    source_file_ = (*header.string_ids())[disk_class_def.source_file_idx_];
  }
  // Annotations.
  const DexFile::AnnotationsDirectoryItem* disk_annotations_directory_item =
      header.dex_file().GetAnnotationsDirectory(disk_class_def);
  if (disk_annotations_directory_item == nullptr) {
    annotations_ = nullptr;
  } else {
    annotations_ = new AnnotationsDirectoryItem(disk_annotations_directory_item, header);
  }
  // Static field initializers.
  const uint8_t* static_data = header.dex_file().GetEncodedStaticFieldValuesArray(disk_class_def);
  if (static_data != nullptr) {
    uint32_t static_value_count = static_data == nullptr ? 0 : DecodeUnsignedLeb128(&static_data);
    if (static_value_count > 0) {
      static_values_ = new std::vector<const ArrayItem*>();
      for (uint32_t i = 0; i < static_value_count; ++i) {
        static_values_->push_back(new ArrayItem(header, &static_data));
      }
    }
  }
  // Read the fields and methods defined by the class, resolving the circular reference from those
  // to classes by setting class at the same time.
  const uint8_t* encoded_data = header.dex_file().GetClassData(disk_class_def);
  if (encoded_data != nullptr) {
    ClassDataItemIterator cdii(header.dex_file(), encoded_data);
    // Static fields.
    for (uint32_t i = 0; cdii.HasNextStaticField(); i++, cdii.Next()) {
      const FieldId* field_item = (*header.field_ids())[cdii.GetMemberIndex()];
      const_cast<FieldId*>(field_item)->setClass(this);
      uint32_t access_flags = cdii.GetRawMemberAccessFlags();
      class_data_.static_fields_.push_back(new FieldItem(access_flags, field_item));
    }
    // Instance fields.
    for (uint32_t i = 0; cdii.HasNextInstanceField(); i++, cdii.Next()) {
      const FieldId* field_item = (*header.field_ids())[cdii.GetMemberIndex()];
      const_cast<FieldId*>(field_item)->setClass(this);
      uint32_t access_flags = cdii.GetRawMemberAccessFlags();
      class_data_.instance_fields_.push_back(new FieldItem(access_flags, field_item));
    }
    // Direct methods.
    for (uint32_t i = 0; cdii.HasNextDirectMethod(); i++, cdii.Next()) {
      const MethodId* method_item = (*header.method_ids())[cdii.GetMemberIndex()];
      const_cast<MethodId*>(method_item)->setClass(this);
      uint32_t access_flags = cdii.GetRawMemberAccessFlags();
      // const DexFile::CodeItem* disk_code_item = cdii.GetMethodCodeItem();
      class_data_.direct_methods_.push_back(new MethodItem(access_flags, method_item, nullptr));
    }
    // Virtual methods.
    for (uint32_t i = 0; cdii.HasNextVirtualMethod(); i++, cdii.Next()) {
      const MethodId* method_item = (*header.method_ids())[cdii.GetMemberIndex()];
      const_cast<MethodId*>(method_item)->setClass(this);
      uint32_t access_flags = cdii.GetRawMemberAccessFlags();
      // const DexFile::CodeItem* disk_code_item = cdii.GetMethodCodeItem();
      class_data_.virtual_methods_.push_back(new MethodItem(access_flags, method_item, nullptr));
    }
  }
}

CodeItem::CodeItem(const DexFile::CodeItem& disk_code_item, Header& header) {
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
      const DexFile::TryItem* disk_try_item = header.dex_file().GetTryItems(disk_code_item, i);
      tries_->push_back(new TryItem(*disk_try_item, disk_code_item, header));
    }
  } else {
    tries_ = nullptr;
  }
}

AnnotationSetItem::AnnotationSetItem(const DexFile::AnnotationSetItem& disk_annotations_item,
                                     Header& header) {
  if (disk_annotations_item.size_ == 0) {
    return;
  }
  for (uint32_t i = 0; i < disk_annotations_item.size_; ++i) {
    const DexFile::AnnotationItem* annotation =
        header.dex_file().GetAnnotationItem(&disk_annotations_item, i);
    if (annotation == nullptr) {
      continue;
    }
    uint8_t visibility = annotation->visibility_;
    const uint8_t* annotation_data = annotation->annotation_;
    const ArrayItem* array_item =
        new ArrayItem(header, &annotation_data, DexFile::kDexAnnotationAnnotation, 0);
    items_.push_back(new AnnotationItem(visibility, array_item));
  }
}

AnnotationsDirectoryItem::AnnotationsDirectoryItem(
    const DexFile::AnnotationsDirectoryItem* disk_annotations_item, Header& header) {
  const DexFile::AnnotationSetItem* class_set_item =
      header.dex_file().GetClassAnnotationSet(disk_annotations_item);
  if (class_set_item != nullptr) {
    class_annotation_ = new AnnotationSetItem(*class_set_item, header);
  }
  const DexFile::FieldAnnotationsItem* fields =
      header.dex_file().GetFieldAnnotations(disk_annotations_item);
  if (fields != nullptr) {
    for (uint32_t i = 0; i < disk_annotations_item->fields_size_; ++i) {
      const FieldId* field_id = (*header.field_ids())[fields[i].field_idx_];
      field_annotations_.push_back(new FieldAnnotation(field_id, nullptr));
    }
  }
  const DexFile::MethodAnnotationsItem* methods =
      header.dex_file().GetMethodAnnotations(disk_annotations_item);
  if (methods != nullptr) {
    for (uint32_t i = 0; i < disk_annotations_item->methods_size_; ++i) {
      const MethodId* method_id = (*header.method_ids())[methods[i].method_idx_];
      method_annotations_.push_back(new MethodAnnotation(method_id, nullptr));
    }
  }
  const DexFile::ParameterAnnotationsItem* parameters =
      header.dex_file().GetParameterAnnotations(disk_annotations_item);
  if (parameters != nullptr) {
    for (uint32_t i = 0; i < disk_annotations_item->parameters_size_; ++i) {
      const MethodId* method_id = (*header.method_ids())[parameters[i].method_idx_];
      const DexFile::AnnotationSetRefList* list =
          header.dex_file().GetParameterAnnotationSetRefList(&parameters[i]);
      parameter_annotations_.push_back(new ParameterAnnotation(method_id, list, header));
    }
  }
}

}  // namespace dex_ir
}  // namespace art
