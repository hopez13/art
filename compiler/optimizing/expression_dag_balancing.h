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

#ifndef ART_COMPILER_OPTIMIZING_EXPRESSION_DAG_BALANCING_H_
#define ART_COMPILER_OPTIMIZING_EXPRESSION_DAG_BALANCING_H_


#include "nodes.h"
#include "optimization.h"
#include "optimizing_compiler_stats.h"
#include <unordered_map>

namespace art {

/**
 * Expression DAG Balancing. Visits each basic block and applies a balancing optimization to its
 * DAG in order to reduce the critical path and create more ILP to be exploited by other
 * optimizations such as instruction scheduling. This optimization uses DFS to traverse the DAG
 * and detects binary subtrees that it tries to apply the balancing function to.
 */
class ExpressionDAGBalancing : public HOptimization {
 public:
  ExpressionDAGBalancing(HGraph* graph,
                         OptimizingCompilerStats* stats = nullptr,
                         const char* name = kExpressionDAGBalancingPassName)
      : HOptimization(graph, name, stats)  {}

  static constexpr const char* kExpressionDAGBalancingPassName = "expression_dag_balancing";

  /**
   * The type of imbalance found in a DAG. The latter two imbalance types only apply to binary
   * subtrees of the DAG. Every subgraph which is not a binary tree is considered balanced since
   * there is nothing to be done about it as the balancing only handles binary trees.
   */
  enum ImbalanceType {
    kBalanced = 0,         // No imbalance.
    kLeftImbalanced = 1,   // Left-imbalanced (left child has greater depth).
    kRightImbalanced = 2,  // Right-imbalanced (right child has greater depth).
  };

  /**
   * Information about a subgraph: depth and the type of imbalance found in it.
   */
  struct SubgraphInfo {
    int depth_;
    ImbalanceType imbalance_type_;

    SubgraphInfo() : depth_(0), imbalance_type_(kBalanced) {}
    SubgraphInfo(int d, ImbalanceType imb) : depth_(d), imbalance_type_(imb) {}
  };

  void Run() OVERRIDE;

 private:
  // Balancing function for a binary subtree of Add instructions.
  bool TryBalanceAddSubtree(HInstruction* instr, SubgraphInfo left_info, SubgraphInfo right_info);

  // Determines the type of imbalance found in a binary subtree.
  ImbalanceType DetermineImbalance(SubgraphInfo left_info, SubgraphInfo right_info);

  // DFS traversal of the DAG rooted at instr and binary subtree detection.
  SubgraphInfo TraverseDAG(HInstruction* instr, std::unordered_map<int, int>* visited);

  DISALLOW_COPY_AND_ASSIGN(ExpressionDAGBalancing);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_EXPRESSION_DAG_BALANCING_H_
