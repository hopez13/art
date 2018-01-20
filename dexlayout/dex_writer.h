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

#include <functional>

#include "base/unix_file/fd_file.h"
#include "dex/compact_dex_level.h"
#include "dex/dex_file.h"
#include "dex_ir.h"
#include "mem_map.h"
#include "os.h"

#include <queue>

namespace art {

class DexLayout;
class DexLayoutHotnessInfo;

struct MapItem {
  // Not using DexFile::MapItemType since compact dex and standard dex file may have different
  // sections.
  MapItem() = default;
  MapItem(uint32_t type, uint32_t size, size_t offset)
      : type_(type), size_(size), offset_(offset) { }

  // Sort by decreasing order since the priority_queue puts largest elements first.
  bool operator>(const MapItem& other) const {
    return offset_ > other.offset_;
  }

  uint32_t type_ = 0u;
  uint32_t size_ = 0u;
  uint32_t offset_ = 0u;
};

class MapItemQueue : public
    std::priority_queue<MapItem, std::vector<MapItem>, std::greater<MapItem>> {
 public:
  void AddIfNotEmpty(const MapItem& item);
};

class DexWriter {
 public:
  static constexpr uint32_t kDataSectionAlignment = sizeof(uint32_t) * 2;
  static constexpr uint32_t kDexSectionWordAlignment = 4;

  class Output {
   public:
    std::vector<uint8_t> data_;
  };

  class Stream {
   public:
    // Functions are not virtual (yet) for speed.

    size_t Tell() const {
      return position_;
    }

    void Seek(size_t position) {
      position_ = position;
    }

    ALWAYS_INLINE size_t Write(const void* buffer, size_t length) {
      EnsureStorage(length);
      memcpy(&data_[position_], buffer, length);
      return length;
    }

    ALWAYS_INLINE size_t WriteSleb128(int32_t value) {
      EnsureStorage(8);
      uint8_t* ptr = &data_[position_];
      const size_t len = EncodeSignedLeb128(ptr, value) - ptr;
      position_ += len;
      return len;
    }

    ALWAYS_INLINE size_t WriteUleb128(uint32_t value) {
      EnsureStorage(8);
      uint8_t* ptr = &data_[position_];
      const size_t len = EncodeUnsignedLeb128(ptr, value) - ptr;
      position_ += len;
      return len;
    }

    ALWAYS_INLINE void AlignTo(const size_t alignment) {
      position_ = RoundUp(position_, alignment);
    }

    ALWAYS_INLINE void Skip(const size_t count) {
      position_ += count;
    }

   private:
    ALWAYS_INLINE void EnsureStorage(size_t length) {
      size_t end = position_ + length;
      while (UNLIKELY(end < data_.size())) {
        data_.resize(data_.size() * 3 / 2 + 1);
      }
    }
    size_t position_;
    std::vector<uint8_t> data_;
  };

  static inline constexpr uint32_t SectionAlignment(DexFile::MapItemType type) {
    switch (type) {
      case DexFile::kDexTypeClassDataItem:
      case DexFile::kDexTypeStringDataItem:
      case DexFile::kDexTypeDebugInfoItem:
      case DexFile::kDexTypeAnnotationItem:
      case DexFile::kDexTypeEncodedArrayItem:
        return alignof(uint8_t);

      default:
        // All other sections are kDexAlignedSection.
        return DexWriter::kDexSectionWordAlignment;
    }
  }

  DexWriter(DexLayout* dex_layout, bool compute_offsets);

  static void Output(DexLayout* dex_layout, bool compute_offsets);

  virtual ~DexWriter() {}

 protected:
  virtual void Write();

  size_t WriteEncodedValue(Stream* stream, dex_ir::EncodedValue* encoded_value);
  size_t WriteEncodedValueHeader(Stream* stream, int8_t value_type, size_t value_arg);
  size_t WriteEncodedArray(Stream* stream, dex_ir::EncodedValueVector* values);
  size_t WriteEncodedAnnotation(Stream* stream, dex_ir::EncodedAnnotation* annotation);
  size_t WriteEncodedFields(Stream* stream, dex_ir::FieldItemVector* fields);
  size_t WriteEncodedMethods(Stream* stream, dex_ir::MethodItemVector* methods);

  // Header and id section
  void WriteToStream(Stream* stream);
  virtual void WriteHeader(Stream* stream);
  virtual size_t GetHeaderSize() const;
  // reserve_only means don't write, only reserve space. This is required since the string data
  // offsets must be assigned.
  uint32_t WriteStringIds(Stream* stream, bool reserve_only);
  uint32_t WriteTypeIds(Stream* stream);
  uint32_t WriteProtoIds(Stream* stream, bool reserve_only);
  uint32_t WriteFieldIds(Stream* stream);
  uint32_t WriteMethodIds(Stream* stream);
  uint32_t WriteClassDefs(Stream* stream, bool reserve_only);
  uint32_t WriteCallSiteIds(Stream* stream, bool reserve_only);

  uint32_t WriteEncodedArrays(Stream* stream);
  uint32_t WriteAnnotations(Stream* stream);
  uint32_t WriteAnnotationSets(Stream* stream);
  uint32_t WriteAnnotationSetRefs(Stream* stream);
  uint32_t WriteAnnotationsDirectories(Stream* stream);

  // Data section.
  uint32_t WriteDebugInfoItems(Stream* stream);
  uint32_t WriteCodeItems(Stream* stream, bool reserve_only);
  uint32_t WriteTypeLists(Stream* stream);
  uint32_t WriteStringDatas(Stream* stream);
  uint32_t WriteClassDatas(Stream* stream);
  uint32_t WriteMethodHandles(Stream* stream);
  uint32_t WriteMapItems(Stream* stream, MapItemQueue* queue);
  uint32_t GenerateAndWriteMapItems(Stream* stream);

  virtual uint32_t WriteCodeItemPostInstructionData(Stream* stream,
                                                    dex_ir::CodeItem* item,
                                                    bool reserve_only);
  virtual uint32_t WriteCodeItem(Stream* stream, dex_ir::CodeItem* item, bool reserve_only);

  // Process an offset, if compute_offset is set, write into the dex ir item, otherwise read the
  // existing offset and use that for writing.
  void ProcessOffset(Stream* stream, dex_ir::Item* item);

  dex_ir::Header* const header_;
  std::vector<uint8_t> out_data_;
  DexLayout* const dex_layout_;
  bool compute_offsets_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DexWriter);
};

}  // namespace art

#endif  // ART_DEXLAYOUT_DEX_WRITER_H_
