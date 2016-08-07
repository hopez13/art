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

#ifndef ART_DEXLAYOUT_DEX_IR_H_
#define ART_DEXLAYOUT_DEX_IR_H_

#include <iostream>
#include <map>
#include <vector>
#include <stdint.h>

#include "dex_file.h"

namespace art {
namespace dex_ir {

// Forward declarations for classes used in containers or pointed to.
class Header;
class StringId;
class TypeId;
class ProtoId;
class FieldId;
class MethodId;
class ClassDef;
class FieldItem;
class MethodItem;
class ArrayItem;
class CodeItem;
class TryItem;
class DebugInfoItem;
class AnnotationSetItem;
class AnnotationsDirectoryItem;
class MapList;
class MapItem;

// Visitor support
class AbstractDispatcher {
 public:
  AbstractDispatcher() = default;
  virtual ~AbstractDispatcher() { }

  virtual void dispatch(Header* header) = 0;
  virtual void dispatch(const StringId* string_id) = 0;
  virtual void dispatch(const TypeId* type_id) = 0;
  virtual void dispatch(const ProtoId* proto_id) = 0;
  virtual void dispatch(const FieldId* field_id) = 0;
  virtual void dispatch(const MethodId* method_id) = 0;
  virtual void dispatch(ClassDef* class_def) = 0;
  virtual void dispatch(FieldItem* field_item) = 0;
  virtual void dispatch(MethodItem* method_item) = 0;
  virtual void dispatch(ArrayItem* array_item) = 0;
  virtual void dispatch(CodeItem* code_item) = 0;
  virtual void dispatch(TryItem* try_item) = 0;
  virtual void dispatch(DebugInfoItem* debug_info_item) = 0;
  virtual void dispatch(AnnotationSetItem* annotation_set_item) = 0;
  virtual void dispatch(AnnotationsDirectoryItem* annotations_directory_item) = 0;
  virtual void dispatch(MapList* map_list) = 0;
  virtual void dispatch(MapItem* map_item) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AbstractDispatcher);
};

template<class T> class CollectionWithOffset {
 public:
  CollectionWithOffset() : offset_(0) { }
  std::vector<T>* collection() { return &collection_; }
  // Read-time support methods
  void add_at_position(uint32_t position, T object) {
    collection_.push_back(object);
    (object)->set_offset(position);
    CHECK_EQ(collection_[position], object);
  }
  // Ordinary object insertion into collection.
  void insert(T object ATTRIBUTE_UNUSED) {
    // TODO(sehr): add ordered insertion support.
    UNIMPLEMENTED(FATAL) << "Insertion not ready";
  }
  uint32_t offset() const { return offset_; }
  void set_offset(uint32_t new_offset) { offset_ = new_offset; }
  uint32_t size() const { return collection_.size(); }
 private:
  std::vector<T> collection_;
  uint32_t offset_;
  DISALLOW_COPY_AND_ASSIGN(CollectionWithOffset);
};

class Item {
 public:
  virtual ~Item() { }
  uint32_t offset() const { return offset_; }
  void set_offset(uint32_t offset) { offset_ = offset; }
 protected:
  uint32_t offset_ = 0;
};

class Header : public Item {
 private:

 public:
  explicit Header(const DexFile& dex_file);
  ~Header() OVERRIDE { }

  const DexFile& dex_file() const { return dex_file_; }

  const uint8_t* magic() const { return magic_; }
  uint32_t checksum() const { return checksum_; }
  const uint8_t* signature() const { return signature_; }
  uint32_t endian_tag() const { return endian_tag_; }
  uint32_t file_size() const { return file_size_; }
  uint32_t header_size() const { return header_size_; }
  uint32_t link_size() const { return link_size_; }
  uint32_t link_offset() const { return link_offset_; }
  uint32_t data_size() const { return data_size_; }
  uint32_t data_offset() const { return data_offset_; }

  void set_checksum(uint32_t new_checksum) { checksum_ = new_checksum; }
  void set_signature(const uint8_t* new_signature) {
    memcpy(signature_, new_signature, sizeof(signature_));
  }
  void set_file_size(uint32_t new_file_size) { file_size_ = new_file_size; }
  void set_header_size(uint32_t new_header_size) { header_size_ = new_header_size; }
  void set_link_size(uint32_t new_link_size) { link_size_ = new_link_size; }
  void set_link_offset(uint32_t new_link_offset) { link_offset_ = new_link_offset; }
  void set_data_size(uint32_t new_data_size) { data_size_ = new_data_size; }
  void set_data_offset(uint32_t new_data_offset) { data_offset_ = new_data_offset; }

  // Collections.
  std::vector<StringId*>* string_ids() { return string_ids_.collection(); }
  std::vector<TypeId*>* type_ids() { return type_ids_.collection(); }
  std::vector<ProtoId*>* proto_ids() { return proto_ids_.collection(); }
  std::vector<FieldId*>* field_ids() { return field_ids_.collection(); }
  std::vector<MethodId*>* method_ids() { return method_ids_.collection(); }
  std::vector<ClassDef*>* class_defs() { return class_defs_.collection(); }
  uint32_t string_ids_offset() const { return string_ids_.offset(); }
  uint32_t type_ids_offset() const { return type_ids_.offset(); }
  uint32_t proto_ids_offset() const { return proto_ids_.offset(); }
  uint32_t field_ids_offset() const { return field_ids_.offset(); }
  uint32_t method_ids_offset() const { return method_ids_.offset(); }
  uint32_t class_defs_offset() const { return class_defs_.offset(); }
  void set_string_ids_offset(uint32_t new_offset) { string_ids_.set_offset(new_offset); }
  void set_type_ids_offset(uint32_t new_offset) { type_ids_.set_offset(new_offset); }
  void set_proto_ids_offset(uint32_t new_offset) { proto_ids_.set_offset(new_offset); }
  void set_field_ids_offset(uint32_t new_offset) { field_ids_.set_offset(new_offset); }
  void set_method_ids_offset(uint32_t new_offset) { method_ids_.set_offset(new_offset); }
  void set_class_defs_offset(uint32_t new_offset) { class_defs_.set_offset(new_offset); }
  uint32_t string_ids_size() const { return string_ids_.size(); }
  uint32_t type_ids_size() const { return type_ids_.size(); }
  uint32_t proto_ids_size() const { return proto_ids_.size(); }
  uint32_t field_ids_size() const { return field_ids_.size(); }
  uint32_t method_ids_size() const { return method_ids_.size(); }
  uint32_t class_defs_size() const { return class_defs_.size(); }

  void accept(AbstractDispatcher* dispatch) { dispatch->dispatch(this); }

 private:
  const DexFile& dex_file_;
  uint8_t magic_[8];
  uint32_t checksum_;
  uint8_t signature_[DexFile::kSha1DigestSize];
  uint32_t endian_tag_;
  uint32_t file_size_;
  uint32_t header_size_;
  uint32_t link_size_;
  uint32_t link_offset_;
  uint32_t data_size_;
  uint32_t data_offset_;

  CollectionWithOffset<StringId*> string_ids_;
  CollectionWithOffset<TypeId*> type_ids_;
  CollectionWithOffset<ProtoId*> proto_ids_;
  CollectionWithOffset<FieldId*> field_ids_;
  CollectionWithOffset<MethodId*> method_ids_;
  CollectionWithOffset<ClassDef*> class_defs_;
  DISALLOW_COPY_AND_ASSIGN(Header);
};

class StringId : public Item {
 public:
  StringId(const DexFile::StringId& disk_string_id, Header& header) :
    data_(strdup(header.dex_file().GetStringData(disk_string_id))) {
  }
  ~StringId() OVERRIDE { }

  const char* data() const { return data_; }

  void accept(AbstractDispatcher* dispatch) const { dispatch->dispatch(this); }

 private:
  const char* data_;
  DISALLOW_COPY_AND_ASSIGN(StringId);
};

class TypeId : public Item {
 public:
  TypeId(const DexFile::TypeId& disk_type_id, Header& header) :
    string_id_((*header.string_ids())[disk_type_id.descriptor_idx_]) {
  }
  ~TypeId() OVERRIDE { }

  const StringId* string_id() const { return string_id_; }

  void accept(AbstractDispatcher* dispatch) const { dispatch->dispatch(this); }

 private:
  const StringId* string_id_;
  DISALLOW_COPY_AND_ASSIGN(TypeId);
};

class ProtoId : public Item {
 public:
  ProtoId(const DexFile::ProtoId& disk_proto_id, Header& header) {
    shorty_ = (*header.string_ids())[disk_proto_id.shorty_idx_];
    return_type_ = (*header.type_ids())[disk_proto_id.return_type_idx_];
    DexFileParameterIterator dfpi(header.dex_file(), disk_proto_id);
    while (dfpi.HasNext()) {
      parameters_.push_back((*header.type_ids())[dfpi.GetTypeIdx()]);
      dfpi.Next();
    }
  }
  ~ProtoId() OVERRIDE { }

  const StringId* shorty() const { return shorty_; }
  const TypeId* return_type() const { return return_type_; }
  std::vector<const TypeId*>* parameters() { return &parameters_; }

  void accept(AbstractDispatcher* dispatch) const { dispatch->dispatch(this); }

 private:
  const StringId* shorty_;
  const TypeId* return_type_;
  std::vector<const TypeId*> parameters_;
  DISALLOW_COPY_AND_ASSIGN(ProtoId);
};

class FieldId : public Item {
 public:
  FieldId(const DexFile::FieldId& disk_field_id, Header& header) {
    class_def_ = nullptr;
    type_ = (*header.type_ids())[disk_field_id.type_idx_];
    name_ = (*header.string_ids())[disk_field_id.name_idx_];
  }
  ~FieldId() OVERRIDE { }

  // Breaks the cyclic type dependence between fields and classes.
  void setClass(const ClassDef* class_def) { class_def_ = class_def; }

  const ClassDef* class_def() const { return class_def_; }
  const TypeId* type() const { return type_; }
  const StringId* name() const { return name_; }

  void accept(AbstractDispatcher* dispatch) const { dispatch->dispatch(this); }

 private:
  const ClassDef* class_def_;
  const TypeId* type_;
  const StringId* name_;
  DISALLOW_COPY_AND_ASSIGN(FieldId);
};

class MethodId : public Item {
 public:
  MethodId(const DexFile::MethodId& disk_method_id, Header& header) {
    class_def_ = nullptr;
    proto_ = (*header.proto_ids())[disk_method_id.proto_idx_];
    name_ = (*header.string_ids())[disk_method_id.name_idx_];
  }
  ~MethodId() OVERRIDE { }

  // Breaks the cyclic type dependence between fields and classes.
  void setClass(const ClassDef* class_def) { class_def_ = class_def; }

  const ClassDef* class_def() const { return class_def_; }
  const ProtoId* proto() const { return proto_; }
  const StringId* name() const { return name_; }

  void accept(AbstractDispatcher* dispatch) const { dispatch->dispatch(this); }

 private:
  const ClassDef* class_def_;
  const ProtoId* proto_;
  const StringId* name_;
  DISALLOW_COPY_AND_ASSIGN(MethodId);
};

class FieldItem : public Item {
 public:
  FieldItem(uint32_t access_flags, const FieldId* field_id) :
    access_flags_(access_flags), field_id_(field_id) { }
  ~FieldItem() OVERRIDE { }

  uint32_t access_flags() const { return access_flags_; }
  const FieldId* field_id() const { return field_id_; }

  void accept(AbstractDispatcher* dispatch) { dispatch->dispatch(this); }

 private:
  uint32_t access_flags_;
  const FieldId* field_id_;
  DISALLOW_COPY_AND_ASSIGN(FieldItem);
};

class MethodItem : public Item {
 public:
  MethodItem(uint32_t access_flags, const MethodId* method_id, const CodeItem* code) :
    access_flags_(access_flags), method_id_(method_id), code_(code) { }
  ~MethodItem() OVERRIDE { }

  uint32_t access_flags() const { return access_flags_; }
  const MethodId* method_id() const { return method_id_; }
  const CodeItem* code() const { return code_; }

  void accept(AbstractDispatcher* dispatch) { dispatch->dispatch(this); }

 private:
  uint32_t access_flags_;
  const MethodId* method_id_;
  const CodeItem* code_;
  DISALLOW_COPY_AND_ASSIGN(MethodItem);
};

class ArrayItem : public Item {
 public:
  class NameValuePair {
   public:
    NameValuePair(StringId* name, ArrayItem* value) :
      name_(name), value_(value) { }

    StringId* name() const { return name_; }
    ArrayItem* value() const { return value_; }

   private:
    StringId* name_;
    ArrayItem* value_;
    DISALLOW_COPY_AND_ASSIGN(NameValuePair);
  };

  ArrayItem(Header& header, const uint8_t** data, uint8_t type, uint8_t length);
  ArrayItem(Header& header, const uint8_t** data);
  ~ArrayItem() OVERRIDE { }

  int8_t type() const { return type_; }
  bool bool_val() const { return item_.bool_val_; }
  int8_t byte_val() const { return item_.byte_val_; }
  int16_t short_val() const { return item_.short_val_; }
  uint16_t char_val() const { return item_.char_val_; }
  int32_t int_val() const { return item_.int_val_; }
  int64_t long_val() const { return item_.long_val_; }
  float float_val() const { return item_.float_val_; }
  double double_val() const { return item_.double_val_; }
  StringId* string_val() const { return item_.string_val_; }
  FieldId* field_val() const { return item_.field_val_; }
  MethodId* method_val() const { return item_.method_val_; }
  std::vector<ArrayItem*>* annotation_array_val() const {
    return item_.annotation_array_val_;
  }
  StringId* annotation_annotation_string() const {
    return item_.annotation_annotation_val_.string_;
  }
  std::vector<NameValuePair*>* annotation_annotation_nvp_array() const {
    return item_.annotation_annotation_val_.array_;
  }

  void accept(AbstractDispatcher* dispatch) { dispatch->dispatch(this); }

 private:
  void Read(Header& header, const uint8_t** data, uint8_t type, uint8_t length);
  uint8_t type_;
  union {
    bool bool_val_;
    int8_t byte_val_;
    int16_t short_val_;
    uint16_t char_val_;
    int32_t int_val_;
    int64_t long_val_;
    float float_val_;
    double double_val_;
    StringId* string_val_;
    FieldId* field_val_;
    MethodId* method_val_;
    std::vector<ArrayItem*>* annotation_array_val_;
    struct {
      StringId* string_;
      std::vector<NameValuePair*>* array_;
    } annotation_annotation_val_;
  } item_;
  DISALLOW_COPY_AND_ASSIGN(ArrayItem);
};

class ClassDef : public Item {
 public:
  struct ClassData : public Item {
   public:
    ClassData() = default;
    ~ClassData() OVERRIDE = default;
    std::vector<FieldItem*>* static_fields() { return &static_fields_; }
    std::vector<FieldItem*>* instance_fields() { return &instance_fields_; }
    std::vector<MethodItem*>* direct_methods() { return &direct_methods_; }
    std::vector<MethodItem*>* virtual_methods() { return &virtual_methods_; }

   private:
    std::vector<FieldItem*> static_fields_;
    std::vector<FieldItem*> instance_fields_;
    std::vector<MethodItem*> direct_methods_;
    std::vector<MethodItem*> virtual_methods_;
    DISALLOW_COPY_AND_ASSIGN(ClassData);
  };

  ClassDef(const DexFile::ClassDef& disk_class_def, Header& header);
  ~ClassDef() OVERRIDE { }

  const TypeId* class_type() const { return class_type_; }
  uint32_t access_flags() const { return access_flags_; }
  const TypeId* superclass() const { return superclass_; }
  std::vector<TypeId*>* interfaces() { return &interfaces_; }
  uint32_t interfaces_offset() const { return interfaces_offset_; }
  void set_interfaces_offset(uint32_t new_offset) { interfaces_offset_ = new_offset; }
  const StringId* source_file() const { return source_file_; }
  AnnotationsDirectoryItem* annotations() const { return annotations_; }
  std::vector<ArrayItem*>* static_values() { return static_values_; }
  ClassData* class_data() { return &class_data_; }

  void accept(AbstractDispatcher* dispatch) { dispatch->dispatch(this); }

 private:
  const TypeId* class_type_;
  uint32_t access_flags_;
  const TypeId* superclass_;
  std::vector<TypeId*> interfaces_;
  uint32_t interfaces_offset_;
  const StringId* source_file_;
  AnnotationsDirectoryItem* annotations_;
  std::vector<ArrayItem*>* static_values_;
  ClassData class_data_;
  DISALLOW_COPY_AND_ASSIGN(ClassDef);
};

class CodeItem : public Item {
 public:
  CodeItem(const DexFile::CodeItem& disk_code_item, Header& header);
  ~CodeItem() OVERRIDE { }

  uint16_t registers_size() const { return registers_size_; }
  uint16_t ins_size() const { return ins_size_; }
  uint16_t outs_size() const { return outs_size_; }
  uint16_t tries_size() const { return tries_size_; }
  std::vector<const DebugInfoItem*>* debug_info() const { return debug_info_; }
  uint32_t insns_size() const { return insns_size_; }
  uint16_t* insns() const { return insns_; }
  std::vector<const TryItem*>* tries() const { return tries_; }

  void accept(AbstractDispatcher* dispatch) { dispatch->dispatch(this); }

 private:
  uint16_t registers_size_;
  uint16_t ins_size_;
  uint16_t outs_size_;
  uint16_t tries_size_;
  std::vector<const DebugInfoItem*>* debug_info_;
  uint32_t insns_size_;
  uint16_t* insns_;
  std::vector<const TryItem*>* tries_;
  DISALLOW_COPY_AND_ASSIGN(CodeItem);
};

class TryItem : public Item {
 public:
  class CatchHandler {
   public:
    CatchHandler(const TypeId* type_id, uint32_t address) : type_id_(type_id), address_(address) { }

    const TypeId* type_id() const { return type_id_; }
    uint32_t address() const { return address_; }

   private:
    const TypeId* type_id_;
    uint32_t address_;
    DISALLOW_COPY_AND_ASSIGN(CatchHandler);
  };

  TryItem(const DexFile::TryItem& disk_try_item, const DexFile::CodeItem& disk_code_item,
          Header& header) {
    start_addr_ = disk_try_item.start_addr_;
    insn_count_ = disk_try_item.insn_count_;
    for (CatchHandlerIterator it(disk_code_item, disk_try_item); it.HasNext(); it.Next()) {
      const uint16_t type_index = it.GetHandlerTypeIndex();
      handlers_.push_back(new CatchHandler((*header.type_ids())[type_index],
                                           it.GetHandlerAddress()));
    }
  }
  ~TryItem() OVERRIDE { }

  uint32_t start_addr() const { return start_addr_; }
  uint16_t insn_count() const { return insn_count_; }
  std::vector<const CatchHandler*>* handlers() { return &handlers_; }

  void accept(AbstractDispatcher* dispatch) { dispatch->dispatch(this); }

 private:
  uint32_t start_addr_;
  uint16_t insn_count_;
  std::vector<const CatchHandler*> handlers_;
  DISALLOW_COPY_AND_ASSIGN(TryItem);
};

// TODO(sehr): implement debug information.
class DebugInfoItem : public Item {
 public:
 private:
  DISALLOW_COPY_AND_ASSIGN(DebugInfoItem);
};

class AnnotationSetItem : public Item {
 public:
  class AnnotationItem {
   public:
    AnnotationItem(uint8_t visibility, ArrayItem* item) :
      visibility_(visibility), item_(item) { }

    uint8_t visibility() const { return visibility_; }
    ArrayItem* item() const { return item_; }

   private:
    uint8_t visibility_;
    ArrayItem* item_;
    DISALLOW_COPY_AND_ASSIGN(AnnotationItem);
  };

  AnnotationSetItem(const DexFile::AnnotationSetItem& disk_annotations_item, Header& header);
  ~AnnotationSetItem() OVERRIDE { }

  std::vector<AnnotationItem*>* items() { return &items_; }

  void accept(AbstractDispatcher* dispatch) { dispatch->dispatch(this); }

 private:
  std::vector<AnnotationItem*> items_;
  DISALLOW_COPY_AND_ASSIGN(AnnotationSetItem);
};

class AnnotationsDirectoryItem : public Item {
 public:
  class FieldAnnotation {
   public:
    FieldAnnotation(FieldId* field_id, AnnotationSetItem* annotation_set_item) :
      field_id_(field_id), annotation_set_item_(annotation_set_item) { }

    FieldId* field_id() const { return field_id_; }
    AnnotationSetItem* annotation_set_item() const { return annotation_set_item_; }

   private:
    FieldId* field_id_;
    AnnotationSetItem* annotation_set_item_;
    DISALLOW_COPY_AND_ASSIGN(FieldAnnotation);
  };

  class MethodAnnotation {
   public:
    MethodAnnotation(MethodId* method_id, AnnotationSetItem* annotation_set_item) :
      method_id_(method_id), annotation_set_item_(annotation_set_item) { }

    MethodId* method_id() const { return method_id_; }
    AnnotationSetItem* annotation_set_item() const { return annotation_set_item_; }

   private:
    MethodId* method_id_;
    AnnotationSetItem* annotation_set_item_;
    DISALLOW_COPY_AND_ASSIGN(MethodAnnotation);
  };

  class ParameterAnnotation {
   public:
    ParameterAnnotation(MethodId* method_id,
                        const DexFile::AnnotationSetRefList* annotation_set_ref_list,
                        Header& header) :
      method_id_(method_id) {
      for (uint32_t i = 0; i < annotation_set_ref_list->size_; ++i) {
        const DexFile::AnnotationSetItem* annotation_set_item =
            header.dex_file().GetSetRefItemItem(&annotation_set_ref_list->list_[i]);
        annotations_.push_back(new AnnotationSetItem(*annotation_set_item, header));
      }
    }

    MethodId* method_id() const { return method_id_; }
    std::vector<AnnotationSetItem*>* annotations() { return &annotations_; }

   private:
    MethodId* method_id_;
    std::vector<AnnotationSetItem*> annotations_;
    DISALLOW_COPY_AND_ASSIGN(ParameterAnnotation);
  };

  AnnotationsDirectoryItem(const DexFile::AnnotationsDirectoryItem* disk_annotations_item,
                           Header& header);

  AnnotationSetItem* class_annotation() const { return class_annotation_; }
  std::vector<FieldAnnotation*>* field_annotations() { return &field_annotations_; }
  std::vector<MethodAnnotation*>* method_annotations() { return &method_annotations_; }
  std::vector<ParameterAnnotation*>* parameter_annotations() { return &parameter_annotations_; }

  void accept(AbstractDispatcher* dispatch) { dispatch->dispatch(this); }

 private:
  AnnotationSetItem* class_annotation_;
  std::vector<FieldAnnotation*> field_annotations_;
  std::vector<MethodAnnotation*> method_annotations_;
  std::vector<ParameterAnnotation*> parameter_annotations_;
  DISALLOW_COPY_AND_ASSIGN(AnnotationsDirectoryItem);
};

// TODO(sehr): implement MapList.
class MapList : public Item {
 public:
  void accept(AbstractDispatcher* dispatch) { dispatch->dispatch(this); }

 private:
  DISALLOW_COPY_AND_ASSIGN(MapList);
};

class MapItem : public Item {
 public:
  void accept(AbstractDispatcher* dispatch) { dispatch->dispatch(this); }

 private:
  DISALLOW_COPY_AND_ASSIGN(MapItem);
};

}  // namespace dex_ir
}  // namespace art

#endif  // ART_DEXLAYOUT_DEX_IR_H_
