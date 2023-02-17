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

#ifndef ART_COMPILER_OPTIMIZING_CODE_SINKING_H_
#define ART_COMPILER_OPTIMIZING_CODE_SINKING_H_

#include "base/macros.h"
#include "nodes.h"
#include "optimization.h"

namespace art HIDDEN {

/**
 * Optimization pass to move instructions into uncommon branches,
 * when it is safe to do so.
 */
class CodeSinking : public HOptimization {
 public:
  CodeSinking(HGraph* graph,
              OptimizingCompilerStats* stats,
              const char* name = kCodeSinkingPassName)
      : HOptimization(graph, name, stats) {}

  bool Run() override;

  static constexpr const char* kCodeSinkingPassName = "code_sinking";

 private:
  // Try to move code only used by `end_block` and all its post-dominated / dominated
  // blocks, to these blocks.
  void SinkCodeToUncommonBranch(HBasicBlock* end_block);

  // Find the ideal position for moving `instruction`. If `filter` is true,
  // we filter out store instructions to that instruction, which are processed
  // first in the step (3) of the sinking algorithm.
  // This method is tailored to the sinking algorithm, unlike
  // the generic HInstruction::MoveBeforeFirstUserAndOutOfLoops.
  HInstruction* FindIdealPosition(HInstruction* instruction,
                                  const ArenaBitVector& post_dominated,
                                  bool filter = false);

  DISALLOW_COPY_AND_ASSIGN(CodeSinking);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_CODE_SINKING_H_
