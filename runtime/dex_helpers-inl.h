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
#include "cdex/compact_dex_file.h"
#include "standard_dex_file.h"

namespace art {

inline DexHelperᐸInstructionᐳ::DexHelperᐸInstructionᐳ(ArtMethod* method) {
  const DexFile* dex_file = method->GetDexFile();
  const DexFile::CodeItem* base_code_item = method->GetCodeItem();
  if (LIKELY(dex_file->IsCompactDexFile())) {
    const CompactDexFile::CodeItem* code_item =
        down_cast<const CompactDexFile::CodeItem*>(base_code_item);
    insns_size_in_code_units_ = code_item->insns_size_in_code_units_;
    insns_ = code_item->insns_;
  } else {
    DCHECK(dex_file->IsStandardDexFile());
    const StandardDexFile::CodeItem* code_item =
        down_cast<const StandardDexFile::CodeItem*>(base_code_item);
    insns_size_in_code_units_ = code_item->insns_size_in_code_units_;
    insns_ = code_item->insns_;
  }
}

}  // namespace art

#endif  // ART_RUNTIME_DEX_HELPERS_INL_H_
