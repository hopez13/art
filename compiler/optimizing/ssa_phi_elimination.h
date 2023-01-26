/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef ART_COMPILER_OPTIMIZING_SSA_PHI_ELIMINATION_H_
#define ART_COMPILER_OPTIMIZING_SSA_PHI_ELIMINATION_H_

#include "base/macros.h"
#include "nodes.h"
#include "optimization.h"

namespace art HIDDEN {

/**
 * Optimization phase that removes dead phis from the graph. Dead phis are unused
 * phis, or phis only used by other phis.
 */
class SsaDeadPhiElimination : public HOptimization {
 public:
  explicit SsaDeadPhiElimination(HGraph* graph)
      : HOptimization(graph, kSsaDeadPhiEliminationPassName) {}

  bool Run() override;

  // Marks phis which are not used by instructions or other live phis. If compiling as debuggable
  // code, phis will also be kept live if they have an environment use.
  void MarkDeadPhis();

  // Make sure environments use the right phi equivalent: a phi marked dead
  // can have a phi equivalent that is not dead. In that case we have to replace
  // it with the live equivalent because deoptimization and try/catch rely on
  // environments containing values of all live vregs at that point. Note that
  // there can be multiple phis for the same Dex register that are live
  // (for example when merging constants), in which case it is okay for the
  // environments to just reference one.
  void FixEnvironmentPhis();

  // Eliminates phis we do not need. 
  void EliminateDeadPhis();

  static constexpr const char* kSsaDeadPhiEliminationPassName = "dead_phi_elimination";

 private:
  DISALLOW_COPY_AND_ASSIGN(SsaDeadPhiElimination);
};

/**
 * Removes redundant phis that may have been introduced when doing SSA conversion.
 * For example, when entering a loop, we create phis for all live registers. These
 * registers might be updated with the same value, or not updated at all. We can just
 * replace the phi with the value when entering the loop.
 */
class SsaRedundantPhiElimination : public HOptimization {
 public:
  explicit SsaRedundantPhiElimination(HGraph* graph)
      : HOptimization(graph, kSsaRedundantPhiEliminationPassName) {}

  bool Run() override;

  static constexpr const char* kSsaRedundantPhiEliminationPassName = "redundant_phi_elimination";

 private:
  DISALLOW_COPY_AND_ASSIGN(SsaRedundantPhiElimination);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_SSA_PHI_ELIMINATION_H_
