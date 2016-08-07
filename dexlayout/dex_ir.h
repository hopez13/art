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

#ifndef DEX_IR_INCLUDED
#define DEX_IR_INCLUDED

#include <stdint.h>
#include <map>
#include <vector>
#include "dex_file.h"

namespace art {
namespace dex_ir {

// Forward declarations for classes used in containers or pointed to.
class StringDataItem;
class TypeIdItem;
class ProtoIdItem;
class FieldIdItem;
class MethodIdItem;
class ClassDefItem;
class ClassDataItem;
class FieldItem;
class MethodItem;
class ArrayItem;
class CodeItem;
class TryItem;
class DebugInfoItem;
class AnnotationsDirectoryItem;
class FieldAnnotation;
class MethodAnnotation;
class ParameterAnnotation;
class AnnotationSetRefItem;
class AnnotationSetItem;
class AnnotationItem;
class MapList;
class MapItem;

class Builder {
 private:
  template<class T>
  class Map {
   public:
    Map() { }
    T Lookup(uint32_t index) {
      if (map_.find(index) == map_.end()) {
        return nullptr;
      } else {
        return map_[index];
      }
    }
    void Set(uint32_t index, T value) {
      map_[index] = value;
    }

   private:
    DISALLOW_COPY_AND_ASSIGN(Map);

    std::map<uint32_t, T> map_;
  };

 public:
  Builder(const DexFile& dex_file) : dex_file_(dex_file) { }

  const DexFile& dex_file_;
  Map<const StringDataItem*> strings_;
  Map<const TypeIdItem*> types_;
  Map<const ProtoIdItem*> protos_;
  Map<const FieldIdItem*> fields_;
  Map<const MethodIdItem*> methods_;
  Map<const ClassDefItem*> classes_;

 private:
  DISALLOW_COPY_AND_ASSIGN(Builder);
};

class Item {
 public:
  virtual ~Item() { }
 protected:
  uint32_t offset_;
};

class Header : public Item {
 public:
  Header(const DexFile::Header& disk_header) {
    memcpy(magic_, disk_header.magic_, sizeof(magic_));
    checksum_ = disk_header.checksum_;
    // TODO(sehr): clearly the signature will need to be recomputed before dumping.
    memcpy(signature_, disk_header.signature_, sizeof(signature_));
    endian_tag_ = disk_header.endian_tag_;
  }
  ~Header() OVERRIDE { }

  const uint8_t* magic() const { return magic_; }
  uint32_t checksum() const { return checksum_; }
  const uint8_t* signature() const { return signature_; }
  uint32_t endian_tag() const { return endian_tag_; }
  std::vector<const StringDataItem*>* strings() { return &strings_; }
  std::vector<const TypeIdItem*>* types() { return &types_; }
  std::vector<const ProtoIdItem*>* protos() { return &protos_; }
  std::vector<const FieldIdItem*>* fields() { return &fields_; }
  std::vector<const MethodIdItem*>* methods() { return &methods_; }
  std::vector<const ClassDefItem*>* classes() { return &classes_; }

 private:
  DISALLOW_COPY_AND_ASSIGN(Header);

  uint8_t magic_[8];
  uint32_t checksum_;
  uint8_t signature_[DexFile::kSha1DigestSize];
  uint32_t endian_tag_;

  std::vector<const StringDataItem*> strings_;
  std::vector<const TypeIdItem*> types_;
  std::vector<const ProtoIdItem*> protos_;
  std::vector<const FieldIdItem*> fields_;
  std::vector<const MethodIdItem*> methods_;
  std::vector<const ClassDefItem*> classes_;
};

class StringDataItem : public Item {
 public:
  static const StringDataItem* Lookup(uint32_t index, Builder& builder) {
    return builder.strings_.Lookup(index);
  }
  static const StringDataItem* Lookup(const DexFile::StringId& string_id, Builder& builder) {
    uint32_t index = builder.dex_file_.GetIndexForStringId(string_id);
    const StringDataItem* item = builder.strings_.Lookup(index);
    if (item == nullptr) {
      item = new StringDataItem(builder.dex_file_.GetStringData(string_id));
      builder.strings_.Set(index, item);
    }
    return item;
  }
  ~StringDataItem() OVERRIDE { }

  const char* data() const { return data_; }

 private:
  DISALLOW_COPY_AND_ASSIGN(StringDataItem);

  StringDataItem(const char* data) : data_(strdup(data)) { }

  const char* data_;
};

class TypeIdItem : public Item {
 public:
  TypeIdItem(const DexFile::TypeId& disk_type, Builder& builder) :
    string_(builder.strings_.Lookup(disk_type.descriptor_idx_)) { }
  ~TypeIdItem() OVERRIDE { }

  const StringDataItem* string() const { return string_; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TypeIdItem);

  const StringDataItem* string_;
};

class ProtoIdItem : public Item {
 public:
  ProtoIdItem(const DexFile::ProtoId& disk_proto, Builder& builder) {
    shorty_ = builder.strings_.Lookup(disk_proto.shorty_idx_);
    return_type_ = builder.types_.Lookup(disk_proto.return_type_idx_);
    DexFileParameterIterator dfpi(builder.dex_file_, disk_proto);
    while (dfpi.HasNext()) {
      parameters_.push_back(builder.types_.Lookup(dfpi.GetTypeIdx()));
    }
  }
  ~ProtoIdItem() OVERRIDE { }

  const StringDataItem* shorty() const { return shorty_; }
  const TypeIdItem* return_type() const { return return_type_; }
  std::vector<const TypeIdItem*>* parameters() { return &parameters_; }

 private:
  DISALLOW_COPY_AND_ASSIGN(ProtoIdItem);

  const StringDataItem* shorty_;
  const TypeIdItem* return_type_;
  std::vector<const TypeIdItem*> parameters_;
};

class FieldIdItem : public Item {
 public:
  FieldIdItem(const DexFile::FieldId& disk_field, Builder& builder) {
    class_def_ = nullptr;
    type_ = builder.types_.Lookup(disk_field.type_idx_);
    name_ = builder.strings_.Lookup(disk_field.name_idx_);
  }
  ~FieldIdItem() OVERRIDE { }

  void setClass(const ClassDefItem* class_def) { class_def_ = class_def; }

  const ClassDefItem* class_def() const { return class_def_; }
  const TypeIdItem* type() const { return type_; }
  const StringDataItem* name() const { return name_; }

 private:
  DISALLOW_COPY_AND_ASSIGN(FieldIdItem);

  const ClassDefItem* class_def_;
  const TypeIdItem* type_;
  const StringDataItem* name_;
};

class MethodIdItem : public Item {
 public:
  MethodIdItem(const DexFile::MethodId& disk_method, Builder& builder) {
    class_def_ = nullptr;
    proto_ = builder.protos_.Lookup(disk_method.proto_idx_);
    name_ = builder.strings_.Lookup(disk_method.name_idx_);
  }
  ~MethodIdItem() OVERRIDE { }

  void setClass(const ClassDefItem* class_def) { class_def_ = class_def; }

  const ClassDefItem* class_def() const { return class_def_; }
  const ProtoIdItem* proto() const { return proto_; }
  const StringDataItem* name() const { return name_; }

 private:
  DISALLOW_COPY_AND_ASSIGN(MethodIdItem);
  const ClassDefItem* class_def_;
  const ProtoIdItem* proto_;
  const StringDataItem* name_;
};

class FieldItem : public Item {
 public:
  FieldItem(uint32_t access_flags, const FieldIdItem* field) :
    access_flags_(access_flags), field_(field) { }
  ~FieldItem() OVERRIDE { }

  uint32_t access_flags() const { return access_flags_; }
  const FieldIdItem* field() const { return field_; }

 private:
  DISALLOW_COPY_AND_ASSIGN(FieldItem);
  uint32_t access_flags_;
  const FieldIdItem* field_;
};

class MethodItem : public Item {
 public:
  MethodItem(uint32_t access_flags, const MethodIdItem* method, const CodeItem* code) :
    access_flags_(access_flags), method_(method), code_(code) { }
  ~MethodItem() OVERRIDE { }

  uint32_t access_flags() const { return access_flags_; }
  const MethodIdItem* method() const { return method_; }
  const CodeItem* code() const { return code_; }

 private:
  DISALLOW_COPY_AND_ASSIGN(MethodItem);
  uint32_t access_flags_;
  const MethodIdItem* method_;
  const CodeItem* code_;
};

class ArrayItem : public Item {
 public:
  class NameValuePair {
   public:
    NameValuePair(const StringDataItem* name, const ArrayItem* value) :
      name_(name), value_(value) { }

    const StringDataItem* name() const { return name_; }
    const ArrayItem* value() const { return value_; }

   private:
    DISALLOW_COPY_AND_ASSIGN(NameValuePair);
    const StringDataItem* name_;
    const ArrayItem* value_;
  };

  bool bool_val() const { return item_.bool_val_; }
  int8_t byte_val() const { return item_.byte_val_; }
  int16_t short_val() const { return item_.short_val_; }
  uint16_t char_val() const { return item_.char_val_; }
  int32_t int_val() const { return item_.int_val_; }
  int64_t long_val() const { return item_.long_val_; }
  float float_val() const { return item_.float_val_; }
  double double_val() const { return item_.double_val_; }
  const StringDataItem* string_val() const { return item_.string_val_; }
  const FieldIdItem* field_val() const { return item_.field_val_; }
  const MethodIdItem* method_val() const { return item_.method_val_; }
  std::vector<const ArrayItem*>* annotation_array_val() const {
    return item_.annotation_array_val_;
  }
  const StringDataItem* annotation_annotation_string() const {
    return item_.annotation_annotation_val_.string_;
  }
  const std::vector<const NameValuePair*>* annotation_annotation_nvp_array() const {
    return item_.annotation_annotation_val_.array_;
  }

  ArrayItem(Builder& builder, const uint8_t** data);
  ~ArrayItem() OVERRIDE { }

 private:
  DISALLOW_COPY_AND_ASSIGN(ArrayItem);

  ArrayItem(uint8_t type, const uint8_t** payload);

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
    const StringDataItem* string_val_;
    const FieldIdItem* field_val_;
    const MethodIdItem* method_val_;
    std::vector<const ArrayItem*>* annotation_array_val_;
    struct {
      const StringDataItem* string_;
      std::vector<const NameValuePair*>* array_;
    } annotation_annotation_val_;
  } item_;
};

class ClassDefItem : public Item {
 public:
  ClassDefItem(const DexFile::ClassDef& disk_class_def, Builder& builder);
  ~ClassDefItem() OVERRIDE { }

  const TypeIdItem* class_type() const { return class_type_; }
  uint32_t access_flags() const { return access_flags_; }
  const TypeIdItem* superclass() const { return superclass_; }
  const std::vector<const TypeIdItem*>* interfaces() const { return &interfaces_; }
  const StringDataItem* source_file() const { return source_file_; }
  const AnnotationsDirectoryItem* annotations() const { return annotations_; }
  const std::vector<const ArrayItem*>* static_values() const { return static_values_; }
  const std::vector<const FieldItem*>* static_fields() const {
    return &class_data_.static_fields_;
  }
  const std::vector<const FieldItem*>* instance_fields() const {
    return &class_data_.instance_fields_;
  }
  const std::vector<const MethodItem*>* direct_methods() const {
    return &class_data_.direct_methods_;
  }
  const std::vector<const MethodItem*>* virtual_methods() const {
    return &class_data_.virtual_methods_;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ClassDefItem);

  const TypeIdItem* class_type_;
  uint32_t access_flags_;
  const TypeIdItem* superclass_;
  std::vector<const TypeIdItem*> interfaces_;
  const StringDataItem* source_file_;
  const AnnotationsDirectoryItem* annotations_;
  std::vector<const ArrayItem*>* static_values_;
  struct ClassDataItem : public Item {
    std::vector<const FieldItem*> static_fields_;
    std::vector<const FieldItem*> instance_fields_;
    std::vector<const MethodItem*> direct_methods_;
    std::vector<const MethodItem*> virtual_methods_;
  } class_data_;
};

class CodeItem : public Item {
 public:
  CodeItem(const DexFile::CodeItem& disk_code_item, Builder& builder);
  ~CodeItem() OVERRIDE { }

  uint16_t registers_size() const { return registers_size_; }
  uint16_t ins_size() const { return ins_size_; }
  uint16_t outs_size() const { return outs_size_; }
  uint16_t tries_size() const { return tries_size_; }
  std::vector<const DebugInfoItem*>* debug_info() const { return debug_info_; }
  uint32_t insns_size() const { return insns_size_; }
  uint16_t* insns() const { return insns_; }
  std::vector<const TryItem*>* tries() const { return tries_; }

 private:
  DISALLOW_COPY_AND_ASSIGN(CodeItem);

  uint16_t registers_size_;
  uint16_t ins_size_;
  uint16_t outs_size_;
  uint16_t tries_size_;
  std::vector<const DebugInfoItem*>* debug_info_;
  uint32_t insns_size_;
  uint16_t* insns_;
  std::vector<const TryItem*>* tries_;
};

class TryItem : public Item {
 public:
  class CatchHandler {
   public:
    CatchHandler(const TypeIdItem* type, uint32_t address) : type_(type), address_(address) { }

    const TypeIdItem* type() const { return type_; }
    uint32_t address() const { return address_; }

   private:
    DISALLOW_COPY_AND_ASSIGN(CatchHandler);

    const TypeIdItem* type_;
    uint32_t address_;
  };

  TryItem(const DexFile::TryItem& disk_try_item, const DexFile::CodeItem& disk_code_item,
          Builder& builder) {
    start_addr_ = disk_try_item.start_addr_;
    insn_count_ = disk_try_item.insn_count_;
    for (CatchHandlerIterator it(disk_code_item, disk_try_item); it.HasNext(); it.Next()) {
      const uint16_t type_index = it.GetHandlerTypeIndex();
      handlers_.push_back(new CatchHandler(builder.types_.Lookup(type_index),
                                           it.GetHandlerAddress()));
    }
  }
  ~TryItem() OVERRIDE { }

  uint32_t start_addr() const { return start_addr_; }
  uint16_t insn_count() const { return insn_count_; }
  std::vector<const CatchHandler*>* handlers() { return &handlers_; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TryItem);

  uint32_t start_addr_;
  uint16_t insn_count_;
  std::vector<const CatchHandler*> handlers_;
};

// TODO(sehr): implement debug information.
class DebugInfoItem : public Item {
 public:
 private:
  DISALLOW_COPY_AND_ASSIGN(DebugInfoItem);
};

// TODO(sehr): we're not done here yet...
class AnnotationSetItem : public Item {
 public:
  class AnnotationItem {
   public:
   private:
    DISALLOW_COPY_AND_ASSIGN(AnnotationItem);
  };

  AnnotationSetItem(const DexFile::AnnotationSetItem& disk_annotations_item, Builder& builder);

 private:
  DISALLOW_COPY_AND_ASSIGN(AnnotationSetItem);
};

class AnnotationsDirectoryItem : public Item {
 public:
  class FieldAnnotation {
   public:
    FieldAnnotation(const FieldIdItem* field_id, const AnnotationSetItem* annotation_set_item) :
      field_id_(field_id), annotation_set_item_(annotation_set_item) { }

    const FieldIdItem* field_id() const { return field_id_; }
    const AnnotationSetItem* annotation_set_item() const { return annotation_set_item_; }

   private:
    DISALLOW_COPY_AND_ASSIGN(FieldAnnotation);
    const FieldIdItem* field_id_;
    const AnnotationSetItem* annotation_set_item_;
  };

  class MethodAnnotation {
   public:
    MethodAnnotation(const MethodIdItem* method_id, const AnnotationSetItem* annotation_set_item) :
      method_id_(method_id), annotation_set_item_(annotation_set_item) { }

    const MethodIdItem* method_id() const { return method_id_; }
    const AnnotationSetItem* annotation_set_item() const { return annotation_set_item_; }

   private:
    DISALLOW_COPY_AND_ASSIGN(MethodAnnotation);
    const MethodIdItem* method_id_;
    const AnnotationSetItem* annotation_set_item_;
  };

  class ParameterAnnotation {
   public:
    ParameterAnnotation(const MethodIdItem* method_id,
                        const DexFile::AnnotationSetRefList* annotation_set_ref_list,
                        Builder& builder) :
      method_id_(method_id) {
      for (uint32_t i = 0; i < annotation_set_ref_list->size_; ++i) {
        const DexFile::AnnotationSetItem* annotation_set_item =
            builder.dex_file_.GetSetRefItemItem(&annotation_set_ref_list->list_[i]);
        annotations_.push_back(new AnnotationSetItem(*annotation_set_item, builder));
      }
    }

    const MethodIdItem* method_id() const { return method_id_; }
    const std::vector<const AnnotationSetItem*>* annotations() const { return &annotations_; }

   private:
    DISALLOW_COPY_AND_ASSIGN(ParameterAnnotation);
    const MethodIdItem* method_id_;
    std::vector<const AnnotationSetItem*> annotations_;
  };

  AnnotationsDirectoryItem(const DexFile::AnnotationsDirectoryItem* disk_annotations_item,
                           Builder& builder);

  const AnnotationSetItem* class_annotation() const { return class_annotation_; }
  const std::vector<const FieldAnnotation*>* field_annotations() const {
    return &field_annotations_;
  }
  const std::vector<const MethodAnnotation*>* method_annotations() const {
    return &method_annotations_;
  }
  const std::vector<const ParameterAnnotation*>* parameter_annotations() const {
    return &parameter_annotations_;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AnnotationsDirectoryItem);

  const AnnotationSetItem* class_annotation_;
  std::vector<const FieldAnnotation*> field_annotations_;
  std::vector<const MethodAnnotation*> method_annotations_;
  std::vector<const ParameterAnnotation*> parameter_annotations_;
};

// TODO(sehr): implement MapList.
class MapList : public Item {
 public:
 private:
  DISALLOW_COPY_AND_ASSIGN(MapList);
};

class MapItem : public Item {
 public:
 private:
  DISALLOW_COPY_AND_ASSIGN(MapItem);
};

}  // namespace dex_ir
}  // namespace art

#endif  // DEX_IR_INCLUDED
