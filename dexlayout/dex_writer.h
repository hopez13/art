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

namespace art {

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
  uint32_t WriteStringIds(uint32_t offset) WARN_UNUSED;
  uint32_t WriteTypeIds(uint32_t offset) WARN_UNUSED;
  uint32_t WriteProtoIds(uint32_t offset) WARN_UNUSED;
  uint32_t WriteFieldIds(uint32_t offset) WARN_UNUSED;
  uint32_t WriteMethodIds(uint32_t offset) WARN_UNUSED;
  uint32_t WriteClassDefs(uint32_t offset) WARN_UNUSED;

  uint32_t WriteCallSiteIds(uint32_t offset) WARN_UNUSED;

  uint32_t WriteEncodedArrays(uint32_t offset) WARN_UNUSED;
  uint32_t WriteAnnotations(uint32_t offset) WARN_UNUSED;
  uint32_t WriteAnnotationSets(uint32_t offset) WARN_UNUSED;
  uint32_t WriteAnnotationSetRefs(uint32_t offset) WARN_UNUSED;
  uint32_t WriteAnnotationsDirectories(uint32_t offset) WARN_UNUSED;

  // Data section.
  uint32_t WriteDebugInfoItems(uint32_t offset) WARN_UNUSED;
  uint32_t WriteCodeItems(uint32_t offset) WARN_UNUSED;
  uint32_t WriteTypeLists(uint32_t offset) WARN_UNUSED;
  uint32_t WriteStringDatas(uint32_t offset) WARN_UNUSED;
  uint32_t WriteClassDatas(uint32_t offset) WARN_UNUSED;
  uint32_t WriteMethodHandles(uint32_t offset) WARN_UNUSED;
  uint32_t WriteMapItem(uint32_t offset) WARN_UNUSED;

  dex_ir::Header* const header_;
  MemMap* const mem_map_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DexWriter);
};

}  // namespace art

#endif  // ART_DEXLAYOUT_DEX_WRITER_H_
