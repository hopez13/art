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
 */

#include "dex_writer.h"

#include <stdint.h>

#include <vector>

#include "cdex/compact_dex_file.h"
#include "compact_dex_writer.h"
#include "dex_file_types.h"
#include "standard_dex_file.h"
#include "utf.h"

namespace art {

static constexpr uint32_t kDataSectionAlignment = sizeof(uint32_t) * 2;
// Byte aligned sections:
//  DexFile::kDexTypeClassDataItem:
//  DexFile::kDexTypeStringDataItem:
//  DexFile::kDexTypeDebugInfoItem:
//  DexFile::kDexTypeAnnotationItem:
//  DexFile::kDexTypeEncodedArrayItem:
// All other sections are kDexAlignedSection
static constexpr uint32_t kDexSectionWordAlignment = 4;

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

union FloatUnion {
  float f_;
  uint32_t i_;
};

size_t EncodeFloatValue(float value, uint8_t* buffer) {
  FloatUnion float_union;
  float_union.f_ = value;
  uint32_t int_value = float_union.i_;
  size_t index = 3;
  do {
    buffer[index--] = int_value >> 24;
    int_value <<= 8;
  } while (int_value != 0);
  return 3 - index;
}

union DoubleUnion {
  double d_;
  uint64_t l_;
};

size_t EncodeDoubleValue(double value, uint8_t* buffer) {
  DoubleUnion double_union;
  double_union.d_ = value;
  uint64_t long_value = double_union.l_;
  size_t index = 7;
  do {
    buffer[index--] = long_value >> 56;
    long_value <<= 8;
  } while (long_value != 0);
  return 7 - index;
}

size_t DexWriter::Write(const void* buffer, size_t length, size_t offset) {
  DCHECK_LE(offset + length, mem_map_->Size());
  memcpy(mem_map_->Begin() + offset, buffer, length);
  return length;
}

size_t DexWriter::WriteSleb128(uint32_t value, size_t offset) {
  uint8_t buffer[8];
  EncodeSignedLeb128(buffer, value);
  return Write(buffer, SignedLeb128Size(value), offset);
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
      length = EncodeIntValue(encoded_value->GetByte(), buffer);
      break;
    case DexFile::kDexAnnotationShort:
      length = EncodeIntValue(encoded_value->GetShort(), buffer);
      break;
    case DexFile::kDexAnnotationChar:
      length = EncodeUIntValue(encoded_value->GetChar(), buffer);
      break;
    case DexFile::kDexAnnotationInt:
      length = EncodeIntValue(encoded_value->GetInt(), buffer);
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
    case DexFile::kDexAnnotationMethodType:
      length = EncodeUIntValue(encoded_value->GetProtoId()->GetIndex(), buffer);
      break;
    case DexFile::kDexAnnotationMethodHandle:
      length = EncodeUIntValue(encoded_value->GetMethodHandle()->GetIndex(), buffer);
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

size_t DexWriter::WriteEncodedValueHeader(int8_t value_type, size_t value_arg, size_t offset) {
  uint8_t buffer[1] = { static_cast<uint8_t>((value_arg << 5) | value_type) };
  return Write(buffer, sizeof(uint8_t), offset);
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
  for (std::unique_ptr<dex_ir::MethodItem>& method : *methods) {
    uint32_t index = method->GetMethodId()->GetIndex();
    uint32_t code_off = method->GetCodeItem() == nullptr ? 0 : method->GetCodeItem()->GetOffset();
    offset += WriteUleb128(index - prev_index, offset);
    offset += WriteUleb128(method->GetAccessFlags(), offset);
    offset += WriteUleb128(code_off, offset);
    prev_index = index;
  }
  return offset - original_offset;
}

MapItem DexWriter::WriteStringIds(uint32_t* const offset, bool reserve_only) {
  const uint32_t start = *offset;
  for (std::unique_ptr<dex_ir::StringId>& string_id : header_->GetCollections().StringIds()) {
    *offset = RoundUp(*offset, kDexSectionWordAlignment);
    if (reserve_only) {
      *offset += string_id->GetSize();
    } else {
      uint32_t string_data_off = string_id->DataItem()->GetOffset();
      string_id->SetOffset(*offset);
      *offset += Write(&string_data_off, string_id->GetSize(), *offset);
    }
  }
  return MapItem(DexFile::kDexTypeStringIdItem, header_->GetCollections().StringIdsSize(), start);
}

MapItem DexWriter::WriteStringDatas(uint32_t* const offset) {
  const uint32_t start = *offset;
  for (std::unique_ptr<dex_ir::StringData>& string_data : header_->GetCollections().StringDatas()) {
    string_data->SetOffset(*offset);
    *offset += WriteUleb128(CountModifiedUtf8Chars(string_data->Data()), *offset);
    *offset += Write(string_data->Data(), strlen(string_data->Data()), *offset);
    // Skip null terminator (already zeroed out, no need to write).
    ++*offset;
  }
  return MapItem(DexFile::kDexTypeStringDataItem,
                 header_->GetCollections().StringDatasSize(),
                 start);
}

MapItem DexWriter::WriteTypeIds(uint32_t* const offset) {
  uint32_t descriptor_idx[1];
  const uint32_t start = *offset;
  for (std::unique_ptr<dex_ir::TypeId>& type_id : header_->GetCollections().TypeIds()) {
    *offset = RoundUp(*offset, kDexSectionWordAlignment);
    descriptor_idx[0] = type_id->GetStringId()->GetIndex();
    type_id->SetOffset(*offset);
    *offset += Write(descriptor_idx, type_id->GetSize(), *offset);
  }
  return MapItem(DexFile::kDexTypeTypeIdItem, header_->GetCollections().TypeIdsSize(), start);
}

MapItem DexWriter::WriteTypeLists(uint32_t* const offset) {
  uint32_t size[1];
  uint16_t list[1];
  const uint32_t start = *offset;
  for (std::unique_ptr<dex_ir::TypeList>& type_list : header_->GetCollections().TypeLists()) {
    *offset = RoundUp(*offset, kDexSectionWordAlignment);
    size[0] = type_list->GetTypeList()->size();
    type_list->SetOffset(*offset);
    *offset += Write(size, sizeof(uint32_t), *offset);
    for (const dex_ir::TypeId* type_id : *type_list->GetTypeList()) {
      list[0] = type_id->GetIndex();
      *offset += Write(list, sizeof(uint16_t), *offset);
    }
  }
  return MapItem(DexFile::kDexTypeTypeList, header_->GetCollections().TypeListsSize(), start);
}

MapItem DexWriter::WriteProtoIds(uint32_t* const offset, bool reserve_only) {
  uint32_t buffer[3];
  const uint32_t start = *offset;
  for (std::unique_ptr<dex_ir::ProtoId>& proto_id : header_->GetCollections().ProtoIds()) {
    *offset = RoundUp(*offset, kDexSectionWordAlignment);
    if (reserve_only) {
      *offset += proto_id->GetSize();
    } else {
      buffer[0] = proto_id->Shorty()->GetIndex();
      buffer[1] = proto_id->ReturnType()->GetIndex();
      buffer[2] = proto_id->Parameters() == nullptr ? 0 : proto_id->Parameters()->GetOffset();
      *offset += Write(buffer, proto_id->GetSize(), *offset);
    }
  }
  return MapItem(DexFile::kDexTypeProtoIdItem, header_->GetCollections().ProtoIdsSize(), start);
}

MapItem DexWriter::WriteFieldIds(uint32_t* const offset) {
  uint16_t buffer[4];
  const uint32_t start = *offset;
  for (std::unique_ptr<dex_ir::FieldId>& field_id : header_->GetCollections().FieldIds()) {
    *offset = RoundUp(*offset, kDexSectionWordAlignment);
    buffer[0] = field_id->Class()->GetIndex();
    buffer[1] = field_id->Type()->GetIndex();
    buffer[2] = field_id->Name()->GetIndex();
    buffer[3] = field_id->Name()->GetIndex() >> 16;
    field_id->SetOffset(*offset);
    *offset += Write(buffer, field_id->GetSize(), *offset);
  }
  return MapItem(DexFile::kDexTypeFieldIdItem, header_->GetCollections().FieldIdsSize(), start);
}

MapItem DexWriter::WriteMethodIds(uint32_t* const offset) {
  uint16_t buffer[4];
  const uint32_t start = *offset;
  for (std::unique_ptr<dex_ir::MethodId>& method_id : header_->GetCollections().MethodIds()) {
    *offset = RoundUp(*offset, kDexSectionWordAlignment);
    buffer[0] = method_id->Class()->GetIndex();
    buffer[1] = method_id->Proto()->GetIndex();
    buffer[2] = method_id->Name()->GetIndex();
    buffer[3] = method_id->Name()->GetIndex() >> 16;
    method_id->SetOffset(*offset);
    *offset += Write(buffer, method_id->GetSize(), *offset);
  }
  return MapItem(DexFile::kDexTypeMethodIdItem, header_->GetCollections().MethodIdsSize(), start);
}

MapItem DexWriter::WriteEncodedArrays(uint32_t* const offset) {
  const uint32_t start = *offset;
  for (std::unique_ptr<dex_ir::EncodedArrayItem>& encoded_array :
      header_->GetCollections().EncodedArrayItems()) {
    encoded_array->SetOffset(*offset);
    *offset += WriteEncodedArray(encoded_array->GetEncodedValues(), *offset);
  }
  return MapItem(DexFile::kDexTypeEncodedArrayItem,
                 header_->GetCollections().EncodedArrayItemsSize(),
                 start);
}

MapItem DexWriter::WriteAnnotations(uint32_t* const offset) {
  uint8_t visibility[1];
  const uint32_t start = *offset;
  for (std::unique_ptr<dex_ir::AnnotationItem>& annotation :
      header_->GetCollections().AnnotationItems()) {
    visibility[0] = annotation->GetVisibility();
    annotation->SetOffset(*offset);
    *offset += Write(visibility, sizeof(uint8_t), *offset);
    *offset += WriteEncodedAnnotation(annotation->GetAnnotation(), *offset);
  }
  return MapItem(DexFile::kDexTypeAnnotationItem,
                 header_->GetCollections().AnnotationItemsSize(),
                 start);
}

MapItem DexWriter::WriteAnnotationSets(uint32_t* const offset) {
  uint32_t size[1];
  uint32_t annotation_off[1];
  const uint32_t start = *offset;
  for (std::unique_ptr<dex_ir::AnnotationSetItem>& annotation_set :
      header_->GetCollections().AnnotationSetItems()) {
    *offset = RoundUp(*offset, kDexSectionWordAlignment);
    size[0] = annotation_set->GetItems()->size();
    annotation_set->SetOffset(*offset);
    *offset += Write(size, sizeof(uint32_t), *offset);
    for (dex_ir::AnnotationItem* annotation : *annotation_set->GetItems()) {
      annotation_off[0] = annotation->GetOffset();
      *offset += Write(annotation_off, sizeof(uint32_t), *offset);
    }
  }
  return MapItem(DexFile::kDexTypeAnnotationSetItem,
                 header_->GetCollections().AnnotationSetItemsSize(),
                 start);
}

MapItem DexWriter::WriteAnnotationSetRefs(uint32_t* const offset) {
  uint32_t size[1];
  uint32_t annotations_off[1];
  const uint32_t start = *offset;
  for (std::unique_ptr<dex_ir::AnnotationSetRefList>& annotation_set_ref :
      header_->GetCollections().AnnotationSetRefLists()) {
    *offset = RoundUp(*offset, kDexSectionWordAlignment);
    size[0] = annotation_set_ref->GetItems()->size();
    annotation_set_ref->SetOffset(*offset);
    *offset += Write(size, sizeof(uint32_t), *offset);
    for (dex_ir::AnnotationSetItem* annotation_set : *annotation_set_ref->GetItems()) {
      annotations_off[0] = annotation_set == nullptr ? 0 : annotation_set->GetOffset();
      *offset += Write(annotations_off, sizeof(uint32_t), *offset);
    }
  }
  return MapItem(DexFile::kDexTypeAnnotationSetRefList,
                 header_->GetCollections().AnnotationSetRefListsSize(),
                 start);
}

MapItem DexWriter::WriteAnnotationsDirectories(uint32_t* const offset) {
  uint32_t directory_buffer[4];
  uint32_t annotation_buffer[2];
  const uint32_t start = *offset;
  for (std::unique_ptr<dex_ir::AnnotationsDirectoryItem>& annotations_directory :
      header_->GetCollections().AnnotationsDirectoryItems()) {
    *offset = RoundUp(*offset, kDexSectionWordAlignment);
    directory_buffer[0] = annotations_directory->GetClassAnnotation() == nullptr ? 0 :
        annotations_directory->GetClassAnnotation()->GetOffset();
    directory_buffer[1] = annotations_directory->GetFieldAnnotations() == nullptr ? 0 :
        annotations_directory->GetFieldAnnotations()->size();
    directory_buffer[2] = annotations_directory->GetMethodAnnotations() == nullptr ? 0 :
        annotations_directory->GetMethodAnnotations()->size();
    directory_buffer[3] = annotations_directory->GetParameterAnnotations() == nullptr ? 0 :
        annotations_directory->GetParameterAnnotations()->size();
    annotations_directory->SetOffset(*offset);
    *offset += Write(directory_buffer, 4 * sizeof(uint32_t), *offset);
    if (annotations_directory->GetFieldAnnotations() != nullptr) {
      for (std::unique_ptr<dex_ir::FieldAnnotation>& field :
          *annotations_directory->GetFieldAnnotations()) {
        annotation_buffer[0] = field->GetFieldId()->GetIndex();
        annotation_buffer[1] = field->GetAnnotationSetItem()->GetOffset();
        *offset += Write(annotation_buffer, 2 * sizeof(uint32_t), *offset);
      }
    }
    if (annotations_directory->GetMethodAnnotations() != nullptr) {
      for (std::unique_ptr<dex_ir::MethodAnnotation>& method :
          *annotations_directory->GetMethodAnnotations()) {
        annotation_buffer[0] = method->GetMethodId()->GetIndex();
        annotation_buffer[1] = method->GetAnnotationSetItem()->GetOffset();
        *offset += Write(annotation_buffer, 2 * sizeof(uint32_t), *offset);
      }
    }
    if (annotations_directory->GetParameterAnnotations() != nullptr) {
      for (std::unique_ptr<dex_ir::ParameterAnnotation>& parameter :
          *annotations_directory->GetParameterAnnotations()) {
        annotation_buffer[0] = parameter->GetMethodId()->GetIndex();
        annotation_buffer[1] = parameter->GetAnnotations()->GetOffset();
        *offset += Write(annotation_buffer, 2 * sizeof(uint32_t), *offset);
      }
    }
  }
  return MapItem(DexFile::kDexTypeAnnotationsDirectoryItem,
                 header_->GetCollections().AnnotationsDirectoryItemsSize(),
                 start);
}

MapItem DexWriter::WriteDebugInfoItems(uint32_t* const offset) {
  const uint32_t start = *offset;
  for (std::unique_ptr<dex_ir::DebugInfoItem>& debug_info :
      header_->GetCollections().DebugInfoItems()) {
    debug_info->SetOffset(*offset);
    *offset += Write(debug_info->GetDebugInfo(), debug_info->GetDebugInfoSize(), *offset);
  }
  return MapItem(DexFile::kDexTypeDebugInfoItem,
                 header_->GetCollections().DebugInfoItemsSize(),
                 start);
}

MapItem DexWriter::WriteCodeItems(uint32_t* const offset) {
  uint16_t uint16_buffer[4];
  uint32_t uint32_buffer[2];
  const uint32_t start = *offset;
  for (auto& code_item : header_->GetCollections().CodeItems()) {
    *offset = RoundUp(*offset, kDexSectionWordAlignment);
    uint16_buffer[0] = code_item->RegistersSize();
    uint16_buffer[1] = code_item->InsSize();
    uint16_buffer[2] = code_item->OutsSize();
    uint16_buffer[3] = code_item->TriesSize();
    uint32_buffer[0] = code_item->DebugInfo() == nullptr ? 0 : code_item->DebugInfo()->GetOffset();
    uint32_buffer[1] = code_item->InsnsSize();
    code_item->SetOffset(*offset);
    *offset += Write(uint16_buffer, 4 * sizeof(uint16_t), *offset);
    *offset += Write(uint32_buffer, 2 * sizeof(uint32_t), *offset);
    *offset += Write(code_item->Insns(), code_item->InsnsSize() * sizeof(uint16_t), *offset);
    if (code_item->TriesSize() != 0) {
      if (code_item->InsnsSize() % 2 != 0) {
        uint16_t padding[1] = { 0 };
        *offset += Write(padding, sizeof(uint16_t), *offset);
      }
      uint32_t start_addr[1];
      uint16_t insn_count_and_handler_off[2];
      for (std::unique_ptr<const dex_ir::TryItem>& try_item : *code_item->Tries()) {
        start_addr[0] = try_item->StartAddr();
        insn_count_and_handler_off[0] = try_item->InsnCount();
        insn_count_and_handler_off[1] = try_item->GetHandlers()->GetListOffset();
        *offset += Write(start_addr, sizeof(uint32_t), *offset);
        *offset += Write(insn_count_and_handler_off, 2 * sizeof(uint16_t), *offset);
      }
      // Leave offset pointing to the end of the try items.
      UNUSED(WriteUleb128(code_item->Handlers()->size(), *offset));
      for (std::unique_ptr<const dex_ir::CatchHandler>& handlers : *code_item->Handlers()) {
        size_t list_offset = *offset + handlers->GetListOffset();
        uint32_t size = handlers->HasCatchAll() ? (handlers->GetHandlers()->size() - 1) * -1 :
            handlers->GetHandlers()->size();
        list_offset += WriteSleb128(size, list_offset);
        for (std::unique_ptr<const dex_ir::TypeAddrPair>& handler : *handlers->GetHandlers()) {
          if (handler->GetTypeId() != nullptr) {
            list_offset += WriteUleb128(handler->GetTypeId()->GetIndex(), list_offset);
          }
          list_offset += WriteUleb128(handler->GetAddress(), list_offset);
        }
      }
    }
    // TODO: Clean this up to properly calculate the size instead of assuming it doesn't change.
    *offset = code_item->GetOffset() + code_item->GetSize();
  }
  return MapItem(DexFile::kDexTypeCodeItem, header_->GetCollections().CodeItemsSize(), start);
}

MapItem DexWriter::WriteClassDefs(uint32_t* const offset, bool reserve_only) {
  const uint32_t start = *offset;
  uint32_t class_def_buffer[8];
  for (std::unique_ptr<dex_ir::ClassDef>& class_def : header_->GetCollections().ClassDefs()) {
    *offset = RoundUp(*offset, kDexSectionWordAlignment);
    if (reserve_only) {
      *offset += class_def->GetSize();
    } else {
      class_def_buffer[0] = class_def->ClassType()->GetIndex();
      class_def_buffer[1] = class_def->GetAccessFlags();
      class_def_buffer[2] = class_def->Superclass() == nullptr ? dex::kDexNoIndex :
          class_def->Superclass()->GetIndex();
      class_def_buffer[3] = class_def->InterfacesOffset();
      class_def_buffer[4] = class_def->SourceFile() == nullptr ? dex::kDexNoIndex :
          class_def->SourceFile()->GetIndex();
      class_def_buffer[5] = class_def->Annotations() == nullptr ? 0 :
          class_def->Annotations()->GetOffset();
      class_def_buffer[6] = class_def->GetClassData() == nullptr ? 0 :
          class_def->GetClassData()->GetOffset();
      class_def_buffer[7] = class_def->StaticValues() == nullptr ? 0 :
          class_def->StaticValues()->GetOffset();
      *offset += Write(class_def_buffer, class_def->GetSize(), *offset);
    }
  }
  return MapItem(DexFile::kDexTypeClassDefItem,
                 header_->GetCollections().ClassDefsSize(),
                 start);
}

MapItem DexWriter::WriteClassDatas(uint32_t* const offset) {
  const uint32_t start = *offset;
  for (const std::unique_ptr<dex_ir::ClassData>& class_data :
      header_->GetCollections().ClassDatas()) {
    class_data->SetOffset(*offset);
    *offset += WriteUleb128(class_data->StaticFields()->size(), *offset);
    *offset += WriteUleb128(class_data->InstanceFields()->size(), *offset);
    *offset += WriteUleb128(class_data->DirectMethods()->size(), *offset);
    *offset += WriteUleb128(class_data->VirtualMethods()->size(), *offset);
    *offset += WriteEncodedFields(class_data->StaticFields(), *offset);
    *offset += WriteEncodedFields(class_data->InstanceFields(), *offset);
    *offset += WriteEncodedMethods(class_data->DirectMethods(), *offset);
    *offset += WriteEncodedMethods(class_data->VirtualMethods(), *offset);
  }
  return MapItem(DexFile::kDexTypeClassDataItem, header_->GetCollections().ClassDatasSize(), start);
}

MapItem DexWriter::WriteCallSiteIds(uint32_t* const offset) {
  const uint32_t start = *offset;
  uint32_t call_site_off[1];
  for (std::unique_ptr<dex_ir::CallSiteId>& call_site_id :
      header_->GetCollections().CallSiteIds()) {
    *offset = RoundUp(*offset, kDexSectionWordAlignment);
    call_site_off[0] = call_site_id->CallSiteItem()->GetOffset();
    *offset += Write(call_site_off, call_site_id->GetSize(), *offset);
  }
  return MapItem(DexFile::kDexTypeMethodIdItem, header_->GetCollections().CallSiteIdsSize(), start);
}

MapItem DexWriter::WriteMethodHandles(uint32_t* const offset) {
  const uint32_t start = *offset;
  uint16_t method_handle_buff[4];
  for (std::unique_ptr<dex_ir::MethodHandleItem>& method_handle :
      header_->GetCollections().MethodHandleItems()) {
    *offset = RoundUp(*offset, kDexSectionWordAlignment);
    method_handle_buff[0] = static_cast<uint16_t>(method_handle->GetMethodHandleType());
    method_handle_buff[1] = 0;  // unused.
    method_handle_buff[2] = method_handle->GetFieldOrMethodId()->GetIndex();
    method_handle_buff[3] = 0;  // unused.
    *offset += Write(method_handle_buff, method_handle->GetSize(), *offset);
  }
  return MapItem(DexFile::kDexTypeMethodIdItem,
                 header_->GetCollections().MethodHandleItemsSize(),
                 start);
}

void DexWriter::WriteMapItems(uint32_t* const offset, MapItemQueue* queue) {
  // All the sections should already have been added.
  uint16_t uint16_buffer[2];
  uint32_t uint32_buffer[2];
  uint16_buffer[1] = 0;
  uint32_buffer[0] = queue->size();
  *offset += Write(uint32_buffer, sizeof(uint32_t), *offset);
  while (!queue->empty()) {
    const MapItem& map_item = queue->top();
    uint16_buffer[0] = map_item.type_;
    uint32_buffer[0] = map_item.size_;
    uint32_buffer[1] = map_item.offset_;
    *offset += Write(uint16_buffer, 2 * sizeof(uint16_t), *offset);
    *offset += Write(uint32_buffer, 2 * sizeof(uint32_t), *offset);
    queue->pop();
  }
}

void DexWriter::WriteHeader() {
  StandardDexFile::Header header;
  static constexpr size_t kMagicAndVersionLen =
      StandardDexFile::kDexMagicSize + StandardDexFile::kDexVersionLen;
  std::copy_n(header_->Magic(), kMagicAndVersionLen, header.magic_);
  header.checksum_ = header_->Checksum();
  std::copy_n(header_->Signature(), DexFile::kSha1DigestSize, header.signature_);
  header.file_size_ = header_->FileSize();
  header.header_size_ = header_->GetSize();
  header.endian_tag_ = header_->EndianTag();
  header.link_size_ = header_->LinkSize();
  header.link_off_ = header_->LinkOffset();
  const dex_ir::Collections& collections = header_->GetCollections();
  header.map_off_ = collections.MapListOffset();
  header.string_ids_size_ = collections.StringIdsSize();
  header.string_ids_off_ = collections.StringIdsOffset();
  header.type_ids_size_ = collections.TypeIdsSize();
  header.type_ids_off_ = collections.TypeIdsOffset();
  header.proto_ids_size_ = collections.ProtoIdsSize();
  header.proto_ids_off_ = collections.ProtoIdsOffset();
  header.field_ids_size_ = collections.FieldIdsSize();
  header.field_ids_off_ = collections.FieldIdsOffset();
  header.method_ids_size_ = collections.MethodIdsSize();
  header.method_ids_off_ = collections.MethodIdsOffset();
  header.class_defs_size_ = collections.ClassDefsSize();
  header.class_defs_off_ = collections.ClassDefsOffset();
  header.data_size_ = header_->DataSize();
  header.data_off_ = header_->DataOffset();

  static_assert(sizeof(header) == 0x70, "Size doesn't match dex spec");
  UNUSED(Write(reinterpret_cast<uint8_t*>(&header), sizeof(header), 0u));
}

void DexWriter::WriteMemMap() {
  // Starting offset is right after the header.
  uint32_t offset = sizeof(StandardDexFile::Header);

  dex_ir::Collections& collection = header_->GetCollections();
  MapItemQueue map_items;
  map_items.push(MapItem(DexFile::kDexTypeHeaderItem, 1, 0));

  auto add_and_get_offset = [&](const MapItem& item) {
    map_items.AddIfNotEmpty(item);
    return item.offset_;
  };

  // Based on: https://source.android.com/devices/tech/dalvik/dex-format
  // Since the offsets may not be calculated already, the writing must be done in the correct order.
  MapItem string_ids = WriteStringIds(&offset, /*reserve_only*/ true);
  collection.SetStringIdsOffset(add_and_get_offset(string_ids));
  collection.SetTypeIdsOffset(add_and_get_offset(WriteTypeIds(&offset)));
  MapItem proto_ids = WriteProtoIds(&offset, /*reserve_only*/ true);
  collection.SetProtoIdsOffset(add_and_get_offset(proto_ids));
  collection.SetFieldIdsOffset(add_and_get_offset(WriteFieldIds(&offset)));
  collection.SetMethodIdsOffset(add_and_get_offset(WriteMethodIds(&offset)));
  MapItem class_defs = WriteClassDefs(&offset, /*reserve_only*/ true);
  collection.SetClassDefsOffset(add_and_get_offset(class_defs));
  collection.SetCallSiteIdsOffset(add_and_get_offset(WriteCallSiteIds(&offset)));
  collection.SetMethodHandleItemsOffset(add_and_get_offset(WriteMethodHandles(&offset)));

  // Data section.
  offset = RoundUp(offset, kDataSectionAlignment);
  header_->SetDataOffset(offset);
  // Write debug info first since code items depend on the offsets being assigned.
  // This is slightly suboptimal since it will cause the encoded methods to be slightly larger
  // from code item offset leb encoding.
  map_items.AddIfNotEmpty(WriteDebugInfoItems(&offset));
  map_items.AddIfNotEmpty(WriteCodeItems(&offset));
  map_items.AddIfNotEmpty(WriteEncodedArrays(&offset));
  map_items.AddIfNotEmpty(WriteAnnotations(&offset));
  map_items.AddIfNotEmpty(WriteAnnotationSets(&offset));
  map_items.AddIfNotEmpty(WriteAnnotationSetRefs(&offset));
  map_items.AddIfNotEmpty(WriteAnnotationsDirectories(&offset));
  map_items.AddIfNotEmpty(WriteTypeLists(&offset));
  map_items.AddIfNotEmpty(WriteClassDatas(&offset));
  map_items.AddIfNotEmpty(WriteStringDatas(&offset));
  offset = RoundUp(offset, kDataSectionAlignment);
  header_->SetDataSize(header_->DataOffset() - offset);

  // Write delayed sections
  uint32_t string_ids_offset = string_ids.offset_;
  WriteStringIds(&string_ids_offset, /*reserve_only*/ false);
  uint32_t proto_ids_offset = proto_ids.offset_;
  WriteProtoIds(&proto_ids_offset, /*reserve_only*/ false);
  uint32_t class_defs_offset = class_defs.offset_;
  WriteClassDefs(&class_defs_offset, /*reserve_only*/ false);

  // TODO: Write link data?

  // Write the map list.
  collection.SetMapListOffset(offset);
  map_items.push(MapItem(DexFile::kDexTypeMapList, 1, collection.MapListOffset()));
  WriteMapItems(&offset, &map_items);

  // Write header last.
  header_->SetFileSize(offset);
  WriteHeader();
}

void DexWriter::Output(dex_ir::Header* header, MemMap* mem_map, CompactDexLevel compact_dex_level) {
  std::unique_ptr<DexWriter> writer;
  if (compact_dex_level != CompactDexLevel::kCompactDexLevelNone) {
    writer.reset(new CompactDexWriter(header, mem_map, compact_dex_level));
  } else {
    writer.reset(new DexWriter(header, mem_map));
  }
  writer->WriteMemMap();
}

void MapItemQueue::AddIfNotEmpty(const MapItem& item) {
  if (item.size_ != 0) {
    push(item);
  }
}

}  // namespace art
