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

#include "graph_checker.h"
#include "nodes.h"
#include "optimizing_unit_test.h"
#include "superblock_cloner.h"

#include "gtest/gtest.h"

namespace art {

using HBasicBlockMap = SuperblockCloner::HBasicBlockMap;
using HInstructionMap = SuperblockCloner::HInstructionMap;
using HBasicBlockSet = SuperblockCloner::HBasicBlockSet;
using HEdgeSet = SuperblockCloner::HEdgeSet;

// This class provides methods and helpers for testing various cloning and copying routines:
// individual instruction cloning and cloning of the more coarse-grain structures.
class ClonerTest : public OptimizingUnitTest {
 public:
  ClonerTest() : graph_(CreateGraph()),
                 entry_block_(nullptr),
                 return_block_(nullptr),
                 exit_block_(nullptr),
                 parameter_(nullptr) {}

  void InitGraph() {
    entry_block_ = new (GetAllocator()) HBasicBlock(graph_);
    graph_->AddBlock(entry_block_);
    graph_->SetEntryBlock(entry_block_);

    return_block_ = new (GetAllocator()) HBasicBlock(graph_);
    graph_->AddBlock(return_block_);

    exit_block_ = new (GetAllocator()) HBasicBlock(graph_);
    graph_->AddBlock(exit_block_);
    graph_->SetExitBlock(exit_block_);

    entry_block_->AddSuccessor(return_block_);
    return_block_->AddSuccessor(exit_block_);

    parameter_ = new (GetAllocator()) HParameterValue(graph_->GetDexFile(),
                                                      dex::TypeIndex(0),
                                                      0,
                                                      DataType::Type::kInt32);
    entry_block_->AddInstruction(parameter_);
    return_block_->AddInstruction(new (GetAllocator()) HReturnVoid());
    exit_block_->AddInstruction(new (GetAllocator()) HExit());
  }

  void CreateBasicLoopControlFlow(HBasicBlock* position,
                                  HBasicBlock* successor,
                                  /* out */ HBasicBlock** header_p,
                                  /* out */ HBasicBlock** body_p) {
    HBasicBlock* loop_preheader = new (GetAllocator()) HBasicBlock(graph_);
    HBasicBlock* loop_header = new (GetAllocator()) HBasicBlock(graph_);
    HBasicBlock* loop_body = new (GetAllocator()) HBasicBlock(graph_);

    graph_->AddBlock(loop_preheader);
    graph_->AddBlock(loop_header);
    graph_->AddBlock(loop_body);

    position->ReplaceSuccessor(successor, loop_preheader);

    loop_preheader->AddSuccessor(loop_header);
    // Loop exit first to have a proper exit condition/target for HIf.
    loop_header->AddSuccessor(successor);
    loop_header->AddSuccessor(loop_body);
    loop_body->AddSuccessor(loop_header);

    *header_p = loop_header;
    *body_p = loop_body;
  }

  void CreateBasicLoopDataFlow(HBasicBlock* loop_header, HBasicBlock* loop_body) {
    uint32_t dex_pc = 0;

    // Entry block.
    HIntConstant* const_0 = graph_->GetIntConstant(0);
    HIntConstant* const_1 = graph_->GetIntConstant(1);
    HIntConstant* const_128 = graph_->GetIntConstant(128);

    // Header block.
    HPhi* phi = new (GetAllocator()) HPhi(GetAllocator(), 0, 0, DataType::Type::kInt32);
    HInstruction* suspend_check = new (GetAllocator()) HSuspendCheck();
    HInstruction* loop_check = new (GetAllocator()) HGreaterThanOrEqual(phi, const_128);

    loop_header->AddPhi(phi);
    loop_header->AddInstruction(suspend_check);
    loop_header->AddInstruction(loop_check);
    loop_header->AddInstruction(new (GetAllocator()) HIf(loop_check));

    // Loop body block.
    HInstruction* null_check = new (GetAllocator()) HNullCheck(parameter_, dex_pc);
    HInstruction* array_length = new (GetAllocator()) HArrayLength(null_check, dex_pc);
    HInstruction* bounds_check = new (GetAllocator()) HBoundsCheck(phi, array_length, dex_pc);
    HInstruction* array_get =
        new (GetAllocator()) HArrayGet(null_check, bounds_check, DataType::Type::kInt32, dex_pc);
    HInstruction* add =  new (GetAllocator()) HAdd(DataType::Type::kInt32, array_get, const_1);
    HInstruction* array_set =
        new (GetAllocator()) HArraySet(null_check, bounds_check, add, DataType::Type::kInt32, dex_pc);
    HInstruction* induction_inc = new (GetAllocator()) HAdd(DataType::Type::kInt32, phi, const_1);

    loop_body->AddInstruction(null_check);
    loop_body->AddInstruction(array_length);
    loop_body->AddInstruction(bounds_check);
    loop_body->AddInstruction(array_get);
    loop_body->AddInstruction(add);
    loop_body->AddInstruction(array_set);
    loop_body->AddInstruction(induction_inc);
    loop_body->AddInstruction(new (GetAllocator()) HGoto());

    phi->AddInput(const_0);
    phi->AddInput(induction_inc);

    graph_->SetHasBoundsChecks(true);

    // Adjust HEnvironment for each instruction which require that.
    ArenaVector<HInstruction*> current_locals({phi, const_128, parameter_},
                                              GetAllocator()->Adapter(kArenaAllocInstruction));

    HEnvironment* env = ManuallyBuildEnvFor(suspend_check, &current_locals);
    null_check->CopyEnvironmentFrom(env);
    bounds_check->CopyEnvironmentFrom(env);
  }

  HEnvironment* ManuallyBuildEnvFor(HInstruction* instruction,
                                    ArenaVector<HInstruction*>* current_locals) {
    HEnvironment* environment = new (GetAllocator()) HEnvironment(
        (GetAllocator()),
        current_locals->size(),
        graph_->GetArtMethod(),
        instruction->GetDexPc(),
        instruction);

    environment->CopyFrom(ArrayRef<HInstruction* const>(*current_locals));
    instruction->SetRawEnvironment(environment);
    return environment;
  }

  bool CheckGraph() {
    GraphChecker checker(graph_);
    checker.Run();
    if (!checker.IsValid()) {
      for (const std::string& error : checker.GetErrors()) {
        std::cout << error << std::endl;
      }
      return false;
    }
    return true;
  }

  HGraph* graph_;

  HBasicBlock* entry_block_;
  HBasicBlock* return_block_;
  HBasicBlock* exit_block_;

  HInstruction* parameter_;
};

TEST_F(ClonerTest, IndividualInstrCloner) {
  HBasicBlock* header = nullptr;
  HBasicBlock* loop_body = nullptr;

  InitGraph();
  CreateBasicLoopControlFlow(entry_block_, return_block_, &header, &loop_body);
  CreateBasicLoopDataFlow(header, loop_body);
  graph_->BuildDominatorTree();
  ASSERT_TRUE(CheckGraph());

  HSuspendCheck* old_suspend_check = header->GetLoopInformation()->GetSuspendCheck();
  CloneAndReplaceInstructionVisitor visitor(graph_);
  // Do instruction cloning and replacement twice with different visiting order.

  visitor.VisitInsertionOrder();
  size_t instr_replaced_by_clones_count = visitor.GetInstrReplacedByClonesCount();
  EXPECT_EQ(instr_replaced_by_clones_count, 12u);
  EXPECT_TRUE(CheckGraph());

  visitor.VisitReversePostOrder();
  instr_replaced_by_clones_count = visitor.GetInstrReplacedByClonesCount();
  EXPECT_EQ(instr_replaced_by_clones_count, 24u);
  EXPECT_TRUE(CheckGraph());

  HSuspendCheck* new_suspend_check = header->GetLoopInformation()->GetSuspendCheck();
  EXPECT_NE(new_suspend_check, old_suspend_check);
  EXPECT_NE(new_suspend_check, nullptr);
}

// Test SuperblockCloner for loop peeling case.
//
// Control Flow of the example (ignoring critical edges splitting).
//
//       Before                    After
//
//          B                        B
//          |                        |
//          v                        v
//          1                        1
//          |                        |
//          v __                     v
//          2<  \                (6) 2A
//         / \  /                   / \
//        v   v/                   /   v
//        4   3                   /(7) 3A
//        |                      /    /
//        v                     |     v __
//        E                      \    2<  \
//                                \ / \  /
//                                 v   v/
//                                 4   3
//                                 |
//                                 v
//                                 E
TEST_F(ClonerTest, LoopPeeling) {
  HBasicBlock* header = nullptr;
  HBasicBlock* loop_body = nullptr;

  InitGraph();
  CreateBasicLoopControlFlow(entry_block_, return_block_, &header, &loop_body);
  CreateBasicLoopDataFlow(header, loop_body);
  graph_->BuildDominatorTree();
  ASSERT_TRUE(CheckGraph());

  HBasicBlockMap bb_map(
      std::less<HBasicBlock*>(), graph_->GetAllocator()->Adapter(kArenaAllocSuperblockCloner));
  HInstructionMap hir_map(
      std::less<HInstruction*>(), graph_->GetAllocator()->Adapter(kArenaAllocSuperblockCloner));

  HLoopInformation* loop_info = header->GetLoopInformation();
  HBasicBlock* new_header = DoPeelUnrollImpl(loop_info, /* unrolling */ false, &bb_map, &hir_map);
  HLoopInformation* new_loop_info = new_header->GetLoopInformation();

  ASSERT_TRUE(CheckGraph());

  // Check loop body successors.
  ASSERT_EQ(loop_body->GetSingleSuccessor(), header);
  ASSERT_EQ(bb_map.Get(loop_body)->GetSingleSuccessor(), header);

  // Check loop structure.
  ASSERT_EQ(header, new_header);
  ASSERT_EQ(new_loop_info->GetHeader(), header);
  ASSERT_EQ(new_loop_info->GetBackEdges().size(), 1u);
  ASSERT_EQ(new_loop_info->GetBackEdges()[0], loop_body);
}

// Test SuperblockCloner for loop unrolling case.
//
// Control Flow of the example (ignoring critical edges splitting).
//
//       Before                    After
//
//          B                        B
//          |                        |
//          v                        v
//          1                        1
//          |                        |
//          v __                     v  _
//          2<  \                (6) 2A< \
//         / \  /                   / \   \
//        v   v/                   /   v   \
//        4   3                   /(7) 3A   \
//        |                      /    /     /
//        v                     |     v    /
//        E                      \    2   /
//                                \ / \  /
//                                 v   v/
//                                 4   3
//                                 |
//                                 v
//                                 E
TEST_F(ClonerTest, LoopUnrolling) {
  HBasicBlock* header = nullptr;
  HBasicBlock* loop_body = nullptr;

  InitGraph();
  CreateBasicLoopControlFlow(entry_block_, return_block_, &header, &loop_body);
  CreateBasicLoopDataFlow(header, loop_body);
  graph_->BuildDominatorTree();
  ASSERT_TRUE(CheckGraph());

  HBasicBlockMap bb_map(
      std::less<HBasicBlock*>(), graph_->GetAllocator()->Adapter(kArenaAllocSuperblockCloner));
  HInstructionMap hir_map(
      std::less<HInstruction*>(), graph_->GetAllocator()->Adapter(kArenaAllocSuperblockCloner));

  HLoopInformation* loop_info = header->GetLoopInformation();
  HBasicBlock* new_header = DoPeelUnrollImpl(loop_info, /* unrolling */ true, &bb_map, &hir_map);
  HLoopInformation* new_loop_info = new_header->GetLoopInformation();

  ASSERT_TRUE(CheckGraph());

  // Check loop body successors.
  ASSERT_EQ(loop_body->GetSingleSuccessor(), bb_map.Get(header));
  ASSERT_EQ(bb_map.Get(loop_body)->GetSingleSuccessor(), header);

  // Check loop structure.
  ASSERT_EQ(bb_map.Get(header), new_header);
  ASSERT_EQ(new_loop_info->GetHeader(), new_header);
  ASSERT_EQ(new_loop_info->GetBackEdges().size(), 1u);
  ASSERT_EQ(new_loop_info->GetBackEdges()[0], loop_body);
}

// Check that loop unrolling works fine for a loop with multiply back edges. Test that after
// the transformation the loop has a single preheader.
TEST_F(ClonerTest, LoopPeelingMultipleBackEdges) {
  HBasicBlock* header = nullptr;
  HBasicBlock* loop_body = nullptr;

  InitGraph();
  CreateBasicLoopControlFlow(entry_block_, return_block_, &header, &loop_body);
  CreateBasicLoopDataFlow(header, loop_body);

  // Transform a basic loop to have multiple back edges.
  HBasicBlock* latch = header->GetSuccessors()[1];
  HBasicBlock* if_block = new (GetAllocator()) HBasicBlock(graph_);
  HBasicBlock* temp1 = new (GetAllocator()) HBasicBlock(graph_);
  graph_->AddBlock(if_block);
  graph_->AddBlock(temp1);
  header->ReplaceSuccessor(latch, if_block);
  if_block->AddSuccessor(latch);
  if_block->AddSuccessor(temp1);
  temp1->AddSuccessor(header);

  if_block->AddInstruction(new (GetAllocator()) HIf(parameter_));

  HInstructionIterator it(header->GetPhis());
  DCHECK(!it.Done());
  HPhi* loop_phi = it.Current()->AsPhi();
  HInstruction* temp_add = new (GetAllocator()) HAdd(DataType::Type::kInt32,
                                                     loop_phi,
                                                     graph_->GetIntConstant(2));
  temp1->AddInstruction(temp_add);
  temp1->AddInstruction(new (GetAllocator()) HGoto());
  loop_phi->AddInput(temp_add);

  graph_->BuildDominatorTree();
  ASSERT_TRUE(CheckGraph());

  HBasicBlockMap bb_map(
      std::less<HBasicBlock*>(), graph_->GetAllocator()->Adapter(kArenaAllocSuperblockCloner));
  HInstructionMap hir_map(
      std::less<HInstruction*>(), graph_->GetAllocator()->Adapter(kArenaAllocSuperblockCloner));

  HLoopInformation* loop_info = header->GetLoopInformation();
  DoPeelUnrollImpl(loop_info, /* unrolling */ false, &bb_map, &hir_map);

  ASSERT_TRUE(CheckGraph());
  ASSERT_EQ(header->GetPredecessors().size(), 3u);
}

static void CheckLoopStructureForLoopPeelingNested(HBasicBlock* loop1_header,
                                                   HBasicBlock* loop2_header,
                                                   HBasicBlock* loop3_header) {
  ASSERT_EQ(loop1_header->GetLoopInformation()->GetHeader(), loop1_header);
  ASSERT_EQ(loop2_header->GetLoopInformation()->GetHeader(), loop2_header);
  ASSERT_EQ(loop3_header->GetLoopInformation()->GetHeader(), loop3_header);
  ASSERT_EQ(loop1_header->GetLoopInformation()->GetPreHeader()->GetLoopInformation(), nullptr);
  ASSERT_EQ(loop2_header->GetLoopInformation()->GetPreHeader()->GetLoopInformation(), nullptr);
  ASSERT_EQ(loop3_header->GetLoopInformation()->GetPreHeader()->GetLoopInformation()->GetHeader(),
            loop2_header);
}

TEST_F(ClonerTest, LoopPeelingNested) {
  HBasicBlock* header = nullptr;
  HBasicBlock* loop_body = nullptr;

  InitGraph();

  // Create the following nested structure of loops
  //   Headers:  1   2 3
  //             [ ] [ [ ] ]
  CreateBasicLoopControlFlow(entry_block_, return_block_, &header, &loop_body);
  CreateBasicLoopDataFlow(header, loop_body);
  HBasicBlock* loop1_header = header;

  CreateBasicLoopControlFlow(header, return_block_, &header, &loop_body);
  CreateBasicLoopDataFlow(header, loop_body);
  HBasicBlock* loop2_header = header;

  CreateBasicLoopControlFlow(header, header->GetSuccessors()[1], &header, &loop_body);
  CreateBasicLoopDataFlow(header, loop_body);
  HBasicBlock* loop3_header = header;

  graph_->BuildDominatorTree();
  ASSERT_TRUE(CheckGraph());

  // Check nested loops structure.
  CheckLoopStructureForLoopPeelingNested(loop1_header, loop2_header, loop3_header);
  PeelUnrollLoop(loop1_header->GetLoopInformation(), /* do_unroll */ false);
  // Check that nested loops structure has not changed after the transformation.
  CheckLoopStructureForLoopPeelingNested(loop1_header, loop2_header, loop3_header);

  ASSERT_TRUE(CheckGraph());
}

// Check that the loop population is correctly propagated after an inner loop is peeled.
TEST_F(ClonerTest, OuterLoopPopulationAfterInnerPeeled) {
  HBasicBlock* header = nullptr;
  HBasicBlock* loop_body = nullptr;

  InitGraph();

  // Create the following nested structure of loops
  //   Headers:  1 2 3       4
  //             [ [ [ ] ] ] [ ]
  CreateBasicLoopControlFlow(entry_block_, return_block_, &header, &loop_body);
  CreateBasicLoopDataFlow(header, loop_body);
  HBasicBlock* loop1_header = header;

  CreateBasicLoopControlFlow(header, header->GetSuccessors()[1], &header, &loop_body);
  CreateBasicLoopDataFlow(header, loop_body);
  HBasicBlock* loop2_header = header;

  CreateBasicLoopControlFlow(header, header->GetSuccessors()[1], &header, &loop_body);
  CreateBasicLoopDataFlow(header, loop_body);
  HBasicBlock* loop3_header = header;

  CreateBasicLoopControlFlow(loop1_header, return_block_, &header, &loop_body);
  CreateBasicLoopDataFlow(header, loop_body);
  HBasicBlock* loop4_header = header;

  graph_->BuildDominatorTree();
  ASSERT_TRUE(CheckGraph());

  HLoopInformation* loop4 = header->GetLoopInformation();

  PeelUnrollLoop(loop3_header->GetLoopInformation(), /* do_unroll */ false);

  HLoopInformation* loop1 = loop1_header->GetLoopInformation();

  ASSERT_TRUE(loop1->Contains(*loop2_header));
  ASSERT_TRUE(loop1->Contains(*loop3_header));
  ASSERT_TRUE(loop1->Contains(*loop3_header->GetLoopInformation()->GetPreHeader()));

  // Check that loop4 info has not been touched after local run of AnalyzeLoops.
  ASSERT_EQ(loop4, loop4_header->GetLoopInformation());

  ASSERT_TRUE(CheckGraph());
}

// Check the case when inner loop have an exit not to its immediate outer_loop but some other loop
// in the hierarchy. Loop population information must be valid after loop peeling.
TEST_F(ClonerTest, NestedCaseExitToOutermost) {
  HBasicBlock* header = nullptr;
  HBasicBlock* loop_body = nullptr;

  InitGraph();

  // Create the following nested structure of loops then peel loop3.
  //   Headers:  1 2 3
  //             [ [ [ ] ] ]
  CreateBasicLoopControlFlow(entry_block_, return_block_, &header, &loop_body);
  CreateBasicLoopDataFlow(header, loop_body);
  HBasicBlock* loop1_header = header;
  HBasicBlock* loop_body1 = loop_body;

  CreateBasicLoopControlFlow(header, header->GetSuccessors()[1], &header, &loop_body);
  CreateBasicLoopDataFlow(header, loop_body);

  CreateBasicLoopControlFlow(header, header->GetSuccessors()[1], &header, &loop_body);
  CreateBasicLoopDataFlow(header, loop_body);
  HBasicBlock* loop3_header = header;
  HBasicBlock* loop_body3 = loop_body;

  // Change the loop3 - insert an exit which leads to loop1.
  HBasicBlock* loop3_extra_if_block = new (GetAllocator()) HBasicBlock(graph_);
  graph_->AddBlock(loop3_extra_if_block);
  loop3_extra_if_block->AddInstruction(new (GetAllocator()) HIf(parameter_));

  loop3_header->ReplaceSuccessor(loop_body3, loop3_extra_if_block);
  loop3_extra_if_block->AddSuccessor(loop_body1);  // Long exit.
  loop3_extra_if_block->AddSuccessor(loop_body3);

  graph_->BuildDominatorTree();
  ASSERT_TRUE(CheckGraph());

  HBasicBlock* loop3_long_exit = loop3_extra_if_block->GetSuccessors()[0];
  ASSERT_TRUE(loop1_header->GetLoopInformation()->Contains(*loop3_long_exit));

  PeelUnrollLoop(loop3_header->GetLoopInformation(), /* do_unroll */ false);

  HLoopInformation* loop1 = loop1_header->GetLoopInformation();
  // Check that after the transformation the local area for CF adjustments has been chosen
  // correctly and loop population has been updated.
  loop3_long_exit = loop3_extra_if_block->GetSuccessors()[0];
  ASSERT_TRUE(loop1->Contains(*loop3_long_exit));

  ASSERT_TRUE(loop1->Contains(*loop3_header));
  ASSERT_TRUE(loop1->Contains(*loop3_header->GetLoopInformation()->GetPreHeader()));

  ASSERT_TRUE(CheckGraph());
}


TEST_F(ClonerTest, FastCaseCheck) {
  HBasicBlock* header = nullptr;
  HBasicBlock* loop_body = nullptr;
  ArenaAllocator* arena = graph_->GetAllocator();

  InitGraph();
  CreateBasicLoopControlFlow(entry_block_, return_block_, &header, &loop_body);
  CreateBasicLoopDataFlow(header, loop_body);
  graph_->BuildDominatorTree();

  HLoopInformation* loop_info = header->GetLoopInformation();

  ArenaBitVector orig_bb_set(
      arena, graph_->GetBlocks().size(), false, kArenaAllocSuperblockCloner);
  orig_bb_set.Union(&loop_info->GetBlocks());

  HEdgeSet remap_orig_internal(graph_->GetAllocator()->Adapter(kArenaAllocSuperblockCloner));
  HEdgeSet remap_copy_internal(graph_->GetAllocator()->Adapter(kArenaAllocSuperblockCloner));
  HEdgeSet remap_incoming(graph_->GetAllocator()->Adapter(kArenaAllocSuperblockCloner));

  CollectRemappingInfoForPeelUnroll(true,
                                    loop_info,
                                    &remap_orig_internal,
                                    &remap_copy_internal,
                                    &remap_incoming);

  // Insert some extra nodes and edges.
  HBasicBlock* preheader = loop_info->GetPreHeader();
  orig_bb_set.SetBit(preheader->GetBlockId());

  // Adjust incoming edges.
  remap_incoming.Clear();
  remap_incoming.Insert(HEdge(preheader->GetSinglePredecessor(), preheader));

  HBasicBlockMap bb_map(std::less<HBasicBlock*>(), arena->Adapter(kArenaAllocSuperblockCloner));
  HInstructionMap hir_map(std::less<HInstruction*>(), arena->Adapter(kArenaAllocSuperblockCloner));

  SuperblockCloner cloner(graph_,
                          &orig_bb_set,
                          &bb_map,
                          &hir_map);
  cloner.SetSuccessorRemappingInfo(&remap_orig_internal, &remap_copy_internal, &remap_incoming);

  ASSERT_FALSE(cloner.IsFastCase());
}

}  // namespace art
