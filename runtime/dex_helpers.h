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

#include "dex_file.h"
#include "dex_instruction.h"
#include "dex_instruction_iterator.h"

namespace art {

// Dex helpers have ART specific utilities, we may want to refactor these for use in dexdump.

class ArtMethod;
class CompactDexFile;
class DexFile;
class StandardDexFile;

// Copies the instruction information from a CompactDex / StandardDexFile code item.
// Doesn't copy the debug info since this will be factored into a different helper.
class DexHelperᐸInstructionᐳ {
 public:
  explicit DexHelperᐸInstructionᐳ(ArtMethod* method);

  IterationRange<DexInstructionIterator> Instructions() const {
    return { DexInstructionIterator(insns_, 0u),
             DexInstructionIterator(insns_, insns_size_in_code_units_) };
  }

 private:
  // size of the insns array, in 2 byte code units
  uint32_t insns_size_in_code_units_;
  // Pointer to the instructions.
  const uint16_t* insns_;
};

}  // namespace art

#endif  // ART_RUNTIME_DEX_HELPERS_H_
