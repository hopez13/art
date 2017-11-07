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

#ifndef ART_RUNTIME_DEX_HELPERS_H_
#define ART_RUNTIME_DEX_HELPERS_H_

#include "cdex/compact_dex_file.h"
#include "dex_file.h"
#include "dex_instruction_iterator.h"
#include "standard_dex_file.h"

namespace art {

// Dex helpers have ART specific APIs, we may want to refactor these for use in dexdump.

class ArtMethod;

// Copies the instruction information from a CompactDex / StandardDexFile code item.
// Doesn't copy the debug info since this will be factored into a different helper.
// Does not handle null code items.
class CodeItemInstructions {
 public:
  ALWAYS_INLINE CodeItemInstructions(const DexFile* dex_file, const DexFile::CodeItem* code_item);

  ALWAYS_INLINE explicit CodeItemInstructions(ArtMethod* method);

  ALWAYS_INLINE IterationRange<DexInstructionIterator> Instructions() const;

  ALWAYS_INLINE uint32_t InsnsSizeInCodeUnits() const {
    return insns_size_in_code_units_;
  }

  ALWAYS_INLINE const uint16_t* Insns() const {
    return insns_;
  }

 protected:
  CodeItemInstructions() = default;

  ALWAYS_INLINE void Init(const CompactDexFile::CodeItem& code_item);

  ALWAYS_INLINE void Init(const StandardDexFile::CodeItem& code_item);

  ALWAYS_INLINE void Init(const DexFile* dex_file, const DexFile::CodeItem* code_item);

 private:
  // size of the insns array, in 2 byte code units. 0 if there is no code item.
  uint32_t insns_size_in_code_units_ = 0;

  // Pointer to the instructions, null if there is no code item.
  const uint16_t* insns_ = 0;
};

// Handles null code items by storing null in the instruction pointer. Returns an empty set of
// iterators for null code items.
class NullableCodeItemInstructions : public CodeItemInstructions {
 public:
  bool HasCodeItem() const {
    return Insns() != nullptr;
  }

  ALWAYS_INLINE explicit NullableCodeItemInstructions(ArtMethod* method);
};

// Code item data excluding the debug info offset.
// Does not handle null code items.
class CodeItemData : public CodeItemInstructions {
 public:
  ALWAYS_INLINE CodeItemData(const DexFile* dex_file, const DexFile::CodeItem* code_item);

  ALWAYS_INLINE explicit CodeItemData(ArtMethod* method);

  ALWAYS_INLINE uint16_t RegistersSize() const {
    return registers_size_;
  }

  ALWAYS_INLINE uint16_t InsSize() const {
    return ins_size_;
  }

  ALWAYS_INLINE uint16_t OutsSize() const {
    return outs_size_;
  }

  ALWAYS_INLINE uint16_t TriesSize() const {
    return tries_size_;
  }

 protected:
  CodeItemData() = default;

  ALWAYS_INLINE void Init(const CompactDexFile::CodeItem& code_item);

  ALWAYS_INLINE void Init(const StandardDexFile::CodeItem& code_item);

  ALWAYS_INLINE void Init(const DexFile* dex_file, const DexFile::CodeItem* code_item);

 private:
  // Fields mirrored from the dex/cdex code item.
  uint16_t registers_size_;
  uint16_t ins_size_;
  uint16_t outs_size_;
  uint16_t tries_size_;
};

// Handles null code items by storing null in the instruction pointer.
class NullableCodeItemData : public CodeItemData {
 public:
  bool HasCodeItem() const {
    return Insns() != nullptr;
  }

  ALWAYS_INLINE explicit NullableCodeItemData(ArtMethod* method);
};

}  // namespace art

#endif  // ART_RUNTIME_DEX_HELPERS_H_
