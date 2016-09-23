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
 * Header file of an in-memory representation of DEX files.
 */

#include <stdint.h>
#include <vector>

#include "dex_writer.h"

namespace art {

size_t EncodeIntValue(int32_t value, uint8_t* buffer) {
  size_t length = 0;
  if (value >= 0) {
    while (value > 0x7f) {
      buffer[length++] = static_cast<uint8_t>(value);
      value >>= 8;
    }
  } else {
    while (value < -0x80) {
      buffer[length++] = static_cast<uint8_t>(value);
      value >>= 8;
    }
  }
  buffer[length++] = static_cast<uint8_t>(value);
  return length;
}

size_t EncodeUIntValue(uint32_t value, uint8_t* buffer) {
  size_t length = 0;
  do {
    buffer[length++] = static_cast<uint8_t>(value);
    value >>= 8;
  } while (value != 0);
  return length;
}

size_t EncodeLongValue(int64_t value, uint8_t* buffer) {
  size_t length = 0;
  if (value >= 0) {
    while (value > 0x7f) {
      buffer[length++] = static_cast<uint8_t>(value);
      value >>= 8;
    }
  } else {
    while (value < -0x80) {
      buffer[length++] = static_cast<uint8_t>(value);
      value >>= 8;
    }
  }
  buffer[length++] = static_cast<uint8_t>(value);
  return length;
}

size_t EncodeFloatValue(float value, uint8_t* buffer) {
  uint8_t* ptr = reinterpret_cast<uint8_t*>(&value);
  size_t index = 3;
  do {
    buffer[index--] = *ptr;
  } while (*ptr != 0);
  return 3 - index;
}

size_t EncodeDoubleValue(double value, uint8_t* buffer) {
  uint8_t* ptr = reinterpret_cast<uint8_t*>(&value);
  size_t index = 7;
  do {
    buffer[index--] = *ptr;
  } while (*ptr != 0);
  return 7 - index;
}

size_t DexWriter::Write(const void* buffer, size_t length, size_t offset) {
  return dex_file_->PwriteFully(buffer, length, offset) ? length : 0;
}

size_t DexWriter::WriteUleb128(uint32_t value, size_t offset) {
  uint8_t buffer[8];
  EncodeUnsignedLeb128(buffer, value);
  return Write(buffer, UnsignedLeb128Size(value), offset);
}

size_t DexWriter::WriteEncodedValue(dex_ir::EncodedValue* encoded_value, size_t offset) {
  size_t original_offset = offset;
  size_t start = 0;
  size_t length;
  uint8_t buffer[8];
  int8_t type = encoded_value->Type();
  switch (type) {
    case DexFile::kDexAnnotationByte:
    case DexFile::kDexAnnotationShort:
    case DexFile::kDexAnnotationInt:
      length = EncodeIntValue(encoded_value->GetInt(), buffer);
      break;
    case DexFile::kDexAnnotationChar:
      length = EncodeUIntValue(encoded_value->GetInt(), buffer);
      break;
    case DexFile::kDexAnnotationLong:
      length = EncodeLongValue(encoded_value->GetLong(), buffer);
      break;
    case DexFile::kDexAnnotationFloat:
      length = EncodeFloatValue(encoded_value->GetFloat(), buffer);
      start = 4 - length;
      break;
    case DexFile::kDexAnnotationDouble:
      length = EncodeDoubleValue(encoded_value->GetDouble(), buffer);
      start = 8 - length;
      break;
    case DexFile::kDexAnnotationString:
      length = EncodeUIntValue(encoded_value->GetStringId()->GetIndex(), buffer);
      break;
    case DexFile::kDexAnnotationType:
      length = EncodeUIntValue(encoded_value->GetTypeId()->GetIndex(), buffer);
      break;
    case DexFile::kDexAnnotationField:
    case DexFile::kDexAnnotationEnum:
      length = EncodeUIntValue(encoded_value->GetFieldId()->GetIndex(), buffer);
      break;
    case DexFile::kDexAnnotationMethod:
      length = EncodeUIntValue(encoded_value->GetMethodId()->GetIndex(), buffer);
      break;
    case DexFile::kDexAnnotationArray:
      offset += WriteEncodedValueHeader(type, 0, offset);
      offset += WriteEncodedArray(encoded_value->GetEncodedArray()->GetEncodedValues(), offset);
      return offset - original_offset;
    case DexFile::kDexAnnotationAnnotation:
      offset += WriteEncodedValueHeader(type, 0, offset);
      offset += WriteEncodedAnnotation(encoded_value->GetEncodedAnnotation(), offset);
      return offset - original_offset;
    case DexFile::kDexAnnotationNull:
      return WriteEncodedValueHeader(type, 0, offset);
    case DexFile::kDexAnnotationBoolean:
      return WriteEncodedValueHeader(type, encoded_value->GetBoolean() ? 1 : 0, offset);
    default:
      return 0;
  }
  offset += WriteEncodedValueHeader(type, length - 1, offset);
  offset += Write(buffer + start, length, offset);
  return offset - original_offset;
}

size_t DexWriter::WriteEncodedValueHeader(int8_t value_type, uint32_t value_arg, size_t offset) {
  uint32_t buffer[1] = { (value_arg << 5) | value_type };
  return Write(buffer, sizeof(uint32_t), offset);
}

size_t DexWriter::WriteEncodedArray(dex_ir::EncodedValueVector* values, size_t offset) {
  size_t original_offset = offset;
  offset += WriteUleb128(values->size(), offset);
  for (std::unique_ptr<dex_ir::EncodedValue>& value : *values) {
    offset += WriteEncodedValue(value.get(), offset);
  }
  return offset - original_offset;
}

size_t DexWriter::WriteEncodedAnnotation(dex_ir::EncodedAnnotation* annotation, size_t offset) {
  size_t original_offset = offset;
  offset += WriteUleb128(annotation->GetType()->GetIndex(), offset);
  offset += WriteUleb128(annotation->GetAnnotationElements()->size(), offset);
  for (std::unique_ptr<dex_ir::AnnotationElement>& annotation_element :
      *annotation->GetAnnotationElements()) {
    offset += WriteUleb128(annotation_element->GetName()->GetIndex(), offset);
    offset += WriteEncodedValue(annotation_element->GetValue(), offset);
  }
  return offset - original_offset;
}

size_t DexWriter::WriteEncodedFields(dex_ir::FieldItemVector* fields, size_t offset) {
  size_t original_offset = offset;
  uint32_t prev_index = 0;
  for (std::unique_ptr<dex_ir::FieldItem>& field : *fields) {
    uint32_t index = field->GetFieldId()->GetIndex();
    offset += WriteUleb128(index - prev_index, offset);
    offset += WriteUleb128(field->GetAccessFlags(), offset);
    prev_index = index;
  }
  return offset - original_offset;
}

size_t DexWriter::WriteEncodedMethods(dex_ir::MethodItemVector* methods, size_t offset) {
  size_t original_offset = offset;
  uint32_t prev_index = 0;
  for (std::unique_ptr<dex_ir::MethodItem>& field : *methods) {
    uint32_t index = field->GetMethodId()->GetIndex();
    offset += WriteUleb128(index - prev_index, offset);
    offset += WriteUleb128(field->GetAccessFlags(), offset);
    offset += WriteUleb128(field->GetCodeItem()->GetOffset(), offset);
    prev_index = index;
  }
  return offset - original_offset;
}

void DexWriter::WriteStrings() {
  uint32_t string_data_off[1];
  for (std::unique_ptr<dex_ir::StringId>& string_id : header_.GetCollections().StringIds()) {
    string_data_off[0] = string_id->DataItem()->GetOffset();
    Write(string_data_off, string_id->GetSize(), string_id->GetOffset());
  }

  for (std::unique_ptr<dex_ir::StringData>& string_data : header_.GetCollections().StringDatas()) {
    uint32_t string_size = strlen(string_data->Data());
    uint32_t offset = string_data->GetOffset();
    offset += WriteUleb128(string_size, offset);
    Write(string_data->Data(), string_size, offset);
  }
}

void DexWriter::WriteTypes() {
  uint32_t descriptor_idx[1];
  for (std::unique_ptr<dex_ir::TypeId>& type_id : header_.GetCollections().TypeIds()) {
    descriptor_idx[0] = type_id->GetStringId()->GetIndex();
    Write(descriptor_idx, type_id->GetSize(), type_id->GetOffset());
  }
}

void DexWriter::WriteTypeLists() {
  uint32_t size[1];
  uint16_t list[1];
  for (std::unique_ptr<dex_ir::TypeList>& type_list : header_.GetCollections().TypeLists()) {
    size[0] = type_list->GetTypeList()->size();
    uint32_t offset = type_list->GetOffset();
    offset += Write(size, sizeof(uint32_t), offset);
    for (const dex_ir::TypeId* type_id : *type_list->GetTypeList()) {
      list[0] = type_id->GetIndex();
      offset += Write(list, sizeof(uint16_t), offset);
    }
  }
}

void DexWriter::WriteProtos() {
  uint32_t proto_id_buffer[3];
  for (std::unique_ptr<dex_ir::ProtoId>& proto_id : header_.GetCollections().ProtoIds()) {
    proto_id_buffer[0] = proto_id->Shorty()->GetIndex();
    proto_id_buffer[1] = proto_id->ReturnType()->GetIndex();
    proto_id_buffer[2] = proto_id->Parameters()->GetOffset();
    Write(proto_id_buffer, proto_id->GetSize(), proto_id->GetOffset());
  }
}

void DexWriter::WriteFields() {
  uint16_t field_id_buffer[4];
  for (std::unique_ptr<dex_ir::FieldId>& field_id : header_.GetCollections().FieldIds()) {
    field_id_buffer[0] = field_id->Class()->GetIndex();
    field_id_buffer[1] = field_id->Type()->GetIndex();
    field_id_buffer[2] = field_id->Name()->GetIndex();
    field_id_buffer[3] = field_id->Name()->GetIndex() >> 16;
    Write(field_id_buffer, field_id->GetSize(), field_id->GetOffset());
  }
}

void DexWriter::WriteMethods() {
  uint16_t method_id_buffer[4];
  for (std::unique_ptr<dex_ir::MethodId>& method_id : header_.GetCollections().MethodIds()) {
    method_id_buffer[0] = method_id->Class()->GetIndex();
    method_id_buffer[1] = method_id->Proto()->GetIndex();
    method_id_buffer[2] = method_id->Name()->GetIndex();
    method_id_buffer[3] = method_id->Name()->GetIndex() >> 16;
    Write(method_id_buffer, method_id->GetSize(), method_id->GetOffset());
  }
}

void DexWriter::WriteEncodedArrays() {
  for (std::unique_ptr<dex_ir::EncodedArrayItem>& encoded_array :
      header_.GetCollections().EncodedArrayItems()) {
    WriteEncodedArray(encoded_array->GetEncodedValues(), encoded_array->GetOffset());
  }
}

void DexWriter::WriteAnnotations() {
  uint8_t visibility[1];
  for (std::unique_ptr<dex_ir::AnnotationItem>& annotation :
      header_.GetCollections().AnnotationItems()) {
    visibility[0] = annotation->GetVisibility();
    size_t offset = annotation->GetOffset();
    offset += Write(visibility, sizeof(uint8_t), offset);
    WriteEncodedAnnotation(annotation->GetAnnotation(), offset);
  }
}

void DexWriter::WriteAnnotationSets() {
  uint32_t size[1];
  uint32_t annotation_off[1];
  for (std::unique_ptr<dex_ir::AnnotationSetItem>& annotation_set :
      header_.GetCollections().AnnotationSetItems()) {
    size[0] = annotation_set->GetItems()->size();
    size_t offset = annotation_set->GetOffset();
    offset += Write(size, sizeof(uint32_t), offset);
    for (dex_ir::AnnotationItem* annotation : *annotation_set->GetItems()) {
      annotation_off[0] = annotation->GetOffset();
      offset += Write(annotation_off, sizeof(uint32_t), offset);
    }
  }
}

void DexWriter::WriteAnnotationSetRefs() {
  uint32_t size[1];
  uint32_t annotations_off[1];
  for (std::unique_ptr<dex_ir::AnnotationSetRefList>& annotation_set_ref :
        header_.GetCollections().AnnotationSetRefLists()) {
    size[0] = annotation_set_ref->GetItems()->size();
    size_t offset = annotation_set_ref->GetOffset();
    offset += Write(size, sizeof(uint32_t), offset);
    for (dex_ir::AnnotationSetItem* annotation_set : *annotation_set_ref->GetItems()) {
      annotations_off[0] = annotation_set->GetOffset();
      offset += Write(annotations_off, sizeof(uint32_t), offset);
    }
  }
}

void DexWriter::WriteAnnotationsDirectories() {
  uint32_t directory_buffer[4];
  uint32_t annotation_buffer[2];
  for (std::unique_ptr<dex_ir::AnnotationsDirectoryItem>& annotations_directory :
          header_.GetCollections().AnnotationsDirectoryItems()) {
    directory_buffer[0] = annotations_directory->GetClassAnnotation()->GetOffset();
    directory_buffer[1] = annotations_directory->GetFieldAnnotations()->size();
    directory_buffer[2] = annotations_directory->GetMethodAnnotations()->size();
    directory_buffer[3] = annotations_directory->GetParameterAnnotations()->size();
    uint32_t offset = annotations_directory->GetOffset();
    offset += Write(directory_buffer, 4 * sizeof(uint32_t), offset);
    for (std::unique_ptr<dex_ir::FieldAnnotation>& field :
        *annotations_directory->GetFieldAnnotations()) {
      annotation_buffer[0] = field->GetFieldId()->GetIndex();
      annotation_buffer[1] = field->GetAnnotationSetItem()->GetOffset();
      offset += Write(annotation_buffer, 2 * sizeof(uint32_t), offset);
    }
    for (std::unique_ptr<dex_ir::MethodAnnotation>& method :
        *annotations_directory->GetMethodAnnotations()) {
      annotation_buffer[0] = method->GetMethodId()->GetIndex();
      annotation_buffer[1] = method->GetAnnotationSetItem()->GetOffset();
      offset += Write(annotation_buffer, 2 * sizeof(uint32_t), offset);
    }
    for (std::unique_ptr<dex_ir::ParameterAnnotation>& parameter :
        *annotations_directory->GetParameterAnnotations()) {
      annotation_buffer[0] = parameter->GetMethodId()->GetIndex();
      annotation_buffer[1] = parameter->GetAnnotations()->GetOffset();
      offset += Write(annotation_buffer, 2 * sizeof(uint32_t), offset);
    }
  }
}

void DexWriter::WriteDebugItems() {
  // TODO:
}

void DexWriter::WriteCodeItems() {
  // TODO:
}

void DexWriter::WriteClasses() {
  uint32_t class_def_buffer[8];
  for (std::unique_ptr<dex_ir::ClassDef>& class_def : header_.GetCollections().ClassDefs()) {
    class_def_buffer[0] = class_def->ClassType()->GetIndex();
    class_def_buffer[1] = class_def->GetAccessFlags();
    class_def_buffer[2] = class_def->Superclass()->GetIndex();
    class_def_buffer[3] = class_def->InterfacesOffset();
    class_def_buffer[4] = class_def->SourceFile()->GetIndex();
    class_def_buffer[5] = class_def->Annotations()->GetOffset();
    class_def_buffer[6] = class_def->GetClassData()->GetOffset();
    class_def_buffer[7] = class_def->StaticValues()->GetOffset();
    size_t offset = class_def->GetOffset();
    Write(class_def_buffer, class_def->GetSize(), offset);
  }

  for (std::unique_ptr<dex_ir::ClassData>& class_data : header_.GetCollections().ClassDatas()) {
    size_t offset = class_data->GetOffset();
    offset += WriteUleb128(class_data->StaticFields()->size(), offset);
    offset += WriteUleb128(class_data->InstanceFields()->size(), offset);
    offset += WriteUleb128(class_data->DirectMethods()->size(), offset);
    offset += WriteUleb128(class_data->VirtualMethods()->size(), offset);
    offset += WriteEncodedFields(class_data->StaticFields(), offset);
    offset += WriteEncodedFields(class_data->InstanceFields(), offset);
    offset += WriteEncodedMethods(class_data->DirectMethods(), offset);
    offset += WriteEncodedMethods(class_data->VirtualMethods(), offset);
  }
}

void DexWriter::WriteFile() {
  if (dex_file_.get() == nullptr) {
    fprintf(stderr, "Can't open output dex file\n");
    return;
  }

  WriteStrings();
  WriteTypes();
  WriteTypeLists();
  WriteProtos();
  WriteFields();
  WriteMethods();
  WriteEncodedArrays();
  WriteAnnotations();
  WriteAnnotationSets();
  WriteAnnotationSetRefs();
  WriteAnnotationsDirectories();
  WriteDebugItems();
  WriteCodeItems();
  WriteClasses();
}

void DexWriter::OutputDexFile(dex_ir::Header& header, const char* file_name) {
  LOG(INFO) << "FILE NAME: " << file_name;
  (new DexWriter(header, file_name))->WriteFile();
}

}  // namespace art
