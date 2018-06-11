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

#include "licg.h"
#include "side_effects_analysis.h"
#include <iostream>

namespace art {

class LICGVisitor : public HGraphDelegateVisitor {
 public:
  explicit LICGVisitor(HGraph* graph)
      : HGraphDelegateVisitor(graph) {}

  bool Run();

 private:
  void RecordSwap() {
    swap_occurred_ = true;
    swaps_at_current_position_++;
    if (!change_was_recorded_) {
      std::cout << "Method: " << GetGraph()->PrettyMethod() << std::endl;
    }
    change_was_recorded_ = true;
  }

  bool IsSwapCandidate(HInstruction* instruction);

  bool TrySwapAssociativeAndCommutativeOperationInputs(HBinaryOperation* instruction);

  void VisitAdd(HAdd* instruction) OVERRIDE;
  void VisitMul(HMul* instruction) OVERRIDE;
  void VisitAnd(HAnd* instruction) OVERRIDE;
  void VisitOr(HOr* instruction) OVERRIDE;
  void VisitXor(HXor* instruction) OVERRIDE;
  void VisitMin(HMin* instruction) OVERRIDE;
  void VisitMax(HMax* instruction) OVERRIDE;

  bool swap_occurred_ = false;
  int swaps_at_current_position_ = 0;
  // We ensure we do not loop infinitely. The value should not be too high, since that
  // would allow looping around the same basic block too many times. The value should
  // not be too low either, however, since we want to allow revisiting a basic block
  // with many statements and simplifications at least once.
  static constexpr int kMaxSamePositionSwaps = 20;

  bool change_was_recorded_;
};

bool LICG::Run() {
  LICGVisitor visitor(graph_);
  return visitor.Run();
}

bool LICGVisitor::Run() {
  bool didLICG = false;

  change_was_recorded_ = false;
  // Post order visit to visit inner loops before outer loops.
  for (HBasicBlock* block : GetGraph()->GetPostOrder()) {
    if (!block->IsLoopHeader()) {
      // Only visit the loop when we reach the header.
      continue;
    }

    HLoopInformation* loop_info = block->GetLoopInformation();

    for (HBlocksInLoopIterator it_loop(*loop_info); !it_loop.Done(); it_loop.Advance()) {
      HBasicBlock* inner = it_loop.Current();
      DCHECK(inner->IsInLoop());
      if (inner->GetLoopInformation() != loop_info) {
        // Thanks to post order visit, inner loops were already visited.
        continue;
      }

      if (loop_info->ContainsIrreducibleLoop()) {
        continue;
      }
      DCHECK(!loop_info->IsIrreducible());

      do {
        swap_occurred_ = false;
        for (HInstructionIterator inst_it(inner->GetInstructions());
             !inst_it.Done();
             inst_it.Advance()) {
          inst_it.Current()->Accept(this);
        }
        if (swap_occurred_) {
          didLICG = true;
        }
      } while (swap_occurred_ &&
               (swaps_at_current_position_ < kMaxSamePositionSwaps));
      swaps_at_current_position_ = 0;
    }
  }
  return didLICG;
}

void LICGVisitor::VisitAdd(HAdd* instruction) {
  TrySwapAssociativeAndCommutativeOperationInputs(instruction);
}

void LICGVisitor::VisitMul(HMul* instruction) {
  TrySwapAssociativeAndCommutativeOperationInputs(instruction);
}

void LICGVisitor::VisitAnd(HAnd* instruction) {
  TrySwapAssociativeAndCommutativeOperationInputs(instruction);
}

void LICGVisitor::VisitOr(HOr* instruction) {
  TrySwapAssociativeAndCommutativeOperationInputs(instruction);
}

void LICGVisitor::VisitXor(HXor* instruction) {
  TrySwapAssociativeAndCommutativeOperationInputs(instruction);
}

void LICGVisitor::VisitMin(HMin* instruction) {
  TrySwapAssociativeAndCommutativeOperationInputs(instruction);
}

void LICGVisitor::VisitMax(HMax* instruction) {
  TrySwapAssociativeAndCommutativeOperationInputs(instruction);
}

bool LICGVisitor::IsSwapCandidate(HInstruction* instruction) {
  return !instruction->IsInLoop() || instruction->InputsAreDefinedBeforeLoop();
}

bool LICGVisitor::TrySwapAssociativeAndCommutativeOperationInputs(HBinaryOperation* instruction) {
  DCHECK(instruction->IsCommutative());

  if (!DataType::IsIntegralType(instruction->GetType())) {
    return false;
  }

  HInstruction* left = instruction->GetLeft();
  HInstruction* right = instruction->GetRight();
  // Variable names as described above.
  HInstruction* invariant_child;
  HBinaryOperation* y;
  HInstruction* dependant_child;
  size_t inv_child_index;
  size_t dep_child_index;

  if (instruction->GetKind() == left->GetKind() && IsSwapCandidate(right)) {
    invariant_child = right;
    inv_child_index = 1;
    y = left->AsBinaryOperation();
  } else if (IsSwapCandidate(left) && instruction->GetKind() == right->GetKind()) {
    invariant_child = left;
    inv_child_index = 0;
    y = right->AsBinaryOperation();
  } else {
    // The node does not match the patten.
    return false;
  }

  HInstruction* y_left = y->GetLeft();
  HInstruction* y_right = y->GetRight();

  if (!IsSwapCandidate(y_left) && IsSwapCandidate(y_right)) {
    dependant_child = y_left;
    dep_child_index = 0;
  } else if (IsSwapCandidate(y_left) && !IsSwapCandidate(y_right)) {
    dependant_child = y_right;
    dep_child_index = 1;
  } else {
    // The node does not match the pattern.
    return false;
  }

  // The result of 'y' must be preserved.
  if (!y->HasOnlyOneNonEnvironmentUse()) {
    return false;
  }

  y->ReplaceInput(invariant_child, dep_child_index);
  instruction->ReplaceInput(dependant_child, inv_child_index);
  // If the swap created a situation where an instruction does not dominate its use, correct
  // this by moving the use after the instruction.
  if (invariant_child->GetBlock() == instruction->GetBlock() &&
      !invariant_child->StrictlyDominates(y)) {
    // Left child would have depth 0 if it was not in the same block as instr.
    instruction->GetBlock()->MoveInstructionAfter(y, invariant_child);
  }

  RecordSwap();
  return true;
}

}  // namespace art
