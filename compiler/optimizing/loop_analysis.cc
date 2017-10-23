/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "loop_analysis.h"

namespace art {

void LoopAnalysis::CalculateLoopBasicProperties(HLoopInformation* loop_info,
                                                LoopAnalysisInfo* analysis_results) {
  for (HBlocksInLoopIterator block_it(*loop_info);
       !block_it.Done();
       block_it.Advance()) {
    HBasicBlock* block = block_it.Current();

    for (HBasicBlock* successor : block->GetSuccessors()) {
      if (!loop_info->Contains(*successor)) {
        analysis_results->exits_num_++;
      }
    }

    for (HInstructionIterator it(block->GetInstructions()); !it.Done(); it.Advance()) {
      HInstruction* instruction = it.Current();
      if (MakesScalarUnrollingNonBeneficial(instruction)) {
        analysis_results->has_instructions_preventing_scalar_unrolling_ = true;
      }
      analysis_results->instr_num_++;
    }
    analysis_results->bb_num_++;
  }
}

}  // namespace art
