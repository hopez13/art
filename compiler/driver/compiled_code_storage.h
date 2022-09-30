/*
 * Copyright (C) 2022 The Android Open Source Project
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

#ifndef ART_COMPILER_DRIVER_COMPILED_CODE_STORAGE_H_
#define ART_COMPILER_DRIVER_COMPILED_CODE_STORAGE_H_

#include <string>

#include "base/array_ref.h"

namespace art {

namespace linker {
class LinkerPatch;
}  // namespace linker

class CompiledMethod;
enum class InstructionSet;

class CompiledCodeStorage {
 public:
  virtual CompiledMethod* CreateCompiledMethod(InstructionSet instruction_set,
                                               ArrayRef<const uint8_t> code,
                                               ArrayRef<const uint8_t> stack_map,
                                               ArrayRef<const uint8_t> cfi,
                                               ArrayRef<const linker::LinkerPatch> patches,
                                               bool is_intrinsic) = 0;
  virtual ArrayRef<const uint8_t> GetThunkCode(const linker::LinkerPatch& patch,
                                               /*out*/ std::string* debug_name = nullptr) = 0;
  virtual void SetThunkCode(const linker::LinkerPatch& patch,
                            ArrayRef<const uint8_t> code,
                            const std::string& debug_name) = 0;

 protected:
  CompiledCodeStorage() {}
  ~CompiledCodeStorage() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(CompiledCodeStorage);
};

}  // namespace art

#endif  // ART_COMPILER_DRIVER_COMPILED_CODE_STORAGE_H_
