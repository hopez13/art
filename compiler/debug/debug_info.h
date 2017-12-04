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

#ifndef ART_COMPILER_DEBUG_DEBUG_INFO_H_
#define ART_COMPILER_DEBUG_DEBUG_INFO_H_

#include <vector>

#include "base/array_ref.h"
#include "method_debug_info.h"

namespace art {
class DexFile;

namespace debug {

// References inputs for all debug information which can be written into the ELF file.
struct DebugInfo {
  ArrayRef<const MethodDebugInfo> compiled_methods;  // Describes native code in .text section.
  const std::vector<const DexFile*>* dex_files;  // Describes dex files in the .dex section.
  size_t dex_files_offset;  // Offset of the first dex file from the start of the .dex section.

  bool Empty() const {
    return compiled_methods.empty() && (dex_files == nullptr || dex_files->empty());
  }
};

}  // namespace debug
}  // namespace art

#endif  // ART_COMPILER_DEBUG_DEBUG_INFO_H_
