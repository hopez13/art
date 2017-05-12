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

#include "load_store_analysis.h"

namespace art {

// A cap for the number of heap locations to prevent pathological time/space consumption.
// The number of heap locations for most of the methods stays below this threshold.
constexpr size_t kMaxNumberOfHeapLocations = 32;
constexpr size_t kMaxAliasAnalysisDepth = 4;

// Check if array indices array[idx1 +/- CONST] and array[idx2] MAY alias.
static bool AddSubConstantAndIndexMayAlias(const HBinaryOperation* idx1, const HInstruction* idx2) {
  DCHECK(idx1 != nullptr);
  DCHECK(idx2 != nullptr);

  HConstant* cst = idx1->GetConstantRight();
  if (cst == nullptr || cst->IsArithmeticZero()) {
    return true;
  }

  if (idx1->GetLeastConstantLeft() == idx2) {
    // for example, array[idx1 + 1] and array[idx1]
    return false;
  }

  return true;
}

// Check if Add and Sub MAY alias when used as indices in arrays.
//
// - If all inputs are different, then the two MAY alias;
// - otherwise, we are in the following situation:
//     Add -> `i + HIR1`
//     Sub -> `i - HIR2` or `HIR2 - i`
//
//   > If HIR1 and HIR2 are both constants, then
//     1) `i + CONST1` and `i - CONST2` MAY alias iff CONST1 = -CONST2.
//     2) `i + CONST1` and `CONST2 - i` MAY alias, unless we have range
//         analysis for i.
//
//   > If HIR1 or HIR2 is constant but not both, then they MAY alias.
//
//   > If HIR1 and HIR2 are both not constants, then they MAY alias.
static bool AddSubMayAlias(const HAdd* add, const HSub* sub) {
  DCHECK(add != nullptr);
  DCHECK(sub != nullptr);

  HConstant* add_cst = add->GetConstantRight();
  HInstruction* add_other = add->GetLeastConstantLeft();
  HConstant* sub_cst = sub->GetConstantRight();
  HInstruction* sub_other = sub->GetLeastConstantLeft();

  if (add_cst == nullptr || sub_cst == nullptr) {
    return true;
  }

  if (add_other == sub_other) {
    // Case 1)
    return add_cst->AsIntConstant()->GetValue() == -(sub_cst->AsIntConstant()->GetValue());
  }

  // All other cases, MAY alias.
  return true;
}

bool HeapLocationCollector::CanArrayIndicesAlias(HInstruction* idx1,
                                                 HInstruction* idx2,
                                                 size_t depth) const {
  DCHECK(idx1 != nullptr);
  DCHECK(idx2 != nullptr);

  if (depth > kMaxAliasAnalysisDepth) {
    return true;
  }

  if (idx1 == idx2) {
    return true;
  }

  if (idx1->IsIntConstant() && idx2->IsIntConstant()) {
    return idx1->AsIntConstant()->GetValue() == idx2->AsIntConstant()->GetValue();
  }

  if (idx1->GetKind() != idx2->GetKind()) {
    // Handle cases like:
    //   array[i]
    //   array[i + 1]
    if ((idx1->IsAdd() || idx1->IsSub()) &&
        !AddSubConstantAndIndexMayAlias(idx1->AsBinaryOperation(), idx2)) {
      return false;
    }
    if ((idx2->IsAdd() || idx2->IsSub()) &&
        !AddSubConstantAndIndexMayAlias(idx2->AsBinaryOperation(), idx1)) {
      return false;
    }
    // Handle aliasing between an Add and a Sub. For example:
    //   array[i + 1]
    //   array[i - 1]
    if (idx1->IsAdd() && idx2->IsSub() && !AddSubMayAlias(idx1->AsAdd(), idx2->AsSub())) {
      return false;
    }
    if (idx1->IsSub() && idx2->IsAdd() && !AddSubMayAlias(idx2->AsAdd(), idx1->AsSub())) {
      return false;
    }
    // By default, MAY alias.
    return true;
  }

  if (idx1->IsAdd() || idx1->IsSub()) {
    // Handle some multiple Add and Sub calculations, For example:
    //   array[i + j + k + 1]
    //   array[i + j + k - 1]
    DCHECK(idx1->GetKind() == idx2->GetKind());
    if (idx1->InputAt(0) == idx2->InputAt(0)) {
      return CanArrayIndicesAlias(idx1->InputAt(1), idx2->InputAt(1), depth + 1);
    }
    if (idx1->InputAt(1) == idx2->InputAt(1)) {
      return CanArrayIndicesAlias(idx1->InputAt(0), idx2->InputAt(0), depth + 1);
    }
    if (idx1->IsAdd())  {
      if (idx1->InputAt(0) == idx2->InputAt(1)) {
        return CanArrayIndicesAlias(idx1->InputAt(1), idx2->InputAt(0), depth + 1);
      }
      if (idx1->InputAt(1) == idx2->InputAt(0)) {
        return CanArrayIndicesAlias(idx1->InputAt(0), idx2->InputAt(1), depth + 1);
      }
    }
    // Above array index aliasing handling is limited,
    // it cannot handle cases where operands come in different order,
    // for example: array[a+b+10+c] and array[a+10+b+c].
    // To handle complex array index calculations,
    // we can rely on other passes like GVN and simplifier to
    // optimize before this LSA pass.

    // By default, MAY alias.
    return true;
  }

  // More than one input differs, MAY alias.
  return true;
}

void LoadStoreAnalysis::Run() {
  for (HBasicBlock* block : graph_->GetReversePostOrder()) {
    heap_location_collector_.VisitBasicBlock(block);
  }

  if (heap_location_collector_.GetNumberOfHeapLocations() > kMaxNumberOfHeapLocations) {
    // Bail out if there are too many heap locations to deal with.
    heap_location_collector_.CleanUp();
    return;
  }
  if (!heap_location_collector_.HasHeapStores()) {
    // Without heap stores, this pass would act mostly as GVN on heap accesses.
    heap_location_collector_.CleanUp();
    return;
  }
  if (heap_location_collector_.HasVolatile() || heap_location_collector_.HasMonitorOps()) {
    // Don't do load/store elimination if the method has volatile field accesses or
    // monitor operations, for now.
    // TODO: do it right.
    heap_location_collector_.CleanUp();
    return;
  }

  heap_location_collector_.BuildAliasingMatrix();
}

}  // namespace art
