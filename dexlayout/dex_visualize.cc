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
 * Implementation file of the dex layout visualization.
 *
 * This is a tool to read dex files into an internal representation,
 * reorganize the representation, and emit dex files with a better
 * file layout.
 */

#include "dex_visualize.h"

#include <inttypes.h>
#include <stdio.h>

#include <memory>
#include <vector>

#include "dex_ir.h"
#include "dexlayout.h"
#include "jit/offline_profiling_info.h"

namespace art {

static FILE* out_file = nullptr;

class ColorTable {
 public:
  // Colors are based on the type of the section in MapList.
  explicit ColorTable(const dex_ir::Collections& collections) {
    table_.emplace_back(DexFile::kDexTypeHeaderItem, 0u);
    table_.emplace_back(DexFile::kDexTypeStringIdItem, collections.StringIdsOffset());
    table_.emplace_back(DexFile::kDexTypeTypeIdItem, collections.TypeIdsOffset());
    table_.emplace_back(DexFile::kDexTypeProtoIdItem, collections.ProtoIdsOffset());
    table_.emplace_back(DexFile::kDexTypeFieldIdItem, collections.FieldIdsOffset());
    table_.emplace_back(DexFile::kDexTypeMethodIdItem, collections.MethodIdsOffset());
    table_.emplace_back(DexFile::kDexTypeClassDefItem, collections.ClassDefsOffset());
    table_.emplace_back(DexFile::kDexTypeTypeList, collections.TypeListsOffset());
    table_.emplace_back(DexFile::kDexTypeAnnotationSetRefList,
                        collections.AnnotationSetRefListsOffset());
    table_.emplace_back(DexFile::kDexTypeAnnotationSetItem, collections.AnnotationSetOffset());
    table_.emplace_back(DexFile::kDexTypeClassDataItem, collections.ClassDatasOffset());
    table_.emplace_back(DexFile::kDexTypeCodeItem, collections.CodeItemsOffset());
    table_.emplace_back(DexFile::kDexTypeStringDataItem, collections.StringDatasOffset());
    table_.emplace_back(DexFile::kDexTypeDebugInfoItem, collections.DebugInfoOffset());
    table_.emplace_back(DexFile::kDexTypeAnnotationItem, collections.AnnotationOffset());
    table_.emplace_back(DexFile::kDexTypeEncodedArrayItem, collections.EncodedArrayOffset());
    table_.emplace_back(DexFile::kDexTypeAnnotationsDirectoryItem,
                        collections.AnnotationsDirectoryOffset());
    // Sort into descending order by offset.
    std::sort(table_.begin(),
              table_.end(),
              [](const SectionColor& a, const SectionColor& b) { return a.offset_ > b.offset_; });
  }

  int GetColor(uint32_t offset) const {
    // The dread linear search to find the right type bin for the reference.
    uint16_t type = 0;
    for (uint16_t i = 0; i < table_.size(); ++i) {
      if (table_[i].offset_ < offset) {
        type = table_[i].type_;
        break;
      }
    }
    // And a lookup table from type to color.
    using ColorMapType = std::map<uint16_t, int>;
    const ColorMapType kColorMap = {
      { DexFile::kDexTypeHeaderItem, 0 },
      { DexFile::kDexTypeStringIdItem, 0 },
      { DexFile::kDexTypeTypeIdItem, 0 },
      { DexFile::kDexTypeProtoIdItem, 0 },
      { DexFile::kDexTypeFieldIdItem, 0 },
      { DexFile::kDexTypeMethodIdItem, 0 },
      { DexFile::kDexTypeClassDefItem, 1 },
      { DexFile::kDexTypeTypeList, 0 },
      { DexFile::kDexTypeAnnotationSetRefList, 0 },
      { DexFile::kDexTypeAnnotationSetItem, 0 },
      { DexFile::kDexTypeClassDataItem, 3 },
      { DexFile::kDexTypeCodeItem, 4 },
      { DexFile::kDexTypeStringDataItem, 0 },
      { DexFile::kDexTypeDebugInfoItem, 0 },
      { DexFile::kDexTypeAnnotationItem, 0 },
      { DexFile::kDexTypeEncodedArrayItem, 0 },
      { DexFile::kDexTypeAnnotationsDirectoryItem, 0 }
    };
    ColorMapType::const_iterator iter = kColorMap.find(type);
    if (iter != kColorMap.end()) {
      return iter->second;
    }
    return 0;
  }

 private:
  struct SectionColor {
   public:
    SectionColor(uint16_t type, uint32_t offset) : type_(type), offset_(offset) { }
    uint16_t type_;
    uint32_t offset_;
  };

  std::vector<SectionColor> table_;
};

static void DumpAddressRange(uint32_t from, uint32_t size, int class_index, const ColorTable& ct) {
  const uint32_t size_delta = (size > 0) ? (size - 1) / kPageSize : 0;
  fprintf(out_file,
          "%d %d %d %d %d\n",
          from / kPageSize,
          class_index,
          size_delta,
          0,
          ct.GetColor(from));
}

static void DumpAddressRange(const dex_ir::Item* item, int class_index, const ColorTable& ct) {
  if (item != nullptr) {
    DumpAddressRange(item->GetOffset(), item->GetSize(), class_index, ct);
  }
}

/*
 * Dumps a gnuplot data file showing the parts of the dex_file that belong to each class.
 * If profiling information is present, it dumps only those classes that are marked as hot.
 */
static void DumpStringData(const dex_ir::StringData* string_data,
                           int class_index,
                           const ColorTable& ct) {
  DumpAddressRange(string_data, class_index, ct);
}

static void DumpStringId(const dex_ir::StringId* string_id,
                         int class_index,
                         const ColorTable& ct) {
  DumpAddressRange(string_id, class_index, ct);
  DumpStringData(string_id->DataItem(), class_index, ct);
}

static void DumpTypeId(const dex_ir::TypeId* type_id, int class_index, const ColorTable& ct) {
  DumpAddressRange(type_id, class_index, ct);
}

static void DumpFieldId(const dex_ir::FieldId* field_id, int class_index, const ColorTable& ct) {
  DumpAddressRange(field_id, class_index, ct);
  DumpTypeId(field_id->Class(), class_index, ct);
  DumpTypeId(field_id->Type(), class_index, ct);
  DumpStringId(field_id->Name(), class_index, ct);
}

static void DumpFieldItem(const dex_ir::FieldItem* field, int class_index, const ColorTable& ct) {
  DumpAddressRange(field, class_index, ct);
  DumpFieldId(field->GetFieldId(), class_index, ct);
}

static void DumpProtoId(const dex_ir::ProtoId* proto_id, int class_index, const ColorTable& ct) {
  DumpAddressRange(proto_id, class_index, ct);
}

static void DumpMethodId(const dex_ir::MethodId* method_id,
                         int class_index,
                         const ColorTable& ct) {
  DumpAddressRange(method_id, class_index, ct);
  DumpTypeId(method_id->Class(), class_index, ct);
  DumpProtoId(method_id->Proto(), class_index, ct);
  DumpStringId(method_id->Name(), class_index, ct);
}

static void DumpMethodItem(const dex_ir::MethodItem* method,
                           int class_index,
                           const ColorTable& ct) {
  DumpAddressRange(method, class_index, ct);
  DumpMethodId(method->GetMethodId(), class_index, ct);
  const dex_ir::CodeItem* code_item = method->GetCodeItem();
  if (code_item != nullptr) {
    DumpAddressRange(code_item, class_index, ct);
  }
}

void VisualizeDexLayout(dex_ir::Header* header,
                        const DexFile* dex_file,
                        size_t dex_file_index) {
  std::unique_ptr<ColorTable> ct(new ColorTable(header->GetCollections()));
  std::string dex_file_name("classes");
  std::string out_file_base_name("layout");
  if (dex_file_index > 0) {
    out_file_base_name += std::to_string(dex_file_index + 1);
    dex_file_name += std::to_string(dex_file_index + 1);
  }
  dex_file_name += ".dex";
  std::string out_file_name(out_file_base_name + ".gnuplot");
  std::string png_file_name(out_file_base_name + ".png");
  out_file = fopen(out_file_name.c_str(), "w");
  fprintf(out_file, "set terminal png\n");
  fprintf(out_file, "set output \"%s\"\n", png_file_name.c_str());
  fprintf(out_file, "set title \"%s\"\n", dex_file_name.c_str());
  fprintf(out_file, "set xlabel \"Page offset into dex\"\n");
  fprintf(out_file, "set ylabel \"ClassDef index\"\n");
  fprintf(out_file,
          "plot '-' using 1:2:3:4:5 with vector nohead linewidth 1 lc variable notitle\n");

  const uint32_t class_defs_size = header->GetCollections().ClassDefsSize();
  for (uint32_t class_index = 0; class_index < class_defs_size; class_index++) {
    dex_ir::ClassDef* class_def = header->GetCollections().GetClassDef(class_index);
    if (profile_info_ && !profile_info_->ContainsClass(*dex_file, class_index)) {
      continue;
    }
    DumpAddressRange(class_def, class_index, *ct);
    // Type id.
    DumpTypeId(class_def->ClassType(), class_index, *ct);
    // Superclass type id.
    DumpTypeId(class_def->Superclass(), class_index, *ct);
    // Interfaces.
    // TODO(jeffhao): get TypeList from class_def to use Item interface.
    static constexpr uint32_t kInterfaceSizeKludge = 8;
    DumpAddressRange(class_def->InterfacesOffset(), kInterfaceSizeKludge, class_index, *ct);
    // Source file info.
    DumpStringId(class_def->SourceFile(), class_index, *ct);
    // Annotations.
    DumpAddressRange(class_def->Annotations(), class_index, *ct);
    // TODO(sehr): walk the annotations and dump them.
    // Class data.
    dex_ir::ClassData* class_data = class_def->GetClassData();
    if (class_data != nullptr) {
      DumpAddressRange(class_data, class_index, *ct);
      if (class_data->StaticFields()) {
        for (auto& field_item : *class_data->StaticFields()) {
          DumpFieldItem(field_item.get(), class_index, *ct);
        }
      }
      if (class_data->InstanceFields()) {
        for (auto& field_item : *class_data->InstanceFields()) {
          DumpFieldItem(field_item.get(), class_index, *ct);
        }
      }
      if (class_data->DirectMethods()) {
        for (auto& method_item : *class_data->DirectMethods()) {
          DumpMethodItem(method_item.get(), class_index, *ct);
        }
      }
      if (class_data->VirtualMethods()) {
        for (auto& method_item : *class_data->VirtualMethods()) {
          DumpMethodItem(method_item.get(), class_index, *ct);
        }
      }
    }
  }  // for
  fclose(out_file);
}

}  // namespace art
