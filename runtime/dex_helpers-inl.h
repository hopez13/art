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

inline DexInstructionsHelper::DexInstructionsHelper(const DexFile* dex_file,
                                                    const DexFile::CodeItem* code_item) {
  DCHECK(dex_file != nullptr);
  DCHECK(code_item != nullptr);
  if (LIKELY(dex_file->IsCompactDexFile())) {
    const CompactDexFile::CodeItem* child_code_item =
        down_cast<const CompactDexFile::CodeItem*>(code_item);
    insns_size_in_code_units_ = child_code_item->insns_size_in_code_units_;
    insns_ = child_code_item->insns_;
  } else {
    DCHECK(dex_file->IsStandardDexFile());
    const StandardDexFile::CodeItem* child_code_item =
        down_cast<const StandardDexFile::CodeItem*>(code_item);
    insns_size_in_code_units_ = child_code_item->insns_size_in_code_units_;
    insns_ = child_code_item->insns_;
  }
}

inline DexInstructionsHelper::DexInstructionsHelper(ArtMethod* method)
    : DexInstructionsHelper(method->GetDexFile(), method->GetCodeItem()) {}

}  // namespace art

#endif  // ART_RUNTIME_DEX_HELPERS_INL_H_
