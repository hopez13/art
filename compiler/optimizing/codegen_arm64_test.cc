/*
 * Copyright (C) 2024 The Android Open Source Project
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

#include <regex>

#include "arch/arm64/instruction_set_features_arm64.h"
#include "codegen_test_utils.h"
#include "disassembler.h"
#include "optimizing_unit_test.h"

#include "gtest/gtest.h"

namespace art HIDDEN {
namespace arm64 {

#define __ GetVIXLAssembler()->

// Special ARM64 code generator for codegen testing in a limited code
// generation environment (i.e. with no runtime support).
//
// To provide the ability to test save/restore logic in the SuspendCheck's
// slowpath we do following things:
// 1. Reserve callee-saved register (in addition to runtime reserved ones)
//    that is used to save and restore value of the thread register (TR).
// 2. Override GenerateFrameEntry and GenerateFrameExit to setup TR to the
//    fake thread structure that has flags with ThreadFlag::kSuspendRequest
//    bit set. So we also go to the slowpath in tests.
// 3. Override InvokeRuntime to prevent generation of the runtime call.
//    So in the SuspendCheck's slowpath we only save and restore registers that
//    is enough for tests.
class TestCodeGeneratorARM64 : public CodeGeneratorARM64 {
 public:
  TestCodeGeneratorARM64(HGraph* graph, const CompilerOptions& compiler_options)
      : CodeGeneratorARM64(graph, compiler_options),
        saved_tr_(TestCodeGeneratorARM64::GetCalleeSavedRegister()) {
    AddAllocatedRegister(helpers::LocationFrom(saved_tr_));
  }

  void InvokeRuntime([[maybe_unused]] QuickEntrypointEnum entrypoint,
                     [[maybe_unused]] HInstruction* instruction,
                     [[maybe_unused]] uint32_t dex_pc,
                     [[maybe_unused]] SlowPathCode* slow_path = nullptr) override {}

  void GenerateFrameEntry() override {
    CodeGeneratorARM64::GenerateFrameEntry();
    __ Mov(saved_tr_, arm64::tr);
    __ Mov(arm64::tr, reinterpret_cast<uintptr_t>(&thread_));
  }

  void GenerateFrameExit() override {
    __ Mov(arm64::tr, saved_tr_);
    CodeGeneratorARM64::GenerateFrameExit();
  }

  void SetupBlockedRegisters() const override {
    CodeGeneratorARM64::SetupBlockedRegisters();
    blocked_core_registers_[saved_tr_.GetCode()] = true;
  }

  // Disable marking register check because it expects the Thread Register (TR) and
  // the Marking Register (MR) to be set to meaningful values. This is not the case
  // in codegen testing (we setup TR later), so just disable them entirely here as
  // it doesn't have any influence on the testing itself.
  void MaybeGenerateMarkingRegisterCheck([[maybe_unused]] int code,
                                         [[maybe_unused]] Location temp_loc) override {}

 private:
  static vixl::aarch64::CPURegister GetCalleeSavedRegister() {
    vixl::aarch64::CPURegList callee_saved_registers = vixl::aarch64::CPURegList::GetCalleeSaved();
    callee_saved_registers.Remove(arm64::runtime_reserved_core_registers);
    CHECK(!callee_saved_registers.IsEmpty()) << "All callee-saved registers are reserved";
    return callee_saved_registers.PopLowestIndex();
  }

  // Fake thread structure is used for tests with SuspendCheck instruction.
  struct FakeThread {
    char padding[Thread::ThreadFlagsOffset<kArm64PointerSize>().SizeValue()];
    uint32_t flags{enum_cast<uint32_t>(ThreadFlag::kSuspendRequest)};
  };

  vixl::aarch64::Register saved_tr_;
  FakeThread thread_;

  ART_FRIEND_TEST(CodegenArm64Test, FrameSizeSIMD);
  ART_FRIEND_TEST(CodegenArm64Test, FrameSizeNoSIMD);
};

class CodegenArm64Test : public OptimizingUnitTest {
 protected:
  void SetUp() override {
    OptimizingUnitTest::SetUp();
    InitGraph();

    std::string features;
    if (kArm64AllowSVE) {
      // If runtime isa is not Arm64 we can enable sve2 feature because in this case simulator
      // is used to execute generated code. Otherwise we need to check runtime capabilities.
      features = "sve2";
      if (kRuntimeISA == InstructionSet::kArm64) {
        Arm64FeaturesUniquePtr runtime_features = Arm64InstructionSetFeatures::FromHwcap();
        if (!runtime_features->HasSVE2()) {
          features = "";
        }
      }
    }

    compiler_options_ =
      CommonCompilerTest::CreateCompilerOptions(InstructionSet::kArm64, "default", features);
    DCHECK(compiler_options_ != nullptr);

    codegen_ = std::unique_ptr<TestCodeGeneratorARM64>(
        new (GetAllocator()) TestCodeGeneratorARM64(graph_, *compiler_options_));
    DCHECK(codegen_.get() != nullptr);

    disasm_info_ = std::make_unique<DisassemblyInformation>(GetAllocator());
    codegen_->SetDisassemblyInformation(disasm_info_.get());
  }

  void TearDown() override {
    codegen_.reset();
    compiler_options_.reset();
    disasm_info_.reset();
    graph_ = nullptr;
    ResetPoolAndAllocator();
    OptimizingUnitTest::TearDown();
  }

  // Constructs a minimal graph with a main body to add instructions into.
  void InitGraph() {
    OptimizingUnitTestHelper::InitGraph();
    main_block_ = AddNewBlock();
    main_block_->InsertBetween(entry_block_, return_block_);
  }

  // Add an instruction into the main block.
  void AddInstruction(HInstruction* instruction) {
    main_block_->AddInstruction(instruction);
  }

  // Create an instruction.
  template <typename T, typename ...Args>
  T* MakeInstruction(Args&& ...args) {
    return new (GetAllocator()) T(GetAllocator(), std::forward<Args>(args)...);
  }

  // Create and add an instruction into the main block.
  template <typename T, typename ...Args>
  T* MakeAndAddInstruction(Args&& ...args) {
    T *node = MakeInstruction<T>(std::forward<Args>(args)...);
    AddInstruction(node);
    return node;
  }

  // Create and add a predicated instruction into the main block.
  template <typename T, typename ...Args>
  T* MakeAndAddPredicatedInstruction(HVecPredSetOperation *predicate, Args&& ...args) {
    T *node = MakeAndAddInstruction<T>(std::forward<Args>(args)...);
    node->SetMergingGoverningPredicate(predicate);
    return node;
  }

  // Replace return instruction in the return block.
  void ReplaceReturnInstruction(HReturn *instruction) {
    ASSERT_TRUE(return_block_->IsSingleReturnOrReturnVoidAllowingPhis());
    return_block_->RemoveInstruction(return_block_->GetLastInstruction());
    return_block_->AddInstruction(instruction);
  }

  // Finalise, validate and run the code.
  void TestCode(bool has_result = false, int32_t expected = 0) {
    graph_->BuildDominatorTree();
    if (CanExecute(InstructionSet::kArm64)) {
      RunCode(codegen_.get(), graph_, [](HGraph*) {}, has_result, expected);
    }
  }

  void DumpGraph(std::ostream& os) {
    HGraphVisualizer visualizer(&os, graph_, codegen_.get());
    visualizer.DumpGraphWithDisassembly();
  }

  void TestParallelMove(const std::function<void(HParallelMove*)>& init_move,
                        const std::string& expected_pattern);

  // Make following instructions for the following pattern:
  //
  //   VecReplicateScalar v0, initial_value
  //   VecReplicateScalar v1, initial_value + 1
  //   ...
  //   VecReplicateScalar v<N - 1>, initial_value + N - 1
  //   VecAdd a0, v0, v1
  //   VecAdd a1, a0, v2
  //   ...
  //   VecAdd a<N - 1>, v<N - 2>, v<N>
  //   VecReduce r0, a<N - 1>
  //   VecExtractScalar r1, r0
  //
  // where:
  //   N = number_of_add_operands
  //
  // Returns expected result of the evaluation.
  int32_t MakeInstructionsForAddReducePattern(DataType::Type packed_type,
                                             size_t vector_length,
                                             size_t number_of_add_operands,
                                             int32_t initial_value,
                                             /*out*/ std::list<HVecOperation*>* instructions);

  std::unique_ptr<CompilerOptions> compiler_options_;
  std::unique_ptr<TestCodeGeneratorARM64> codegen_;
  HBasicBlock* main_block_;

 private:
  std::unique_ptr<DisassemblyInformation> disasm_info_;
};

void CodegenArm64Test::TestParallelMove(const std::function<void(HParallelMove*)>& init_move,
                                        const std::string& expected_pattern) {
  HParallelMove* move = new (GetAllocator()) HParallelMove(GetAllocator());
  init_move(move);

  codegen_->GetMoveResolver()->EmitNativeCode(move);
  codegen_->Finalize();

  ArrayRef<const uint8_t> code = codegen_->GetCode();

  std::unique_ptr<Disassembler> disasm(Disassembler::Create(
      InstructionSet::kArm64,
      new DisassemblerOptions(false,
                              code.begin(),
                              code.end(),
                              /* can_read_literals_= */ false,
                              [](std::ostream& os, uint32_t offset) { os << offset; })));

  std::ostringstream oss;
  disasm->Dump(oss, code.begin(), code.end());

  std::regex r(expected_pattern);
  EXPECT_TRUE(std::regex_search(oss.str(), r))
      << "Disassembly:" << std::endl
      << oss.str() << "Expected pattern: " << expected_pattern;
}

int32_t CodegenArm64Test::MakeInstructionsForAddReducePattern(
    DataType::Type packed_type,
    size_t vector_length,
    size_t number_of_add_operands,
    int32_t initial_value,
    /*out*/ std::list<HVecOperation*>* instructions) {
  HVecReplicateScalar* repl_scalar = MakeInstruction<HVecReplicateScalar>(
      graph_->GetIntConstant(initial_value), packed_type, vector_length, kNoDexPc);
  instructions->push_back(repl_scalar);

  HInstruction* result = repl_scalar;
  int32_t expected = initial_value;
  std::vector<HVecAdd*> adds;

  for (int i = 1; i < number_of_add_operands; i++) {
    HIntConstant* const_i = graph_->GetIntConstant(initial_value + i);
    repl_scalar =
        MakeInstruction<HVecReplicateScalar>(const_i, packed_type, vector_length, kNoDexPc);
    instructions->push_back(repl_scalar);

    HVecAdd* add =
        MakeInstruction<HVecAdd>(result, repl_scalar, packed_type, vector_length, kNoDexPc);
    adds.push_back(add);

    result = add;
    expected += i + initial_value;
  }

  for (HVecAdd* add : adds) {
    instructions->push_back(add);
  }

  HVecReduce* reduce =
      MakeInstruction<HVecReduce>(result, packed_type, vector_length, HVecReduce::kSum, kNoDexPc);
  instructions->push_back(reduce);

  expected *= vector_length;

  HVecExtractScalar* extract_scalar =
      MakeInstruction<HVecExtractScalar>(reduce, packed_type, vector_length, 0, kNoDexPc);
  instructions->push_back(extract_scalar);

  return expected;
}

// Regression test for b/34760542.
TEST_F(CodegenArm64Test, ParallelMoveResolverB34760542) {
  codegen_->Initialize();

  // The following ParallelMove used to fail this assertion:
  //
  //   Assertion failed (!available->IsEmpty())
  //
  // in vixl::aarch64::UseScratchRegisterScope::AcquireNextAvailable,
  // because of the following situation:
  //
  //   1. a temp register (IP0) is allocated as a scratch register by
  //      the parallel move resolver to solve a cycle (swap):
  //
  //        [ source=DS0 destination=DS257 type=PrimDouble instruction=null ]
  //        [ source=DS257 destination=DS0 type=PrimDouble instruction=null ]
  //
  //   2. within CodeGeneratorARM64::MoveLocation, another temp
  //      register (IP1) is allocated to generate the swap between two
  //      double stack slots;
  //
  //   3. VIXL requires a third temp register to emit the `Ldr` or
  //      `Str` operation from CodeGeneratorARM64::MoveLocation (as
  //      one of the stack slots' offsets cannot be encoded as an
  //      immediate), but the pool of (core) temp registers is now
  //      empty.
  //
  // The solution used so far is to use a floating-point temp register
  // (D31) in step #2, so that IP1 is available for step #3.

  HParallelMove* move = new (GetAllocator()) HParallelMove(GetAllocator());
  move->AddMove(Location::DoubleStackSlot(0),
                Location::DoubleStackSlot(257),
                DataType::Type::kFloat64,
                nullptr);
  move->AddMove(Location::DoubleStackSlot(257),
                Location::DoubleStackSlot(0),
                DataType::Type::kFloat64,
                nullptr);
  codegen_->GetMoveResolver()->EmitNativeCode(move);

  codegen_->Finalize();
}

// Check that ParallelMoveResolver works fine for ARM64 for both cases when SIMD is on and off -
// for traditional SIMD.
TEST_F(CodegenArm64Test, ParallelMoveResolverTraditionalSIMDOnAndOff) {
  codegen_->Initialize();

  constexpr size_t kSIMDSlotSizeInStackSlots = vixl::aarch64::kQRegSizeInBytes / kVRegSize;

  auto add_and_resolves_moves = [&]() {
    HParallelMove* move = new (GetAllocator()) HParallelMove(GetAllocator());
    move->AddMove(Location::SIMDStackSlot(0, kSIMDSlotSizeInStackSlots),
                  Location::SIMDStackSlot(257, kSIMDSlotSizeInStackSlots),
                  DataType::Type::kFloat64,
                  nullptr);
    move->AddMove(Location::SIMDStackSlot(257, kSIMDSlotSizeInStackSlots),
                  Location::SIMDStackSlot(0, kSIMDSlotSizeInStackSlots),
                  DataType::Type::kFloat64,
                  nullptr);
    move->AddMove(Location::FpuRegisterLocation(0),
                  Location::FpuRegisterLocation(1),
                  DataType::Type::kFloat64,
                  nullptr);
    move->AddMove(Location::FpuRegisterLocation(1),
                  Location::FpuRegisterLocation(0),
                  DataType::Type::kFloat64,
                  nullptr);
    codegen_->GetMoveResolver()->EmitNativeCode(move);
  };

  graph_->SetHasTraditionalSIMD(true);
  add_and_resolves_moves();

  graph_->SetHasTraditionalSIMD(false);
  add_and_resolves_moves();

  ASSERT_NO_FATAL_FAILURE(codegen_->Finalize());
}

// Check that ART ISA Features are propagated to VIXL for arm64 (using cortex-a75 as example).
TEST_F(CodegenArm64Test, IsaVIXLFeaturesA75) {
  std::unique_ptr<CompilerOptions> compiler_options =
      CommonCompilerTest::CreateCompilerOptions(InstructionSet::kArm64, "cortex-a75");
  arm64::CodeGeneratorARM64 codegen(graph_, *compiler_options);
  vixl::CPUFeatures* features = codegen.GetVIXLAssembler()->GetCPUFeatures();

  EXPECT_TRUE(features->Has(vixl::CPUFeatures::kCRC32));
  EXPECT_TRUE(features->Has(vixl::CPUFeatures::kDotProduct));
  EXPECT_TRUE(features->Has(vixl::CPUFeatures::kFPHalf));
  EXPECT_TRUE(features->Has(vixl::CPUFeatures::kNEONHalf));
  EXPECT_TRUE(features->Has(vixl::CPUFeatures::kAtomics));
}

// Check that ART ISA Features are propagated to VIXL for arm64 (using cortex-a53 as example).
TEST_F(CodegenArm64Test, IsaVIXLFeaturesA53) {
  std::unique_ptr<CompilerOptions> compiler_options =
      CommonCompilerTest::CreateCompilerOptions(InstructionSet::kArm64, "cortex-a53");
  arm64::CodeGeneratorARM64 codegen(graph_, *compiler_options);
  vixl::CPUFeatures* features = codegen.GetVIXLAssembler()->GetCPUFeatures();

  EXPECT_TRUE(features->Has(vixl::CPUFeatures::kCRC32));
  EXPECT_FALSE(features->Has(vixl::CPUFeatures::kDotProduct));
  EXPECT_FALSE(features->Has(vixl::CPUFeatures::kFPHalf));
  EXPECT_FALSE(features->Has(vixl::CPUFeatures::kNEONHalf));
  EXPECT_FALSE(features->Has(vixl::CPUFeatures::kAtomics));
}

constexpr static size_t kExpectedFPSpillSize = 8 * vixl::aarch64::kDRegSizeInBytes;

// The following two tests check that for both SIMD and non-SIMD graphs exactly 64-bit is
// allocated on stack per callee-saved FP register to be preserved in the frame entry as
// ABI states.
TEST_F(CodegenArm64Test, FrameSizeSIMD) {
  codegen_->Initialize();
  graph_->SetHasTraditionalSIMD(true);

  DCHECK_EQ(arm64::callee_saved_fp_registers.GetCount(), 8);
  vixl::aarch64::CPURegList reg_list = arm64::callee_saved_fp_registers;
  while (!reg_list.IsEmpty()) {
    uint32_t reg_code = reg_list.PopLowestIndex().GetCode();
    codegen_->AddAllocatedRegister(Location::FpuRegisterLocation(reg_code));
  }
  codegen_->ComputeSpillMask();

  EXPECT_EQ(codegen_->GetFpuSpillSize(), kExpectedFPSpillSize);
}

TEST_F(CodegenArm64Test, FrameSizeNoSIMD) {
  codegen_->Initialize();
  graph_->SetHasTraditionalSIMD(false);
  graph_->SetHasPredicatedSIMD(false);

  DCHECK_EQ(arm64::callee_saved_fp_registers.GetCount(), 8);
  vixl::aarch64::CPURegList reg_list = arm64::callee_saved_fp_registers;
  while (!reg_list.IsEmpty()) {
    uint32_t reg_code = reg_list.PopLowestIndex().GetCode();
    codegen_->AddAllocatedRegister(Location::FpuRegisterLocation(reg_code));
  }
  codegen_->ComputeSpillMask();

  EXPECT_EQ(codegen_->GetFpuSpillSize(), kExpectedFPSpillSize);
}

// Check that ParallelMoveResolver works fine for predicated SIMD mode.
TEST_F(CodegenArm64Test, ParallelMoveResolverPredicatedSIMD) {
  if (!kArm64AllowSVE) {
    GTEST_SKIP() << "Test requires enabled SVE support in codegen";
  }

  constexpr size_t kSVERegisterWidth = 32;
  constexpr size_t kSVESlotSizeInStackSlots = kSVERegisterWidth / kVRegSize;
  constexpr size_t kNEONSlotSizeInStackSlots = vixl::aarch64::kQRegSizeInBytes / kVRegSize;

  graph_->SetHasPredicatedSIMD(true);

  // Test LoadSIMDRegFromStack
  TestParallelMove(
      [](HParallelMove* move) {
        move->AddMove(Location::SIMDStackSlot(0, kSVESlotSizeInStackSlots),
                      Location::FpuRegisterLocation(0),
                      DataType::Type::kFloat64,
                      nullptr);
      },
      R"(ldr z0)");

  TestParallelMove(
      [](HParallelMove* move) {
        move->AddMove(Location::SIMDStackSlot(0, kNEONSlotSizeInStackSlots),
                      Location::FpuRegisterLocation(0),
                      DataType::Type::kFloat64,
                      nullptr);
      },
      R"(ldr q0)");

  // Test MoveToSIMDStackSlot
  TestParallelMove(
      [](HParallelMove* move) {
        move->AddMove(Location::FpuRegisterLocation(0),
                      Location::SIMDStackSlot(0, kSVESlotSizeInStackSlots),
                      DataType::Type::kFloat64,
                      nullptr);
      },
      R"(str z0)");

  TestParallelMove(
      [](HParallelMove* move) {
        move->AddMove(Location::FpuRegisterLocation(0),
                      Location::SIMDStackSlot(0, kNEONSlotSizeInStackSlots),
                      DataType::Type::kFloat64,
                      nullptr);
      },
      R"(str q0)");

  TestParallelMove(
      [](HParallelMove* move) {
        move->AddMove(Location::SIMDStackSlot(0, kSVESlotSizeInStackSlots),
                      Location::SIMDStackSlot(257, kSVESlotSizeInStackSlots),
                      DataType::Type::kFloat64,
                      nullptr);
      },
      R"(ldr z(\d+)(.|\n)+)"
      R"(str z\1)");

  TestParallelMove(
      [](HParallelMove* move) {
        move->AddMove(Location::SIMDStackSlot(0, kNEONSlotSizeInStackSlots),
                      Location::SIMDStackSlot(257, kNEONSlotSizeInStackSlots),
                      DataType::Type::kFloat64,
                      nullptr);
      },
      R"(ldr q(\d+)(.|\n)+)"
      R"(str q\1)");

  // MoveSIMDRegToSIMDReg
  TestParallelMove(
      [](HParallelMove* move) {
        move->AddMove(Location::FpuRegisterLocation(0),
                      Location::FpuRegisterLocation(1),
                      DataType::Type::kFloat64,
                      nullptr);
      },
      R"(mov z1.d, z0.d)");

  // Test AllocateSIMDScratchLocation/FreeSIMDScratchLocation
  TestParallelMove(
      [](HParallelMove* move) {
        move->AddMove(Location::FpuRegisterLocation(0),
                      Location::FpuRegisterLocation(1),
                      DataType::Type::kFloat64,
                      nullptr);
        move->AddMove(Location::FpuRegisterLocation(1),
                      Location::FpuRegisterLocation(2),
                      DataType::Type::kFloat64,
                      nullptr);
        move->AddMove(Location::FpuRegisterLocation(2),
                      Location::FpuRegisterLocation(0),
                      DataType::Type::kFloat64,
                      nullptr);
      },
      R"(mov z(\d+)\.d, z2\.d(.|\n)+)"
      R"(mov z2\.d, z1\.d(.|\n)+)"
      R"(mov z1\.d, z0\.d(.|\n)+)"
      R"(mov z0\.d, z\1\.d)");
}

// Check that ParallelMoveResolver works fine for traditional SIMD mode.
TEST_F(CodegenArm64Test, ParallelMoveResolverTraditionalSIMD) {
  constexpr size_t kNEONSlotSizeInStackSlots = vixl::aarch64::kQRegSizeInBytes / kVRegSize;

  graph_->SetHasTraditionalSIMD(true);

  // Test LoadSIMDRegFromStack
  TestParallelMove(
      [](HParallelMove* move) {
        move->AddMove(Location::SIMDStackSlot(0, kNEONSlotSizeInStackSlots),
                      Location::FpuRegisterLocation(0),
                      DataType::Type::kFloat64,
                      nullptr);
      },
      R"(ldr q0)");

  // Test MoveToSIMDStackSlot
  TestParallelMove(
      [](HParallelMove* move) {
        move->AddMove(Location::FpuRegisterLocation(0),
                      Location::SIMDStackSlot(0, kNEONSlotSizeInStackSlots),
                      DataType::Type::kFloat64,
                      nullptr);
      },
      R"(str q0)");

  TestParallelMove(
      [](HParallelMove* move) {
        move->AddMove(Location::SIMDStackSlot(0, kNEONSlotSizeInStackSlots),
                      Location::SIMDStackSlot(257, kNEONSlotSizeInStackSlots),
                      DataType::Type::kFloat64,
                      nullptr);
      },
      R"(ldr q(\d+)(.|\n)+)"
      R"(str q\1)");

  // MoveSIMDRegToSIMDReg
  TestParallelMove(
      [](HParallelMove* move) {
        move->AddMove(Location::FpuRegisterLocation(0),
                      Location::FpuRegisterLocation(1),
                      DataType::Type::kFloat64,
                      nullptr);
      },
      R"(mov v1\.16b, v0\.16b)");

  // Test AllocateSIMDScratchLocation/FreeSIMDScratchLocation
  TestParallelMove(
      [](HParallelMove* move) {
        move->AddMove(Location::FpuRegisterLocation(0),
                      Location::FpuRegisterLocation(1),
                      DataType::Type::kFloat64,
                      nullptr);
        move->AddMove(Location::FpuRegisterLocation(1),
                      Location::FpuRegisterLocation(2),
                      DataType::Type::kFloat64,
                      nullptr);
        move->AddMove(Location::FpuRegisterLocation(2),
                      Location::FpuRegisterLocation(0),
                      DataType::Type::kFloat64,
                      nullptr);
      },
      R"(mov v(\d+)\.16b, v2\.16b(.|\n)+)"
      R"(mov v2\.16b, v1\.16b(.|\n)+)"
      R"(mov v1\.16b, v0\.16b(.|\n)+)"
      R"(mov v0\.16b, v\1\.16b)");
}

// Ensure spilling saves full SIMD values in the traditional SIMD mode.
//
// In this test we build a following graph:
//
//   VecReplicateScalar v0, 0
//   VecReplicateScalar v1, 1
//   ...
//   VecReplicateScalar v32, 32
//   VecAdd a0, v0, v1
//   VecAdd a1, a0, v2
//   ...
//   VecAdd a32, a31, v32
//   VecReduce r0, a32
//   VecExtractScalar r1, r0
//   Return r1
//
// We define more SVE vector values (VecReplicateScalar nodes) than the number of V registers.
// It leads to spilling them on the stack.
TEST_F(CodegenArm64Test, SpillingSIMDValuesTraditionalSIMD) {
  constexpr DataType::Type packed_type = DataType::Type::kInt32;
  size_t vector_length = codegen_->GetTraditionalSIMDRegisterWidth() / DataType::Size(packed_type);
  constexpr size_t number_of_vector_values = vixl::aarch64::kNumberOfVRegisters + 1;

  graph_->SetHasTraditionalSIMD(true);

  std::list<HVecOperation*> instructions;
  int32_t expected = MakeInstructionsForAddReducePattern(
      packed_type, vector_length, number_of_vector_values, 0, &instructions);

  for (HVecOperation* inst : instructions) {
    AddInstruction(inst);
  }

  ReplaceReturnInstruction(new (GetAllocator()) HReturn(instructions.back()));

  TestCode(true, expected);
}

// Ensure spilling saves full SIMD values in the predicated SIMD mode.
//
// In this test we build a following graph:
//
//   VecPredSetAll pred, 1
//   VecReplicateScalar v0, 0, pred
//   VecReplicateScalar v1, 1, pred
//   ...
//   VecReplicateScalar v32, 32, pred
//   VecAdd a0, v0, v1, pred
//   VecAdd a1, a0, v2, pred
//   ...
//   VecAdd a32, a31, v32, pred
//   VecReduce r0, a32, pred
//   VecExtractScalar r1, r0, pred
//   Return r1
//
// We define more SVE vector values (VecReplicateScalar nodes) than the number of Z registers.
// It leads to spilling them on the stack.
TEST_F(CodegenArm64Test, SpillingSIMDValuesPredicatedSIMD) {
  if (!kArm64AllowSVE) {
    GTEST_SKIP() << "Test requires enabled SVE support in codegen";
  }

  constexpr DataType::Type packed_type = DataType::Type::kInt32;
  size_t vector_length = codegen_->GetPredicatedSIMDRegisterWidth() / DataType::Size(packed_type);
  constexpr size_t number_of_vector_values = vixl::aarch64::kNumberOfZRegisters + 1;

  graph_->SetHasPredicatedSIMD(true);

  HVecPredSetAll* all_true_pred = MakeAndAddInstruction<HVecPredSetAll>(graph_->GetIntConstant(1),
                                                                        packed_type,
                                                                        vector_length,
                                                                        kNoDexPc);

  std::list<HVecOperation*> instructions;
  int32_t expected = MakeInstructionsForAddReducePattern(
      packed_type, vector_length, number_of_vector_values, 0, &instructions);

  for (HVecOperation* inst : instructions) {
    AddInstruction(inst);
    inst->SetMergingGoverningPredicate(all_true_pred);
  }

  ReplaceReturnInstruction(new (GetAllocator()) HReturn(instructions.back()));

  TestCode(true, expected);
}

// In a graph with traditional and predicated SIMD, check that the stack slot size for SIMD values,
// spilt in the tranditional mode, is chosen correctly - to match the value size and vector register
// size. Also validates, that such values could be correctly preserved over the following predicated
// instructions.
//
// In this test we build a following graph:
//
//   VecPredSetAll pred, 1
//
//   VecReplicateScalar n0, 0         --+
//   VecReplicateScalar n1, 1           + NEON
//   ...                                +
//   VecReplicateScalar n31, 31       --+
//
//   VecReplicateScalar s0, 32, pred  --+
//   VecReplicateScalar s1, 33, pred    + SVE
//   ...                                +
//   VecReplicateScalar s31, 63, pred --+
//
//   VecAdd n32, n0, n1               --+
//   VecAdd n33, n32, n2                +
//   ...                                + NEON
//   VecAdd n63, n62, n31               +
//   VecReduce n64, n63                 +
//   VecExtractScalar nres, n64       --+
//
//   VecAdd s32, s0, s1, pred         --+
//   VecAdd s33, s32, s2, pred          +
//   ...                                + SVE
//   VecAdd s63, s62, s31, pred         +
//   VecReduce s64, s63, pred           +
//   VecExtractScalar sres, s64, pred --+
//
//   Mul res, nres, sres
//   Return res
//
// We have SVE instructions between NEON vector values definitions (VecReplicateScalar nodes)
// and usages (VecAdd nodes) here. It leads to spilling NEON values on the stack, the same for
// SVE ones.
TEST_F(CodegenArm64Test, SpillingSIMDValuesMixedSIMD) {
  if (!kArm64AllowSVE) {
    GTEST_SKIP() << "Test requires enabled SVE support in codegen";
  }

  constexpr DataType::Type packed_type = DataType::Type::kInt32;
  size_t sve_vector_length =
      codegen_->GetPredicatedSIMDRegisterWidth() / DataType::Size(packed_type);
  size_t neon_vector_length =
      codegen_->GetTraditionalSIMDRegisterWidth() / DataType::Size(packed_type);
  constexpr size_t number_of_vector_values = vixl::aarch64::kNumberOfZRegisters;

  graph_->SetHasPredicatedSIMD(true);
  graph_->SetHasTraditionalSIMD(true);

  HVecPredSetAll* all_true_pred = MakeAndAddInstruction<HVecPredSetAll>(graph_->GetIntConstant(1),
                                                                        packed_type,
                                                                        sve_vector_length,
                                                                        kNoDexPc);

  std::list<HVecOperation*> instructions;
  int32_t neon_expected = MakeInstructionsForAddReducePattern(
      packed_type, neon_vector_length, number_of_vector_values, 0, &instructions);

  std::list<HVecOperation*> sve_instructions;
  int32_t sve_expected = MakeInstructionsForAddReducePattern(
      packed_type, sve_vector_length, number_of_vector_values, 32, &sve_instructions);

  auto neon_add = std::find_if(instructions.begin(), instructions.end(), [](HVecOperation* inst) {
    return inst->IsVecAdd();
  });

  auto sve_add =
      std::find_if(sve_instructions.cbegin(), sve_instructions.cend(), [](HVecOperation* inst) {
    return inst->IsVecAdd();
  });

  HVecOperation* neon_result_inst = instructions.back();
  HVecOperation* sve_result_inst = sve_instructions.back();

  instructions.insert(neon_add, sve_instructions.cbegin(), sve_add);
  instructions.insert(instructions.end(), sve_add, sve_instructions.cend());

  for (HVecOperation* inst : instructions) {
    AddInstruction(inst);
  }

  for (HVecOperation* inst : sve_instructions) {
    inst->SetMergingGoverningPredicate(all_true_pred);
  }

  HMul* mul = new (GetAllocator()) HMul(packed_type, neon_result_inst, sve_result_inst);
  AddInstruction(mul);

  int32_t expected = neon_expected * sve_expected;

  ReplaceReturnInstruction(new (GetAllocator()) HReturn(mul));

  TestCode(true, expected);
}

// Ensure that SIMD registers are correctly saved/restored in the SuspendCheck's slowpath
// in the traditional SIMD mode.
//
// In this test we build a following graph:
//
//   VecReplicateScalar v0, 0
//   VecReplicateScalar v1, 1
//   ...
//   VecReplicateScalar v31, 31
//   SuspendCheck
//   VecAdd a0, v0, v1
//   VecAdd a1, a0, v2
//   ...
//   VecAdd a31, a30, v31
//   VecReduce r0, a31
//   VecExtractScalar r1, r0
//   Return r1
//
// We define the same number of SVE values (VecReplicateScalar nodes) as the number of V registers
// and have a suspend check between their definitions and usages. This suspend check is always true
// so we always execute its slowpath that saves and restores live fp registers.
TEST_F(CodegenArm64Test, SpillingSIMDValuesInSuspendCheckSlowpathTraditionalSIMD) {
  constexpr DataType::Type packed_type = DataType::Type::kInt32;
  size_t vector_length = codegen_->GetTraditionalSIMDRegisterWidth() / DataType::Size(packed_type);
  constexpr size_t number_of_vector_values = vixl::aarch64::kNumberOfVRegisters;

  graph_->SetHasTraditionalSIMD(true);

  std::list<HVecOperation*> instructions;
  int32_t expected = MakeInstructionsForAddReducePattern(
      packed_type, vector_length, number_of_vector_values, 0, &instructions);

  for (HVecOperation* inst : instructions) {
    AddInstruction(inst);
  }

  HSuspendCheck *suspend_check = new (GetAllocator()) HSuspendCheck(kNoDexPc);

  auto add = std::find_if(instructions.cbegin(), instructions.cend(), [](HVecOperation* inst) {
    return inst->IsVecAdd();
  });

  main_block_->InsertInstructionBefore(suspend_check, *add);
  ManuallyBuildEnvFor(suspend_check, {});

  ReplaceReturnInstruction(new (GetAllocator()) HReturn(instructions.back()));

  TestCode(true, expected);
}

// Ensure that SIMD registers are correctly saved/restored in the SuspendCheck's slowpath
// in the predicated SIMD mode.
//
// In this test we build a following graph:
//
//   VecPredSetAll pred, 1
//   VecReplicateScalar v0, 0, pred
//   VecReplicateScalar v1, 1, pred
//   ...
//   VecReplicateScalar v31, 31, pred
//   SuspendCheck
//   VecAdd a0, v0, v1, pred
//   VecAdd a1, a0, v2, pred
//   ...
//   VecAdd a31, a30, v31, pred
//   VecReduce r0, a31, pred
//   VecExtractScalar r1, r0, pred
//   Return r1
//
// We define the same number of SVE values (VecReplicateScalar nodes) as the number of Z registers
// and have a suspend check between their definitions and usages. This suspend check is always true
// so we always execute its slowpath that saves and restores live fp registers.
TEST_F(CodegenArm64Test, SpillingSIMDValuesInSuspendCheckSlowpathPredicatedSIMD) {
  if (!kArm64AllowSVE) {
    GTEST_SKIP() << "Test requires enabled SVE support in codegen";
  }

  constexpr DataType::Type packed_type = DataType::Type::kInt32;
  size_t vector_length = codegen_->GetPredicatedSIMDRegisterWidth() / DataType::Size(packed_type);
  constexpr size_t number_of_vector_values = vixl::aarch64::kNumberOfZRegisters;

  graph_->SetHasPredicatedSIMD(true);

  HVecPredSetAll* all_true_pred = MakeAndAddInstruction<HVecPredSetAll>(
      graph_->GetIntConstant(1), packed_type, vector_length, kNoDexPc);

  std::list<HVecOperation*> instructions;
  int32_t expected = MakeInstructionsForAddReducePattern(
      packed_type, vector_length, number_of_vector_values, 0, &instructions);

  for (HVecOperation* inst : instructions) {
    AddInstruction(inst);
    inst->SetMergingGoverningPredicate(all_true_pred);
  }

  HSuspendCheck *suspend_check = new (GetAllocator()) HSuspendCheck(kNoDexPc);

  auto add = std::find_if(instructions.cbegin(), instructions.cend(), [](HVecOperation* inst) {
    return inst->IsVecAdd();
  });

  main_block_->InsertInstructionBefore(suspend_check, *add);
  ManuallyBuildEnvFor(suspend_check, {});

  ReplaceReturnInstruction(new (GetAllocator()) HReturn(instructions.back()));

  TestCode(true, expected);
}

// Ensure that SIMD registers are correctly saved/restored in the SuspendCheck's slowpath
// in the mixed SIMD mode.
//
// In this test we build a following graph:
//
//   VecPredSetAll pred, 1
//
//   VecReplicateScalar n0, 0         --+
//   VecReplicateScalar n1, 1           + NEON
//   ...                                +
//   VecReplicateScalar n15, 15       --+
//
//   VecReplicateScalar s0, 0, pred   --+
//   VecReplicateScalar s1, 1, pred     + SVE
//   ...                                +
//   VecReplicateScalar s15, 15, pred --+
//
//   SuspendCheck
//
//   VecAdd s16, s0, s1, pred         --+
//   VecAdd s17, s16, s2, pred          +
//   ...                                + SVE
//   VecAdd s31, s30, s15, pred         +
//   VecReduce s32, s31, pred           +
//   VecExtractScalar sr, s32, pred   --+
//
//   VecAdd n16, n0, n1               --+
//   VecAdd n17, n16, n2                +
//   ...                                + NEON
//   VecAdd n31, n30, n15               +
//   VecReduce n32, n31                 +
//   VecExtractScalar nr, n32         --+
//
//   Mul r, nr, sr
//   Return r
//
// We have a suspend check between definitions of SIMD values (both NEON and SVE ones) and their
// usages. This suspend check is always true so we always execute its slowpath that saves and
// restores live fp registers
TEST_F(CodegenArm64Test, SpillingSIMDValuesInSuspendCheckSlowpathMixedSIMD) {
  if (!kArm64AllowSVE) {
    GTEST_SKIP() << "Test requires enabled SVE support in codegen";
  }

  constexpr DataType::Type packed_type = DataType::Type::kInt32;
  size_t sve_vector_length =
      codegen_->GetPredicatedSIMDRegisterWidth() / DataType::Size(packed_type);
  size_t neon_vector_length =
      codegen_->GetTraditionalSIMDRegisterWidth() / DataType::Size(packed_type);
  constexpr size_t number_of_vector_values = vixl::aarch64::kNumberOfZRegisters;

  graph_->SetHasPredicatedSIMD(true);
  graph_->SetHasTraditionalSIMD(true);

  HVecPredSetAll* all_true_pred = MakeAndAddInstruction<HVecPredSetAll>(
      graph_->GetIntConstant(1), packed_type, sve_vector_length, kNoDexPc);

  std::list<HVecOperation*> instructions;
  int32_t neon_expected = MakeInstructionsForAddReducePattern(
      packed_type, neon_vector_length, number_of_vector_values / 2, 0, &instructions);

  std::list<HVecOperation*> sve_instructions;
  int32_t sve_expected = MakeInstructionsForAddReducePattern(
      packed_type, sve_vector_length, number_of_vector_values / 2, 0, &sve_instructions);

  auto neon_add = std::find_if(instructions.cbegin(), instructions.cend(), [](HVecOperation* inst) {
    return inst->IsVecAdd();
  });

  HVecOperation* neon_result_inst = instructions.back();
  HVecOperation* sve_result_inst = sve_instructions.back();

  instructions.insert(neon_add, sve_instructions.cbegin(), sve_instructions.cend());

  for (HVecOperation* inst : instructions) {
    AddInstruction(inst);
  }

  for (HVecOperation* inst : sve_instructions) {
    inst->SetMergingGoverningPredicate(all_true_pred);
  }

  HSuspendCheck *suspend_check = new (GetAllocator()) HSuspendCheck(kNoDexPc);

  auto sve_add =
      std::find_if(sve_instructions.cbegin(), sve_instructions.cend(), [](HVecOperation* inst) {
    return inst->IsVecAdd();
  });

  main_block_->InsertInstructionBefore(suspend_check, *sve_add);
  ManuallyBuildEnvFor(suspend_check, {});

  HMul* mul = new (GetAllocator()) HMul(packed_type, neon_result_inst, sve_result_inst);
  AddInstruction(mul);

  int32_t expected = neon_expected * sve_expected;

  ReplaceReturnInstruction(new (GetAllocator()) HReturn(mul));

  TestCode(true, expected);
}

}  // namespace arm64
}  // namespace art
