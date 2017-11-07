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

#ifndef ART_RUNTIME_DEX_HELPERS_INL_H_
#define ART_RUNTIME_DEX_HELPERS_INL_H_

#include "dex_helpers.h"
#include "art_method-inl.h"
#include "cdex/compact_dex_file.h"
#include "standard_dex_file.h"

namespace art {

inline void CodeItemInstructions::Init(const CompactDexFile::CodeItem& code_item) {
  insns_size_in_code_units_ = code_item.insns_size_in_code_units_;
  insns_ = code_item.insns_;
}

inline void CodeItemInstructions::Init(const StandardDexFile::CodeItem& code_item) {
  insns_size_in_code_units_ = code_item.insns_size_in_code_units_;
  insns_ = code_item.insns_;
}

inline void CodeItemInstructions::Init(const DexFile* dex_file,
                                       const DexFile::CodeItem* code_item) {
  DCHECK(dex_file != nullptr);
  DCHECK(code_item != nullptr);
  if (dex_file->IsCompactDexFile()) {
    Init(down_cast<const CompactDexFile::CodeItem&>(*code_item));
  } else {
    DCHECK(dex_file->IsStandardDexFile());
    Init(down_cast<const StandardDexFile::CodeItem&>(*code_item));
  }
}

inline CodeItemInstructions::CodeItemInstructions(const DexFile* dex_file,
                                                  const DexFile::CodeItem* code_item) {
  Init(dex_file, code_item);
}

inline CodeItemInstructions::CodeItemInstructions(ArtMethod* method)
    : CodeItemInstructions(method->GetDexFile(), method->GetCodeItem()) {}

inline IterationRange<DexInstructionIterator> CodeItemInstructions::Instructions() const {
  return { DexInstructionIterator(insns_, 0u),
           DexInstructionIterator(insns_, insns_size_in_code_units_) };
}

inline NullableCodeItemInstructions::NullableCodeItemInstructions(ArtMethod* method) {
  DCHECK(method != nullptr);
  const DexFile::CodeItem* code_item = method->GetCodeItem();
  if (code_item != nullptr) {
    Init(method->GetDexFile(), code_item);
  } else {
    DCHECK(!HasCodeItem()) << "Should be null initialized";
  }
}

inline void CodeItemData::Init(const CompactDexFile::CodeItem& code_item) {
  CodeItemInstructions::Init(code_item);
  registers_size_ = code_item.registers_size_;
  ins_size_ = code_item.ins_size_;
  outs_size_ = code_item.outs_size_;
  tries_size_ = code_item.tries_size_;
}

inline void CodeItemData::Init(const StandardDexFile::CodeItem& code_item) {
  CodeItemInstructions::Init(code_item);
  registers_size_ = code_item.registers_size_;
  ins_size_ = code_item.ins_size_;
  outs_size_ = code_item.outs_size_;
  tries_size_ = code_item.tries_size_;
}

inline void CodeItemData::Init(const DexFile* dex_file, const DexFile::CodeItem* code_item) {
  DCHECK(dex_file != nullptr);
  DCHECK(code_item != nullptr);
  if (dex_file->IsCompactDexFile()) {
    CodeItemData::Init(down_cast<const CompactDexFile::CodeItem&>(*code_item));
  } else {
    DCHECK(dex_file->IsStandardDexFile());
    CodeItemData::Init(down_cast<const StandardDexFile::CodeItem&>(*code_item));
  }
}

inline CodeItemData::CodeItemData(const DexFile* dex_file, const DexFile::CodeItem* code_item) {
  Init(dex_file, code_item);
}

inline CodeItemData::CodeItemData(ArtMethod* method)
    : CodeItemData(method->GetDexFile(), method->GetCodeItem()) {}

inline NullableCodeItemData::NullableCodeItemData(ArtMethod* method) {
  DCHECK(method != nullptr);
  const DexFile::CodeItem* code_item = method->GetCodeItem();
  if (code_item != nullptr) {
    Init(method->GetDexFile(), code_item);
  } else {
    DCHECK(!HasCodeItem()) << "Should be null initialized";
  }
}

}  // namespace art

#endif  // ART_RUNTIME_DEX_HELPERS_INL_H_
