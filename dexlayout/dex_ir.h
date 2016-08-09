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
class EncodedFieldItem;
class EncodedMethodItem;
class CodeItem;

template<class T>
class BuilderMap {
 public:
  BuilderMap() { }

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
  DISALLOW_COPY_AND_ASSIGN(BuilderMap);

  std::map<uint32_t, T> map_;
};

class Builder {
 public:
  Builder(const DexFile& dex_file) : dex_file_(dex_file) { }

  const DexFile& dex_file_;
  BuilderMap<const StringDataItem*> strings_;
  BuilderMap<const TypeIdItem*> types_;
  BuilderMap<const ProtoIdItem*> protos_;
  BuilderMap<const FieldIdItem*> fields_;
  BuilderMap<const MethodIdItem*> methods_;
  BuilderMap<const ClassDefItem*> classes_;

 private:
  DISALLOW_COPY_AND_ASSIGN(Builder);
};

class Item {
 public:
  void Write(void** addr);
 private:
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

 private:
  DISALLOW_COPY_AND_ASSIGN(StringDataItem);

  StringDataItem(const char* data) : data_(strdup(data)) { }

  const char* data_;
};

class TypeIdItem : public Item {
 public:
  TypeIdItem(const DexFile::TypeId& disk_type, Builder& builder) :
    string_(builder.strings_.Lookup(disk_type.descriptor_idx_)) { }

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

 private:
  DISALLOW_COPY_AND_ASSIGN(ProtoIdItem);

  const StringDataItem* shorty_;
  const TypeIdItem* return_type_;
  std::vector<const TypeIdItem*> parameters_;
};

class FieldIdItem : public Item {
 public:
  FieldIdItem(const DexFile::FieldId& disk_field, Builder& builder) {
    class_ = nullptr;
    type_ = builder.types_.Lookup(disk_field.type_idx_);
    name_ = builder.strings_.Lookup(disk_field.name_idx_);
  }
  void setClass(const ClassDefItem* class_def) { class_ = class_def; }

 private:
  DISALLOW_COPY_AND_ASSIGN(FieldIdItem);

  const ClassDefItem* class_;
  const TypeIdItem* type_;
  const StringDataItem* name_;
};

class MethodIdItem : public Item {
 public:
  MethodIdItem(const DexFile::MethodId& disk_method, Builder& builder) {
    class_ = nullptr;
    proto_ = builder.protos_.Lookup(disk_method.proto_idx_);
    name_ = builder.strings_.Lookup(disk_method.name_idx_);
  }

  void setClass(const ClassDefItem* class_def) { class_ = class_def; }

 private:
  const ClassDefItem* class_;
  const ProtoIdItem* proto_;
  const StringDataItem* name_;
};

class EncodedFieldItem : public Item {
 public:
  EncodedFieldItem(uint32_t access_flags, const FieldIdItem* field) :
    access_flags_(access_flags), field_(field) { }
  EncodedFieldItem(EncodedFieldItem&& other) :
    access_flags_(std::move(other.access_flags_)), field_(std::move(other.field_)) { }

 private:
  DISALLOW_COPY_AND_ASSIGN(EncodedFieldItem);
  uint32_t access_flags_;
  const FieldIdItem* field_;
};

class EncodedMethodItem : public Item {
 public:
  EncodedMethodItem(uint32_t access_flags, const MethodIdItem* method, const CodeItem* code) :
    access_flags_(access_flags), method_(method), code_(code) { }
  EncodedMethodItem(EncodedMethodItem&& other) :
    access_flags_(std::move(other.access_flags_)), method_(std::move(other.method_)),
    code_(std::move(other.code_)) { }

 private:
  DISALLOW_COPY_AND_ASSIGN(EncodedMethodItem);
  uint32_t access_flags_;
  const MethodIdItem* method_;
  const CodeItem* code_;
};

class ClassDefItem : public Item {
 public:
  ClassDefItem(const DexFile::ClassDef& disk_class_def, Builder& builder);

 private:
  DISALLOW_COPY_AND_ASSIGN(ClassDefItem);

  const TypeIdItem* class_;
  uint32_t access_flags_;
  const TypeIdItem* superclass_;
  std::vector<const TypeIdItem*> interfaces_;
  const StringDataItem* source_file_;
  // const AnnotationsDirectoryItem* annotations_
  struct ClassDataItem {
    std::vector<EncodedFieldItem> static_fields_;
    std::vector<EncodedFieldItem> instance_fields_;
    std::vector<EncodedMethodItem> direct_methods_;
    std::vector<EncodedMethodItem> virtual_methods_;
  } class_data_;
  // std::vector<const EncodedArrayItem*> static_values_;
};

class CodeItem : public Item {
 public:
 private:
  DISALLOW_COPY_AND_ASSIGN(CodeItem);
};

class TryItem : public Item {
 public:
 private:
  DISALLOW_COPY_AND_ASSIGN(TryItem);
};

class EncodedCatchHandler : public Item {
 public:
 private:
  DISALLOW_COPY_AND_ASSIGN(EncodedCatchHandler);
};

class EncodedTypeAddrPair : public Item {
 public:
 private:
  DISALLOW_COPY_AND_ASSIGN(EncodedTypeAddrPair);
};

class DebugInfoItem : public Item {
 public:
 private:
  DISALLOW_COPY_AND_ASSIGN(DebugInfoItem);
};

class AnnotationsDirectoryItem : public Item {
 public:
 private:
  DISALLOW_COPY_AND_ASSIGN(AnnotationsDirectoryItem);
};

class FieldAnnotation : public Item {
 public:
 private:
  DISALLOW_COPY_AND_ASSIGN(FieldAnnotation);
};

class MethodAnnotation : public Item {
 public:
 private:
  DISALLOW_COPY_AND_ASSIGN(MethodAnnotation);
};

class ParameterAnnotation : public Item {
 public:
 private:
  DISALLOW_COPY_AND_ASSIGN(ParameterAnnotation);
};

class AnnotationSetRefItem : public Item {
 public:
 private:
  DISALLOW_COPY_AND_ASSIGN(AnnotationSetRefItem);
};

class AnnotationSetItem : public Item {
 public:
 private:
  DISALLOW_COPY_AND_ASSIGN(AnnotationSetItem);
};

class AnnotationItem : public Item {
 public:
 private:
  DISALLOW_COPY_AND_ASSIGN(AnnotationItem);
};

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
