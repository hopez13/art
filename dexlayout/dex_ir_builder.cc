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

#include "dex_ir_builder.h"

namespace art {
namespace dex_ir {

namespace {

static bool GetPositionsCb(void* context, const DexFile::PositionInfo& entry) {
  DebugInfoItem* debug_info = reinterpret_cast<DebugInfoItem*>(context);
  PositionInfoVector& positions = debug_info->GetPositionInfo();
  positions.push_back(std::unique_ptr<PositionInfo>(new PositionInfo(entry.address_, entry.line_)));
  return false;
}

static void GetLocalsCb(void* context, const DexFile::LocalInfo& entry) {
  DebugInfoItem* debug_info = reinterpret_cast<DebugInfoItem*>(context);
  LocalInfoVector& locals = debug_info->GetLocalInfo();
  const char* name = entry.name_ != nullptr ? entry.name_ : "(null)";
  const char* signature = entry.signature_ != nullptr ? entry.signature_ : "";
  locals.push_back(std::unique_ptr<LocalInfo>(
      new LocalInfo(name, entry.descriptor_, signature, entry.start_address_,
                    entry.end_address_, entry.reg_)));
}

CodeItem* ReadCodeItem(const DexFile& dex_file,
                       const DexFile::CodeItem& disk_code_item,
                       Header* header) {
  uint16_t registers_size = disk_code_item.registers_size_;
  uint16_t ins_size = disk_code_item.ins_size_;
  uint16_t outs_size = disk_code_item.outs_size_;
  uint32_t tries_size = disk_code_item.tries_size_;

  const uint8_t* debug_info_stream = dex_file.GetDebugInfoStream(&disk_code_item);
  DebugInfoItem* debug_info = nullptr;
  if (debug_info_stream != nullptr) {
    debug_info = new DebugInfoItem();
  }

  uint32_t insns_size = disk_code_item.insns_size_in_code_units_;
  uint16_t* insns = new uint16_t[insns_size];
  memcpy(insns, disk_code_item.insns_, insns_size * sizeof(uint16_t));

  TryItemVector* tries = nullptr;
  if (tries_size > 0) {
    tries = new TryItemVector();
    for (uint32_t i = 0; i < tries_size; ++i) {
      const DexFile::TryItem* disk_try_item = dex_file.GetTryItems(disk_code_item, i);
      uint32_t start_addr = disk_try_item->start_addr_;
      uint16_t insn_count = disk_try_item->insn_count_;
      CatchHandlerVector* handlers = new CatchHandlerVector();
      for (CatchHandlerIterator it(disk_code_item, *disk_try_item); it.HasNext(); it.Next()) {
        const uint16_t type_index = it.GetHandlerTypeIndex();
        const TypeId* type_id = header->GetCollections().GetTypeIdOrNullPtr(type_index);
        handlers->push_back(std::unique_ptr<const CatchHandler>(
            new CatchHandler(type_id, it.GetHandlerAddress())));
      }
      TryItem* try_item = new TryItem(start_addr, insn_count, handlers);
      tries->push_back(std::unique_ptr<const TryItem>(try_item));
    }
  }
  return new CodeItem(registers_size, ins_size, outs_size, debug_info, insns_size, insns, tries);
}

MethodItem* GenerateMethodItem(const DexFile& dex_file,
                               dex_ir::Header* header,
                               ClassDataItemIterator& cdii) {
  MethodId* method_item = header->GetCollections().GetMethodId(cdii.GetMemberIndex());
  uint32_t access_flags = cdii.GetRawMemberAccessFlags();
  const DexFile::CodeItem* disk_code_item = cdii.GetMethodCodeItem();
  CodeItem* code_item = nullptr;
  DebugInfoItem* debug_info = nullptr;
  if (disk_code_item != nullptr) {
    code_item = ReadCodeItem(dex_file, *disk_code_item, header);
    code_item->SetOffset(cdii.GetMethodCodeItemOffset());
    debug_info = code_item->DebugInfo();
  }
  if (debug_info != nullptr) {
    bool is_static = (access_flags & kAccStatic) != 0;
    dex_file.DecodeDebugLocalInfo(
        disk_code_item, is_static, cdii.GetMemberIndex(), GetLocalsCb, debug_info);
    dex_file.DecodeDebugPositionInfo(disk_code_item, GetPositionsCb, debug_info);
  }
  return new MethodItem(access_flags, method_item, code_item);
}

AnnotationSetItem* ReadAnnotationSetItem(const DexFile& dex_file,
                                         const DexFile::AnnotationSetItem& disk_annotations_item,
                                         Header* header) {
  if (disk_annotations_item.size_ == 0) {
    return nullptr;
  }
  AnnotationItemVector* items = new AnnotationItemVector();
  for (uint32_t i = 0; i < disk_annotations_item.size_; ++i) {
    const DexFile::AnnotationItem* annotation =
        dex_file.GetAnnotationItem(&disk_annotations_item, i);
    if (annotation == nullptr) {
      continue;
    }
    uint8_t visibility = annotation->visibility_;
    const uint8_t* annotation_data = annotation->annotation_;
    // TODO: Move caller into dex_ir::Collections?
    EncodedValue* encoded_value = header->GetCollections().ReadEncodedValue(
        &annotation_data, DexFile::kDexAnnotationAnnotation, 0);
    items->push_back(std::unique_ptr<AnnotationItem>(
        new AnnotationItem(visibility, encoded_value->ReleaseEncodedAnnotation())));
  }
  return new AnnotationSetItem(items);
}

ParameterAnnotation* ReadParameterAnnotation(
    const DexFile& dex_file,
    MethodId* method_id,
    const DexFile::AnnotationSetRefList* annotation_set_ref_list,
    Header* header) {
  AnnotationSetItemVector* annotations = new AnnotationSetItemVector();
  for (uint32_t i = 0; i < annotation_set_ref_list->size_; ++i) {
    const DexFile::AnnotationSetItem* annotation_set_item =
        dex_file.GetSetRefItemItem(&annotation_set_ref_list->list_[i]);
    annotations->push_back(std::unique_ptr<AnnotationSetItem>(
        ReadAnnotationSetItem(dex_file, *annotation_set_item, header)));
  }
  return new ParameterAnnotation(method_id, annotations);
}

AnnotationsDirectoryItem* ReadAnnotationsDirectoryItem(
    const DexFile& dex_file,
    const DexFile::AnnotationsDirectoryItem* disk_annotations_item,
    Header* header) {
  const DexFile::AnnotationSetItem* class_set_item =
      dex_file.GetClassAnnotationSet(disk_annotations_item);
  AnnotationSetItem* class_annotation = nullptr;
  if (class_set_item != nullptr) {
    class_annotation = ReadAnnotationSetItem(dex_file, *class_set_item, header);
  }
  const DexFile::FieldAnnotationsItem* fields =
      dex_file.GetFieldAnnotations(disk_annotations_item);
  FieldAnnotationVector* field_annotations = nullptr;
  if (fields != nullptr) {
    field_annotations = new FieldAnnotationVector();
    for (uint32_t i = 0; i < disk_annotations_item->fields_size_; ++i) {
      FieldId* field_id = header->GetCollections().GetFieldId(fields[i].field_idx_);
      const DexFile::AnnotationSetItem* field_set_item =
          dex_file.GetFieldAnnotationSetItem(fields[i]);
      AnnotationSetItem* annotation_set_item =
          ReadAnnotationSetItem(dex_file, *field_set_item, header);
      field_annotations->push_back(std::unique_ptr<FieldAnnotation>(
          new FieldAnnotation(field_id, annotation_set_item)));
    }
  }
  const DexFile::MethodAnnotationsItem* methods =
      dex_file.GetMethodAnnotations(disk_annotations_item);
  MethodAnnotationVector* method_annotations = nullptr;
  if (methods != nullptr) {
    method_annotations = new MethodAnnotationVector();
    for (uint32_t i = 0; i < disk_annotations_item->methods_size_; ++i) {
      MethodId* method_id = header->GetCollections().GetMethodId(methods[i].method_idx_);
      const DexFile::AnnotationSetItem* method_set_item =
          dex_file.GetMethodAnnotationSetItem(methods[i]);
      AnnotationSetItem* annotation_set_item =
          ReadAnnotationSetItem(dex_file, *method_set_item, header);
      method_annotations->push_back(std::unique_ptr<MethodAnnotation>(
          new MethodAnnotation(method_id, annotation_set_item)));
    }
  }
  const DexFile::ParameterAnnotationsItem* parameters =
      dex_file.GetParameterAnnotations(disk_annotations_item);
  ParameterAnnotationVector* parameter_annotations = nullptr;
  if (parameters != nullptr) {
    parameter_annotations = new ParameterAnnotationVector();
    for (uint32_t i = 0; i < disk_annotations_item->parameters_size_; ++i) {
      MethodId* method_id = header->GetCollections().GetMethodId(parameters[i].method_idx_);
      const DexFile::AnnotationSetRefList* list =
          dex_file.GetParameterAnnotationSetRefList(&parameters[i]);
      parameter_annotations->push_back(std::unique_ptr<ParameterAnnotation>(
          ReadParameterAnnotation(dex_file, method_id, list, header)));
    }
  }

  return new AnnotationsDirectoryItem(class_annotation,
                                      field_annotations,
                                      method_annotations,
                                      parameter_annotations);
}

}  // namespace

ClassDef* ReadClassDef(const DexFile& dex_file,
                       const DexFile::ClassDef& disk_class_def,
                       Header* header) {
  Collections& collections = header->GetCollections();
  const TypeId* class_type = collections.GetTypeId(disk_class_def.class_idx_);
  uint32_t access_flags = disk_class_def.access_flags_;
  const TypeId* superclass = collections.GetTypeIdOrNullPtr(disk_class_def.superclass_idx_);

  const DexFile::TypeList* type_list = dex_file.GetInterfacesList(disk_class_def);
  TypeList* interfaces_type_list =
      collections.CreateTypeList(type_list, disk_class_def.interfaces_off_, false);

  const StringId* source_file = collections.GetStringIdOrNullPtr(disk_class_def.source_file_idx_);
  // Annotations.
  AnnotationsDirectoryItem* annotations = nullptr;
  const DexFile::AnnotationsDirectoryItem* disk_annotations_directory_item =
      dex_file.GetAnnotationsDirectory(disk_class_def);
  if (disk_annotations_directory_item != nullptr) {
    annotations = ReadAnnotationsDirectoryItem(dex_file, disk_annotations_directory_item, header);
    annotations->SetOffset(disk_class_def.annotations_off_);
  }
  // Static field initializers.
  const uint8_t* static_data = dex_file.GetEncodedStaticFieldValuesArray(disk_class_def);
  EncodedArrayItem* static_values =
      collections.CreateEncodedArrayItem(static_data, disk_class_def.static_values_off_);

  // Read the fields and methods defined by the class, resolving the circular reference from those
  // to classes by setting class at the same time.
  const uint8_t* encoded_data = dex_file.GetClassData(disk_class_def);
  ClassData* class_data = nullptr;
  if (encoded_data != nullptr) {
    uint32_t offset = disk_class_def.class_data_off_;
    ClassDataItemIterator cdii(dex_file, encoded_data);
    // Static fields.
    FieldItemVector* static_fields = new FieldItemVector();
    for (uint32_t i = 0; cdii.HasNextStaticField(); i++, cdii.Next()) {
      FieldId* field_item = collections.GetFieldId(cdii.GetMemberIndex());
      uint32_t access_flags = cdii.GetRawMemberAccessFlags();
      static_fields->push_back(std::unique_ptr<FieldItem>(new FieldItem(access_flags, field_item)));
    }
    // Instance fields.
    FieldItemVector* instance_fields = new FieldItemVector();
    for (uint32_t i = 0; cdii.HasNextInstanceField(); i++, cdii.Next()) {
      FieldId* field_item = collections.GetFieldId(cdii.GetMemberIndex());
      uint32_t access_flags = cdii.GetRawMemberAccessFlags();
      instance_fields->push_back(
          std::unique_ptr<FieldItem>(new FieldItem(access_flags, field_item)));
    }
    // Direct methods.
    MethodItemVector* direct_methods = new MethodItemVector();
    for (uint32_t i = 0; cdii.HasNextDirectMethod(); i++, cdii.Next()) {
      direct_methods->push_back(
          std::unique_ptr<MethodItem>(GenerateMethodItem(dex_file, header, cdii)));
    }
    // Virtual methods.
    MethodItemVector* virtual_methods = new MethodItemVector();
    for (uint32_t i = 0; cdii.HasNextVirtualMethod(); i++, cdii.Next()) {
      virtual_methods->push_back(
          std::unique_ptr<MethodItem>(GenerateMethodItem(dex_file, header, cdii)));
    }
    class_data = new ClassData(static_fields, instance_fields, direct_methods, virtual_methods);
    class_data->SetOffset(offset);
  }
  return new ClassDef(class_type,
                      access_flags,
                      superclass,
                      interfaces_type_list,
                      source_file,
                      annotations,
                      static_values,
                      class_data);
}

Header* DexIrBuilder(const DexFile& dex_file) {
  const DexFile::Header& disk_header = dex_file.GetHeader();
  Header* header = new Header(disk_header.magic_,
                              disk_header.checksum_,
                              disk_header.signature_,
                              disk_header.endian_tag_,
                              disk_header.file_size_,
                              disk_header.header_size_,
                              disk_header.link_size_,
                              disk_header.link_off_,
                              disk_header.data_size_,
                              disk_header.data_off_);
  // Walk the rest of the header fields.
  // StringId table.
  header->GetCollections().SetStringIdsOffset(disk_header.string_ids_off_);
  for (uint32_t i = 0; i < dex_file.NumStringIds(); ++i) {
    header->GetCollections().CreateStringId(dex_file, i);
  }
  // TypeId table.
  header->GetCollections().SetTypeIdsOffset(disk_header.type_ids_off_);
  for (uint32_t i = 0; i < dex_file.NumTypeIds(); ++i) {
    header->GetCollections().CreateTypeId(dex_file, i);
  }
  // ProtoId table.
  header->GetCollections().SetProtoIdsOffset(disk_header.proto_ids_off_);
  for (uint32_t i = 0; i < dex_file.NumProtoIds(); ++i) {
    header->GetCollections().CreateProtoId(dex_file, i);
  }
  // FieldId table.
  header->GetCollections().SetFieldIdsOffset(disk_header.field_ids_off_);
  for (uint32_t i = 0; i < dex_file.NumFieldIds(); ++i) {
    header->GetCollections().CreateFieldId(dex_file, i);
  }
  // MethodId table.
  header->GetCollections().SetMethodIdsOffset(disk_header.method_ids_off_);
  for (uint32_t i = 0; i < dex_file.NumMethodIds(); ++i) {
    header->GetCollections().CreateMethodId(dex_file, i);
  }
  // ClassDef table.
  header->GetCollections().SetClassDefsOffset(disk_header.class_defs_off_);
  for (uint32_t i = 0; i < dex_file.NumClassDefs(); ++i) {
    header->GetCollections().CreateClassDef(dex_file, header, i);
  }

  return header;
}

}  // namespace dex_ir
}  // namespace art
