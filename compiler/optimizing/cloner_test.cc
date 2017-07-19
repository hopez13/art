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


#include "gtest/gtest.h"

namespace art {

class ClonerTest : public CommonCompilerTest {
 public:
  ClonerTest() : pool_(), allocator_(&pool_) {
    graph_ = CreateGraph(&allocator_);
  }

  void CreateBasicLoopControlFlow(HBasicBlock **header_p, HBasicBlock** body_p) {
    entry_block_ = new (&allocator_) HBasicBlock(graph_);
    graph_->AddBlock(entry_block_);
    graph_->SetEntryBlock(entry_block_);

    HBasicBlock* loop_preheader = new (&allocator_) HBasicBlock(graph_);
    HBasicBlock* loop_header = new (&allocator_) HBasicBlock(graph_);
    HBasicBlock* loop_body = new (&allocator_) HBasicBlock(graph_);
    HBasicBlock* loop_exit = new (&allocator_) HBasicBlock(graph_);

    graph_->AddBlock(loop_preheader);
    graph_->AddBlock(loop_header);
    graph_->AddBlock(loop_body);
    graph_->AddBlock(loop_exit);

    exit_block_ = new (&allocator_) HBasicBlock(graph_);
    graph_->AddBlock(exit_block_);
    graph_->SetExitBlock(exit_block_);

    entry_block_->AddSuccessor(loop_preheader);
    loop_preheader->AddSuccessor(loop_header);
    // Loop exit first to have a proper exit condition/target for HIf.
    loop_header->AddSuccessor(loop_exit);
    loop_header->AddSuccessor(loop_body);
    loop_body->AddSuccessor(loop_header);
    loop_exit->AddSuccessor(exit_block_);

    *header_p = loop_header;
    *body_p = loop_body;

    parameter_ = new (&allocator_) HParameterValue(graph_->GetDexFile(),
                                                   dex::TypeIndex(0),
                                                   0,
                                                   Primitive::kPrimInt);
    entry_block_->AddInstruction(parameter_);
    loop_exit->AddInstruction(new (&allocator_) HReturnVoid());
    exit_block_->AddInstruction(new (&allocator_) HExit());
  }

  void CreateBasicLoopDataFlow(HBasicBlock *loop_header, HBasicBlock* loop_body) {
    uint32_t dex_pc = 0;

    // Entry block.
    HIntConstant* const_0 = graph_->GetIntConstant(0);
    HIntConstant* const_1 = graph_->GetIntConstant(1);
    HIntConstant* const_128 = graph_->GetIntConstant(128);

    // Header block.
    HPhi* phi = new (&allocator_) HPhi(&allocator_, 0, 0, Primitive::kPrimInt);
    HInstruction* suspend_check = new (&allocator_) HSuspendCheck();

    loop_header->AddPhi(phi);
    loop_header->AddInstruction(suspend_check);
    loop_header->AddInstruction(new (&allocator_) HGreaterThanOrEqual(phi, const_128));
    loop_header->AddInstruction(new (&allocator_) HIf(parameter_));

    // Loop body block.
    HInstruction* null_check = new (&allocator_) HNullCheck(parameter_, dex_pc);
    HInstruction* array_length = new (&allocator_) HArrayLength(null_check, dex_pc);
    HInstruction* bounds_check = new (&allocator_) HBoundsCheck(phi, array_length, dex_pc);
    HInstruction* array_get =
        new (&allocator_) HArrayGet(null_check, bounds_check, Primitive::kPrimInt, dex_pc);
    HInstruction* add =  new (&allocator_) HAdd(Primitive::kPrimInt, array_get, const_1);
    HInstruction* array_set =
        new (&allocator_) HArraySet(null_check, bounds_check, add, Primitive::kPrimInt, dex_pc);
    HInstruction* induction_inc = new (&allocator_) HAdd(Primitive::kPrimInt, phi, const_1);

    loop_body->AddInstruction(null_check);
    loop_body->AddInstruction(array_length);
    loop_body->AddInstruction(bounds_check);
    loop_body->AddInstruction(array_get);
    loop_body->AddInstruction(add);
    loop_body->AddInstruction(array_set);
    loop_body->AddInstruction(induction_inc);
    loop_body->AddInstruction(new (&allocator_) HGoto());

    phi->AddInput(const_0);
    phi->AddInput(induction_inc);

    graph_->SetHasBoundsChecks(true);

    // Adjust HEnvironment for all instruction which require that.
    ArenaVector<HInstruction*> current_locals(allocator_.Adapter(kArenaAllocInstruction));
    current_locals.push_back(phi);
    current_locals.push_back(const_128);
    current_locals.push_back(parameter_);

    HEnvironment* env = ManuallyBuildEnvFor(suspend_check, &current_locals);
    null_check->CopyEnvironmentFrom(env);
    bounds_check->CopyEnvironmentFrom(env);
  }

  HEnvironment* ManuallyBuildEnvFor(HInstruction* instruction,
                                    ArenaVector<HInstruction*>* current_locals) {
    HEnvironment* environment = new (&allocator_) HEnvironment(
        (&allocator_),
        current_locals->size(),
        graph_->GetArtMethod(),
        instruction->GetDexPc(),
        instruction);

    environment->CopyFrom(*current_locals);
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

  ArenaPool pool_;
  ArenaAllocator allocator_;
  HGraph* graph_;

  HBasicBlock* entry_block_;
  HBasicBlock* exit_block_;

  HInstruction* parameter_;
};

TEST_F(ClonerTest, IndividualInstrCloner) {
  HBasicBlock* header = nullptr;
  HBasicBlock* loop_body = nullptr;

  CreateBasicLoopControlFlow(&header, &loop_body);
  CreateBasicLoopDataFlow(header, loop_body);
  graph_->BuildDominatorTree();
  ASSERT_TRUE(CheckGraph());

  int instr_replaced_by_clones_count = 0;
  for (HBasicBlock* block : graph_->GetReversePostOrder()) {
    for (HInstructionIterator it(block->GetPhis()); !it.Done(); ) {
      HInstruction* instr = it.Current();
      it.Advance();
      if (instr->IsClonable()) {
        ReplaceInstrOrPhiByClone(instr);
        instr_replaced_by_clones_count++;
      }
    }
    for (HInstructionIterator it(block->GetInstructions()); !it.Done(); ) {
      HInstruction* instr = it.Current();
      it.Advance();
      if (instr->IsClonable()) {
        ReplaceInstrOrPhiByClone(instr);
        instr_replaced_by_clones_count++;
      }
    }
  }

  ASSERT_EQ(instr_replaced_by_clones_count, 12);
  ASSERT_TRUE(CheckGraph());
}

}  // namespace art
