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


#include "base/arena_allocator.h"
#include "builder.h"
#include "expression_dag_balancing.h"
#include "nodes.h"
#include "optimizing_unit_test.h"
#include "graph_checker.h"

#include "gtest/gtest.h"
#include <unordered_set>

namespace art {

/**
 * Fixture class for the Expression Tree Height tests.
 */
class ExpressionDAGBalancingTest : public OptimizingUnitTest {
 public:
  ExpressionDAGBalancingTest()  : graph_(CreateGraph()) { }

  ~ExpressionDAGBalancingTest() { }

  void RunDAGBalance() {
    graph_->BuildDominatorTree();
    ExpressionDAGBalancing(graph_).Run();
    GraphChecker graph_checker(graph_);
    graph_checker.Run();
    ASSERT_TRUE(graph_checker.IsValid());
  }

  HGraph* graph_;
};

// (Graph)   Before:                           After:
//                  +                                +
//                 / \                              / \
//                v   v                            /   \
//                +  not                          +     +
//               / \    \                        / \   / \
//              v   v    v                      /  |   |  \
//              +  not  neg                    v   v   v   v
//             / \    \    \                  not not not not
//            v   v    v    v                  |   |   |   |
//           not not  neg   d                  v   v   v   v
//            |   |    |                      neg neg neg neg
//            v   v    v                       |   |   |   |
//           neg neg   c                       v   v   v   v
//            |   |                            d   c   a   b
//            v   v
//            a   b
TEST_F(ExpressionDAGBalancingTest, BalanceLLSubtreeWithUnaryOperations) {
  HBasicBlock* entry = new (GetAllocator()) HBasicBlock(graph_);
  graph_->AddBlock(entry);
  graph_->SetEntryBlock(entry);

  HInstruction* a = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(0), 0, DataType::Type::kInt32);
  HInstruction* b = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(0), 0, DataType::Type::kInt32);
  HInstruction* c = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(0), 0, DataType::Type::kInt32);
  HInstruction* d = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(0), 0, DataType::Type::kInt32);
  entry->AddInstruction(a);
  entry->AddInstruction(b);
  entry->AddInstruction(c);
  entry->AddInstruction(d);

  HBasicBlock* block = new (GetAllocator()) HBasicBlock(graph_);
  graph_->AddBlock(block);
  HInstruction* neg_a = new (GetAllocator()) HNeg(DataType::Type::kInt32, a);
  HInstruction* not_a = new (GetAllocator()) HNot(DataType::Type::kInt32, neg_a);
  HInstruction* neg_b = new (GetAllocator()) HNeg(DataType::Type::kInt32, b);
  HInstruction* not_b = new (GetAllocator()) HNot(DataType::Type::kInt32, neg_b);
  HInstruction* add_ab = new (GetAllocator()) HAdd(DataType::Type::kInt32, not_a, not_b);
  HInstruction* neg_c = new (GetAllocator()) HNeg(DataType::Type::kInt32, c);
  HInstruction* not_c = new (GetAllocator()) HNot(DataType::Type::kInt32, neg_c);
  HInstruction* add_c = new (GetAllocator()) HAdd(DataType::Type::kInt32, add_ab, not_c);
  HInstruction* neg_d = new (GetAllocator()) HNeg(DataType::Type::kInt32, d);
  HInstruction* not_d = new (GetAllocator()) HNot(DataType::Type::kInt32, neg_d);
  HInstruction* sum = new (GetAllocator()) HAdd(DataType::Type::kInt32, add_c, not_d);

  block->AddInstruction(neg_a);
  block->AddInstruction(not_a);
  block->AddInstruction(neg_b);
  block->AddInstruction(not_b);
  block->AddInstruction(add_ab);
  block->AddInstruction(neg_c);
  block->AddInstruction(not_c);
  block->AddInstruction(add_c);
  block->AddInstruction(neg_d);
  block->AddInstruction(not_d);
  block->AddInstruction(sum);
  entry->AddSuccessor(block);

  HBasicBlock* exit = new (GetAllocator()) HBasicBlock(graph_);
  graph_->AddBlock(exit);
  block->AddSuccessor(exit);
  exit->AddInstruction(new (GetAllocator()) HExit());

  RunDAGBalance();

  ASSERT_TRUE(not_d->StrictlyDominates(add_c));
}

// (Graph)   Before:                                      After:
//                  +         +                                +     +
//                 / \       / \                              / \   / \
//                /   v     v   \                            /   \ /   \
//               v    +     +    v                          v     v     v
//              not  / \   / \  not                         +     +     +
//               |  v   \ /   v  |                         / \   / \   / \
//               v  g    v    e  v                        v   v v   v v   v
//               h       +       f                       not  e +   + g  not
//                      / \                               |    / \ / \    |
//                     v   v                              v   |  | |  |   v
//                     +   d                              f   v  v v  v   h
//                    / \                                     d  c a  b
//                   v   v
//                   +   c
//                  / \
//                 v   v
//                 a   b
TEST_F(ExpressionDAGBalancingTest, BalanceRRLLLLSubtree) {
  HBasicBlock* entry = new (GetAllocator()) HBasicBlock(graph_);
  graph_->AddBlock(entry);
  graph_->SetEntryBlock(entry);

  HInstruction* a = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(0), 0, DataType::Type::kInt32);
  HInstruction* b = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(0), 0, DataType::Type::kInt32);
  HInstruction* c = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(0), 0, DataType::Type::kInt32);
  HInstruction* d = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(0), 0, DataType::Type::kInt32);
  HInstruction* e = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(0), 0, DataType::Type::kInt32);
  HInstruction* f = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(0), 0, DataType::Type::kInt32);
  HInstruction* g = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(0), 0, DataType::Type::kInt32);
  HInstruction* h = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(0), 0, DataType::Type::kInt32);

  entry->AddInstruction(a);
  entry->AddInstruction(b);
  entry->AddInstruction(c);
  entry->AddInstruction(d);
  entry->AddInstruction(e);
  entry->AddInstruction(f);
  entry->AddInstruction(g);
  entry->AddInstruction(h);

  HBasicBlock* block = new (GetAllocator()) HBasicBlock(graph_);
  graph_->AddBlock(block);
  HInstruction* add_ab = new (GetAllocator()) HAdd(DataType::Type::kInt32, a, b);
  HInstruction* add_c = new (GetAllocator()) HAdd(DataType::Type::kInt32, add_ab, c);
  HInstruction* add_d = new (GetAllocator()) HAdd(DataType::Type::kInt32, add_c, d);
  HInstruction* add_e = new (GetAllocator()) HAdd(DataType::Type::kInt32, add_d, e);
  HInstruction* not_f = new (GetAllocator()) HNot(DataType::Type::kInt32, f);
  HInstruction* add_not_f = new (GetAllocator()) HAdd(DataType::Type::kInt32, add_e, not_f);
  HInstruction* add_g = new (GetAllocator()) HAdd(DataType::Type::kInt32, g, add_d);
  HInstruction* not_h = new (GetAllocator()) HNot(DataType::Type::kInt32, h);
  HInstruction* add_not_h = new (GetAllocator()) HAdd(DataType::Type::kInt32, not_h, add_g);

  block->AddInstruction(add_ab);
  block->AddInstruction(add_c);
  block->AddInstruction(add_d);
  block->AddInstruction(add_e);
  block->AddInstruction(not_f);
  block->AddInstruction(add_not_f);
  block->AddInstruction(add_g);
  block->AddInstruction(not_h);
  block->AddInstruction(add_not_h);
  entry->AddSuccessor(block);

  HBasicBlock* exit = new (GetAllocator()) HBasicBlock(graph_);
  graph_->AddBlock(exit);
  block->AddSuccessor(exit);
  exit->AddInstruction(new (GetAllocator()) HExit());

  RunDAGBalance();

  ASSERT_EQ(add_d->InputAt(1)->GetId(), add_ab->GetId());
  ASSERT_TRUE(not_h->StrictlyDominates(add_g));
  ASSERT_TRUE(not_f->StrictlyDominates(add_e));
}

}  // namespace art
