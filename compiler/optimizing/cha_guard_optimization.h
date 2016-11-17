/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef ART_RUNTIME_CHA_GUARD_OPTIMIZATION_H_
#define ART_RUNTIME_CHA_GUARD_OPTIMIZATION_H_

#include "nodes.h"

namespace art {

class CHAGuardOptimization {
 public:
  // Note that enabled should only be set to true for one optimization pass,
  // since if a guard is not removed, another guard might be removed due to
  // the existence of the first guard. The first guard should not be further
  // removed in another pass.
  CHAGuardOptimization(HGraphVisitor* graph_visitor, bool enabled_(enabled),
        block_has_cha_guard_(graph_visitor->GetGraph()->GetBlocks().size(), false) {
    if (enabled) {
      // Will set to true again if there is any guard that cannot be eliminated.
      graph_visitor_->GetGraph()->SetHasCHAGuards(false);
    }
  }

  void OptimizeGuard(HShouldDeoptimizeFlag* flag);

 private:
  void RemoveGuard(HShouldDeoptimizeFlag* flag);
  // Return true if `flag` is removed.
  bool OptimizeForParameter(HShouldDeoptimizeFlag* flag, HInstruction* receiver);
  // Return true if `flag` is removed.
  bool OptimizeWithDominatingGuard(HShouldDeoptimizeFlag* flag, HInstruction* receiver);
  // Return true if `flag` is hoisted.
  bool HoistGuard(HShouldDeoptimizeFlag* flag, HInstruction* receiver);

  HGraphVisitor* graph_visitor_;

  bool enabled_;

  // Record if each block has any CHA guard. It's updated during the
  // reverse post order visit. Use int instead of bool since ArenaVector
  // does not support int.
  std::vector<bool> block_has_cha_guard_;

  DISALLOW_COPY_AND_ASSIGN(CHAGuardOptimization);
};

} // namespace art

#endif // ART_RUNTIME_CHA_GUARD_OPTIMIZATION_H_
