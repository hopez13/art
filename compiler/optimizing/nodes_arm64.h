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

#ifndef ART_COMPILER_OPTIMIZING_NODES_ARM64_H_
#define ART_COMPILER_OPTIMIZING_NODES_ARM64_H_

// This #include should never be used by compilation, because this header file (nodes_vector.h)
// is included in the header file nodes.h itself. However it gives editing tools better context.
#include "nodes.h"

namespace art {

/**
 * Represents an Arm64 LDP instruction.
 */
class HArmLoadPair final : public HMOExpression<2, 2> {
 public:
  HArmLoadPair(HInstruction* array,
               HInstruction* index,
               DataType::Type type,
               uint32_t dex_pc)
      : HMOExpression(kArmLoadPair,
                      type,
                      SideEffects::ArrayReadOfType(type),
                      dex_pc) {
    SetRawInputAt(0, array);
    SetRawInputAt(1, index);
  }

  DECLARE_INSTRUCTION(ArmLoadPair);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_NODES_ARM64_H_
