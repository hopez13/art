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

#ifndef ART_DEXLAYOUT_DEX_WRITER_H_
#define ART_DEXLAYOUT_DEX_WRITER_H_

#include "base/unix_file/fd_file.h"
#include "cdex/compact_dex_level.h"
#include "dex_ir.h"
#include "mem_map.h"
#include "os.h"

#include <queue>

namespace art {

struct MapItem {
  // Not using DexFile::MapItemType since compact dex and standard dex file may have differnet
  // sections.
  MapItem() = default;
  MapItem(uint32_t type, uint32_t size, uint32_t offset)
      : type_(type), size_(size), offset_(offset) { }

  bool operator<(const MapItem& other) const {
    return offset_ > other.offset_;
  }

  uint32_t type_ = 0u;
  uint32_t size_ = 0u;
  uint32_t offset_ = 0u;
};

class MapItemQueue : public std::priority_queue<MapItem> {
 public:
  void AddIfNotEmpty(const MapItem& item);
};

class DexWriter {
 public:
  DexWriter(dex_ir::Header* header, MemMap* mem_map) : header_(header), mem_map_(mem_map) {}

  static void Output(dex_ir::Header* header, MemMap* mem_map, CompactDexLevel compact_dex_level);

  virtual ~DexWriter() {}

 protected:
  void WriteMemMap();

  size_t Write(const void* buffer, size_t length, size_t offset) WARN_UNUSED;
  size_t WriteSleb128(uint32_t value, size_t offset) WARN_UNUSED;
  size_t WriteUleb128(uint32_t value, size_t offset) WARN_UNUSED;
  size_t WriteEncodedValue(dex_ir::EncodedValue* encoded_value, size_t offset) WARN_UNUSED;
  size_t WriteEncodedValueHeader(int8_t value_type, size_t value_arg, size_t offset) WARN_UNUSED;
  size_t WriteEncodedArray(dex_ir::EncodedValueVector* values, size_t offset) WARN_UNUSED;
  size_t WriteEncodedAnnotation(dex_ir::EncodedAnnotation* annotation, size_t offset) WARN_UNUSED;
  size_t WriteEncodedFields(dex_ir::FieldItemVector* fields, size_t offset) WARN_UNUSED;
  size_t WriteEncodedMethods(dex_ir::MethodItemVector* methods, size_t offset) WARN_UNUSED;

  // Header and id section
  virtual void WriteHeader();
  // reserve_only means don't write, only reserve space. This is required since the string data
  // offsets must be assigned.
  MapItem WriteStringIds(uint32_t* const offset, bool reserve_only);
  MapItem WriteTypeIds(uint32_t* const offset);
  MapItem WriteProtoIds(uint32_t* const offset, bool reserve_only);
  MapItem WriteFieldIds(uint32_t* const offset);
  MapItem WriteMethodIds(uint32_t* const offset);
  MapItem WriteClassDefs(uint32_t* const offset, bool reserve_only);

  MapItem WriteCallSiteIds(uint32_t* const offset);

  MapItem WriteEncodedArrays(uint32_t* const offset);
  MapItem WriteAnnotations(uint32_t* const offset);
  MapItem WriteAnnotationSets(uint32_t* const offset);
  MapItem WriteAnnotationSetRefs(uint32_t* const offset);
  MapItem WriteAnnotationsDirectories(uint32_t* const offset);

  // Data section.
  MapItem WriteDebugInfoItems(uint32_t* const offset);
  MapItem WriteCodeItems(uint32_t* const offset);
  MapItem WriteTypeLists(uint32_t* const offset);
  MapItem WriteStringDatas(uint32_t* const offset);
  MapItem WriteClassDatas(uint32_t* const offset);
  MapItem WriteMethodHandles(uint32_t* const offset);
  void WriteMapItems(uint32_t* const offset, MapItemQueue* queue);

  dex_ir::Header* const header_;
  MemMap* const mem_map_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DexWriter);
};

}  // namespace art

#endif  // ART_DEXLAYOUT_DEX_WRITER_H_
