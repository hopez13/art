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

#include "expression_dag_balancing.h"

namespace art {

// Minimum depth difference between left and right children for a left-imbalance.
static constexpr const int kImbalancedAddSubtreeLeft = 1;

// Minimum depth difference between left and right children for a right-imbalance.
static constexpr const int kImbalancedAddSubtreeRight = -1;

void ExpressionDAGBalancing::Run() {
  for (HBasicBlock* block : graph_->GetReversePostOrder()) {
    // We use a map to remember which instructions have been visited and to remember the depth of
    // the DAG rooted at each visited instruction, so that if the traversal function visits it
    // again (e.g. for instructions with multiple uses) it can make use of the information gathered
    // in the previous pass, without the need to traverse the subgraph again.
    std::unordered_map<int, int> visited;

    // Iterate over the instructions in the basic block in 'backward' order, so that the algorithm
    // begins always at a tree 'root' (a node with no incoming edges in the DAG).
    for (HBackwardInstructionIteratorHandleChanges it(block->GetInstructions());
         !it.Done(); it.Advance()) {
      HInstruction* instr = it.Current();
      // Each instruction is only visited once.
      if (visited.find(instr->GetId()) == visited.end()) {
        TraverseDAG(instr, &visited);
      }
    }
  }
}

// Tries to balance a subtree rooted at instr by swapping the more shallow child with the deeper
// child of the deeper child. This has the effect of reducing the depth of the deeper child by 1
// and increasing the depth of the more shallow child by 1, hence reducing the critical path of the
// DAG by 1. Note that OP can be any instruction, as long as the depth of the DAG rooted at OP is at
// least 1, and that OP can have multiple uses since its result is preserved.
// Example (Left-Left imbalanced binary tree):
//
// Before:                                         After:
//         +                                              +
//        / \                                            / \
//       v   v                                          /   \
//       +   d                                         v     v
//      / \                                            +     OP
//     v   v                                          / \   / \
//     OP  c                                         v   v v   v
//    / \                                            d   c a   b
//   v   v
//   a   b
bool ExpressionDAGBalancing::TryBalanceAddSubtree(HInstruction* instr,
                                                  SubgraphInfo left_info,
                                                  SubgraphInfo right_info) {
  int depth_diff = left_info.depth_ - right_info.depth_;

  // See if any balancing can be done for this subtree. If the left (or right, depending on how
  // the subtree is imbalanced) child has multiple uses, then this optimization cannot be applied
  // since it involves changing one of its inputs and, hence, its result (which must be preserved).
  // This optimization applies only to operations which are both associative and commutative.
  // Checking that they are of integral type is necessary because for other types the properties
  // mentioned above cannot be guaranteed (e.g. for float associativity cannot be guaranteed
  // because of rounding).
  if (instr->IsAdd() && DataType::IsIntegralType(instr->GetType())) {
    HInstruction* left = instr->InputAt(0);
    HInstruction* right = instr->InputAt(1);

    if (depth_diff > kImbalancedAddSubtreeLeft &&
        DataType::IsIntegralType(left->GetType()) &&
        instr->InstructionTypeEquals(left) &&
        left->HasOnlyOneNonEnvironmentUse() &&
        left_info.imbalance_type_ != kBalanced) {
      HInstruction* left_left = left->InputAt(0);
      HInstruction* left_right = left->InputAt(1);

      if (left_info.imbalance_type_ == kLeftImbalanced) {
        // Left-Left imbalanced binary subtree.
        left->ReplaceInput(right, 0);
        instr->ReplaceInput(left_left, 1);
      } else {
        // Left-Right imbalanced binary subtree.
        left->ReplaceInput(right, 1);
        instr->ReplaceInput(left_right, 1);
      }

      // If the swap created a situation where an instruction does not dominate its use, correct
      // this by moving the use after the instruction.
      if (right->GetBlock() == instr->GetBlock() && !right->StrictlyDominates(left)) {
        // Left child would have depth 0 if it was not in the same block as instr.
        instr->GetBlock()->MoveInstructionAfter(left, right);
      }
      return true;
    } else if (depth_diff < kImbalancedAddSubtreeRight &&
               DataType::IsIntegralType(right->GetType()) &&
               instr->InstructionTypeEquals(right) &&
               right->HasOnlyOneNonEnvironmentUse() &&
               right_info.imbalance_type_ != kBalanced) {
      HInstruction* right_left = right->InputAt(0);
      HInstruction* right_right = right->InputAt(1);

      if (right_info.imbalance_type_ == kLeftImbalanced) {
        // Right-Left imbalanced binary subtree.
        right->ReplaceInput(left, 0);
        instr->ReplaceInput(right_left, 0);
      } else {
        // Right-Right imbalanced binary subtree.
        right->ReplaceInput(left, 1);
        instr->ReplaceInput(right_right, 0);
      }

      // If the swap created a situation where an instruction does not dominate its use, correct
      // this by moving the use after the instruction.
      if (left->GetBlock() == instr->GetBlock() && !left->StrictlyDominates(right)) {
        // Right child would have depth 0 if it was not in the same block as instr.
        instr->GetBlock()->MoveInstructionAfter(right, left);
      }
      return true;
    }
  }

  return false;
}

// Calculates the kind of imbalance found in a binary subtree from the information about its left
// and right children.
ExpressionDAGBalancing::ImbalanceType
    ExpressionDAGBalancing::DetermineImbalance(SubgraphInfo left_info,
                                               SubgraphInfo right_info) {
  int left_depth = left_info.depth_;
  int right_depth = right_info.depth_;

  if (left_depth == right_depth) {
    return kBalanced;
  } else if (left_depth > right_depth) {
    return kLeftImbalanced;
  } else {
    return kRightImbalanced;
  }
}

// Use DFS to traverse the subgraph rooted at instr and detect binary subtrees. Whenever a binary
// subtree is discovered, balancing is attempted. A consequence of this and the DFS is that the
// children of instr are already as balanced as possible (using the balancing function). This
// function returns information about the subgraph rooted at instr and updates the map of visited
// instructions.
ExpressionDAGBalancing::SubgraphInfo
    ExpressionDAGBalancing::TraverseDAG(HInstruction* instr,
                                        std::unordered_map<int, int>* visited) {
  std::unordered_map<int, int>::const_iterator got = visited->find(instr->GetId());
  if (got != visited->end()) {
    // If the instruction has been already visited, then we already know the depth of the DAG
    // rooted at instr. The DAG already is as balanced as possible since it was previously
    // traversed by this function.
    return SubgraphInfo(got->second, kBalanced);
  }

  // Mark the instruction visited.
  visited->emplace(instr->GetId(), 0);
  int dag_depth = 0;

  if (instr->InputCount() == 0) {
    return SubgraphInfo(0, kBalanced);
  } else if (instr->IsBinaryOperation()) {
    // We have found a binary tree in the DAG.
    HInstruction* left = instr->InputAt(0);
    HInstruction* right = instr->InputAt(1);

    // Information about left and right subtrees.
    SubgraphInfo left_info, right_info;

    // Make sure we don't move past the borders of the basic block.
    if (left->GetBlock() != instr->GetBlock()) {
      left_info = SubgraphInfo(0, kBalanced);
    } else {
      left_info = TraverseDAG(left, visited);
      DCHECK(visited->find(left->GetId()) != visited->end());
    }

    // Make sure we don't move past the borders of the basic block.
    if (right->GetBlock() != instr->GetBlock()) {
      right_info = SubgraphInfo(0, kBalanced);
    } else {
      right_info = TraverseDAG(right, visited);
      DCHECK(visited->find(right->GetId()) != visited->end());
    }

    bool optimized = TryBalanceAddSubtree(instr, left_info, right_info);

    if (optimized) {
      // A successful balancing attempt reduces the depth of the DAG by 1.
      dag_depth = std::max(left_info.depth_, right_info.depth_);
    } else {
      dag_depth = std::max(left_info.depth_, right_info.depth_) + 1;
    }
    // Update depth information in the hash map.
    visited->at(instr->GetId()) = dag_depth;
    // Determine the type of imbalance found in this binary subtree.
    ImbalanceType imbalance = optimized ? kBalanced : DetermineImbalance(left_info, right_info);

    return SubgraphInfo(dag_depth, imbalance);
  } else {
    SubgraphInfo child_info;
    for (size_t i = 0; i < instr->InputCount(); i++) {
      // Visit each child of instr, making sure we don't move past the borders of the basic block.
      // The depth of the subgraph rooted at instr will be: max(children depths) + 1.
      if (instr->InputAt(i)->GetBlock() == instr->GetBlock()) {
        child_info = TraverseDAG(instr->InputAt(i), visited);
        if (child_info.depth_ > dag_depth) {
          dag_depth = child_info.depth_;
        }
      }
    }

    // Update depth information in the hash map.
    visited->at(instr->GetId()) = dag_depth + 1;
    return SubgraphInfo(dag_depth + 1, kBalanced);
  }
}

}  // namespace art
