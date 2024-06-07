/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef ART_LIBDEXFILE_DEX_COMPACT_DEX_FILE_H_
#define ART_LIBDEXFILE_DEX_COMPACT_DEX_FILE_H_

#include "base/macros.h"

namespace art {

// CompactDex was an ART internal dex file format that aimed to reduce storage/RAM usage.
// TODO(b/325430813): Remove this.
class CompactDexFile : public DexFile {
 public:
  struct CodeItem : public dex::CodeItem {
    static constexpr size_t kAlignment = sizeof(uint16_t);
    // Max preheader size in uint16_ts.
    static constexpr size_t kMaxPreHeaderSize = 6;

    static constexpr size_t FieldsOffset() {
      return OFFSETOF_MEMBER(CodeItem, fields_);
    }

    static constexpr size_t InsnsCountAndFlagsOffset() {
      return OFFSETOF_MEMBER(CodeItem, insns_count_and_flags_);
    }

    static constexpr size_t InsnsOffset() {
      return OFFSETOF_MEMBER(CodeItem, insns_);
    }

    static constexpr size_t kRegistersSizeShift = 12;
    static constexpr size_t kInsSizeShift = 8;
    static constexpr size_t kOutsSizeShift = 4;
    static constexpr size_t kTriesSizeSizeShift = 0;
    static constexpr uint16_t kBitPreHeaderRegistersSize = 0;
    static constexpr uint16_t kBitPreHeaderInsSize = 1;
    static constexpr uint16_t kBitPreHeaderOutsSize = 2;
    static constexpr uint16_t kBitPreHeaderTriesSize = 3;
    static constexpr uint16_t kBitPreHeaderInsnsSize = 4;
    static constexpr uint16_t kFlagPreHeaderRegistersSize = 0x1 << kBitPreHeaderRegistersSize;
    static constexpr uint16_t kFlagPreHeaderInsSize = 0x1 << kBitPreHeaderInsSize;
    static constexpr uint16_t kFlagPreHeaderOutsSize = 0x1 << kBitPreHeaderOutsSize;
    static constexpr uint16_t kFlagPreHeaderTriesSize = 0x1 << kBitPreHeaderTriesSize;
    static constexpr uint16_t kFlagPreHeaderInsnsSize = 0x1 << kBitPreHeaderInsnsSize;
    static constexpr size_t kInsnsSizeShift = 5;
    static constexpr size_t kInsnsSizeBits = sizeof(uint16_t) * kBitsPerByte -  kInsnsSizeShift;

   private:
    // clang doesn't count OFFSETOF_MEMBER as use.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-private-field"
    uint16_t fields_;
    uint16_t insns_count_and_flags_;
    uint16_t insns_[1];
#pragma clang diagnostic pop

    DISALLOW_COPY_AND_ASSIGN(CodeItem);
  };

 private:
  DISALLOW_COPY_AND_ASSIGN(CompactDexFile);
};

}  // namespace art

#endif  // ART_LIBDEXFILE_DEX_COMPACT_DEX_FILE_H_
