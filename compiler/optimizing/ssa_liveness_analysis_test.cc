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

#include "ssa_liveness_analysis.h"

#include "arch/instruction_set.h"
#include "arch/instruction_set_features.h"
#include "base/arena_allocator.h"
#include "base/arena_containers.h"
#include "code_generator.h"
#include "driver/compiler_options.h"
#include "nodes.h"
#include "optimizing_unit_test.h"

namespace art {

class SsaLivenessAnalysisTest : public OptimizingUnitTest {
 protected:
  void SetUp() override {
    OptimizingUnitTest::SetUp();
    graph_ = CreateGraph();
    compiler_options_ = CommonCompilerTest::CreateCompilerOptions(kRuntimeISA, "default");
    codegen_ = CodeGenerator::Create(graph_, *compiler_options_);
    CHECK(codegen_ != nullptr);
    // Create entry block.
    entry_ = new (GetAllocator()) HBasicBlock(graph_);
    graph_->AddBlock(entry_);
    graph_->SetEntryBlock(entry_);
  }

 protected:
  HBasicBlock* CreateSuccessor(HBasicBlock* block) {
    HGraph* graph = block->GetGraph();
    HBasicBlock* successor = new (GetAllocator()) HBasicBlock(graph);
    graph->AddBlock(successor);
    block->AddSuccessor(successor);
    return successor;
  }

  HGraph* graph_;
  std::unique_ptr<CompilerOptions> compiler_options_;
  std::unique_ptr<CodeGenerator> codegen_;
  HBasicBlock* entry_;
};

TEST_F(SsaLivenessAnalysisTest, TestReturnArg) {
  HInstruction* arg = new (GetAllocator()) HParameterValue(
      graph_->GetDexFile(), dex::TypeIndex(0), 0, DataType::Type::kInt32);
  entry_->AddInstruction(arg);

  HBasicBlock* block = CreateSuccessor(entry_);
  HInstruction* ret = new (GetAllocator()) HReturn(arg);
  block->AddInstruction(ret);
  block->AddInstruction(new (GetAllocator()) HExit());

  graph_->BuildDominatorTree();
  SsaLivenessAnalysis ssa_analysis(graph_, codegen_.get(), GetScopedAllocator());
  ssa_analysis.Analyze();

  std::ostringstream arg_dump;
  arg->GetLiveInterval()->Dump(arg_dump);
  EXPECT_STREQ("ranges: { [2,6) }, uses: { 6 }, { } is_fixed: 0, is_split: 0 is_low: 0 is_high: 0",
               arg_dump.str().c_str());
}

TEST_F(SsaLivenessAnalysisTest, TestAput) {
  HInstruction* array = new (GetAllocator()) HParameterValue(
      graph_->GetDexFile(), dex::TypeIndex(0), 0, DataType::Type::kReference);
  HInstruction* index = new (GetAllocator()) HParameterValue(
      graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kInt32);
  HInstruction* value = new (GetAllocator()) HParameterValue(
      graph_->GetDexFile(), dex::TypeIndex(2), 2, DataType::Type::kInt32);
  HInstruction* extra_arg1 = new (GetAllocator()) HParameterValue(
      graph_->GetDexFile(), dex::TypeIndex(3), 3, DataType::Type::kInt32);
  HInstruction* extra_arg2 = new (GetAllocator()) HParameterValue(
      graph_->GetDexFile(), dex::TypeIndex(4), 4, DataType::Type::kReference);
  HInstruction* const args[] = { array, index, value, extra_arg1, extra_arg2 };
  for (HInstruction* insn : args) {
    entry_->AddInstruction(insn);
  }

  HBasicBlock* block = CreateSuccessor(entry_);
  HInstruction* null_check = new (GetAllocator()) HNullCheck(array, 0);
  block->AddInstruction(null_check);
  HEnvironment* null_check_env = new (GetAllocator()) HEnvironment(GetAllocator(),
                                                                   /* number_of_vregs= */ 5,
                                                                   /* method= */ nullptr,
                                                                   /* dex_pc= */ 0u,
                                                                   null_check);
  null_check_env->CopyFrom(ArrayRef<HInstruction* const>(args));
  null_check->SetRawEnvironment(null_check_env);
  HInstruction* length = new (GetAllocator()) HArrayLength(array, 0);
  block->AddInstruction(length);
  HInstruction* bounds_check = new (GetAllocator()) HBoundsCheck(index, length, /* dex_pc= */ 0u);
  block->AddInstruction(bounds_check);
  HEnvironment* bounds_check_env = new (GetAllocator()) HEnvironment(GetAllocator(),
                                                                     /* number_of_vregs= */ 5,
                                                                     /* method= */ nullptr,
                                                                     /* dex_pc= */ 0u,
                                                                     bounds_check);
  bounds_check_env->CopyFrom(ArrayRef<HInstruction* const>(args));
  bounds_check->SetRawEnvironment(bounds_check_env);
  HInstruction* array_set =
      new (GetAllocator()) HArraySet(array, index, value, DataType::Type::kInt32, /* dex_pc= */ 0);
  block->AddInstruction(array_set);

  graph_->BuildDominatorTree();
  SsaLivenessAnalysis ssa_analysis(graph_, codegen_.get(), GetScopedAllocator());
  ssa_analysis.Analyze();

  EXPECT_FALSE(graph_->IsDebuggable());
  EXPECT_EQ(18u, bounds_check->GetLifetimePosition());
  static const char* const expected[] = {
      "ranges: { [2,21) }, uses: { 15 17 21 }, { 15 19 } is_fixed: 0, is_split: 0 is_low: 0 "
          "is_high: 0",
      "ranges: { [4,21) }, uses: { 19 21 }, { } is_fixed: 0, is_split: 0 is_low: 0 "
          "is_high: 0",
      "ranges: { [6,21) }, uses: { 21 }, { } is_fixed: 0, is_split: 0 is_low: 0 "
          "is_high: 0",
      // Environment uses do not keep the non-reference argument alive.
      "ranges: { [8,10) }, uses: { }, { } is_fixed: 0, is_split: 0 is_low: 0 is_high: 0",
      // Environment uses keep the reference argument alive.
      "ranges: { [10,19) }, uses: { }, { 15 19 } is_fixed: 0, is_split: 0 is_low: 0 is_high: 0",
  };
  static_assert(arraysize(expected) == arraysize(args), "Array size check.");
  size_t arg_index = 0u;
  for (HInstruction* arg : args) {
    std::ostringstream arg_dump;
    arg->GetLiveInterval()->Dump(arg_dump);
    EXPECT_STREQ(expected[arg_index], arg_dump.str().c_str()) << arg_index;
    ++arg_index;
  }
}

TEST_F(SsaLivenessAnalysisTest, TestDeoptimize) {
  HInstruction* array = new (GetAllocator()) HParameterValue(
      graph_->GetDexFile(), dex::TypeIndex(0), 0, DataType::Type::kReference);
  HInstruction* index = new (GetAllocator()) HParameterValue(
      graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kInt32);
  HInstruction* value = new (GetAllocator()) HParameterValue(
      graph_->GetDexFile(), dex::TypeIndex(2), 2, DataType::Type::kInt32);
  HInstruction* extra_arg1 = new (GetAllocator()) HParameterValue(
      graph_->GetDexFile(), dex::TypeIndex(3), 3, DataType::Type::kInt32);
  HInstruction* extra_arg2 = new (GetAllocator()) HParameterValue(
      graph_->GetDexFile(), dex::TypeIndex(4), 4, DataType::Type::kReference);
  HInstruction* const args[] = { array, index, value, extra_arg1, extra_arg2 };
  for (HInstruction* insn : args) {
    entry_->AddInstruction(insn);
  }

  HBasicBlock* block = CreateSuccessor(entry_);
  HInstruction* null_check = new (GetAllocator()) HNullCheck(array, 0);
  block->AddInstruction(null_check);
  HEnvironment* null_check_env = new (GetAllocator()) HEnvironment(GetAllocator(),
                                                                   /* number_of_vregs= */ 5,
                                                                   /* method= */ nullptr,
                                                                   /* dex_pc= */ 0u,
                                                                   null_check);
  null_check_env->CopyFrom(ArrayRef<HInstruction* const>(args));
  null_check->SetRawEnvironment(null_check_env);
  HInstruction* length = new (GetAllocator()) HArrayLength(array, 0);
  block->AddInstruction(length);
  // Use HAboveOrEqual+HDeoptimize as the bounds check.
  HInstruction* ae = new (GetAllocator()) HAboveOrEqual(index, length);
  block->AddInstruction(ae);
  HInstruction* deoptimize = new(GetAllocator()) HDeoptimize(
      GetAllocator(), ae, DeoptimizationKind::kBlockBCE, /* dex_pc= */ 0u);
  block->AddInstruction(deoptimize);
  HEnvironment* deoptimize_env = new (GetAllocator()) HEnvironment(GetAllocator(),
                                                                   /* number_of_vregs= */ 5,
                                                                   /* method= */ nullptr,
                                                                   /* dex_pc= */ 0u,
                                                                   deoptimize);
  deoptimize_env->CopyFrom(ArrayRef<HInstruction* const>(args));
  deoptimize->SetRawEnvironment(deoptimize_env);
  HInstruction* array_set =
      new (GetAllocator()) HArraySet(array, index, value, DataType::Type::kInt32, /* dex_pc= */ 0);
  block->AddInstruction(array_set);

  graph_->BuildDominatorTree();
  SsaLivenessAnalysis ssa_analysis(graph_, codegen_.get(), GetScopedAllocator());
  ssa_analysis.Analyze();

  EXPECT_FALSE(graph_->IsDebuggable());
  EXPECT_EQ(20u, deoptimize->GetLifetimePosition());
  static const char* const expected[] = {
      "ranges: { [2,23) }, uses: { 15 17 23 }, { 15 21 } is_fixed: 0, is_split: 0 is_low: 0 "
          "is_high: 0",
      "ranges: { [4,23) }, uses: { 19 23 }, { 21 } is_fixed: 0, is_split: 0 is_low: 0 "
          "is_high: 0",
      "ranges: { [6,23) }, uses: { 23 }, { 21 } is_fixed: 0, is_split: 0 is_low: 0 is_high: 0",
      // Environment use in HDeoptimize keeps even the non-reference argument alive.
      "ranges: { [8,21) }, uses: { }, { 21 } is_fixed: 0, is_split: 0 is_low: 0 is_high: 0",
      // Environment uses keep the reference argument alive.
      "ranges: { [10,21) }, uses: { }, { 15 21 } is_fixed: 0, is_split: 0 is_low: 0 is_high: 0",
  };
  static_assert(arraysize(expected) == arraysize(args), "Array size check.");
  size_t arg_index = 0u;
  for (HInstruction* arg : args) {
    std::ostringstream arg_dump;
    arg->GetLiveInterval()->Dump(arg_dump);
    EXPECT_STREQ(expected[arg_index], arg_dump.str().c_str()) << arg_index;
    ++arg_index;
  }
}

#if defined(ART_ENABLE_CODEGEN_arm64)
// Test that instructions with multiple outputs are given multiple live intervals.
TEST_F(SsaLivenessAnalysisTest, TestMultipleOutputs) {
  HInstruction* array = new (GetAllocator()) HParameterValue(
      graph_->GetDexFile(), dex::TypeIndex(0), 0, DataType::Type::kReference);
  HInstruction* index = new (GetAllocator()) HParameterValue(
      graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kInt32);
  HInstruction* const0 = graph_->GetIntConstant(0);
  HInstruction* const1 = graph_->GetIntConstant(1);
  HInstruction* const entry[] = { array, index };
  for (HInstruction* insn : entry) {
    entry_->AddInstruction(insn);
  }

  HBasicBlock* block = CreateSuccessor(entry_);
  HInstruction* ldp = new (GetAllocator()) HArmLoadPair(
      array, index, DataType::Type::kInt32, 0);
  block->AddInstruction(ldp);
  HInstruction* projection0 = new (GetAllocator()) HProjectionNode(
      ldp, const0, DataType::Type::kInt32, 0);
  block->AddInstruction(projection0);
  HInstruction* projection1 = new (GetAllocator()) HProjectionNode(
      ldp, const1, DataType::Type::kInt32, 0);
  block->AddInstruction(projection1);
  HInstruction* const instructions[] = { ldp, projection0, projection1 };

  graph_->BuildDominatorTree();
  SsaLivenessAnalysis ssa_analysis(graph_, codegen_.get(), GetScopedAllocator());
  ssa_analysis.Analyze();

  EXPECT_FALSE(graph_->IsDebuggable());
  EXPECT_EQ(12u, ldp->GetLifetimePosition());
  static const char* const expected[] = {
      "ranges: { [12,14) }, uses: { 14 }, { } is_fixed: 0, is_split: 0 is_low: 0 is_high: 0",
      "ranges: { [12,16) }, uses: { 16 }, { } is_fixed: 0, is_split: 0 is_low: 0 is_high: 0",
      "ranges: { [14,16) }, uses: { }, { } is_fixed: 0, is_split: 0 is_low: 0 is_high: 0",
      "ranges: { [16,18) }, uses: { }, { } is_fixed: 0, is_split: 0 is_low: 0 is_high: 0"
  };
  size_t instruction_index = 0u;
  for (HInstruction* instruction : instructions) {
    for (size_t out_index = 0; out_index < instruction->OutputCount(); out_index++) {
      std::ostringstream instruction_dump;
      instruction->GetLiveInterval(out_index)->Dump(instruction_dump);
      EXPECT_STREQ(expected[instruction_index], instruction_dump.str().c_str())
          << instruction_index;
      ++instruction_index;
    }
  }
}
#endif  // defined(ART_ENABLE_CODEGEN_arm64)

}  // namespace art
