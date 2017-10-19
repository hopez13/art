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

#ifndef ART_COMPILER_OPTIMIZING_LOOP_ANALYSIS_H_
#define ART_COMPILER_OPTIMIZING_LOOP_ANALYSIS_H_

#include "nodes.h"

namespace art {

class LoopAnalysis;

// Class to hold cached information on properties of the loop.
class LoopAnalysisInfo : public ValueObject {
 public:
  explicit LoopAnalysisInfo(HLoopInformation* loop_info)
      : bb_num_(0),
        instr_num_(0),
        exits_num_(0),
        has_instructions_preventing_scalar_peeling_(false),
        has_instructions_preventing_scalar_unrolling_(false),
        loop_info_(loop_info) {}

  size_t GetNumberOfBasicBlocks() const { return bb_num_; }
  size_t GetNumberOfInstructions() const { return instr_num_; }
  size_t GetNumberOfExits() const { return exits_num_; }
  bool HasInstructionsPreventingScalarPeeling() const {
    return has_instructions_preventing_scalar_peeling_;
  }
  bool HasInstructionsPreventingScalarUnrolling() const {
    return has_instructions_preventing_scalar_unrolling_;
  }
  const HLoopInformation* GetLoopInfo() const { return loop_info_; }

 private:
  // Number of basic blocks in the loop body.
  size_t bb_num_;
  // Number of instructions in the loop body.
  size_t instr_num_;
  // Number of loop's exits.
  size_t exits_num_;
  // Whether the loop has instructions which make scalar loop peeling non-beneficial.
  bool has_instructions_preventing_scalar_peeling_;
  // Whether the loop has instructions which make scalar loop unrolling non-beneficial.
  bool has_instructions_preventing_scalar_unrolling_;

  // Corresponding HLoopInformation.
  const HLoopInformation* loop_info_;

  friend class LoopAnalysis;
};

// Placeholder class for methods and routines used to analyse loops, calculate loop properties
// and characteristics.
class LoopAnalysis : public ValueObject {
 public:
  // Calculates loops basic properties like body size, exits number, etc. and fills
  // 'analysis_results' with this information.
  static void CalculateLoopBasicProperties(HLoopInformation* loop_info,
                                           LoopAnalysisInfo* analysis_results);

  // Returns whether the loop has at least one loop invariant exit.
  static bool HasLoopAtLeastOneInvariantExit(HLoopInformation* loop_info);

  // Returns whether HIf's true or false successor is outside the specified loop.
  //
  // Prerequisite: HIf must be in the specified loop.
  static bool IsLoopExit(HLoopInformation* loop_info, const HIf* hif) {
    DCHECK(loop_info->Contains(*hif->GetBlock()));
    HBasicBlock* true_succ = hif->IfTrueSuccessor();
    HBasicBlock* false_succ = hif->IfFalseSuccessor();
    return (!loop_info->Contains(*true_succ) || !loop_info->Contains(*false_succ));
  }

 private:
  // Returns whether an instruction makes scalar loop peeling/unrolling non-beneficial.
  //
  // If in the loop body we have a dex/runtime call then its contribution to the whole
  // loop performance will probably prevail. So peeling/unrolling optimization will not bring
  // any noticeable performance improvement however will increase the code size.
  static bool MakesScalarPeelingUnrollingNonBeneficial(HInstruction* instruction) {
    return (instruction->IsNewArray() ||
        instruction->IsNewInstance() ||
        instruction->IsUnresolvedInstanceFieldGet() ||
        instruction->IsUnresolvedInstanceFieldSet() ||
        instruction->IsUnresolvedStaticFieldGet() ||
        instruction->IsUnresolvedStaticFieldSet() ||
        // TODO: Support loops with intrinsified invokes.
        instruction->IsInvoke() ||
        // TODO: Support loops with ClinitChecks.
        instruction->IsClinitCheck());
  }
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_LOOP_ANALYSIS_H_
