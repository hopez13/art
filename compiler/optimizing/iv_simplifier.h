/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_COMPILER_OPTIMIZING_IV_SIMPLIFIER_H_
#define ART_COMPILER_OPTIMIZING_IV_SIMPLIFIER_H_

#include "nodes.h"
#include "induction_var_analysis.h"
#include <tuple>

namespace art {

class InductionVarSimplification {
 public:
  explicit  InductionVarSimplification(HInductionVarAnalysis* analysis);

  /**
   * @brief Performs Simplification on the loop
   * @param loop The input loop in the graph.
   */
  bool SimplifyLoop(HLoopInformation* loop);
  /**
   * @brief Checks if the phi is a basic induction variable
   *        the loop
   * @param loop The loop to be checked.
   * @param phi The phi to be checked.
   */
  bool IsCandidatePhi(HLoopInformation* loop, HPhi* phi);

 private:
  /**
   * @brief Performs Strength Reduction to eliminate multplication and
   *        replace with equivalent addition
   * @param loop The loop to be simplified.
   * @param derived_var The derived induction variable upon which
   *        simplification is applied
   * @param candidate The mapping of derived induction variables.
   */
  void PerformReduction(HLoopInformation* loop, HInstruction* derived_var,
                  std::tuple<HInstruction*, HConstant*, HConstant*> val);
   /**
   * @brief Checks if an instruction is suitable candidate to perform
   *         strength reducion
   * @param loop The loop to be simplified.
   * @param to_check The instruction in the loop to be checked
   * @param candidate A mapping from biv to corresponding
   *        derived induction variables
   *        Stores an induction of the form j = a*i +b where i is biv,
   *        as an entry [j, <i, a, b>]
   */
  bool IsCandidateForReduction(HLoopInformation* loop, HPhi* biv, HBinaryOperation* to_check,
                               std::map<HBinaryOperation*, std::tuple<HInstruction*, HConstant*, HConstant*>>* candidate);

  HInductionVarAnalysis* induction_analysis_;
  HBinaryOperation* biv_increment_;
  /* Results of induction var analysis */
  friend class HInductionVarAnalysis;

  DISALLOW_COPY_AND_ASSIGN(InductionVarSimplification);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_IV_SIMPLIFIER_H_
