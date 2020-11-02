/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include "load_store_elimination.h"

#include <memory>
#include <tuple>
#include <variant>

#include "compilation_kind.h"
#include "entrypoints/quick/quick_entrypoints_enum.h"
#include "gtest/gtest.h"
#include "handle_scope.h"
#include "load_store_analysis.h"
#include "nodes.h"
#include "optimizing/optimizing_compiler_stats.h"
#include "optimizing_unit_test.h"
#include "scoped_thread_state_change.h"

namespace art {

struct InstructionDumper {
 public:
  HInstruction* ins_;
};

std::ostream& operator<<(std::ostream& os, const InstructionDumper& id) {
  if (id.ins_ == nullptr) {
    return os << "NULL";
  } else {
    return os << *id.ins_;
  }
}

#define CHECK_SUBROUTINE_FAILURE() \
  do {                             \
    if (HasFatalFailure()) {       \
      return;                      \
    }                              \
  } while (false)
#define EXPECT_INS_EQ(a, b) \
  EXPECT_EQ(a, b) << (InstructionDumper{a}) << " vs " << (InstructionDumper{b})
#define EXPECT_INS_REMOVED(a) EXPECT_TRUE(IsRemoved(a)) << "Not removed: " << *a
#define EXPECT_INS_RETAINED(a) EXPECT_FALSE(IsRemoved(a)) << "Removed: " << *a
#define ASSERT_INS_EQ(a, b) \
  ASSERT_EQ(a, b) << (InstructionDumper{a}) << " vs " << (InstructionDumper{b})
#define ASSERT_INS_REMOVED(a) ASSERT_TRUE(IsRemoved(a)) << "Not removed: " << *a
#define ASSERT_INS_RETAINED(a) ASSERT_FALSE(IsRemoved(a)) << "Removed: " << *a

template <typename SuperTest>
class LoadStoreEliminationTestBase : public SuperTest, public OptimizingUnitTestHelper {
 public:
  void SetUp() override {
    SuperTest::SetUp();
    gLogVerbosity.compiler = true;
  }
  void TearDown() override {
    SuperTest::TearDown();
    gLogVerbosity.compiler = false;
  }
  AdjacencyListGraph SetupFromAdjacencyList(const std::string_view entry_name,
                                            const std::string_view exit_name,
                                            const std::vector<AdjacencyListGraph::Edge>& adj) {
    return AdjacencyListGraph(graph_, GetAllocator(), entry_name, exit_name, adj);
  }

  void PerformLSE(bool with_partial = true) {
    graph_->BuildDominatorTree();
    LoadStoreElimination lse(graph_, /*stats=*/nullptr);
    lse.Run(with_partial);
    std::ostringstream oss;
    EXPECT_TRUE(CheckGraphSkipRefTypeInfoChecks(oss)) << oss.str();
  }

  void PerformLSEWithPartial() {
    PerformLSE(true);
  }

  void PerformLSENoPartial() {
    PerformLSE(false);
  }

  // Create instructions shared among tests.
  void CreateEntryBlockInstructions() {
    HInstruction* c1 = graph_->GetIntConstant(1);
    HInstruction* c4 = graph_->GetIntConstant(4);
    i_add1_ = new (GetAllocator()) HAdd(DataType::Type::kInt32, i_, c1);
    i_add4_ = new (GetAllocator()) HAdd(DataType::Type::kInt32, i_, c4);
    entry_block_->AddInstruction(i_add1_);
    entry_block_->AddInstruction(i_add4_);
    entry_block_->AddInstruction(new (GetAllocator()) HGoto());
  }

  // Create the major CFG used by tests:
  //    entry
  //      |
  //  pre_header
  //      |
  //    loop[]
  //      |
  //   return
  //      |
  //     exit
  void CreateTestControlFlowGraph() {
    InitGraphAndParameters();
    pre_header_ = AddNewBlock();
    loop_ = AddNewBlock();

    entry_block_->ReplaceSuccessor(return_block_, pre_header_);
    pre_header_->AddSuccessor(loop_);
    loop_->AddSuccessor(loop_);
    loop_->AddSuccessor(return_block_);

    HInstruction* c0 = graph_->GetIntConstant(0);
    HInstruction* c1 = graph_->GetIntConstant(1);
    HInstruction* c128 = graph_->GetIntConstant(128);

    CreateEntryBlockInstructions();

    // pre_header block
    //   phi = 0;
    phi_ = new (GetAllocator()) HPhi(GetAllocator(), 0, 0, DataType::Type::kInt32);
    loop_->AddPhi(phi_);
    pre_header_->AddInstruction(new (GetAllocator()) HGoto());
    phi_->AddInput(c0);

    // loop block:
    //   suspend_check
    //   phi++;
    //   if (phi >= 128)
    suspend_check_ = new (GetAllocator()) HSuspendCheck();
    HInstruction* inc_phi = new (GetAllocator()) HAdd(DataType::Type::kInt32, phi_, c1);
    HInstruction* cmp = new (GetAllocator()) HGreaterThanOrEqual(phi_, c128);
    HInstruction* hif = new (GetAllocator()) HIf(cmp);
    loop_->AddInstruction(suspend_check_);
    loop_->AddInstruction(inc_phi);
    loop_->AddInstruction(cmp);
    loop_->AddInstruction(hif);
    phi_->AddInput(inc_phi);

    CreateEnvForSuspendCheck();
  }

  void CreateEnvForSuspendCheck() {
    ArenaVector<HInstruction*> current_locals({array_, i_, j_},
                                              GetAllocator()->Adapter(kArenaAllocInstruction));
    ManuallyBuildEnvFor(suspend_check_, &current_locals);
  }

  // Create the diamond-shaped CFG:
  //      upper
  //      /   \
  //    left  right
  //      \   /
  //      down
  //
  // Return: the basic blocks forming the CFG in the following order {upper, left, right, down}.
  std::tuple<HBasicBlock*, HBasicBlock*, HBasicBlock*, HBasicBlock*> CreateDiamondShapedCFG() {
    InitGraphAndParameters();
    CreateEntryBlockInstructions();

    HBasicBlock* upper = AddNewBlock();
    HBasicBlock* left = AddNewBlock();
    HBasicBlock* right = AddNewBlock();

    entry_block_->ReplaceSuccessor(return_block_, upper);
    upper->AddSuccessor(left);
    upper->AddSuccessor(right);
    left->AddSuccessor(return_block_);
    right->AddSuccessor(return_block_);

    HInstruction* cmp = new (GetAllocator()) HGreaterThanOrEqual(i_, j_);
    HInstruction* hif = new (GetAllocator()) HIf(cmp);
    upper->AddInstruction(cmp);
    upper->AddInstruction(hif);

    left->AddInstruction(new (GetAllocator()) HGoto());
    right->AddInstruction(new (GetAllocator()) HGoto());

    return std::make_tuple(upper, left, right, return_block_);
  }

  // Add a HVecLoad instruction to the end of the provided basic block.
  //
  // Return: the created HVecLoad instruction.
  HInstruction* AddVecLoad(HBasicBlock* block, HInstruction* array, HInstruction* index) {
    DCHECK(block != nullptr);
    DCHECK(array != nullptr);
    DCHECK(index != nullptr);
    HInstruction* vload =
        new (GetAllocator()) HVecLoad(GetAllocator(),
                                      array,
                                      index,
                                      DataType::Type::kInt32,
                                      SideEffects::ArrayReadOfType(DataType::Type::kInt32),
                                      4,
                                      /*is_string_char_at*/ false,
                                      kNoDexPc);
    block->InsertInstructionBefore(vload, block->GetLastInstruction());
    return vload;
  }

  // Add a HVecStore instruction to the end of the provided basic block.
  // If no vdata is specified, generate HVecStore: array[index] = [1,1,1,1].
  //
  // Return: the created HVecStore instruction.
  HInstruction* AddVecStore(HBasicBlock* block,
                            HInstruction* array,
                            HInstruction* index,
                            HInstruction* vdata = nullptr) {
    DCHECK(block != nullptr);
    DCHECK(array != nullptr);
    DCHECK(index != nullptr);
    if (vdata == nullptr) {
      HInstruction* c1 = graph_->GetIntConstant(1);
      vdata = new (GetAllocator())
          HVecReplicateScalar(GetAllocator(), c1, DataType::Type::kInt32, 4, kNoDexPc);
      block->InsertInstructionBefore(vdata, block->GetLastInstruction());
    }
    HInstruction* vstore =
        new (GetAllocator()) HVecStore(GetAllocator(),
                                       array,
                                       index,
                                       vdata,
                                       DataType::Type::kInt32,
                                       SideEffects::ArrayWriteOfType(DataType::Type::kInt32),
                                       4,
                                       kNoDexPc);
    block->InsertInstructionBefore(vstore, block->GetLastInstruction());
    return vstore;
  }

  // Add a HArrayGet instruction to the end of the provided basic block.
  //
  // Return: the created HArrayGet instruction.
  HInstruction* AddArrayGet(HBasicBlock* block, HInstruction* array, HInstruction* index) {
    DCHECK(block != nullptr);
    DCHECK(array != nullptr);
    DCHECK(index != nullptr);
    HInstruction* get = new (GetAllocator()) HArrayGet(array, index, DataType::Type::kInt32, 0);
    block->InsertInstructionBefore(get, block->GetLastInstruction());
    return get;
  }

  // Add a HArraySet instruction to the end of the provided basic block.
  // If no data is specified, generate HArraySet: array[index] = 1.
  //
  // Return: the created HArraySet instruction.
  HInstruction* AddArraySet(HBasicBlock* block,
                            HInstruction* array,
                            HInstruction* index,
                            HInstruction* data = nullptr) {
    DCHECK(block != nullptr);
    DCHECK(array != nullptr);
    DCHECK(index != nullptr);
    if (data == nullptr) {
      data = graph_->GetIntConstant(1);
    }
    HInstruction* store =
        new (GetAllocator()) HArraySet(array, index, data, DataType::Type::kInt32, 0);
    block->InsertInstructionBefore(store, block->GetLastInstruction());
    return store;
  }

  void InitGraphAndParameters() {
    InitGraph();
    AddParameter(new (GetAllocator()) HParameterValue(
        graph_->GetDexFile(), dex::TypeIndex(0), 0, DataType::Type::kInt32));
    array_ = parameters_.back();
    AddParameter(new (GetAllocator()) HParameterValue(
        graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kInt32));
    i_ = parameters_.back();
    AddParameter(new (GetAllocator()) HParameterValue(
        graph_->GetDexFile(), dex::TypeIndex(1), 2, DataType::Type::kInt32));
    j_ = parameters_.back();
  }

  HBasicBlock* pre_header_;
  HBasicBlock* loop_;

  HInstruction* array_;
  HInstruction* i_;
  HInstruction* j_;
  HInstruction* i_add1_;
  HInstruction* i_add4_;
  HInstruction* suspend_check_;

  HPhi* phi_;
};

class LoadStoreEliminationTest : public LoadStoreEliminationTestBase<CommonCompilerTest> {};

enum class TestOrder { kSameAsAlloc, kReverseOfAlloc };
std::ostream& operator<<(std::ostream& os, const TestOrder& ord) {
  switch (ord) {
    case TestOrder::kSameAsAlloc:
      return os << "SameAsAlloc";
    case TestOrder::kReverseOfAlloc:
      return os << "ReverseOfAlloc";
  }
}

class OrderDependentTestGroup
    : public LoadStoreEliminationTestBase<CommonCompilerTestWithParam<TestOrder>> {};

// Various configs we can use for testing. Currently used in PartialComparison tests.
struct PartialComparisonKind {
 public:
  enum class Type : uint8_t { kEquals, kNotEquals };
  enum class Target : uint8_t { kNull, kValue, kSelf };
  enum class Position : uint8_t { kLeft, kRight };

  const Type type_;
  const Target target_;
  const Position position_;

  bool IsDefinitelyFalse() const {
    return !IsPossiblyTrue();
  }
  bool IsPossiblyFalse() const {
    return !IsDefinitelyTrue();
  }
  bool IsDefinitelyTrue() const {
    if (target_ == Target::kSelf) {
      return type_ == Type::kEquals;
    } else if (target_ == Target::kNull) {
      return type_ == Type::kNotEquals;
    } else {
      return false;
    }
  }
  bool IsPossiblyTrue() const {
    if (target_ == Target::kSelf) {
      return type_ == Type::kEquals;
    } else if (target_ == Target::kNull) {
      return type_ == Type::kNotEquals;
    } else {
      return true;
    }
  }
  std::ostream& Dump(std::ostream& os) const {
    os << "PartialComparisonKind{" << (type_ == Type::kEquals ? "kEquals" : "kNotEquals") << ", "
       << (target_ == Target::kNull ? "kNull" : (target_ == Target::kSelf ? "kSelf" : "kValue"))
       << ", " << (position_ == Position::kLeft ? "kLeft" : "kRight") << "}";
    return os;
  }
};

std::ostream& operator<<(std::ostream& os, const PartialComparisonKind& comp) {
  return comp.Dump(os);
}

class PartialComparisonTestGroup
    : public LoadStoreEliminationTestBase<CommonCompilerTestWithParam<PartialComparisonKind>> {
 public:
  enum class ComparisonPlacement {
    kBeforeEscape,
    kInEscape,
    kAfterEscape,
  };
  void CheckFinalInstruction(HInstruction* ins, ComparisonPlacement placement) {
    using Target = PartialComparisonKind::Target;
    using Type = PartialComparisonKind::Type;
    using Position = PartialComparisonKind::Position;
    PartialComparisonKind kind = GetParam();
    if (ins->IsIntConstant()) {
      if (kind.IsDefinitelyTrue()) {
        EXPECT_TRUE(ins->AsIntConstant()->IsTrue()) << kind << " " << *ins;
      } else if (kind.IsDefinitelyFalse()) {
        EXPECT_TRUE(ins->AsIntConstant()->IsFalse()) << kind << " " << *ins;
      } else {
        EXPECT_EQ(placement, ComparisonPlacement::kBeforeEscape);
        EXPECT_EQ(kind.target_, Target::kValue);
        // We are before escape so value is not the object
        if (kind.type_ == Type::kEquals) {
          EXPECT_TRUE(ins->AsIntConstant()->IsFalse()) << kind << " " << *ins;
        } else {
          EXPECT_TRUE(ins->AsIntConstant()->IsTrue()) << kind << " " << *ins;
        }
      }
      return;
    }
    if (placement == ComparisonPlacement::kBeforeEscape) {
      // eq is always false unless self.
      EXPECT_TRUE(ins->IsIntConstant()) << *ins;
      if (kind.type_ == Type::kEquals) {
        EXPECT_TRUE(ins->IsIntConstant() && ins->AsIntConstant()->IsFalse()) << *ins;
      } else {
        EXPECT_TRUE(ins->IsIntConstant() && ins->AsIntConstant()->IsTrue()) << *ins;
      }
    } else if (placement == ComparisonPlacement::kInEscape) {
      // Should be the same type.
      ASSERT_TRUE(ins->IsEqual() || ins->IsNotEqual()) << *ins;
      HInstruction* other = kind.position_ == Position::kLeft ? ins->AsBinaryOperation()->GetRight()
                                                              : ins->AsBinaryOperation()->GetLeft();
      if (kind.target_ == Target::kSelf) {
        EXPECT_INS_EQ(ins->AsBinaryOperation()->GetLeft(), ins->AsBinaryOperation()->GetRight())
            << " ins is: " << *ins;
      } else if (kind.target_ == Target::kNull) {
        EXPECT_INS_EQ(other, graph_->GetNullConstant()) << " ins is: " << *ins;
      } else {
        EXPECT_TRUE(other->IsStaticFieldGet()) << " ins is: " << *ins;
      }
      if (kind.type_ == Type::kEquals) {
        EXPECT_TRUE(ins->IsEqual()) << *ins;
      } else {
        EXPECT_TRUE(ins->IsNotEqual()) << *ins;
      }
    } else {
      ASSERT_EQ(placement, ComparisonPlacement::kAfterEscape);
      if (kind.type_ == Type::kEquals) {
        // obj == <anything> can only be true if (1) it's obj == obj or (2) obj has escaped.
        EXPECT_TRUE(ins->IsAnd()) << *ins;
        EXPECT_TRUE(ins->InputAt(1)->IsEqual()) << *ins;
      } else {
        // obj != <anything> is true if (2) obj has escaped.
        EXPECT_TRUE(ins->IsOr()) << *ins;
        EXPECT_TRUE(ins->InputAt(1)->IsNotEqual()) << *ins;
      }
      // Check the first part of AND is the obj-has-escaped
      EXPECT_TRUE(ins->InputAt(0)->IsNotEqual()) << *ins;
      EXPECT_TRUE(ins->InputAt(0)->InputAt(0)->IsPhi()) << *ins;
      EXPECT_TRUE(ins->InputAt(0)->InputAt(1)->IsNullConstant()) << *ins;
      // Check the second part of AND is the eq other
      EXPECT_INS_EQ(ins->InputAt(1)->InputAt(kind.position_ == Position::kLeft ? 0 : 1),
                    ins->InputAt(0)->InputAt(0))
          << *ins;
      // EXPECT_TRUE(false) << "figure what to do here";
    }
  }

  struct ComparisonInstructions {
    void AddSetup(HBasicBlock* blk) const {
      for (HInstruction* i : setup_instructions_) {
        blk->AddInstruction(i);
      }
    }
    void AddEnvironment(HEnvironment* env) const {
      for (HInstruction* i : setup_instructions_) {
        if (i->NeedsEnvironment()) {
          i->CopyEnvironmentFrom(env);
        }
      }
    }

    const std::vector<HInstruction*> setup_instructions_;
    HInstruction* const cmp_;
  };
  ComparisonInstructions GetComparisonInstructions(HInstruction* partial) {
    PartialComparisonKind kind = GetParam();
    std::vector<HInstruction*> setup;
    HInstruction* target_other;
    switch (kind.target_) {
      case PartialComparisonKind::Target::kSelf:
        target_other = partial;
        break;
      case PartialComparisonKind::Target::kNull:
        target_other = graph_->GetNullConstant();
        break;
      case PartialComparisonKind::Target::kValue: {
        HInstruction* cls = new (GetAllocator()) HLoadClass(graph_->GetCurrentMethod(),
                                                            dex::TypeIndex(20),
                                                            graph_->GetDexFile(),
                                                            ScopedNullHandle<mirror::Class>(),
                                                            false,
                                                            0,
                                                            false);
        HInstruction* static_read = new (GetAllocator()) HStaticFieldGet(cls,
                                                                         nullptr,
                                                                         DataType::Type::kReference,
                                                                         MemberOffset(40),
                                                                         false,
                                                                         0,
                                                                         0,
                                                                         graph_->GetDexFile(),
                                                                         0);
        setup.push_back(cls);
        setup.push_back(static_read);
        target_other = static_read;
        break;
      }
    }
    HInstruction* target_left;
    HInstruction* target_right;
    std::tie(target_left, target_right) = kind.position_ == PartialComparisonKind::Position::kLeft
                                              ? std::pair{partial, target_other}
                                              : std::pair{target_other, partial};
    HInstruction* cmp =
        kind.type_ == PartialComparisonKind::Type::kEquals
            ? static_cast<HInstruction*>(new (GetAllocator()) HEqual(target_left, target_right))
            : static_cast<HInstruction*>(new (GetAllocator()) HNotEqual(target_left, target_right));
    return {setup, cmp};
  }
};

TEST_F(LoadStoreEliminationTest, ArrayGetSetElimination) {
  CreateTestControlFlowGraph();

  HInstruction* c1 = graph_->GetIntConstant(1);
  HInstruction* c2 = graph_->GetIntConstant(2);
  HInstruction* c3 = graph_->GetIntConstant(3);

  // array[1] = 1;
  // x = array[1];  <--- Remove.
  // y = array[2];
  // array[1] = 1;  <--- Remove, since it stores same value.
  // array[i] = 3;  <--- MAY alias.
  // array[1] = 1;  <--- Cannot remove, even if it stores the same value.
  AddArraySet(entry_block_, array_, c1, c1);
  HInstruction* load1 = AddArrayGet(entry_block_, array_, c1);
  HInstruction* load2 = AddArrayGet(entry_block_, array_, c2);
  HInstruction* store1 = AddArraySet(entry_block_, array_, c1, c1);
  AddArraySet(entry_block_, array_, i_, c3);
  HInstruction* store2 = AddArraySet(entry_block_, array_, c1, c1);

  PerformLSE();

  ASSERT_TRUE(IsRemoved(load1));
  ASSERT_FALSE(IsRemoved(load2));
  ASSERT_TRUE(IsRemoved(store1));
  ASSERT_FALSE(IsRemoved(store2));
}

TEST_F(LoadStoreEliminationTest, SameHeapValue1) {
  CreateTestControlFlowGraph();

  HInstruction* c1 = graph_->GetIntConstant(1);
  HInstruction* c2 = graph_->GetIntConstant(2);

  // Test LSE handling same value stores on array.
  // array[1] = 1;
  // array[2] = 1;
  // array[1] = 1;  <--- Can remove.
  // array[1] = 2;  <--- Can NOT remove.
  AddArraySet(entry_block_, array_, c1, c1);
  AddArraySet(entry_block_, array_, c2, c1);
  HInstruction* store1 = AddArraySet(entry_block_, array_, c1, c1);
  HInstruction* store2 = AddArraySet(entry_block_, array_, c1, c2);

  PerformLSE();

  ASSERT_TRUE(IsRemoved(store1));
  ASSERT_FALSE(IsRemoved(store2));
}

TEST_F(LoadStoreEliminationTest, SameHeapValue2) {
  CreateTestControlFlowGraph();

  // Test LSE handling same value stores on vector.
  // vdata = [0x1, 0x2, 0x3, 0x4, ...]
  // VecStore array[i...] = vdata;
  // VecStore array[j...] = vdata;  <--- MAY ALIAS.
  // VecStore array[i...] = vdata;  <--- Cannot Remove, even if it's same value.
  AddVecStore(entry_block_, array_, i_);
  AddVecStore(entry_block_, array_, j_);
  HInstruction* vstore = AddVecStore(entry_block_, array_, i_);

  PerformLSE();

  ASSERT_FALSE(IsRemoved(vstore));
}

TEST_F(LoadStoreEliminationTest, SameHeapValue3) {
  CreateTestControlFlowGraph();

  // VecStore array[i...] = vdata;
  // VecStore array[i+1...] = vdata;  <--- MAY alias due to partial overlap.
  // VecStore array[i...] = vdata;    <--- Cannot remove, even if it's same value.
  AddVecStore(entry_block_, array_, i_);
  AddVecStore(entry_block_, array_, i_add1_);
  HInstruction* vstore = AddVecStore(entry_block_, array_, i_);

  PerformLSE();

  ASSERT_FALSE(IsRemoved(vstore));
}

TEST_F(LoadStoreEliminationTest, OverlappingLoadStore) {
  CreateTestControlFlowGraph();

  HInstruction* c1 = graph_->GetIntConstant(1);

  // Test LSE handling array LSE when there is vector store in between.
  // a[i] = 1;
  // .. = a[i];                <-- Remove.
  // a[i,i+1,i+2,i+3] = data;  <-- PARTIAL OVERLAP !
  // .. = a[i];                <-- Cannot remove.
  AddArraySet(entry_block_, array_, i_, c1);
  HInstruction* load1 = AddArrayGet(entry_block_, array_, i_);
  AddVecStore(entry_block_, array_, i_);
  HInstruction* load2 = AddArrayGet(entry_block_, array_, i_);

  // Test LSE handling vector load/store partial overlap.
  // a[i,i+1,i+2,i+3] = data;
  // a[i+4,i+5,i+6,i+7] = data;
  // .. = a[i,i+1,i+2,i+3];
  // .. = a[i+4,i+5,i+6,i+7];
  // a[i+1,i+2,i+3,i+4] = data;  <-- PARTIAL OVERLAP !
  // .. = a[i,i+1,i+2,i+3];
  // .. = a[i+4,i+5,i+6,i+7];
  AddVecStore(entry_block_, array_, i_);
  AddVecStore(entry_block_, array_, i_add4_);
  HInstruction* vload1 = AddVecLoad(entry_block_, array_, i_);
  HInstruction* vload2 = AddVecLoad(entry_block_, array_, i_add4_);
  AddVecStore(entry_block_, array_, i_add1_);
  HInstruction* vload3 = AddVecLoad(entry_block_, array_, i_);
  HInstruction* vload4 = AddVecLoad(entry_block_, array_, i_add4_);

  // Test LSE handling vector LSE when there is array store in between.
  // a[i,i+1,i+2,i+3] = data;
  // a[i+1] = 1;                 <-- PARTIAL OVERLAP !
  // .. = a[i,i+1,i+2,i+3];
  AddVecStore(entry_block_, array_, i_);
  AddArraySet(entry_block_, array_, i_, c1);
  HInstruction* vload5 = AddVecLoad(entry_block_, array_, i_);

  PerformLSE();

  ASSERT_TRUE(IsRemoved(load1));
  ASSERT_FALSE(IsRemoved(load2));

  ASSERT_TRUE(IsRemoved(vload1));
  ASSERT_TRUE(IsRemoved(vload2));
  ASSERT_FALSE(IsRemoved(vload3));
  ASSERT_FALSE(IsRemoved(vload4));

  ASSERT_FALSE(IsRemoved(vload5));
}
// function (int[] a, int j) {
// a[j] = 1;
// for (int i=0; i<128; i++) {
//    /* doesn't do any write */
// }
// a[j] = 1;
TEST_F(LoadStoreEliminationTest, StoreAfterLoopWithoutSideEffects) {
  CreateTestControlFlowGraph();

  HInstruction* c1 = graph_->GetIntConstant(1);

  // a[j] = 1
  AddArraySet(pre_header_, array_, j_, c1);

  // LOOP BODY:
  // .. = a[i,i+1,i+2,i+3];
  AddVecLoad(loop_, array_, phi_);

  // a[j] = 1;
  HInstruction* array_set = AddArraySet(return_block_, array_, j_, c1);

  PerformLSE();

  ASSERT_TRUE(IsRemoved(array_set));
}

// function (int[] a, int j) {
//   int[] b = new int[128];
//   a[j] = 0;
//   for (int phi=0; phi<128; phi++) {
//     a[phi,phi+1,phi+2,phi+3] = [1,1,1,1];
//     b[phi,phi+1,phi+2,phi+3] = a[phi,phi+1,phi+2,phi+3];
//   }
//   a[j] = 0;
// }
TEST_F(LoadStoreEliminationTest, StoreAfterSIMDLoopWithSideEffects) {
  CreateTestControlFlowGraph();

  HInstruction* c0 = graph_->GetIntConstant(0);
  HInstruction* c128 = graph_->GetIntConstant(128);

  HInstruction* array_b = new (GetAllocator()) HNewArray(c0, c128, 0, 0);
  pre_header_->InsertInstructionBefore(array_b, pre_header_->GetLastInstruction());
  array_b->CopyEnvironmentFrom(suspend_check_->GetEnvironment());

  // a[j] = 0;
  AddArraySet(pre_header_, array_, j_, c0);

  // LOOP BODY:
  // a[phi,phi+1,phi+2,phi+3] = [1,1,1,1];
  // b[phi,phi+1,phi+2,phi+3] = a[phi,phi+1,phi+2,phi+3];
  AddVecStore(loop_, array_, phi_);
  HInstruction* vload = AddVecLoad(loop_, array_, phi_);
  AddVecStore(loop_, array_b, phi_, vload->AsVecLoad());

  // a[j] = 0;
  HInstruction* a_set = AddArraySet(return_block_, array_, j_, c0);

  PerformLSE();

  ASSERT_TRUE(IsRemoved(vload));
  ASSERT_FALSE(IsRemoved(a_set));  // Cannot remove due to write side-effect in the loop.
}

// function (int[] a, int j) {
//   int[] b = new int[128];
//   a[j] = 0;
//   for (int phi=0; phi<128; phi++) {
//     a[phi,phi+1,phi+2,phi+3] = [1,1,1,1];
//     b[phi,phi+1,phi+2,phi+3] = a[phi,phi+1,phi+2,phi+3];
//   }
//   x = a[j];
// }
TEST_F(LoadStoreEliminationTest, LoadAfterSIMDLoopWithSideEffects) {
  CreateTestControlFlowGraph();

  HInstruction* c0 = graph_->GetIntConstant(0);
  HInstruction* c128 = graph_->GetIntConstant(128);

  HInstruction* array_b = new (GetAllocator()) HNewArray(c0, c128, 0, 0);
  pre_header_->InsertInstructionBefore(array_b, pre_header_->GetLastInstruction());
  array_b->CopyEnvironmentFrom(suspend_check_->GetEnvironment());

  // a[j] = 0;
  AddArraySet(pre_header_, array_, j_, c0);

  // LOOP BODY:
  // a[phi,phi+1,phi+2,phi+3] = [1,1,1,1];
  // b[phi,phi+1,phi+2,phi+3] = a[phi,phi+1,phi+2,phi+3];
  AddVecStore(loop_, array_, phi_);
  HInstruction* vload = AddVecLoad(loop_, array_, phi_);
  AddVecStore(loop_, array_b, phi_, vload->AsVecLoad());

  // x = a[j];
  HInstruction* load = AddArrayGet(return_block_, array_, j_);

  PerformLSE();

  ASSERT_TRUE(IsRemoved(vload));
  ASSERT_FALSE(IsRemoved(load));  // Cannot remove due to write side-effect in the loop.
}

// Check that merging works correctly when there are VecStors in predecessors.
//
//                  vstore1: a[i,... i + 3] = [1,...1]
//                       /          \
//                      /            \
// vstore2: a[i,... i + 3] = [1,...1]  vstore3: a[i+1, ... i + 4] = [1, ... 1]
//                     \              /
//                      \            /
//                  vstore4: a[i,... i + 3] = [1,...1]
//
// Expected:
//   'vstore2' is removed.
//   'vstore3' is not removed.
//   'vstore4' is not removed. Such cases are not supported at the moment.
TEST_F(LoadStoreEliminationTest, MergePredecessorVecStores) {
  HBasicBlock* upper;
  HBasicBlock* left;
  HBasicBlock* right;
  HBasicBlock* down;
  std::tie(upper, left, right, down) = CreateDiamondShapedCFG();

  // upper: a[i,... i + 3] = [1,...1]
  HInstruction* vstore1 = AddVecStore(upper, array_, i_);
  HInstruction* vdata = vstore1->InputAt(2);

  // left: a[i,... i + 3] = [1,...1]
  HInstruction* vstore2 = AddVecStore(left, array_, i_, vdata);

  // right: a[i+1, ... i + 4] = [1, ... 1]
  HInstruction* vstore3 = AddVecStore(right, array_, i_add1_, vdata);

  // down: a[i,... i + 3] = [1,...1]
  HInstruction* vstore4 = AddVecStore(down, array_, i_, vdata);

  PerformLSE();

  ASSERT_TRUE(IsRemoved(vstore2));
  ASSERT_FALSE(IsRemoved(vstore3));
  ASSERT_FALSE(IsRemoved(vstore4));
}

// Check that merging works correctly when there are ArraySets in predecessors.
//
//          a[i] = 1
//        /          \
//       /            \
// store1: a[i] = 1  store2: a[i+1] = 1
//       \            /
//        \          /
//          store3: a[i] = 1
//
// Expected:
//   'store1' is removed.
//   'store2' is not removed.
//   'store3' is removed.
TEST_F(LoadStoreEliminationTest, MergePredecessorStores) {
  HBasicBlock* upper;
  HBasicBlock* left;
  HBasicBlock* right;
  HBasicBlock* down;
  std::tie(upper, left, right, down) = CreateDiamondShapedCFG();

  // upper: a[i,... i + 3] = [1,...1]
  AddArraySet(upper, array_, i_);

  // left: a[i,... i + 3] = [1,...1]
  HInstruction* store1 = AddArraySet(left, array_, i_);

  // right: a[i+1, ... i + 4] = [1, ... 1]
  HInstruction* store2 = AddArraySet(right, array_, i_add1_);

  // down: a[i,... i + 3] = [1,...1]
  HInstruction* store3 = AddArraySet(down, array_, i_);

  PerformLSE();

  ASSERT_TRUE(IsRemoved(store1));
  ASSERT_FALSE(IsRemoved(store2));
  ASSERT_TRUE(IsRemoved(store3));
}

// Check that redundant VStore/VLoad are removed from a SIMD loop.
//
//  LOOP BODY
//     vstore1: a[i,... i + 3] = [1,...1]
//     vload:   x = a[i,... i + 3]
//     vstore2: b[i,... i + 3] = x
//     vstore3: a[i,... i + 3] = [1,...1]
//
// Return 'a' from the method to make it escape.
//
// Expected:
//   'vstore1' is not removed.
//   'vload' is removed.
//   'vstore2' is removed because 'b' does not escape.
//   'vstore3' is removed.
TEST_F(LoadStoreEliminationTest, RedundantVStoreVLoadInLoop) {
  CreateTestControlFlowGraph();

  HInstruction* c0 = graph_->GetIntConstant(0);
  HInstruction* c128 = graph_->GetIntConstant(128);

  HInstruction* array_a = new (GetAllocator()) HNewArray(c0, c128, 0, 0);
  pre_header_->InsertInstructionBefore(array_a, pre_header_->GetLastInstruction());
  array_a->CopyEnvironmentFrom(suspend_check_->GetEnvironment());

  ASSERT_TRUE(return_block_->GetLastInstruction()->IsReturnVoid());
  HInstruction* ret = new (GetAllocator()) HReturn(array_a);
  return_block_->ReplaceAndRemoveInstructionWith(return_block_->GetLastInstruction(), ret);

  HInstruction* array_b = new (GetAllocator()) HNewArray(c0, c128, 0, 0);
  pre_header_->InsertInstructionBefore(array_b, pre_header_->GetLastInstruction());
  array_b->CopyEnvironmentFrom(suspend_check_->GetEnvironment());

  // LOOP BODY:
  //    a[i,... i + 3] = [1,...1]
  //    x = a[i,... i + 3]
  //    b[i,... i + 3] = x
  //    a[i,... i + 3] = [1,...1]
  HInstruction* vstore1 = AddVecStore(loop_, array_a, phi_);
  HInstruction* vload = AddVecLoad(loop_, array_a, phi_);
  HInstruction* vstore2 = AddVecStore(loop_, array_b, phi_, vload->AsVecLoad());
  HInstruction* vstore3 = AddVecStore(loop_, array_a, phi_, vstore1->InputAt(2));

  PerformLSE();

  ASSERT_FALSE(IsRemoved(vstore1));
  ASSERT_TRUE(IsRemoved(vload));
  ASSERT_TRUE(IsRemoved(vstore2));
  ASSERT_TRUE(IsRemoved(vstore3));
}

// Loop writes invalidate only possibly aliased heap locations.
TEST_F(LoadStoreEliminationTest, StoreAfterLoopWithSideEffects) {
  CreateTestControlFlowGraph();

  HInstruction* c0 = graph_->GetIntConstant(0);
  HInstruction* c2 = graph_->GetIntConstant(2);
  HInstruction* c128 = graph_->GetIntConstant(128);

  // array[0] = 2;
  // loop:
  //   b[i] = array[i]
  // array[0] = 2
  HInstruction* store1 = AddArraySet(entry_block_, array_, c0, c2);

  HInstruction* array_b = new (GetAllocator()) HNewArray(c0, c128, 0, 0);
  pre_header_->InsertInstructionBefore(array_b, pre_header_->GetLastInstruction());
  array_b->CopyEnvironmentFrom(suspend_check_->GetEnvironment());

  HInstruction* load = AddArrayGet(loop_, array_, phi_);
  HInstruction* store2 = AddArraySet(loop_, array_b, phi_, load);

  HInstruction* store3 = AddArraySet(return_block_, array_, c0, c2);

  PerformLSE();

  ASSERT_FALSE(IsRemoved(store1));
  ASSERT_TRUE(IsRemoved(store2));
  ASSERT_TRUE(IsRemoved(store3));
}

// Loop writes invalidate only possibly aliased heap locations.
TEST_F(LoadStoreEliminationTest, StoreAfterLoopWithSideEffects2) {
  CreateTestControlFlowGraph();

  // Add another array parameter that may alias with `array_`.
  // Note: We're not adding it to the suspend check environment.
  AddParameter(new (GetAllocator()) HParameterValue(
      graph_->GetDexFile(), dex::TypeIndex(0), 3, DataType::Type::kInt32));
  HInstruction* array2 = parameters_.back();

  HInstruction* c0 = graph_->GetIntConstant(0);
  HInstruction* c2 = graph_->GetIntConstant(2);

  // array[0] = 2;
  // loop:
  //   array2[i] = array[i]
  // array[0] = 2
  HInstruction* store1 = AddArraySet(entry_block_, array_, c0, c2);

  HInstruction* load = AddArrayGet(loop_, array_, phi_);
  HInstruction* store2 = AddArraySet(loop_, array2, phi_, load);

  HInstruction* store3 = AddArraySet(return_block_, array_, c0, c2);

  PerformLSE();

  ASSERT_FALSE(IsRemoved(store1));
  ASSERT_FALSE(IsRemoved(store2));
  ASSERT_FALSE(IsRemoved(store3));
}

// As it is not allowed to use defaults for VecLoads, check if there is a new created array
// a VecLoad used in a loop and after it is not replaced with a default.
TEST_F(LoadStoreEliminationTest, VLoadDefaultValueInLoopWithoutWriteSideEffects) {
  CreateTestControlFlowGraph();

  HInstruction* c0 = graph_->GetIntConstant(0);
  HInstruction* c128 = graph_->GetIntConstant(128);

  HInstruction* array_a = new (GetAllocator()) HNewArray(c0, c128, 0, 0);
  pre_header_->InsertInstructionBefore(array_a, pre_header_->GetLastInstruction());
  array_a->CopyEnvironmentFrom(suspend_check_->GetEnvironment());

  // LOOP BODY:
  //    v = a[i,... i + 3]
  // array[0,... 3] = v
  HInstruction* vload = AddVecLoad(loop_, array_a, phi_);
  HInstruction* vstore = AddVecStore(return_block_, array_, c0, vload->AsVecLoad());

  PerformLSE();

  ASSERT_FALSE(IsRemoved(vload));
  ASSERT_FALSE(IsRemoved(vstore));
}

// As it is not allowed to use defaults for VecLoads, check if there is a new created array
// a VecLoad is not replaced with a default.
TEST_F(LoadStoreEliminationTest, VLoadDefaultValue) {
  CreateTestControlFlowGraph();

  HInstruction* c0 = graph_->GetIntConstant(0);
  HInstruction* c128 = graph_->GetIntConstant(128);

  HInstruction* array_a = new (GetAllocator()) HNewArray(c0, c128, 0, 0);
  pre_header_->InsertInstructionBefore(array_a, pre_header_->GetLastInstruction());
  array_a->CopyEnvironmentFrom(suspend_check_->GetEnvironment());

  // v = a[0,... 3]
  // array[0,... 3] = v
  HInstruction* vload = AddVecLoad(pre_header_, array_a, c0);
  HInstruction* vstore = AddVecStore(return_block_, array_, c0, vload->AsVecLoad());

  PerformLSE();

  ASSERT_FALSE(IsRemoved(vload));
  ASSERT_FALSE(IsRemoved(vstore));
}

// As it is allowed to use defaults for ordinary loads, check if there is a new created array
// a load used in a loop and after it is replaced with a default.
TEST_F(LoadStoreEliminationTest, LoadDefaultValueInLoopWithoutWriteSideEffects) {
  CreateTestControlFlowGraph();

  HInstruction* c0 = graph_->GetIntConstant(0);
  HInstruction* c128 = graph_->GetIntConstant(128);

  HInstruction* array_a = new (GetAllocator()) HNewArray(c0, c128, 0, 0);
  pre_header_->InsertInstructionBefore(array_a, pre_header_->GetLastInstruction());
  array_a->CopyEnvironmentFrom(suspend_check_->GetEnvironment());

  // LOOP BODY:
  //    v = a[i]
  // array[0] = v
  HInstruction* load = AddArrayGet(loop_, array_a, phi_);
  HInstruction* store = AddArraySet(return_block_, array_, c0, load);

  PerformLSE();

  ASSERT_TRUE(IsRemoved(load));
  ASSERT_FALSE(IsRemoved(store));
}

// As it is allowed to use defaults for ordinary loads, check if there is a new created array
// a load is replaced with a default.
TEST_F(LoadStoreEliminationTest, LoadDefaultValue) {
  CreateTestControlFlowGraph();

  HInstruction* c0 = graph_->GetIntConstant(0);
  HInstruction* c128 = graph_->GetIntConstant(128);

  HInstruction* array_a = new (GetAllocator()) HNewArray(c0, c128, 0, 0);
  pre_header_->InsertInstructionBefore(array_a, pre_header_->GetLastInstruction());
  array_a->CopyEnvironmentFrom(suspend_check_->GetEnvironment());

  // v = a[0]
  // array[0] = v
  HInstruction* load = AddArrayGet(pre_header_, array_a, c0);
  HInstruction* store = AddArraySet(return_block_, array_, c0, load);

  PerformLSE();

  ASSERT_TRUE(IsRemoved(load));
  ASSERT_FALSE(IsRemoved(store));
}

// As it is not allowed to use defaults for VecLoads but allowed for regular loads,
// check if there is a new created array, a VecLoad and a load used in a loop and after it,
// VecLoad is not replaced with a default but the load is.
TEST_F(LoadStoreEliminationTest, VLoadAndLoadDefaultValueInLoopWithoutWriteSideEffects) {
  CreateTestControlFlowGraph();

  HInstruction* c0 = graph_->GetIntConstant(0);
  HInstruction* c128 = graph_->GetIntConstant(128);

  HInstruction* array_a = new (GetAllocator()) HNewArray(c0, c128, 0, 0);
  pre_header_->InsertInstructionBefore(array_a, pre_header_->GetLastInstruction());
  array_a->CopyEnvironmentFrom(suspend_check_->GetEnvironment());

  // LOOP BODY:
  //    v = a[i,... i + 3]
  //    v1 = a[i]
  // array[0,... 3] = v
  // array[0] = v1
  HInstruction* vload = AddVecLoad(loop_, array_a, phi_);
  HInstruction* load = AddArrayGet(loop_, array_a, phi_);
  HInstruction* vstore = AddVecStore(return_block_, array_, c0, vload->AsVecLoad());
  HInstruction* store = AddArraySet(return_block_, array_, c0, load);

  PerformLSE();

  ASSERT_FALSE(IsRemoved(vload));
  ASSERT_TRUE(IsRemoved(load));
  ASSERT_FALSE(IsRemoved(vstore));
  ASSERT_FALSE(IsRemoved(store));
}

// As it is not allowed to use defaults for VecLoads but allowed for regular loads,
// check if there is a new created array, a VecLoad and a load,
// VecLoad is not replaced with a default but the load is.
TEST_F(LoadStoreEliminationTest, VLoadAndLoadDefaultValue) {
  CreateTestControlFlowGraph();

  HInstruction* c0 = graph_->GetIntConstant(0);
  HInstruction* c128 = graph_->GetIntConstant(128);

  HInstruction* array_a = new (GetAllocator()) HNewArray(c0, c128, 0, 0);
  pre_header_->InsertInstructionBefore(array_a, pre_header_->GetLastInstruction());
  array_a->CopyEnvironmentFrom(suspend_check_->GetEnvironment());

  // v = a[0,... 3]
  // v1 = a[0]
  // array[0,... 3] = v
  // array[0] = v1
  HInstruction* vload = AddVecLoad(pre_header_, array_a, c0);
  HInstruction* load = AddArrayGet(pre_header_, array_a, c0);
  HInstruction* vstore = AddVecStore(return_block_, array_, c0, vload->AsVecLoad());
  HInstruction* store = AddArraySet(return_block_, array_, c0, load);

  PerformLSE();

  ASSERT_FALSE(IsRemoved(vload));
  ASSERT_TRUE(IsRemoved(load));
  ASSERT_FALSE(IsRemoved(vstore));
  ASSERT_FALSE(IsRemoved(store));
}

// It is not allowed to use defaults for VecLoads. However it should not prevent from removing
// loads getting the same value.
// Check a load getting a known value is eliminated (a loop test case).
TEST_F(LoadStoreEliminationTest, VLoadDefaultValueAndVLoadInLoopWithoutWriteSideEffects) {
  CreateTestControlFlowGraph();

  HInstruction* c0 = graph_->GetIntConstant(0);
  HInstruction* c128 = graph_->GetIntConstant(128);

  HInstruction* array_a = new (GetAllocator()) HNewArray(c0, c128, 0, 0);
  pre_header_->InsertInstructionBefore(array_a, pre_header_->GetLastInstruction());
  array_a->CopyEnvironmentFrom(suspend_check_->GetEnvironment());

  // LOOP BODY:
  //    v = a[i,... i + 3]
  //    v1 = a[i,... i + 3]
  // array[0,... 3] = v
  // array[128,... 131] = v1
  HInstruction* vload1 = AddVecLoad(loop_, array_a, phi_);
  HInstruction* vload2 = AddVecLoad(loop_, array_a, phi_);
  HInstruction* vstore1 = AddVecStore(return_block_, array_, c0, vload1->AsVecLoad());
  HInstruction* vstore2 = AddVecStore(return_block_, array_, c128, vload2->AsVecLoad());

  PerformLSE();

  ASSERT_FALSE(IsRemoved(vload1));
  ASSERT_TRUE(IsRemoved(vload2));
  ASSERT_FALSE(IsRemoved(vstore1));
  ASSERT_FALSE(IsRemoved(vstore2));
}

// It is not allowed to use defaults for VecLoads. However it should not prevent from removing
// loads getting the same value.
// Check a load getting a known value is eliminated.
TEST_F(LoadStoreEliminationTest, VLoadDefaultValueAndVLoad) {
  CreateTestControlFlowGraph();

  HInstruction* c0 = graph_->GetIntConstant(0);
  HInstruction* c128 = graph_->GetIntConstant(128);

  HInstruction* array_a = new (GetAllocator()) HNewArray(c0, c128, 0, 0);
  pre_header_->InsertInstructionBefore(array_a, pre_header_->GetLastInstruction());
  array_a->CopyEnvironmentFrom(suspend_check_->GetEnvironment());

  // v = a[0,... 3]
  // v1 = a[0,... 3]
  // array[0,... 3] = v
  // array[128,... 131] = v1
  HInstruction* vload1 = AddVecLoad(pre_header_, array_a, c0);
  HInstruction* vload2 = AddVecLoad(pre_header_, array_a, c0);
  HInstruction* vstore1 = AddVecStore(return_block_, array_, c0, vload1->AsVecLoad());
  HInstruction* vstore2 = AddVecStore(return_block_, array_, c128, vload2->AsVecLoad());

  PerformLSE();

  ASSERT_FALSE(IsRemoved(vload1));
  ASSERT_TRUE(IsRemoved(vload2));
  ASSERT_FALSE(IsRemoved(vstore1));
  ASSERT_FALSE(IsRemoved(vstore2));
}

// Object o = new Obj();
// // Needed because otherwise we short-circuit LSA since GVN would get almost
// // everything other than this. Also since this isn't expected to be a very
// // common pattern it's not worth changing the LSA logic.
// o.foo = 3;
// return o.shadow$_klass_;
TEST_F(LoadStoreEliminationTest, DefaultShadowClass) {
  CreateGraph();
  AdjacencyListGraph blocks(
      graph_, GetAllocator(), "entry", "exit", {{"entry", "main"}, {"main", "exit"}});
#define GET_BLOCK(name) HBasicBlock* name = blocks.Get(#name)
  GET_BLOCK(entry);
  GET_BLOCK(main);
  GET_BLOCK(exit);
#undef GET_BLOCK

  HInstruction* suspend_check = new (GetAllocator()) HSuspendCheck();
  entry->AddInstruction(suspend_check);
  entry->AddInstruction(new (GetAllocator()) HGoto());
  ArenaVector<HInstruction*> current_locals({}, GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(suspend_check, &current_locals);

  HInstruction* cls = new (GetAllocator()) HLoadClass(graph_->GetCurrentMethod(),
                                                      dex::TypeIndex(10),
                                                      graph_->GetDexFile(),
                                                      ScopedNullHandle<mirror::Class>(),
                                                      false,
                                                      0,
                                                      false);
  HInstruction* new_inst =
      new (GetAllocator()) HNewInstance(cls,
                                        0,
                                        dex::TypeIndex(10),
                                        graph_->GetDexFile(),
                                        false,
                                        QuickEntrypointEnum::kQuickAllocObjectInitialized);
  HInstruction* const_fence = new (GetAllocator()) HConstructorFence(new_inst, 0, GetAllocator());
  HInstruction* set_field = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                   graph_->GetIntConstant(33),
                                                                   nullptr,
                                                                   DataType::Type::kReference,
                                                                   MemberOffset(10),
                                                                   false,
                                                                   0,
                                                                   0,
                                                                   graph_->GetDexFile(),
                                                                   0);
  HInstruction* get_field = new (GetAllocator()) HInstanceFieldGet(new_inst,
                                                                   nullptr,
                                                                   DataType::Type::kReference,
                                                                   mirror::Object::ClassOffset(),
                                                                   false,
                                                                   0,
                                                                   0,
                                                                   graph_->GetDexFile(),
                                                                   0);
  HInstruction* return_val = new (GetAllocator()) HReturn(get_field);
  main->AddInstruction(cls);
  main->AddInstruction(new_inst);
  main->AddInstruction(const_fence);
  main->AddInstruction(set_field);
  main->AddInstruction(get_field);
  main->AddInstruction(return_val);
  cls->CopyEnvironmentFrom(suspend_check->GetEnvironment());
  new_inst->CopyEnvironmentFrom(suspend_check->GetEnvironment());

  exit->AddInstruction(new (GetAllocator()) HExit());

  graph_->ClearDominanceInformation();
  PerformLSE();

  EXPECT_TRUE(IsRemoved(new_inst));
  EXPECT_TRUE(IsRemoved(const_fence));
  EXPECT_TRUE(IsRemoved(get_field));
  EXPECT_TRUE(IsRemoved(set_field));
  EXPECT_FALSE(IsRemoved(cls));
  EXPECT_EQ(cls, return_val->InputAt(0));
}

// void DO_CAL() {
//   int i = 1;
//   int[] w = new int[80];
//   int t = 0;
//   while (i < 80) {
//     w[i] = PLEASE_INTERLEAVE(w[i - 1], 1)
//     t = PLEASE_SELECT(w[i], t);
//     i++;
//   }
//   return t;
// }
TEST_F(LoadStoreEliminationTest, ArrayLoopOverlap) {
  ScopedObjectAccess soa(Thread::Current());
  VariableSizedHandleScope vshs(soa.Self());
  CreateGraph(&vshs);
  AdjacencyListGraph blocks(graph_,
                            GetAllocator(),
                            "entry",
                            "exit",
                            { { "entry", "loop_pre_header" },
                              { "loop_pre_header", "loop_entry" },
                              { "loop_entry", "loop_body" },
                              { "loop_entry", "loop_post" },
                              { "loop_body", "loop_entry" },
                              { "loop_post", "exit" } });
#define GET_BLOCK(name) HBasicBlock* name = blocks.Get(#name)
  GET_BLOCK(entry);
  GET_BLOCK(loop_pre_header);
  GET_BLOCK(loop_entry);
  GET_BLOCK(loop_body);
  GET_BLOCK(loop_post);
  GET_BLOCK(exit);
#undef GET_BLOCK

  HInstruction* zero_const = graph_->GetConstant(DataType::Type::kInt32, 0);
  HInstruction* one_const = graph_->GetConstant(DataType::Type::kInt32, 1);
  HInstruction* eighty_const = graph_->GetConstant(DataType::Type::kInt32, 80);
  HInstruction* entry_goto = new (GetAllocator()) HGoto();
  entry->AddInstruction(entry_goto);

  HInstruction* alloc_w = new (GetAllocator()) HNewArray(zero_const, eighty_const, 0, 0);
  HInstruction* pre_header_goto = new (GetAllocator()) HGoto();
  loop_pre_header->AddInstruction(alloc_w);
  loop_pre_header->AddInstruction(pre_header_goto);
  // environment
  ArenaVector<HInstruction*> alloc_locals({}, GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(alloc_w, &alloc_locals);

  // loop-start
  HPhi* i_phi = new (GetAllocator()) HPhi(GetAllocator(), 0, 0, DataType::Type::kInt32);
  HPhi* t_phi = new (GetAllocator()) HPhi(GetAllocator(), 1, 0, DataType::Type::kInt32);
  HInstruction* suspend = new (GetAllocator()) HSuspendCheck();
  HInstruction* i_cmp_top = new (GetAllocator()) HGreaterThanOrEqual(i_phi, eighty_const);
  HInstruction* loop_start_branch = new (GetAllocator()) HIf(i_cmp_top);
  loop_entry->AddPhi(i_phi);
  loop_entry->AddPhi(t_phi);
  loop_entry->AddInstruction(suspend);
  loop_entry->AddInstruction(i_cmp_top);
  loop_entry->AddInstruction(loop_start_branch);
  CHECK_EQ(loop_entry->GetSuccessors().size(), 2u);
  if (loop_entry->GetNormalSuccessors()[1] != loop_body) {
    loop_entry->SwapSuccessors();
  }
  CHECK_EQ(loop_entry->GetPredecessors().size(), 2u);
  if (loop_entry->GetPredecessors()[0] != loop_pre_header) {
    loop_entry->SwapPredecessors();
  }
  i_phi->AddInput(one_const);
  t_phi->AddInput(zero_const);

  // environment
  ArenaVector<HInstruction*> suspend_locals({ alloc_w, i_phi, t_phi },
                                            GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(suspend, &suspend_locals);

  // BODY
  HInstruction* last_i = new (GetAllocator()) HSub(DataType::Type::kInt32, i_phi, one_const);
  HInstruction* last_get =
      new (GetAllocator()) HArrayGet(alloc_w, last_i, DataType::Type::kInt32, 0);
  HInvoke* body_value = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            2,
                            DataType::Type::kInt32,
                            0,
                            { nullptr, 0 },
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            { nullptr, 0 },
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  body_value->SetRawInputAt(0, last_get);
  body_value->SetRawInputAt(1, one_const);
  HInstruction* body_set =
      new (GetAllocator()) HArraySet(alloc_w, i_phi, body_value, DataType::Type::kInt32, 0);
  HInstruction* body_get =
      new (GetAllocator()) HArrayGet(alloc_w, i_phi, DataType::Type::kInt32, 0);
  HInvoke* t_next = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            2,
                            DataType::Type::kInt32,
                            0,
                            { nullptr, 0 },
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            { nullptr, 0 },
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  t_next->SetRawInputAt(0, body_get);
  t_next->SetRawInputAt(1, t_phi);
  HInstruction* i_next = new (GetAllocator()) HAdd(DataType::Type::kInt32, i_phi, one_const);
  HInstruction* body_goto = new (GetAllocator()) HGoto();
  loop_body->AddInstruction(last_i);
  loop_body->AddInstruction(last_get);
  loop_body->AddInstruction(body_value);
  loop_body->AddInstruction(body_set);
  loop_body->AddInstruction(body_get);
  loop_body->AddInstruction(t_next);
  loop_body->AddInstruction(i_next);
  loop_body->AddInstruction(body_goto);
  body_value->CopyEnvironmentFrom(suspend->GetEnvironment());

  i_phi->AddInput(i_next);
  t_phi->AddInput(t_next);
  t_next->CopyEnvironmentFrom(suspend->GetEnvironment());

  // loop-post
  HInstruction* return_inst = new (GetAllocator()) HReturn(t_phi);
  loop_post->AddInstruction(return_inst);

  // exit
  HInstruction* exit_inst = new (GetAllocator()) HExit();
  exit->AddInstruction(exit_inst);

  graph_->ClearDominanceInformation();
  graph_->ClearLoopInformation();
  PerformLSE();

  // TODO Technically this is optimizable. LSE just needs to add phis to keep
  // track of the last `N` values set where `N` is how many locations we can go
  // back into the array.
  if (IsRemoved(last_get)) {
    // If we were able to remove the previous read the entire array should be removable.
    EXPECT_TRUE(IsRemoved(body_set));
    EXPECT_TRUE(IsRemoved(alloc_w));
  } else {
    // This is the branch we actually take for now. If we rely on being able to
    // read the array we'd better remember to write to it as well.
    EXPECT_FALSE(IsRemoved(body_set));
  }
  // The last 'get' should always be removable.
  EXPECT_TRUE(IsRemoved(body_get));
}

// void DO_CAL2() {
//   int i = 1;
//   int[] w = new int[80];
//   int t = 0;
//   while (i < 80) {
//     w[i] = PLEASE_INTERLEAVE(w[i - 1], 1) // <-- removed
//     t = PLEASE_SELECT(w[i], t);
//     w[i] = PLEASE_INTERLEAVE(w[i - 1], 1) // <-- removed
//     t = PLEASE_SELECT(w[i], t);
//     w[i] = PLEASE_INTERLEAVE(w[i - 1], 1) // <-- kept
//     t = PLEASE_SELECT(w[i], t);
//     i++;
//   }
//   return t;
// }
TEST_F(LoadStoreEliminationTest, ArrayLoopOverlap2) {
  ScopedObjectAccess soa(Thread::Current());
  VariableSizedHandleScope vshs(soa.Self());
  CreateGraph(&vshs);
  AdjacencyListGraph blocks(graph_,
                            GetAllocator(),
                            "entry",
                            "exit",
                            { { "entry", "loop_pre_header" },
                              { "loop_pre_header", "loop_entry" },
                              { "loop_entry", "loop_body" },
                              { "loop_entry", "loop_post" },
                              { "loop_body", "loop_entry" },
                              { "loop_post", "exit" } });
#define GET_BLOCK(name) HBasicBlock* name = blocks.Get(#name)
  GET_BLOCK(entry);
  GET_BLOCK(loop_pre_header);
  GET_BLOCK(loop_entry);
  GET_BLOCK(loop_body);
  GET_BLOCK(loop_post);
  GET_BLOCK(exit);
#undef GET_BLOCK

  HInstruction* zero_const = graph_->GetConstant(DataType::Type::kInt32, 0);
  HInstruction* one_const = graph_->GetConstant(DataType::Type::kInt32, 1);
  HInstruction* eighty_const = graph_->GetConstant(DataType::Type::kInt32, 80);
  HInstruction* entry_goto = new (GetAllocator()) HGoto();
  entry->AddInstruction(entry_goto);

  HInstruction* alloc_w = new (GetAllocator()) HNewArray(zero_const, eighty_const, 0, 0);
  HInstruction* pre_header_goto = new (GetAllocator()) HGoto();
  loop_pre_header->AddInstruction(alloc_w);
  loop_pre_header->AddInstruction(pre_header_goto);
  // environment
  ArenaVector<HInstruction*> alloc_locals({}, GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(alloc_w, &alloc_locals);

  // loop-start
  HPhi* i_phi = new (GetAllocator()) HPhi(GetAllocator(), 0, 0, DataType::Type::kInt32);
  HPhi* t_phi = new (GetAllocator()) HPhi(GetAllocator(), 1, 0, DataType::Type::kInt32);
  HInstruction* suspend = new (GetAllocator()) HSuspendCheck();
  HInstruction* i_cmp_top = new (GetAllocator()) HGreaterThanOrEqual(i_phi, eighty_const);
  HInstruction* loop_start_branch = new (GetAllocator()) HIf(i_cmp_top);
  loop_entry->AddPhi(i_phi);
  loop_entry->AddPhi(t_phi);
  loop_entry->AddInstruction(suspend);
  loop_entry->AddInstruction(i_cmp_top);
  loop_entry->AddInstruction(loop_start_branch);
  CHECK_EQ(loop_entry->GetSuccessors().size(), 2u);
  if (loop_entry->GetNormalSuccessors()[1] != loop_body) {
    loop_entry->SwapSuccessors();
  }
  CHECK_EQ(loop_entry->GetPredecessors().size(), 2u);
  if (loop_entry->GetPredecessors()[0] != loop_pre_header) {
    loop_entry->SwapPredecessors();
  }
  i_phi->AddInput(one_const);
  t_phi->AddInput(zero_const);

  // environment
  ArenaVector<HInstruction*> suspend_locals({ alloc_w, i_phi, t_phi },
                                            GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(suspend, &suspend_locals);

  // BODY
  HInstruction* last_i = new (GetAllocator()) HSub(DataType::Type::kInt32, i_phi, one_const);
  HInstruction *last_get_1, *last_get_2, *last_get_3;
  HInstruction *body_value_1, *body_value_2, *body_value_3;
  HInstruction *body_set_1, *body_set_2, *body_set_3;
  HInstruction *body_get_1, *body_get_2, *body_get_3;
  HInstruction *t_next_1, *t_next_2, *t_next_3;
  auto make_instructions = [&](HInstruction* last_t_value) {
    HInstruction* last_get =
        new (GetAllocator()) HArrayGet(alloc_w, last_i, DataType::Type::kInt32, 0);
    HInvoke* body_value = new (GetAllocator())
        HInvokeStaticOrDirect(GetAllocator(),
                              2,
                              DataType::Type::kInt32,
                              0,
                              { nullptr, 0 },
                              nullptr,
                              {},
                              InvokeType::kStatic,
                              { nullptr, 0 },
                              HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
    body_value->SetRawInputAt(0, last_get);
    body_value->SetRawInputAt(1, one_const);
    HInstruction* body_set =
        new (GetAllocator()) HArraySet(alloc_w, i_phi, body_value, DataType::Type::kInt32, 0);
    HInstruction* body_get =
        new (GetAllocator()) HArrayGet(alloc_w, i_phi, DataType::Type::kInt32, 0);
    HInvoke* t_next = new (GetAllocator())
        HInvokeStaticOrDirect(GetAllocator(),
                              2,
                              DataType::Type::kInt32,
                              0,
                              { nullptr, 0 },
                              nullptr,
                              {},
                              InvokeType::kStatic,
                              { nullptr, 0 },
                              HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
    t_next->SetRawInputAt(0, body_get);
    t_next->SetRawInputAt(1, last_t_value);
    loop_body->AddInstruction(last_get);
    loop_body->AddInstruction(body_value);
    loop_body->AddInstruction(body_set);
    loop_body->AddInstruction(body_get);
    loop_body->AddInstruction(t_next);
    return std::make_tuple(last_get, body_value, body_set, body_get, t_next);
  };
  std::tie(last_get_1, body_value_1, body_set_1, body_get_1, t_next_1) = make_instructions(t_phi);
  std::tie(last_get_2, body_value_2, body_set_2, body_get_2, t_next_2) =
      make_instructions(t_next_1);
  std::tie(last_get_3, body_value_3, body_set_3, body_get_3, t_next_3) =
      make_instructions(t_next_2);
  HInstruction* i_next = new (GetAllocator()) HAdd(DataType::Type::kInt32, i_phi, one_const);
  HInstruction* body_goto = new (GetAllocator()) HGoto();
  loop_body->InsertInstructionBefore(last_i, last_get_1);
  loop_body->AddInstruction(i_next);
  loop_body->AddInstruction(body_goto);
  body_value_1->CopyEnvironmentFrom(suspend->GetEnvironment());
  body_value_2->CopyEnvironmentFrom(suspend->GetEnvironment());
  body_value_3->CopyEnvironmentFrom(suspend->GetEnvironment());

  i_phi->AddInput(i_next);
  t_phi->AddInput(t_next_3);
  t_next_1->CopyEnvironmentFrom(suspend->GetEnvironment());
  t_next_2->CopyEnvironmentFrom(suspend->GetEnvironment());
  t_next_3->CopyEnvironmentFrom(suspend->GetEnvironment());

  // loop-post
  HInstruction* return_inst = new (GetAllocator()) HReturn(t_phi);
  loop_post->AddInstruction(return_inst);

  // exit
  HInstruction* exit_inst = new (GetAllocator()) HExit();
  exit->AddInstruction(exit_inst);

  graph_->ClearDominanceInformation();
  graph_->ClearLoopInformation();
  PerformLSE();

  // TODO Technically this is optimizable. LSE just needs to add phis to keep
  // track of the last `N` values set where `N` is how many locations we can go
  // back into the array.
  if (IsRemoved(last_get_1)) {
    // If we were able to remove the previous read the entire array should be removable.
    EXPECT_TRUE(IsRemoved(body_set_1));
    EXPECT_TRUE(IsRemoved(body_set_2));
    EXPECT_TRUE(IsRemoved(body_set_3));
    EXPECT_TRUE(IsRemoved(last_get_1));
    EXPECT_TRUE(IsRemoved(last_get_2));
    EXPECT_TRUE(IsRemoved(alloc_w));
  } else {
    // This is the branch we actually take for now. If we rely on being able to
    // read the array we'd better remember to write to it as well.
    EXPECT_FALSE(IsRemoved(body_set_3));
  }
  // The last 'get' should always be removable.
  EXPECT_TRUE(IsRemoved(body_get_1));
  EXPECT_TRUE(IsRemoved(body_get_2));
  EXPECT_TRUE(IsRemoved(body_get_3));
  // shadowed writes should always be removed
  EXPECT_TRUE(IsRemoved(body_set_1));
  EXPECT_TRUE(IsRemoved(body_set_2));
}

TEST_F(LoadStoreEliminationTest, ArrayNonLoopPhi) {
  ScopedObjectAccess soa(Thread::Current());
  VariableSizedHandleScope vshs(soa.Self());
  CreateGraph(&vshs);
  AdjacencyListGraph blocks(graph_,
                            GetAllocator(),
                            "entry",
                            "exit",
                            { { "entry", "start" },
                              { "start", "left" },
                              { "start", "right" },
                              { "left", "ret" },
                              { "right", "ret" },
                              { "ret", "exit" } });
#define GET_BLOCK(name) HBasicBlock* name = blocks.Get(#name)
  GET_BLOCK(entry);
  GET_BLOCK(start);
  GET_BLOCK(left);
  GET_BLOCK(right);
  GET_BLOCK(ret);
  GET_BLOCK(exit);
#undef GET_BLOCK

  HInstruction* zero_const = graph_->GetConstant(DataType::Type::kInt32, 0);
  HInstruction* one_const = graph_->GetConstant(DataType::Type::kInt32, 1);
  HInstruction* two_const = graph_->GetConstant(DataType::Type::kInt32, 2);
  HInstruction* param = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 0, DataType::Type::kBool);
  HInstruction* entry_goto = new (GetAllocator()) HGoto();
  entry->AddInstruction(param);
  entry->AddInstruction(entry_goto);

  HInstruction* alloc_w = new (GetAllocator()) HNewArray(zero_const, two_const, 0, 0);
  HInstruction* branch = new (GetAllocator()) HIf(param);
  start->AddInstruction(alloc_w);
  start->AddInstruction(branch);
  // environment
  ArenaVector<HInstruction*> alloc_locals({}, GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(alloc_w, &alloc_locals);

  // left
  HInvoke* left_value = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            1,
                            DataType::Type::kInt32,
                            0,
                            { nullptr, 0 },
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            { nullptr, 0 },
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  left_value->SetRawInputAt(0, zero_const);
  HInstruction* left_set_1 =
      new (GetAllocator()) HArraySet(alloc_w, zero_const, left_value, DataType::Type::kInt32, 0);
  HInstruction* left_set_2 =
      new (GetAllocator()) HArraySet(alloc_w, one_const, zero_const, DataType::Type::kInt32, 0);
  HInstruction* left_goto = new (GetAllocator()) HGoto();
  left->AddInstruction(left_value);
  left->AddInstruction(left_set_1);
  left->AddInstruction(left_set_2);
  left->AddInstruction(left_goto);
  ArenaVector<HInstruction*> left_locals({ alloc_w },
                                         GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(left_value, &alloc_locals);

  // right
  HInvoke* right_value = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            1,
                            DataType::Type::kInt32,
                            0,
                            { nullptr, 0 },
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            { nullptr, 0 },
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  right_value->SetRawInputAt(0, one_const);
  HInstruction* right_set_1 =
      new (GetAllocator()) HArraySet(alloc_w, zero_const, right_value, DataType::Type::kInt32, 0);
  HInstruction* right_set_2 =
      new (GetAllocator()) HArraySet(alloc_w, one_const, zero_const, DataType::Type::kInt32, 0);
  HInstruction* right_goto = new (GetAllocator()) HGoto();
  right->AddInstruction(right_value);
  right->AddInstruction(right_set_1);
  right->AddInstruction(right_set_2);
  right->AddInstruction(right_goto);
  ArenaVector<HInstruction*> right_locals({ alloc_w },
                                          GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(right_value, &alloc_locals);

  // ret
  HInstruction* read_1 =
      new (GetAllocator()) HArrayGet(alloc_w, zero_const, DataType::Type::kInt32, 0);
  HInstruction* read_2 =
      new (GetAllocator()) HArrayGet(alloc_w, one_const, DataType::Type::kInt32, 0);
  HInstruction* add = new (GetAllocator()) HAdd(DataType::Type::kInt32, read_1, read_2);
  HInstruction* return_inst = new (GetAllocator()) HReturn(add);
  ret->AddInstruction(read_1);
  ret->AddInstruction(read_2);
  ret->AddInstruction(add);
  ret->AddInstruction(return_inst);

  // exit
  HInstruction* exit_inst = new (GetAllocator()) HExit();
  exit->AddInstruction(exit_inst);

  graph_->ClearDominanceInformation();
  graph_->ClearLoopInformation();
  PerformLSE();

  EXPECT_TRUE(IsRemoved(read_1));
  EXPECT_TRUE(IsRemoved(read_2));
  EXPECT_TRUE(IsRemoved(left_set_1));
  EXPECT_TRUE(IsRemoved(left_set_2));
  EXPECT_TRUE(IsRemoved(right_set_1));
  EXPECT_TRUE(IsRemoved(right_set_2));
  EXPECT_TRUE(IsRemoved(alloc_w));

  EXPECT_FALSE(IsRemoved(left_value));
  EXPECT_FALSE(IsRemoved(right_value));
}

TEST_F(LoadStoreEliminationTest, ArrayMergeDefault) {
  ScopedObjectAccess soa(Thread::Current());
  VariableSizedHandleScope vshs(soa.Self());
  CreateGraph(&vshs);
  AdjacencyListGraph blocks(graph_,
                            GetAllocator(),
                            "entry",
                            "exit",
                            { { "entry", "start" },
                              { "start", "left" },
                              { "start", "right" },
                              { "left", "ret" },
                              { "right", "ret" },
                              { "ret", "exit" } });
#define GET_BLOCK(name) HBasicBlock* name = blocks.Get(#name)
  GET_BLOCK(entry);
  GET_BLOCK(start);
  GET_BLOCK(left);
  GET_BLOCK(right);
  GET_BLOCK(ret);
  GET_BLOCK(exit);
#undef GET_BLOCK

  HInstruction* zero_const = graph_->GetConstant(DataType::Type::kInt32, 0);
  HInstruction* one_const = graph_->GetConstant(DataType::Type::kInt32, 1);
  HInstruction* two_const = graph_->GetConstant(DataType::Type::kInt32, 2);
  HInstruction* param = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 0, DataType::Type::kBool);
  HInstruction* entry_goto = new (GetAllocator()) HGoto();
  entry->AddInstruction(param);
  entry->AddInstruction(entry_goto);

  HInstruction* alloc_w = new (GetAllocator()) HNewArray(zero_const, two_const, 0, 0);
  HInstruction* branch = new (GetAllocator()) HIf(param);
  start->AddInstruction(alloc_w);
  start->AddInstruction(branch);
  // environment
  ArenaVector<HInstruction*> alloc_locals({}, GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(alloc_w, &alloc_locals);

  // left
  HInstruction* left_set_1 =
      new (GetAllocator()) HArraySet(alloc_w, zero_const, one_const, DataType::Type::kInt32, 0);
  HInstruction* left_set_2 =
      new (GetAllocator()) HArraySet(alloc_w, zero_const, zero_const, DataType::Type::kInt32, 0);
  HInstruction* left_goto = new (GetAllocator()) HGoto();
  left->AddInstruction(left_set_1);
  left->AddInstruction(left_set_2);
  left->AddInstruction(left_goto);

  // right
  HInstruction* right_set_1 =
      new (GetAllocator()) HArraySet(alloc_w, one_const, one_const, DataType::Type::kInt32, 0);
  HInstruction* right_set_2 =
      new (GetAllocator()) HArraySet(alloc_w, one_const, zero_const, DataType::Type::kInt32, 0);
  HInstruction* right_goto = new (GetAllocator()) HGoto();
  right->AddInstruction(right_set_1);
  right->AddInstruction(right_set_2);
  right->AddInstruction(right_goto);

  // ret
  HInstruction* read_1 =
      new (GetAllocator()) HArrayGet(alloc_w, zero_const, DataType::Type::kInt32, 0);
  HInstruction* read_2 =
      new (GetAllocator()) HArrayGet(alloc_w, one_const, DataType::Type::kInt32, 0);
  HInstruction* add = new (GetAllocator()) HAdd(DataType::Type::kInt32, read_1, read_2);
  HInstruction* return_inst = new (GetAllocator()) HReturn(add);
  ret->AddInstruction(read_1);
  ret->AddInstruction(read_2);
  ret->AddInstruction(add);
  ret->AddInstruction(return_inst);

  // exit
  HInstruction* exit_inst = new (GetAllocator()) HExit();
  exit->AddInstruction(exit_inst);

  graph_->ClearDominanceInformation();
  graph_->ClearLoopInformation();
  PerformLSE();

  EXPECT_TRUE(IsRemoved(read_1));
  EXPECT_TRUE(IsRemoved(read_2));
  EXPECT_TRUE(IsRemoved(left_set_1));
  EXPECT_TRUE(IsRemoved(left_set_2));
  EXPECT_TRUE(IsRemoved(right_set_1));
  EXPECT_TRUE(IsRemoved(right_set_2));
  EXPECT_TRUE(IsRemoved(alloc_w));
}

// // ENTRY
// obj = new Obj();
// // ALL should be kept
// switch (parameter_value) {
//   case 1:
//     // Case1
//     obj.field = 1;
//     call_func(obj);
//     break;
//   case 2:
//     // Case2
//     obj.field = 2;
//     call_func(obj);
//     // We don't know what obj.field is now we aren't able to eliminate the read below!
//     break;
//   default:
//     // Case3
//     // TODO This only happens because of limitations on our LSE which is unable
//     //      to materialize co-dependent loop and non-loop phis.
//     // Ideally we'd want to generate
//     // P1 = PHI[3, loop_val]
//     // while (test()) {
//     //   if (test2()) { goto; } else { goto; }
//     //   loop_val = [P1, 5]
//     // }
//     // Currently we aren't able to unfortunately.
//     obj.field = 3;
//     while (test()) {
//       if (test2()) { } else { obj.field = 5; }
//     }
//     break;
// }
// EXIT
// return obj.field
TEST_F(LoadStoreEliminationTest, PartialUnknownMerge) {
  CreateGraph();
  AdjacencyListGraph blks(SetupFromAdjacencyList("entry",
                                                 "exit",
                                                 {{"entry", "bswitch"},
                                                  {"bswitch", "case1"},
                                                  {"bswitch", "case2"},
                                                  {"bswitch", "case3"},
                                                  {"case1", "breturn"},
                                                  {"case2", "breturn"},
                                                  {"case3", "loop_pre_header"},
                                                  {"loop_pre_header", "loop_header"},
                                                  {"loop_header", "loop_body"},
                                                  {"loop_body", "loop_if_left"},
                                                  {"loop_body", "loop_if_right"},
                                                  {"loop_if_left", "loop_end"},
                                                  {"loop_if_right", "loop_end"},
                                                  {"loop_end", "loop_header"},
                                                  {"loop_header", "breturn"},
                                                  {"breturn", "exit"}}));
#define GET_BLOCK(name) HBasicBlock* name = blks.Get(#name)
  GET_BLOCK(entry);
  GET_BLOCK(bswitch);
  GET_BLOCK(exit);
  GET_BLOCK(breturn);
  GET_BLOCK(case1);
  GET_BLOCK(case2);
  GET_BLOCK(case3);

  GET_BLOCK(loop_pre_header);
  GET_BLOCK(loop_header);
  GET_BLOCK(loop_body);
  GET_BLOCK(loop_if_left);
  GET_BLOCK(loop_if_right);
  GET_BLOCK(loop_end);
#undef GET_BLOCK
  HInstruction* switch_val = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kInt32);
  HInstruction* c1 = graph_->GetIntConstant(1);
  HInstruction* c2 = graph_->GetIntConstant(2);
  HInstruction* c3 = graph_->GetIntConstant(3);
  HInstruction* c5 = graph_->GetIntConstant(5);
  HInstruction* cls = new (GetAllocator()) HLoadClass(graph_->GetCurrentMethod(),
                                                      dex::TypeIndex(10),
                                                      graph_->GetDexFile(),
                                                      ScopedNullHandle<mirror::Class>(),
                                                      false,
                                                      0,
                                                      false);
  HInstruction* new_inst =
      new (GetAllocator()) HNewInstance(cls,
                                        0,
                                        dex::TypeIndex(10),
                                        graph_->GetDexFile(),
                                        false,
                                        QuickEntrypointEnum::kQuickAllocObjectInitialized);
  HInstruction* entry_goto = new (GetAllocator()) HGoto();
  entry->AddInstruction(switch_val);
  entry->AddInstruction(cls);
  entry->AddInstruction(new_inst);
  entry->AddInstruction(entry_goto);
  ArenaVector<HInstruction*> current_locals({}, GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(cls, &current_locals);
  new_inst->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* switch_inst = new (GetAllocator()) HPackedSwitch(0, 2, switch_val);
  bswitch->AddInstruction(switch_inst);

  HInstruction* write_c1 = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                  c1,
                                                                  nullptr,
                                                                  DataType::Type::kInt32,
                                                                  MemberOffset(10),
                                                                  false,
                                                                  0,
                                                                  0,
                                                                  graph_->GetDexFile(),
                                                                  0);
  HInstruction* call_c1 = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            1,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* goto_c1 = new (GetAllocator()) HGoto();
  call_c1->AsInvoke()->SetRawInputAt(0, new_inst);
  case1->AddInstruction(write_c1);
  case1->AddInstruction(call_c1);
  case1->AddInstruction(goto_c1);
  call_c1->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* write_c2 = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                  c2,
                                                                  nullptr,
                                                                  DataType::Type::kInt32,
                                                                  MemberOffset(10),
                                                                  false,
                                                                  0,
                                                                  0,
                                                                  graph_->GetDexFile(),
                                                                  0);
  HInstruction* call_c2 = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            1,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* goto_c2 = new (GetAllocator()) HGoto();
  call_c2->AsInvoke()->SetRawInputAt(0, new_inst);
  case2->AddInstruction(write_c2);
  case2->AddInstruction(call_c2);
  case2->AddInstruction(goto_c2);
  call_c2->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* write_c3 = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                  c3,
                                                                  nullptr,
                                                                  DataType::Type::kInt32,
                                                                  MemberOffset(10),
                                                                  false,
                                                                  0,
                                                                  0,
                                                                  graph_->GetDexFile(),
                                                                  0);
  HInstruction* goto_c3 = new (GetAllocator()) HGoto();
  case3->AddInstruction(write_c3);
  case3->AddInstruction(goto_c3);

  HInstruction* goto_preheader = new (GetAllocator()) HGoto();
  loop_pre_header->AddInstruction(goto_preheader);

  HInstruction* suspend_check_header = new (GetAllocator()) HSuspendCheck();
  HInstruction* call_loop_header = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            0,
                            DataType::Type::kBool,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* if_loop_header = new (GetAllocator()) HIf(call_loop_header);
  loop_header->AddInstruction(suspend_check_header);
  loop_header->AddInstruction(call_loop_header);
  loop_header->AddInstruction(if_loop_header);
  call_loop_header->CopyEnvironmentFrom(cls->GetEnvironment());
  suspend_check_header->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* call_loop_body = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            0,
                            DataType::Type::kBool,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* if_loop_body = new (GetAllocator()) HIf(call_loop_body);
  loop_body->AddInstruction(call_loop_body);
  loop_body->AddInstruction(if_loop_body);
  call_loop_body->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* goto_loop_left = new (GetAllocator()) HGoto();
  loop_if_left->AddInstruction(goto_loop_left);

  HInstruction* write_loop_right = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                          c5,
                                                                          nullptr,
                                                                          DataType::Type::kInt32,
                                                                          MemberOffset(10),
                                                                          false,
                                                                          0,
                                                                          0,
                                                                          graph_->GetDexFile(),
                                                                          0);
  HInstruction* goto_loop_right = new (GetAllocator()) HGoto();
  loop_if_right->AddInstruction(write_loop_right);
  loop_if_right->AddInstruction(goto_loop_right);

  HInstruction* goto_loop_end = new (GetAllocator()) HGoto();
  loop_end->AddInstruction(goto_loop_end);

  HInstruction* read_bottom = new (GetAllocator()) HInstanceFieldGet(new_inst,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* return_exit = new (GetAllocator()) HReturn(read_bottom);
  breturn->AddInstruction(read_bottom);
  breturn->AddInstruction(return_exit);

  HInstruction* exit_ins = new (GetAllocator()) HExit();
  exit->AddInstruction(exit_ins);
  // PerformLSE expects this to be empty.
  graph_->ClearDominanceInformation();
  LOG(INFO) << "Pre LSE " << blks;
  PerformLSENoPartial();

  EXPECT_FALSE(IsRemoved(read_bottom));
  EXPECT_FALSE(IsRemoved(write_c1));
  EXPECT_FALSE(IsRemoved(write_c2));
  EXPECT_FALSE(IsRemoved(write_c3));
  EXPECT_FALSE(IsRemoved(write_loop_right));
}

// // ENTRY
// obj = new Obj();
// if (parameter_value) {
//   // LEFT
//   obj.field = 1;
//   call_func(obj);
//   foo_r = obj.field
// } else {
//   // TO BE ELIMINATED
//   obj.field = 2;
//   // RIGHT
//   // TO BE ELIMINATED
//   foo_l = obj.field;
// }
// EXIT
// return PHI(foo_l, foo_r)
TEST_F(LoadStoreEliminationTest, PartialLoadElimination) {
  ScopedObjectAccess soa(Thread::Current());
  VariableSizedHandleScope vshs(soa.Self());
  CreateGraph(&vshs);
  AdjacencyListGraph blks(SetupFromAdjacencyList("entry",
                                                 "exit_REAL",
                                                 { { "entry", "left" },
                                                   { "entry", "right" },
                                                   { "left", "exit" },
                                                   { "right", "exit" },
                                                   { "exit", "exit_REAL" } }));
  HBasicBlock* entry = blks.Get("entry");
  HBasicBlock* left = blks.Get("left");
  HBasicBlock* right = blks.Get("right");
  HBasicBlock* exit = blks.Get("exit");
  HInstruction* bool_value = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kBool);
  HInstruction* c1 = graph_->GetIntConstant(1);
  HInstruction* c2 = graph_->GetIntConstant(2);
  HInstruction* cls = new (GetAllocator()) HLoadClass(graph_->GetCurrentMethod(),
                                                      dex::TypeIndex(10),
                                                      graph_->GetDexFile(),
                                                      ScopedNullHandle<mirror::Class>(),
                                                      false,
                                                      0,
                                                      false);
  HInstruction* new_inst =
      new (GetAllocator()) HNewInstance(cls,
                                        0,
                                        dex::TypeIndex(10),
                                        graph_->GetDexFile(),
                                        false,
                                        QuickEntrypointEnum::kQuickAllocObjectInitialized);
  HInstruction* if_inst = new (GetAllocator()) HIf(bool_value);
  entry->AddInstruction(bool_value);
  entry->AddInstruction(cls);
  entry->AddInstruction(new_inst);
  entry->AddInstruction(if_inst);
  ArenaVector<HInstruction*> current_locals({}, GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(cls, &current_locals);
  new_inst->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* write_left = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                    c1,
                                                                    nullptr,
                                                                    DataType::Type::kInt32,
                                                                    MemberOffset(10),
                                                                    false,
                                                                    0,
                                                                    0,
                                                                    graph_->GetDexFile(),
                                                                    0);
  HInstruction* call_left = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            1,
                            DataType::Type::kVoid,
                            0,
                            { nullptr, 0 },
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            { nullptr, 0 },
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* read_left = new (GetAllocator()) HInstanceFieldGet(new_inst,
                                                                   nullptr,
                                                                   DataType::Type::kInt32,
                                                                   MemberOffset(16),
                                                                   false,
                                                                   0,
                                                                   0,
                                                                   graph_->GetDexFile(),
                                                                   0);
  HInstruction* goto_left = new (GetAllocator()) HGoto();
  call_left->AsInvoke()->SetRawInputAt(0, new_inst);
  left->AddInstruction(write_left);
  left->AddInstruction(call_left);
  left->AddInstruction(read_left);
  left->AddInstruction(goto_left);
  call_left->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* write_right = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                     c2,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(16),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* read_right = new (GetAllocator()) HInstanceFieldGet(new_inst,
                                                                    nullptr,
                                                                    DataType::Type::kInt32,
                                                                    MemberOffset(16),
                                                                    false,
                                                                    0,
                                                                    0,
                                                                    graph_->GetDexFile(),
                                                                    0);
  HInstruction* goto_right = new (GetAllocator()) HGoto();
  right->AddInstruction(write_right);
  right->AddInstruction(read_right);
  right->AddInstruction(goto_right);

  HInstruction* phi_final =
      new (GetAllocator()) HPhi(GetAllocator(), 12, 2, DataType::Type::kInt32);
  phi_final->SetRawInputAt(0, read_left);
  phi_final->SetRawInputAt(1, read_right);
  HInstruction* return_exit = new (GetAllocator()) HReturn(phi_final);
  exit->AddPhi(phi_final->AsPhi());
  exit->AddInstruction(return_exit);

  // PerformLSE expects this to be empty.
  graph_->ClearDominanceInformation();
  PerformLSE();

  ASSERT_TRUE(IsRemoved(read_right));
  ASSERT_FALSE(IsRemoved(read_left));
  ASSERT_FALSE(IsRemoved(phi_final));
  ASSERT_TRUE(phi_final->GetInputs()[1] == c2);
  ASSERT_TRUE(phi_final->GetInputs()[0] == read_left);
  ASSERT_TRUE(IsRemoved(write_right));
}

// // ENTRY
// obj = new Obj();
// if (parameter_value) {
//   // LEFT
//   obj.field = 1;
//   call_func(obj);
//   // We don't know what obj.field is now we aren't able to eliminate the read below!
// } else {
//   // DO NOT ELIMINATE
//   obj.field = 2;
//   // RIGHT
// }
// EXIT
// return obj.field
// Old test of partial escape analysis from before full partial LSE was
// implemented. Disabled as functionality is not used.
TEST_F(LoadStoreEliminationTest, PartialLoadPreserved) {
  ScopedObjectAccess soa(Thread::Current());
  VariableSizedHandleScope vshs(soa.Self());
  CreateGraph(&vshs);
  AdjacencyListGraph blks(SetupFromAdjacencyList("entry",
                                                 "exit_REAL",
                                                 { { "entry", "left" },
                                                   { "entry", "right" },
                                                   { "left", "exit" },
                                                   { "right", "exit" },
                                                   { "exit", "exit_REAL" } }));
  HBasicBlock* entry = blks.Get("entry");
  HBasicBlock* left = blks.Get("left");
  HBasicBlock* right = blks.Get("right");
  HBasicBlock* exit = blks.Get("exit");
  HInstruction* bool_value = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kBool);
  HInstruction* c1 = graph_->GetIntConstant(1);
  HInstruction* c2 = graph_->GetIntConstant(2);
  HInstruction* cls = new (GetAllocator()) HLoadClass(graph_->GetCurrentMethod(),
                                                      dex::TypeIndex(10),
                                                      graph_->GetDexFile(),
                                                      ScopedNullHandle<mirror::Class>(),
                                                      false,
                                                      0,
                                                      false);
  HInstruction* new_inst =
      new (GetAllocator()) HNewInstance(cls,
                                        0,
                                        dex::TypeIndex(10),
                                        graph_->GetDexFile(),
                                        false,
                                        QuickEntrypointEnum::kQuickAllocObjectInitialized);
  HInstruction* if_inst = new (GetAllocator()) HIf(bool_value);
  entry->AddInstruction(bool_value);
  entry->AddInstruction(cls);
  entry->AddInstruction(new_inst);
  entry->AddInstruction(if_inst);
  ArenaVector<HInstruction*> current_locals({}, GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(cls, &current_locals);
  new_inst->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* write_left = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                    c1,
                                                                    nullptr,
                                                                    DataType::Type::kInt32,
                                                                    MemberOffset(10),
                                                                    false,
                                                                    0,
                                                                    0,
                                                                    graph_->GetDexFile(),
                                                                    0);
  HInstruction* call_left = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            1,
                            DataType::Type::kVoid,
                            0,
                            { nullptr, 0 },
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            { nullptr, 0 },
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* goto_left = new (GetAllocator()) HGoto();
  call_left->AsInvoke()->SetRawInputAt(0, new_inst);
  left->AddInstruction(write_left);
  left->AddInstruction(call_left);
  left->AddInstruction(goto_left);
  call_left->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* write_right = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                     c2,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* goto_right = new (GetAllocator()) HGoto();
  right->AddInstruction(write_right);
  right->AddInstruction(goto_right);

  HInstruction* read_bottom = new (GetAllocator()) HInstanceFieldGet(new_inst,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* return_exit = new (GetAllocator()) HReturn(read_bottom);
  exit->AddInstruction(read_bottom);
  exit->AddInstruction(return_exit);
  // PerformLSE expects this to be empty.
  graph_->ClearDominanceInformation();
  PerformLSENoPartial();

  EXPECT_FALSE(IsRemoved(read_bottom)) << *read_bottom;
  EXPECT_FALSE(IsRemoved(write_right)) << *write_right;
}

// // ENTRY
// obj = new Obj();
// if (parameter_value) {
//   // LEFT
//   obj.field = 1;
//   call_func(obj);
//   // We don't know what obj.field is now we aren't able to eliminate the read below!
// } else {
//   // DO NOT ELIMINATE
//   if (param2) {
//     obj.field = 2;
//   } else {
//     obj.field = 3;
//   }
//   // RIGHT
// }
// EXIT
// return obj.field
// NB This test is for non-partial LSE flow. Normally the obj.field writes will be removed
TEST_F(LoadStoreEliminationTest, PartialLoadPreserved2) {
  ScopedObjectAccess soa(Thread::Current());
  VariableSizedHandleScope vshs(soa.Self());
  CreateGraph(&vshs);
  AdjacencyListGraph blks(SetupFromAdjacencyList("entry",
                                                 "exit_REAL",
                                                 { { "entry", "left" },
                                                   { "entry", "right_start" },
                                                   { "left", "exit" },
                                                   { "right_start", "right_first" },
                                                   { "right_start", "right_second" },
                                                   { "right_first", "right_end" },
                                                   { "right_second", "right_end" },
                                                   { "right_end", "exit" },
                                                   { "exit", "exit_REAL" } }));
  HBasicBlock* entry = blks.Get("entry");
  HBasicBlock* left = blks.Get("left");
  HBasicBlock* right_start = blks.Get("right_start");
  HBasicBlock* right_first = blks.Get("right_first");
  HBasicBlock* right_second = blks.Get("right_second");
  HBasicBlock* right_end = blks.Get("right_end");
  HBasicBlock* exit = blks.Get("exit");
  HInstruction* bool_value = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kBool);
  HInstruction* bool_value_2 = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 2, DataType::Type::kBool);
  HInstruction* c1 = graph_->GetIntConstant(1);
  HInstruction* c2 = graph_->GetIntConstant(2);
  HInstruction* c3 = graph_->GetIntConstant(3);
  HInstruction* cls = new (GetAllocator()) HLoadClass(graph_->GetCurrentMethod(),
                                                      dex::TypeIndex(10),
                                                      graph_->GetDexFile(),
                                                      ScopedNullHandle<mirror::Class>(),
                                                      false,
                                                      0,
                                                      false);
  HInstruction* new_inst =
      new (GetAllocator()) HNewInstance(cls,
                                        0,
                                        dex::TypeIndex(10),
                                        graph_->GetDexFile(),
                                        false,
                                        QuickEntrypointEnum::kQuickAllocObjectInitialized);
  HInstruction* if_inst = new (GetAllocator()) HIf(bool_value);
  entry->AddInstruction(bool_value);
  entry->AddInstruction(bool_value_2);
  entry->AddInstruction(cls);
  entry->AddInstruction(new_inst);
  entry->AddInstruction(if_inst);
  ArenaVector<HInstruction*> current_locals({}, GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(cls, &current_locals);
  new_inst->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* write_left = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                    c1,
                                                                    nullptr,
                                                                    DataType::Type::kInt32,
                                                                    MemberOffset(10),
                                                                    false,
                                                                    0,
                                                                    0,
                                                                    graph_->GetDexFile(),
                                                                    0);
  HInstruction* call_left = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            1,
                            DataType::Type::kVoid,
                            0,
                            { nullptr, 0 },
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            { nullptr, 0 },
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* goto_left = new (GetAllocator()) HGoto();
  call_left->AsInvoke()->SetRawInputAt(0, new_inst);
  left->AddInstruction(write_left);
  left->AddInstruction(call_left);
  left->AddInstruction(goto_left);
  call_left->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* right_if = new (GetAllocator()) HIf(bool_value_2);
  right_start->AddInstruction(right_if);

  HInstruction* write_right_first = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                           c2,
                                                                           nullptr,
                                                                           DataType::Type::kInt32,
                                                                           MemberOffset(10),
                                                                           false,
                                                                           0,
                                                                           0,
                                                                           graph_->GetDexFile(),
                                                                           0);
  HInstruction* goto_right_first = new (GetAllocator()) HGoto();
  right_first->AddInstruction(write_right_first);
  right_first->AddInstruction(goto_right_first);

  HInstruction* write_right_second = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                            c3,
                                                                            nullptr,
                                                                            DataType::Type::kInt32,
                                                                            MemberOffset(10),
                                                                            false,
                                                                            0,
                                                                            0,
                                                                            graph_->GetDexFile(),
                                                                            0);
  HInstruction* goto_right_second = new (GetAllocator()) HGoto();
  right_second->AddInstruction(write_right_second);
  right_second->AddInstruction(goto_right_second);

  HInstruction* goto_right_end = new (GetAllocator()) HGoto();
  right_end->AddInstruction(goto_right_end);

  HInstruction* read_bottom = new (GetAllocator()) HInstanceFieldGet(new_inst,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* return_exit = new (GetAllocator()) HReturn(read_bottom);
  exit->AddInstruction(read_bottom);
  exit->AddInstruction(return_exit);
  // PerformLSE expects this to be empty.
  graph_->ClearDominanceInformation();
  PerformLSENoPartial();

  EXPECT_FALSE(IsRemoved(read_bottom));
  EXPECT_FALSE(IsRemoved(write_right_first));
  EXPECT_FALSE(IsRemoved(write_right_second));
}

// // ENTRY
// obj = new Obj();
// if (parameter_value) {
//   // LEFT
//   // DO NOT ELIMINATE
//   escape(obj);
//   obj.field = 1;
// } else {
//   // RIGHT
//   // ELIMINATE
//   obj.field = 2;
// }
// EXIT
// ELIMINATE
// return obj.field
TEST_F(LoadStoreEliminationTest, PartialLoadElimination2) {
  ScopedObjectAccess soa(Thread::Current());
  VariableSizedHandleScope vshs(soa.Self());
  CreateGraph(&vshs);
  AdjacencyListGraph blks(SetupFromAdjacencyList("entry",
                                                 "exit",
                                                 {{"entry", "left"},
                                                  {"entry", "right"},
                                                  {"left", "breturn"},
                                                  {"right", "breturn"},
                                                  {"breturn", "exit"}}));
#define GET_BLOCK(name) HBasicBlock* name = blks.Get(#name)
  GET_BLOCK(entry);
  GET_BLOCK(exit);
  GET_BLOCK(breturn);
  GET_BLOCK(left);
  GET_BLOCK(right);
#undef GET_BLOCK
  HInstruction* bool_value = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kBool);
  HInstruction* c1 = graph_->GetIntConstant(1);
  HInstruction* c2 = graph_->GetIntConstant(2);
  HInstruction* cls = new (GetAllocator()) HLoadClass(graph_->GetCurrentMethod(),
                                                      dex::TypeIndex(10),
                                                      graph_->GetDexFile(),
                                                      ScopedNullHandle<mirror::Class>(),
                                                      false,
                                                      0,
                                                      false);
  HInstruction* new_inst =
      new (GetAllocator()) HNewInstance(cls,
                                        0,
                                        dex::TypeIndex(10),
                                        graph_->GetDexFile(),
                                        false,
                                        QuickEntrypointEnum::kQuickAllocObjectInitialized);
  HInstruction* if_inst = new (GetAllocator()) HIf(bool_value);
  entry->AddInstruction(bool_value);
  entry->AddInstruction(cls);
  entry->AddInstruction(new_inst);
  entry->AddInstruction(if_inst);
  ArenaVector<HInstruction*> current_locals({}, GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(cls, &current_locals);
  new_inst->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* call_left = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            1,
                            DataType::Type::kVoid,
                            0,
                            { nullptr, 0 },
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            { nullptr, 0 },
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* write_left = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                    c1,
                                                                    nullptr,
                                                                    DataType::Type::kInt32,
                                                                    MemberOffset(10),
                                                                    false,
                                                                    0,
                                                                    0,
                                                                    graph_->GetDexFile(),
                                                                    0);
  HInstruction* goto_left = new (GetAllocator()) HGoto();
  call_left->AsInvoke()->SetRawInputAt(0, new_inst);
  left->AddInstruction(call_left);
  left->AddInstruction(write_left);
  left->AddInstruction(goto_left);
  call_left->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* write_right = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                     c2,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* goto_right = new (GetAllocator()) HGoto();
  right->AddInstruction(write_right);
  right->AddInstruction(goto_right);

  HInstruction* read_bottom = new (GetAllocator()) HInstanceFieldGet(new_inst,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* return_exit = new (GetAllocator()) HReturn(read_bottom);
  breturn->AddInstruction(read_bottom);
  breturn->AddInstruction(return_exit);

  HInstruction* exit_instruction = new (GetAllocator()) HExit();
  exit->AddInstruction(exit_instruction);
  // PerformLSE expects this to be empty.
  graph_->ClearDominanceInformation();
  PerformLSE();

  EXPECT_TRUE(IsRemoved(read_bottom));
  EXPECT_TRUE(IsRemoved(write_right));
  EXPECT_FALSE(IsRemoved(write_left));
  EXPECT_FALSE(IsRemoved(call_left));
}

class PatternMatchGraphVisitor : public HGraphVisitor {
 private:
  struct HandlerWrapper {
   public:
    virtual ~HandlerWrapper() {}
    virtual void operator()(HInstruction* h) = 0;
  };

  template <HInstruction::InstructionKind kKind, typename F>
  struct KindWrapper;

#define GEN_HANDLER(nm, unused)                                                         \
  template <typename F>                                                                 \
  struct KindWrapper<HInstruction::InstructionKind::k##nm, F> : public HandlerWrapper { \
   public:                                                                              \
    explicit KindWrapper(F f) : f_(f) {}                                                \
    void operator()(HInstruction* h) override {                                         \
      if constexpr (std::is_invocable<F, H##nm*>::value) {                              \
        f_(h->As##nm());                                                                \
      } else {                                                                          \
        LOG(FATAL) << "Incorrect call with " << #nm;                                    \
      }                                                                                 \
    }                                                                                   \
                                                                                        \
   private:                                                                             \
    F f_;                                                                               \
  };

  FOR_EACH_CONCRETE_INSTRUCTION(GEN_HANDLER)
#undef GEN_HANDLER

  template <typename F>
  std::unique_ptr<HandlerWrapper> GetWrapper(HInstruction::InstructionKind kind, F f) {
    switch (kind) {
#define GEN_GETTER(nm, unused)               \
  case HInstruction::InstructionKind::k##nm: \
    return std::unique_ptr<HandlerWrapper>(  \
        new KindWrapper<HInstruction::InstructionKind::k##nm, F>(f));
      FOR_EACH_CONCRETE_INSTRUCTION(GEN_GETTER)
#undef GEN_GETTER
      default:
        LOG(FATAL) << "Unable to handle kind " << kind;
        return nullptr;
    }
  }

 public:
  template <typename... Inst>
  explicit PatternMatchGraphVisitor(HGraph* graph, Inst... handlers) : HGraphVisitor(graph) {
    FillHandlers(handlers...);
  }

  void VisitInstruction(HInstruction* instruction) override {
    auto& h = handlers_[instruction->GetKind()];
    if (h.get() != nullptr) {
      (*h)(instruction);
    }
  }

 private:
  template <typename Func>
  constexpr HInstruction::InstructionKind GetKind() {
#define CHECK_INST(nm, unused)                            \
  if constexpr (std::is_invocable<Func, H##nm*>::value) { \
    return HInstruction::InstructionKind::k##nm;          \
  }
    FOR_EACH_CONCRETE_INSTRUCTION(CHECK_INST);
#undef CHECK_INST
#define STATIC_ASSERT_INST(nm, unused) || std::is_invocable<Func, H##nm*>::value
#define STATIC_ASSERT_ABST(nm, unused) &&!std::is_invocable<Func, H##nm*>::value
    static_assert((!std::is_invocable<Func, HInstruction*>::value FOR_EACH_ABSTRACT_INSTRUCTION(
                      STATIC_ASSERT_ABST)) &&
                      (false FOR_EACH_CONCRETE_INSTRUCTION(STATIC_ASSERT_INST)),
                  "Bad instruction type!");
#undef STATIC_ASSERT_INST
    return HInstruction::InstructionKind::kLastInstructionKind;
  }
  template <typename First>
  void FillHandlers(First h1) {
    HInstruction::InstructionKind type = GetKind<First>();
    CHECK_NE(type, HInstruction::kLastInstructionKind)
        << "Unknown instruction kind. Only concrete ones please.";
    handlers_[type] = GetWrapper(type, h1);
  }

  template <typename First, typename... Inst>
  void FillHandlers(First h1, Inst... handlers) {
    FillHandlers(h1);
    FillHandlers<Inst...>(handlers...);
  }

  std::array<std::unique_ptr<HandlerWrapper>, HInstruction::InstructionKind::kLastInstructionKind>
      handlers_;
};

template <typename... Target>
std::tuple<std::vector<Target*>...> FindAllInstructions(
    HGraph* graph,
    std::variant<std::nullopt_t, HBasicBlock*, std::initializer_list<HBasicBlock*>> blks =
        std::nullopt) {
  std::tuple<std::vector<Target*>...> res;
  PatternMatchGraphVisitor vis(
      graph, [&](Target* t) { std::get<std::vector<Target*>>(res).push_back(t); }...);

  if (std::holds_alternative<std::initializer_list<HBasicBlock*>>(blks)) {
    for (HBasicBlock* blk : std::get<std::initializer_list<HBasicBlock*>>(blks)) {
      vis.VisitBasicBlock(blk);
    }
  } else if (std::holds_alternative<std::nullopt_t>(blks)) {
    vis.VisitInsertionOrder();
  } else {
    vis.VisitBasicBlock(std::get<HBasicBlock*>(blks));
  }
  return res;
}

template <typename... Target>
std::tuple<Target*...> FindSingleInstructions(
    HGraph* graph,
    std::variant<std::nullopt_t, HBasicBlock*, std::initializer_list<HBasicBlock*>> blks =
        std::nullopt) {
  std::tuple<Target*...> res;
  PatternMatchGraphVisitor vis(graph, [&](Target* t) {
    EXPECT_EQ(std::get<Target*>(res), nullptr)
        << *std::get<Target*>(res) << " already found but found " << *t << "!";
    std::get<Target*>(res) = t;
  }...);
  if (std::holds_alternative<std::initializer_list<HBasicBlock*>>(blks)) {
    for (HBasicBlock* blk : std::get<std::initializer_list<HBasicBlock*>>(blks)) {
      vis.VisitBasicBlock(blk);
    }
  } else if (std::holds_alternative<std::nullopt_t>(blks)) {
    vis.VisitInsertionOrder();
  } else {
    vis.VisitBasicBlock(std::get<HBasicBlock*>(blks));
  }
  return res;
}

template <typename Target>
Target* FindSingleInstruction(
    HGraph* graph,
    std::variant<std::nullopt_t, HBasicBlock*, std::initializer_list<HBasicBlock*>> blks =
        std::nullopt) {
  Target* res = nullptr;
  PatternMatchGraphVisitor vis(graph, [&](Target* t) {
    EXPECT_EQ(res, nullptr) << "Found " << *t << " but " << *res << " already found!";
    res = t;
  });
  if (std::holds_alternative<std::initializer_list<HBasicBlock*>>(blks)) {
    for (HBasicBlock* blk : std::get<std::initializer_list<HBasicBlock*>>(blks)) {
      vis.VisitBasicBlock(blk);
    }
  } else if (std::holds_alternative<std::nullopt_t>(blks)) {
    vis.VisitInsertionOrder();
  } else {
    vis.VisitBasicBlock(std::get<HBasicBlock*>(blks));
  }
  return res;
}

// // ENTRY
// Obj new_inst = new Obj();
// new_inst.foo = 12;
// Obj obj;
// Obj out;
// int first;
// if (param0) {
//   if (param1) {
//     // LEFT_START
//     if (param2) {
//       // LEFT_LEFT
//       obj = new_inst;
//     } else {
//       // LEFT_RIGHT
//       obj = obj_param;
//     }
//     // LEFT_MERGE
//     // technically the phi is enough to cause an escape but might as well be
//     // thorough.
//     // obj = phi[new_inst, param]
//     escape(obj);
//     out = obj;
//   } else {
//     // RIGHT
//     out = obj_param;
//   }
//   // EXIT
//   // Can't do anything with this since we don't have good tracking for the heap-locations
//   // out = phi[param, phi[new_inst, param]]
//   first = out.foo
// } else {
//   new_inst.foo = 15;
//   first = 13;
// }
// // first = phi[out.foo, 13]
// return first + new_inst.foo;
TEST_F(LoadStoreEliminationTest, PartialPhiPropagation) {
  ScopedObjectAccess soa(Thread::Current());
  VariableSizedHandleScope vshs(soa.Self());
  CreateGraph(&vshs);
  AdjacencyListGraph blks(SetupFromAdjacencyList("entry",
                                                 "exit",
                                                 {{"entry", "escape_route_crit_break"},
                                                  {"entry", "noescape_route"},
                                                  {"escape_route_crit_break", "escape_route"},
                                                  {"escape_route", "left_crit_break"},
                                                  {"escape_route", "right"},
                                                  {"left_crit_break", "left"},
                                                  {"left", "left_left"},
                                                  {"left", "left_right"},
                                                  {"left_left", "left_merge"},
                                                  {"left_right", "left_merge"},
                                                  {"left_merge", "left_merge_crit_break"},
                                                  {"left_merge_crit_break", "escape_end"},
                                                  {"right", "escape_end"},
                                                  {"escape_end", "escape_end_crit_break"},
                                                  {"escape_end_crit_break", "breturn"},
                                                  {"noescape_route", "breturn"},
                                                  {"breturn", "exit"}}));
#define GET_BLOCK(name) HBasicBlock* name = blks.Get(#name)
  GET_BLOCK(entry);
  GET_BLOCK(exit);
  GET_BLOCK(breturn);
  GET_BLOCK(left);
  GET_BLOCK(right);
  GET_BLOCK(left_crit_break);
  GET_BLOCK(left_left);
  GET_BLOCK(left_right);
  GET_BLOCK(left_merge);
  GET_BLOCK(left_merge_crit_break);
  GET_BLOCK(escape_end);
  GET_BLOCK(escape_end_crit_break);
  GET_BLOCK(escape_route);
  GET_BLOCK(escape_route_crit_break);
  GET_BLOCK(noescape_route);
#undef GET_BLOCK
  EnsurePredecessorOrder(escape_end, {left_merge_crit_break, right});
  EnsurePredecessorOrder(left_merge, {left_left, left_right});
  EnsurePredecessorOrder(breturn, {escape_end_crit_break, noescape_route});
  HInstruction* param0 = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kBool);
  HInstruction* param1 = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 2, DataType::Type::kBool);
  HInstruction* param2 = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 3, DataType::Type::kBool);
  HInstruction* obj_param = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(10), 4, DataType::Type::kReference);
  HInstruction* c12 = graph_->GetIntConstant(12);
  HInstruction* c13 = graph_->GetIntConstant(13);
  HInstruction* c15 = graph_->GetIntConstant(15);
  HInstruction* cls = new (GetAllocator()) HLoadClass(graph_->GetCurrentMethod(),
                                                      dex::TypeIndex(10),
                                                      graph_->GetDexFile(),
                                                      ScopedNullHandle<mirror::Class>(),
                                                      false,
                                                      0,
                                                      false);
  HInstruction* new_inst =
      new (GetAllocator()) HNewInstance(cls,
                                        0,
                                        dex::TypeIndex(10),
                                        graph_->GetDexFile(),
                                        false,
                                        QuickEntrypointEnum::kQuickAllocObjectInitialized);
  HInstruction* store = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                               c12,
                                                               nullptr,
                                                               DataType::Type::kInt32,
                                                               MemberOffset(10),
                                                               false,
                                                               0,
                                                               0,
                                                               graph_->GetDexFile(),
                                                               0);
  HInstruction* if_param0 = new (GetAllocator()) HIf(param0);
  entry->AddInstruction(param0);
  entry->AddInstruction(param1);
  entry->AddInstruction(param2);
  entry->AddInstruction(obj_param);
  entry->AddInstruction(cls);
  entry->AddInstruction(new_inst);
  entry->AddInstruction(store);
  entry->AddInstruction(if_param0);
  ArenaVector<HInstruction*> current_locals({}, GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(cls, &current_locals);
  new_inst->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* store_noescape = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                        c15,
                                                                        nullptr,
                                                                        DataType::Type::kInt32,
                                                                        MemberOffset(10),
                                                                        false,
                                                                        0,
                                                                        0,
                                                                        graph_->GetDexFile(),
                                                                        0);
  noescape_route->AddInstruction(store_noescape);
  noescape_route->AddInstruction(new (GetAllocator()) HGoto());

  escape_route_crit_break->AddInstruction(new (GetAllocator()) HGoto());

  escape_route->AddInstruction(new (GetAllocator()) HIf(param1));

  HInstruction* left_crit_break_goto = new (GetAllocator()) HGoto();
  left_crit_break->AddInstruction(left_crit_break_goto);

  HInstruction* if_left = new (GetAllocator()) HIf(param2);
  left->AddInstruction(if_left);

  HInstruction* goto_left_left = new (GetAllocator()) HGoto();
  left_left->AddInstruction(goto_left_left);

  HInstruction* goto_left_right = new (GetAllocator()) HGoto();
  left_right->AddInstruction(goto_left_right);

  HPhi* left_phi =
      new (GetAllocator()) HPhi(GetAllocator(), kNoRegNumber, 2, DataType::Type::kReference);
  HInstruction* call_left = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            1,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* goto_left_merge = new (GetAllocator()) HGoto();
  left_phi->SetRawInputAt(0, obj_param);
  left_phi->SetRawInputAt(1, new_inst);
  call_left->AsInvoke()->SetRawInputAt(0, left_phi);
  // NB The call-left needs to be added first.
  left_merge->AddInstruction(call_left);
  left_merge->AddPhi(left_phi);
  left_merge->AddInstruction(goto_left_merge);
  left_phi->SetCanBeNull(true);
  call_left->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* goto_left_merge_crit_break = new (GetAllocator()) HGoto();
  left_merge_crit_break->AddInstruction(goto_left_merge_crit_break);

  HInstruction* goto_right = new (GetAllocator()) HGoto();
  right->AddInstruction(goto_right);

  HPhi* escape_end_phi =
      new (GetAllocator()) HPhi(GetAllocator(), kNoRegNumber, 2, DataType::Type::kReference);
  HInstruction* read_escape_end = new (GetAllocator()) HInstanceFieldGet(escape_end_phi,
                                                                         nullptr,
                                                                         DataType::Type::kInt32,
                                                                         MemberOffset(10),
                                                                         false,
                                                                         0,
                                                                         0,
                                                                         graph_->GetDexFile(),
                                                                         0);
  HInstruction* goto_escape_end = new (GetAllocator()) HGoto();
  escape_end_phi->SetRawInputAt(0, left_phi);
  escape_end_phi->SetRawInputAt(1, obj_param);
  escape_end->AddPhi(escape_end_phi);
  escape_end->AddInstruction(read_escape_end);
  escape_end->AddInstruction(goto_escape_end);

  escape_end_crit_break->AddInstruction(new (GetAllocator()) HGoto());

  HPhi* return_phi =
      new (GetAllocator()) HPhi(GetAllocator(), kNoRegNumber, 2, DataType::Type::kInt32);
  HInstruction* read_exit = new (GetAllocator()) HInstanceFieldGet(new_inst,
                                                                   nullptr,
                                                                   DataType::Type::kInt32,
                                                                   MemberOffset(10),
                                                                   false,
                                                                   0,
                                                                   0,
                                                                   graph_->GetDexFile(),
                                                                   0);
  HInstruction* add_exit = new (GetAllocator()) HAdd(DataType::Type::kInt32, return_phi, read_exit);
  HInstruction* return_exit = new (GetAllocator()) HReturn(add_exit);
  return_phi->SetRawInputAt(0, read_escape_end);
  return_phi->SetRawInputAt(1, c13);
  breturn->AddPhi(return_phi);
  breturn->AddInstruction(read_exit);
  breturn->AddInstruction(add_exit);
  breturn->AddInstruction(return_exit);

  HInstruction* exit_instruction = new (GetAllocator()) HExit();
  exit->AddInstruction(exit_instruction);

  // PerformLSE expects this to be empty.
  graph_->ClearDominanceInformation();
  LOG(INFO) << "Pre LSE " << blks;
  PerformLSEWithPartial();
  LOG(INFO) << "Post LSE " << blks;

  HPredicatedInstanceFieldGet* pred_get =
      FindSingleInstruction<HPredicatedInstanceFieldGet>(graph_);
  std::vector<HPhi*> all_return_phis;
  std::tie(all_return_phis) = FindAllInstructions<HPhi>(graph_, breturn);
  EXPECT_EQ(all_return_phis.size(), 3u);
  EXPECT_INS_RETAINED(return_phi);
  EXPECT_TRUE(std::find(all_return_phis.begin(), all_return_phis.end(), return_phi) !=
              all_return_phis.end());
  HPhi* instance_phi =
      *std::find_if(all_return_phis.begin(), all_return_phis.end(), [&](HPhi* phi) {
        return phi != return_phi && phi->GetType() == DataType::Type::kReference;
      });
  HPhi* value_phi = *std::find_if(all_return_phis.begin(), all_return_phis.end(), [&](HPhi* phi) {
    return phi != return_phi && phi->GetType() == DataType::Type::kInt32;
  });
  EXPECT_INS_EQ(
      instance_phi->InputAt(0),
      FindSingleInstruction<HNewInstance>(graph_, escape_route_crit_break->GetSinglePredecessor()));
  EXPECT_INS_EQ(instance_phi->InputAt(1), graph_->GetNullConstant());
  // Check materialize block
  EXPECT_INS_EQ(FindSingleInstruction<HInstanceFieldSet>(
                    graph_, escape_route_crit_break->GetSinglePredecessor())
                    ->InputAt(1),
                c12);

  EXPECT_INS_EQ(instance_phi->InputAt(1), graph_->GetNullConstant());
  EXPECT_INS_EQ(value_phi->InputAt(0), graph_->GetIntConstant(0));
  EXPECT_INS_EQ(value_phi->InputAt(1), c15);
  EXPECT_INS_REMOVED(store_noescape);
  EXPECT_INS_EQ(pred_get->InputAt(0), instance_phi);
  EXPECT_INS_EQ(pred_get->InputAt(1), value_phi);
}

// // ENTRY
// // To be moved
// // NB Order important. By having alloc and store of obj1 before obj2 that
// // ensure we'll build the materialization for obj1 first (just due to how
// // we iterate.)
// obj1 = new Obj();
// obj.foo = 12;
// obj2 = new Obj(); // has env[obj1]
// obj2.foo = 15;
// if (param1) {
//   // LEFT
//   // Need to update env to nullptr
//   escape(obj1/2);
//   if (param2) {
//     // LEFT_LEFT
//     escape(obj2/1);
//   } else {}
// } else {}
// return obj1.foo + obj2.foo;
// EXIT
TEST_P(OrderDependentTestGroup, PredicatedEnvUse) {
  ScopedObjectAccess soa(Thread::Current());
  VariableSizedHandleScope vshs(soa.Self());
  CreateGraph(&vshs);
  AdjacencyListGraph blks(SetupFromAdjacencyList("entry",
                                                 "exit",
                                                 {{"entry", "left_crit_break"},
                                                  {"entry", "right"},
                                                  {"left_crit_break", "left"},
                                                  {"left", "left_left"},
                                                  {"left", "left_right"},
                                                  {"left_left", "left_end"},
                                                  {"left_right", "left_end"},
                                                  {"left_end", "breturn"},
                                                  {"right", "breturn"},
                                                  {"breturn", "exit"}}));
#define GET_BLOCK(name) HBasicBlock* name = blks.Get(#name)
  GET_BLOCK(entry);
  GET_BLOCK(exit);
  GET_BLOCK(breturn);
  GET_BLOCK(right);
  GET_BLOCK(left);
  GET_BLOCK(left_crit_break);
  GET_BLOCK(left_left);
  GET_BLOCK(left_right);
  GET_BLOCK(left_end);
#undef GET_BLOCK
  TestOrder order = GetParam();
  EnsurePredecessorOrder(breturn, {left_end, right});
  EnsurePredecessorOrder(left_end, {left_left, left_right});
  HInstruction* param1 = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kBool);
  HInstruction* param2 = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 2, DataType::Type::kBool);
  HInstruction* c12 = graph_->GetIntConstant(12);
  HInstruction* c15 = graph_->GetIntConstant(15);
  HInstruction* cls1 = new (GetAllocator()) HLoadClass(graph_->GetCurrentMethod(),
                                                       dex::TypeIndex(10),
                                                       graph_->GetDexFile(),
                                                       ScopedNullHandle<mirror::Class>(),
                                                       false,
                                                       0,
                                                       false);
  HInstruction* cls2 = new (GetAllocator()) HLoadClass(graph_->GetCurrentMethod(),
                                                       dex::TypeIndex(20),
                                                       graph_->GetDexFile(),
                                                       ScopedNullHandle<mirror::Class>(),
                                                       false,
                                                       0,
                                                       false);
  HInstruction* new_inst1 =
      new (GetAllocator()) HNewInstance(cls1,
                                        0,
                                        dex::TypeIndex(10),
                                        graph_->GetDexFile(),
                                        false,
                                        QuickEntrypointEnum::kQuickAllocObjectInitialized);
  HInstruction* store1 = new (GetAllocator()) HInstanceFieldSet(new_inst1,
                                                                c12,
                                                                nullptr,
                                                                DataType::Type::kInt32,
                                                                MemberOffset(10),
                                                                false,
                                                                0,
                                                                0,
                                                                graph_->GetDexFile(),
                                                                0);
  HInstruction* new_inst2 =
      new (GetAllocator()) HNewInstance(cls2,
                                        0,
                                        dex::TypeIndex(10),
                                        graph_->GetDexFile(),
                                        false,
                                        QuickEntrypointEnum::kQuickAllocObjectInitialized);
  HInstruction* store2 = new (GetAllocator()) HInstanceFieldSet(new_inst2,
                                                                c15,
                                                                nullptr,
                                                                DataType::Type::kInt32,
                                                                MemberOffset(10),
                                                                false,
                                                                0,
                                                                0,
                                                                graph_->GetDexFile(),
                                                                0);
  HInstruction* if_inst = new (GetAllocator()) HIf(param1);
  entry->AddInstruction(param1);
  entry->AddInstruction(param2);
  entry->AddInstruction(cls1);
  entry->AddInstruction(cls2);
  entry->AddInstruction(new_inst1);
  entry->AddInstruction(store1);
  entry->AddInstruction(new_inst2);
  entry->AddInstruction(store2);
  entry->AddInstruction(if_inst);
  ArenaVector<HInstruction*> current_locals({}, GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(cls1, &current_locals);
  cls2->CopyEnvironmentFrom(cls1->GetEnvironment());
  new_inst1->CopyEnvironmentFrom(cls1->GetEnvironment());
  ArenaVector<HInstruction*> current_locals_2({new_inst1},
                                              GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(new_inst2, &current_locals_2);

  HInstruction* first_inst = new_inst1;
  HInstruction* second_inst = new_inst2;

  if (order == TestOrder::kReverseOfAlloc) {
    std::swap(first_inst, second_inst);
  }

  left_crit_break->AddInstruction(new (GetAllocator()) HGoto());

  HInstruction* call_left = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            1,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* if_left = new (GetAllocator()) HIf(param2);
  call_left->AsInvoke()->SetRawInputAt(0, first_inst);
  left->AddInstruction(call_left);
  left->AddInstruction(if_left);
  call_left->CopyEnvironmentFrom(new_inst2->GetEnvironment());

  HInstruction* call_left_left = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            1,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* goto_left_left = new (GetAllocator()) HGoto();
  call_left_left->AsInvoke()->SetRawInputAt(0, second_inst);
  left_left->AddInstruction(call_left_left);
  left_left->AddInstruction(goto_left_left);
  call_left_left->CopyEnvironmentFrom(new_inst2->GetEnvironment());

  left_right->AddInstruction(new (GetAllocator()) HGoto());
  left_end->AddInstruction(new (GetAllocator()) HGoto());

  right->AddInstruction(new (GetAllocator()) HGoto());

  HInstruction* read1 = new (GetAllocator()) HInstanceFieldGet(new_inst1,
                                                               nullptr,
                                                               DataType::Type::kInt32,
                                                               MemberOffset(10),
                                                               false,
                                                               0,
                                                               0,
                                                               graph_->GetDexFile(),
                                                               0);
  HInstruction* read2 = new (GetAllocator()) HInstanceFieldGet(new_inst2,
                                                               nullptr,
                                                               DataType::Type::kInt32,
                                                               MemberOffset(10),
                                                               false,
                                                               0,
                                                               0,
                                                               graph_->GetDexFile(),
                                                               0);
  HInstruction* add_return = new (GetAllocator()) HAdd(DataType::Type::kInt32, read1, read2);
  HInstruction* return_exit = new (GetAllocator()) HReturn(add_return);
  breturn->AddInstruction(read1);
  breturn->AddInstruction(read2);
  breturn->AddInstruction(add_return);
  breturn->AddInstruction(return_exit);

  HInstruction* exit_instruction = new (GetAllocator()) HExit();
  exit->AddInstruction(exit_instruction);
  // PerformLSE expects this to be empty.
  graph_->ClearDominanceInformation();
  LOG(INFO) << "Pre LSE " << blks;
  PerformLSEWithPartial();
  LOG(INFO) << "Post LSE " << blks;

  HNewInstance* moved_new_inst1;
  HInstanceFieldSet* moved_set1;
  HNewInstance* moved_new_inst2;
  HInstanceFieldSet* moved_set2;
  HBasicBlock* first_mat_block = left_crit_break->GetSinglePredecessor();
  HBasicBlock* second_mat_block = left_left->GetSinglePredecessor();
  if (order == TestOrder::kReverseOfAlloc) {
    std::swap(first_mat_block, second_mat_block);
  }
  std::tie(moved_new_inst1, moved_set1) =
      FindSingleInstructions<HNewInstance, HInstanceFieldSet>(graph_, first_mat_block);
  std::tie(moved_new_inst2, moved_set2) =
      FindSingleInstructions<HNewInstance, HInstanceFieldSet>(graph_, second_mat_block);
  std::vector<HPredicatedInstanceFieldGet*> pred_gets;
  std::vector<HPhi*> phis;
  std::tie(pred_gets, phis) = FindAllInstructions<HPredicatedInstanceFieldGet, HPhi>(graph_);
  EXPECT_NE(moved_new_inst1, nullptr);
  EXPECT_NE(moved_new_inst2, nullptr);
  EXPECT_NE(moved_set1, nullptr);
  EXPECT_NE(moved_set2, nullptr);
  EXPECT_INS_RETAINED(call_left);
  EXPECT_INS_RETAINED(call_left_left);
  EXPECT_INS_REMOVED(store1);
  EXPECT_INS_REMOVED(store2);
  EXPECT_INS_REMOVED(read1);
  EXPECT_INS_REMOVED(read2);
  EXPECT_INS_EQ(moved_new_inst2->GetEnvironment()->GetInstructionAt(0),
                order == TestOrder::kSameAsAlloc
                    ? moved_new_inst1
                    : static_cast<HInstruction*>(graph_->GetNullConstant()));
}

// // ENTRY
// obj1 = new Obj1();
// obj2 = new Obj2();
// val1 = 3;
// val2 = 13;
// // The exact order the stores are written affects what the order we perform
// // partial LSE on the values
// obj1/2.field = val1/2;
// obj2/1.field = val2/1;
// if (parameter_value) {
//   // LEFT
//   escape(obj1);
//   escape(obj2);
// } else {
//   // RIGHT
//   // ELIMINATE
//   obj1.field = 2;
//   obj2.field = 12;
// }
// EXIT
// predicated-ELIMINATE
// return obj1.field + obj2.field
TEST_P(OrderDependentTestGroup, FieldSetOrderEnv) {
  ScopedObjectAccess soa(Thread::Current());
  VariableSizedHandleScope vshs(soa.Self());
  CreateGraph(/*handles=*/&vshs);
  AdjacencyListGraph blks(SetupFromAdjacencyList("entry",
                                                 "exit",
                                                 {{"entry", "left"},
                                                  {"entry", "right"},
                                                  {"left", "breturn"},
                                                  {"right", "breturn"},
                                                  {"breturn", "exit"}}));
#define GET_BLOCK(name) HBasicBlock* name = blks.Get(#name)
  GET_BLOCK(entry);
  GET_BLOCK(exit);
  GET_BLOCK(breturn);
  GET_BLOCK(left);
  GET_BLOCK(right);
#undef GET_BLOCK
  TestOrder order = GetParam();
  EnsurePredecessorOrder(breturn, {left, right});
  HInstruction* bool_value = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kBool);
  // HInstruction* null_const = graph_->GetNullConstant();
  HInstruction* c2 = graph_->GetIntConstant(2);
  HInstruction* c3 = graph_->GetIntConstant(3);
  HInstruction* c12 = graph_->GetIntConstant(12);
  HInstruction* c13 = graph_->GetIntConstant(13);
  HInstruction* cls1 = new (GetAllocator()) HLoadClass(graph_->GetCurrentMethod(),
                                                       dex::TypeIndex(10),
                                                       graph_->GetDexFile(),
                                                       ScopedNullHandle<mirror::Class>(),
                                                       false,
                                                       0,
                                                       false);
  HInstruction* cls2 = new (GetAllocator()) HLoadClass(graph_->GetCurrentMethod(),
                                                       dex::TypeIndex(20),
                                                       graph_->GetDexFile(),
                                                       ScopedNullHandle<mirror::Class>(),
                                                       false,
                                                       0,
                                                       false);
  HInstruction* new_inst1 =
      new (GetAllocator()) HNewInstance(cls1,
                                        0,
                                        dex::TypeIndex(10),
                                        graph_->GetDexFile(),
                                        false,
                                        QuickEntrypointEnum::kQuickAllocObjectInitialized);
  HInstruction* new_inst2 =
      new (GetAllocator()) HNewInstance(cls2,
                                        0,
                                        dex::TypeIndex(20),
                                        graph_->GetDexFile(),
                                        false,
                                        QuickEntrypointEnum::kQuickAllocObjectInitialized);
  HInstruction* write_entry1 = new (GetAllocator()) HInstanceFieldSet(new_inst1,
                                                                      c3,
                                                                      nullptr,
                                                                      DataType::Type::kInt32,
                                                                      MemberOffset(10),
                                                                      false,
                                                                      0,
                                                                      0,
                                                                      graph_->GetDexFile(),
                                                                      0);
  HInstruction* write_entry2 = new (GetAllocator()) HInstanceFieldSet(new_inst2,
                                                                      c13,
                                                                      nullptr,
                                                                      DataType::Type::kInt32,
                                                                      MemberOffset(10),
                                                                      false,
                                                                      0,
                                                                      0,
                                                                      graph_->GetDexFile(),
                                                                      0);
  HInstruction* if_inst = new (GetAllocator()) HIf(bool_value);
  entry->AddInstruction(bool_value);
  entry->AddInstruction(cls1);
  entry->AddInstruction(cls2);
  entry->AddInstruction(new_inst1);
  entry->AddInstruction(new_inst2);
  if (order == TestOrder::kSameAsAlloc) {
    entry->AddInstruction(write_entry1);
    entry->AddInstruction(write_entry2);
  } else {
    entry->AddInstruction(write_entry2);
    entry->AddInstruction(write_entry1);
  }
  entry->AddInstruction(if_inst);
  ArenaVector<HInstruction*> current_locals({}, GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(cls1, &current_locals);
  cls2->CopyEnvironmentFrom(cls1->GetEnvironment());
  new_inst1->CopyEnvironmentFrom(cls1->GetEnvironment());
  ArenaVector<HInstruction*> current_locals_2({new_inst1},
                                              GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(new_inst2, &current_locals_2);

  HInstruction* call_left1 = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            1,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* call_left2 = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            1,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* goto_left = new (GetAllocator()) HGoto();
  call_left1->AsInvoke()->SetRawInputAt(0, new_inst1);
  call_left2->AsInvoke()->SetRawInputAt(0, new_inst2);
  left->AddInstruction(call_left1);
  left->AddInstruction(call_left2);
  left->AddInstruction(goto_left);
  call_left1->CopyEnvironmentFrom(cls1->GetEnvironment());
  call_left2->CopyEnvironmentFrom(cls1->GetEnvironment());

  HInstruction* write_right1 = new (GetAllocator()) HInstanceFieldSet(new_inst1,
                                                                      c2,
                                                                      nullptr,
                                                                      DataType::Type::kInt32,
                                                                      MemberOffset(10),
                                                                      false,
                                                                      0,
                                                                      0,
                                                                      graph_->GetDexFile(),
                                                                      0);
  HInstruction* write_right2 = new (GetAllocator()) HInstanceFieldSet(new_inst2,
                                                                      c12,
                                                                      nullptr,
                                                                      DataType::Type::kInt32,
                                                                      MemberOffset(10),
                                                                      false,
                                                                      0,
                                                                      0,
                                                                      graph_->GetDexFile(),
                                                                      0);
  HInstruction* goto_right = new (GetAllocator()) HGoto();
  right->AddInstruction(write_right1);
  right->AddInstruction(write_right2);
  right->AddInstruction(goto_right);

  HInstruction* read_bottom1 = new (GetAllocator()) HInstanceFieldGet(new_inst1,
                                                                      nullptr,
                                                                      DataType::Type::kInt32,
                                                                      MemberOffset(10),
                                                                      false,
                                                                      0,
                                                                      0,
                                                                      graph_->GetDexFile(),
                                                                      0);
  HInstruction* read_bottom2 = new (GetAllocator()) HInstanceFieldGet(new_inst2,
                                                                      nullptr,
                                                                      DataType::Type::kInt32,
                                                                      MemberOffset(10),
                                                                      false,
                                                                      0,
                                                                      0,
                                                                      graph_->GetDexFile(),
                                                                      0);
  HInstruction* combine =
      new (GetAllocator()) HAdd(DataType::Type::kInt32, read_bottom1, read_bottom2);
  HInstruction* return_exit = new (GetAllocator()) HReturn(combine);
  breturn->AddInstruction(read_bottom1);
  breturn->AddInstruction(read_bottom2);
  breturn->AddInstruction(combine);
  breturn->AddInstruction(return_exit);

  HInstruction* exit_instruction = new (GetAllocator()) HExit();
  exit->AddInstruction(exit_instruction);
  // PerformLSE expects this to be empty.
  graph_->ClearDominanceInformation();
  LOG(INFO) << "Pre LSE " << blks;
  PerformLSEWithPartial();
  LOG(INFO) << "Post LSE " << blks;

  EXPECT_TRUE(IsRemoved(read_bottom1));
  EXPECT_TRUE(IsRemoved(read_bottom2));
  EXPECT_TRUE(IsRemoved(write_right1));
  EXPECT_TRUE(IsRemoved(write_right2));
  EXPECT_FALSE(IsRemoved(call_left1));
  EXPECT_FALSE(IsRemoved(call_left2));
  std::vector<HPhi*> merges;
  std::vector<HPredicatedInstanceFieldGet*> pred_gets;
  std::vector<HNewInstance*> materializations;
  std::tie(merges, pred_gets) =
      FindAllInstructions<HPhi, HPredicatedInstanceFieldGet>(graph_, breturn);
  std::tie(materializations) = FindAllInstructions<HNewInstance>(graph_);
  ASSERT_EQ(merges.size(), 4u);
  ASSERT_EQ(pred_gets.size(), 2u);
  ASSERT_EQ(materializations.size(), 2u);
  HPhi* merge_value_return1 = *std::find_if(merges.begin(), merges.end(), [&](HPhi* p) {
    return p->GetType() == DataType::Type::kInt32 && p->InputAt(1) == c2;
  });
  HPhi* merge_value_return2 = *std::find_if(merges.begin(), merges.end(), [&](HPhi* p) {
    return p->GetType() == DataType::Type::kInt32 && p->InputAt(1) == c12;
  });
  HNewInstance* mat_alloc1 = *std::find_if(materializations.begin(),
                                           materializations.end(),
                                           [&](HNewInstance* n) { return n->InputAt(0) == cls1; });
  HNewInstance* mat_alloc2 = *std::find_if(materializations.begin(),
                                           materializations.end(),
                                           [&](HNewInstance* n) { return n->InputAt(0) == cls2; });
  HPhi* merge_alloc1 = *std::find_if(merges.begin(), merges.end(), [&](HPhi* p) {
    return p->GetType() == DataType::Type::kReference && p->InputAt(0)->InputAt(0) == cls1;
  });
  HPhi* merge_alloc2 = *std::find_if(merges.begin(), merges.end(), [&](HPhi* p) {
    return p->GetType() == DataType::Type::kReference && p->InputAt(0)->InputAt(0) == cls2;
  });
  HPredicatedInstanceFieldGet* pred_get1 =
      *std::find_if(pred_gets.begin(), pred_gets.end(), [&](HPredicatedInstanceFieldGet* pg) {
        return pg->InputAt(0) == merge_alloc1;
      });
  HPredicatedInstanceFieldGet* pred_get2 =
      *std::find_if(pred_gets.begin(), pred_gets.end(), [&](HPredicatedInstanceFieldGet* pg) {
        return pg->InputAt(0) == merge_alloc2;
      });
  ASSERT_NE(merge_alloc1, nullptr);
  EXPECT_INS_EQ(merge_alloc1->InputAt(0), mat_alloc1);
  EXPECT_INS_EQ(merge_alloc1->InputAt(1), graph_->GetNullConstant());
  ASSERT_NE(merge_alloc2, nullptr);
  EXPECT_INS_EQ(merge_alloc2->InputAt(0), mat_alloc2);
  EXPECT_INS_EQ(merge_alloc2->InputAt(1), graph_->GetNullConstant());
  ASSERT_NE(pred_get1, nullptr);
  EXPECT_INS_EQ(pred_get1->InputAt(0), merge_alloc1);
  EXPECT_INS_EQ(pred_get1->InputAt(1), merge_value_return1) << " pred-get is: " << *pred_get1;
  EXPECT_INS_EQ(merge_value_return1->InputAt(0), graph_->GetIntConstant(0))
      << " merge val is: " << *merge_value_return1;
  EXPECT_INS_EQ(merge_value_return1->InputAt(1), c2) << " merge val is: " << *merge_value_return1;
  ASSERT_NE(pred_get2, nullptr);
  EXPECT_INS_EQ(pred_get2->InputAt(0), merge_alloc2);
  EXPECT_INS_EQ(pred_get2->InputAt(1), merge_value_return2) << " pred-get is: " << *pred_get2;
  EXPECT_INS_EQ(merge_value_return2->InputAt(0), graph_->GetIntConstant(0))
      << " merge val is: " << *merge_value_return1;
  EXPECT_INS_EQ(merge_value_return2->InputAt(1), c12) << " merge val is: " << *merge_value_return1;
  EXPECT_INS_EQ(mat_alloc2->GetEnvironment()->GetInstructionAt(0), mat_alloc1);
}

INSTANTIATE_TEST_SUITE_P(LoadStoreEliminationTest,
                         OrderDependentTestGroup,
                         testing::Values(TestOrder::kSameAsAlloc, TestOrder::kReverseOfAlloc));

// // ENTRY
// // To be moved
// obj = new Obj();
// obj.foo = 12;
// if (parameter_value) {
//   // LEFT
//   escape(obj);
// }
// EXIT
TEST_F(LoadStoreEliminationTest, MovePredicatedAlloc) {
  ScopedObjectAccess soa(Thread::Current());
  VariableSizedHandleScope vshs(soa.Self());
  CreateGraph(&vshs);
  AdjacencyListGraph blks(SetupFromAdjacencyList(
      "entry",
      "exit",
      {{"entry", "left"}, {"entry", "breturn"}, {"left", "breturn"}, {"breturn", "exit"}}));
#define GET_BLOCK(name) HBasicBlock* name = blks.Get(#name)
  GET_BLOCK(entry);
  GET_BLOCK(exit);
  GET_BLOCK(breturn);
  GET_BLOCK(left);
#undef GET_BLOCK
  EnsurePredecessorOrder(breturn, {left, entry});
  HInstruction* bool_value = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kBool);
  HInstruction* c12 = graph_->GetIntConstant(12);
  HInstruction* cls = new (GetAllocator()) HLoadClass(graph_->GetCurrentMethod(),
                                                      dex::TypeIndex(10),
                                                      graph_->GetDexFile(),
                                                      ScopedNullHandle<mirror::Class>(),
                                                      false,
                                                      0,
                                                      false);
  HInstruction* new_inst =
      new (GetAllocator()) HNewInstance(cls,
                                        0,
                                        dex::TypeIndex(10),
                                        graph_->GetDexFile(),
                                        false,
                                        QuickEntrypointEnum::kQuickAllocObjectInitialized);
  HInstruction* store = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                               c12,
                                                               nullptr,
                                                               DataType::Type::kInt32,
                                                               MemberOffset(10),
                                                               false,
                                                               0,
                                                               0,
                                                               graph_->GetDexFile(),
                                                               0);
  HInstruction* if_inst = new (GetAllocator()) HIf(bool_value);
  entry->AddInstruction(bool_value);
  entry->AddInstruction(cls);
  entry->AddInstruction(new_inst);
  entry->AddInstruction(store);
  entry->AddInstruction(if_inst);
  ArenaVector<HInstruction*> current_locals({}, GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(cls, &current_locals);
  new_inst->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* call_left = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            1,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* goto_left = new (GetAllocator()) HGoto();
  call_left->AsInvoke()->SetRawInputAt(0, new_inst);
  left->AddInstruction(call_left);
  left->AddInstruction(goto_left);
  call_left->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* return_exit = new (GetAllocator()) HReturnVoid();
  breturn->AddInstruction(return_exit);

  HInstruction* exit_instruction = new (GetAllocator()) HExit();
  exit->AddInstruction(exit_instruction);
  // PerformLSE expects this to be empty.
  graph_->ClearDominanceInformation();
  LOG(INFO) << "Pre LSE " << blks;
  PerformLSEWithPartial();
  LOG(INFO) << "Post LSE " << blks;

  HNewInstance* moved_new_inst = nullptr;
  HInstanceFieldSet* moved_set = nullptr;
  std::tie(moved_new_inst, moved_set) =
      FindSingleInstructions<HNewInstance, HInstanceFieldSet>(graph_);
  EXPECT_NE(moved_new_inst, nullptr);
  EXPECT_NE(moved_set, nullptr);
  EXPECT_FALSE(IsRemoved(call_left));
  // store removed or moved.
  EXPECT_NE(store->GetBlock(), entry);
  // New-inst removed or moved.
  EXPECT_NE(new_inst->GetBlock(), entry);
  EXPECT_INS_EQ(moved_set->InputAt(0), moved_new_inst);
  EXPECT_INS_EQ(moved_set->InputAt(1), c12);
}

// // ENTRY
// // To be moved
// obj = new Obj();
// obj.foo = 12;
// if (parameter_value) {
//   // LEFT
//   escape(obj);
// }
// EXIT
// int a = obj.foo;
// obj.foo = 13;
// noescape();
// int b = obj.foo;
// obj.foo = 14;
// noescape();
// int c = obj.foo;
// obj.foo = 15;
// noescape();
// return a + b + c
TEST_F(LoadStoreEliminationTest, MutiPartialLoadStore) {
  ScopedObjectAccess soa(Thread::Current());
  VariableSizedHandleScope vshs(soa.Self());
  CreateGraph(&vshs);
  AdjacencyListGraph blks(SetupFromAdjacencyList("entry",
                                                 "exit",
                                                 {{"entry", "left"},
                                                  {"entry", "right"},
                                                  {"right", "breturn"},
                                                  {"left", "breturn"},
                                                  {"breturn", "exit"}}));
#define GET_BLOCK(name) HBasicBlock* name = blks.Get(#name)
  GET_BLOCK(entry);
  GET_BLOCK(exit);
  GET_BLOCK(breturn);
  GET_BLOCK(left);
  GET_BLOCK(right);
#undef GET_BLOCK
  EnsurePredecessorOrder(breturn, {left, right});
  HInstruction* bool_value = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kBool);
  HInstruction* c12 = graph_->GetIntConstant(12);
  HInstruction* c13 = graph_->GetIntConstant(13);
  HInstruction* c14 = graph_->GetIntConstant(14);
  HInstruction* c15 = graph_->GetIntConstant(15);
  HInstruction* cls = new (GetAllocator()) HLoadClass(graph_->GetCurrentMethod(),
                                                      dex::TypeIndex(10),
                                                      graph_->GetDexFile(),
                                                      ScopedNullHandle<mirror::Class>(),
                                                      false,
                                                      0,
                                                      false);
  HInstruction* new_inst =
      new (GetAllocator()) HNewInstance(cls,
                                        0,
                                        dex::TypeIndex(10),
                                        graph_->GetDexFile(),
                                        false,
                                        QuickEntrypointEnum::kQuickAllocObjectInitialized);
  HInstruction* store = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                               c12,
                                                               nullptr,
                                                               DataType::Type::kInt32,
                                                               MemberOffset(10),
                                                               false,
                                                               0,
                                                               0,
                                                               graph_->GetDexFile(),
                                                               0);
  HInstruction* if_inst = new (GetAllocator()) HIf(bool_value);
  entry->AddInstruction(bool_value);
  entry->AddInstruction(cls);
  entry->AddInstruction(new_inst);
  entry->AddInstruction(store);
  entry->AddInstruction(if_inst);
  ArenaVector<HInstruction*> current_locals({}, GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(cls, &current_locals);
  new_inst->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* call_left = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            1,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* goto_left = new (GetAllocator()) HGoto();
  call_left->AsInvoke()->SetRawInputAt(0, new_inst);
  left->AddInstruction(call_left);
  left->AddInstruction(goto_left);
  call_left->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* goto_right = new (GetAllocator()) HGoto();
  right->AddInstruction(goto_right);

  HInstruction* a_val = new (GetAllocator()) HInstanceFieldGet(new_inst,
                                                               nullptr,
                                                               DataType::Type::kInt32,
                                                               MemberOffset(10),
                                                               false,
                                                               0,
                                                               0,
                                                               graph_->GetDexFile(),
                                                               0);
  HInstruction* a_reset = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                 c13,
                                                                 nullptr,
                                                                 DataType::Type::kInt32,
                                                                 MemberOffset(10),
                                                                 false,
                                                                 0,
                                                                 0,
                                                                 graph_->GetDexFile(),
                                                                 0);
  HInstruction* a_noescape = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            0,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* b_val = new (GetAllocator()) HInstanceFieldGet(new_inst,
                                                               nullptr,
                                                               DataType::Type::kInt32,
                                                               MemberOffset(10),
                                                               false,
                                                               0,
                                                               0,
                                                               graph_->GetDexFile(),
                                                               0);
  HInstruction* b_reset = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                 c14,
                                                                 nullptr,
                                                                 DataType::Type::kInt32,
                                                                 MemberOffset(10),
                                                                 false,
                                                                 0,
                                                                 0,
                                                                 graph_->GetDexFile(),
                                                                 0);
  HInstruction* b_noescape = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            0,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* c_val = new (GetAllocator()) HInstanceFieldGet(new_inst,
                                                               nullptr,
                                                               DataType::Type::kInt32,
                                                               MemberOffset(10),
                                                               false,
                                                               0,
                                                               0,
                                                               graph_->GetDexFile(),
                                                               0);
  HInstruction* c_reset = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                 c15,
                                                                 nullptr,
                                                                 DataType::Type::kInt32,
                                                                 MemberOffset(10),
                                                                 false,
                                                                 0,
                                                                 0,
                                                                 graph_->GetDexFile(),
                                                                 0);
  HInstruction* c_noescape = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            0,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* add_1_exit = new (GetAllocator()) HAdd(DataType::Type::kInt32, a_val, b_val);
  HInstruction* add_2_exit = new (GetAllocator()) HAdd(DataType::Type::kInt32, c_val, add_1_exit);
  HInstruction* return_exit = new (GetAllocator()) HReturn(add_2_exit);
  breturn->AddInstruction(a_val);
  breturn->AddInstruction(a_reset);
  breturn->AddInstruction(a_noescape);
  breturn->AddInstruction(b_val);
  breturn->AddInstruction(b_reset);
  breturn->AddInstruction(b_noescape);
  breturn->AddInstruction(c_val);
  breturn->AddInstruction(c_reset);
  breturn->AddInstruction(c_noescape);
  breturn->AddInstruction(add_1_exit);
  breturn->AddInstruction(add_2_exit);
  breturn->AddInstruction(return_exit);
  ArenaVector<HInstruction*> current_locals_a({new_inst, a_val},
                                              GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(a_noescape, &current_locals_a);
  ArenaVector<HInstruction*> current_locals_b({new_inst, a_val, b_val},
                                              GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(b_noescape, &current_locals_b);
  ArenaVector<HInstruction*> current_locals_c({new_inst, a_val, b_val, c_val},
                                              GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(c_noescape, &current_locals_c);

  HInstruction* exit_instruction = new (GetAllocator()) HExit();
  exit->AddInstruction(exit_instruction);
  // PerformLSE expects this to be empty.
  graph_->ClearDominanceInformation();
  LOG(INFO) << "Pre LSE " << blks;
  PerformLSEWithPartial();
  LOG(INFO) << "Post LSE " << blks;

  HNewInstance* moved_new_inst = nullptr;
  HInstanceFieldSet* moved_set = nullptr;
  std::tie(moved_new_inst, moved_set) =
      FindSingleInstructions<HNewInstance, HInstanceFieldSet>(graph_, left->GetSinglePredecessor());
  std::vector<HPredicatedInstanceFieldGet*> pred_gets;
  std::vector<HInstanceFieldSet*> pred_sets;
  std::vector<HPhi*> return_phis;
  std::tie(return_phis, pred_gets, pred_sets) =
      FindAllInstructions<HPhi, HPredicatedInstanceFieldGet, HInstanceFieldSet>(graph_, breturn);
  ASSERT_EQ(return_phis.size(), 2u);
  HPhi* inst_phi = return_phis[0];
  HPhi* val_phi = return_phis[1];
  if (inst_phi->GetType() != DataType::Type::kReference) {
    std::swap(inst_phi, val_phi);
  }
  EXPECT_INS_EQ(inst_phi->InputAt(0), moved_new_inst);
  EXPECT_INS_EQ(inst_phi->InputAt(1), graph_->GetNullConstant());
  EXPECT_INS_EQ(val_phi->InputAt(0), graph_->GetIntConstant(0));
  EXPECT_EQ(val_phi->InputAt(1), c12);
  ASSERT_EQ(pred_gets.size(), 3u);
  ASSERT_EQ(pred_gets.size(), pred_sets.size());
  std::vector<HInstruction*> set_values{c13, c14, c15};
  std::vector<HInstruction*> get_values{val_phi, c13, c14};
  EXPECT_NE(moved_new_inst, nullptr);
  EXPECT_NE(moved_set, nullptr);
  EXPECT_INS_EQ(moved_set->InputAt(0), moved_new_inst);
  EXPECT_INS_EQ(moved_set->InputAt(1), c12);
  EXPECT_FALSE(IsRemoved(call_left));
  // store removed or moved.
  EXPECT_NE(store->GetBlock(), entry);
  // New-inst removed or moved.
  EXPECT_NE(new_inst->GetBlock(), entry);
  for (auto [get, val] : ZipLeft(MakeIterationRange(pred_gets), MakeIterationRange(get_values))) {
    EXPECT_INS_EQ(get->InputAt(1), val);
  }
  for (auto [set, val] : ZipLeft(MakeIterationRange(pred_sets), MakeIterationRange(set_values))) {
    EXPECT_INS_EQ(set->InputAt(1), val);
    EXPECT_TRUE(set->GetIsPredicatedSet()) << *set;
  }
  EXPECT_FALSE(IsRemoved(a_noescape));
  EXPECT_FALSE(IsRemoved(b_noescape));
  EXPECT_FALSE(IsRemoved(c_noescape));
  EXPECT_INS_EQ(add_1_exit->InputAt(0), pred_gets[0]);
  EXPECT_INS_EQ(add_1_exit->InputAt(1), pred_gets[1]);
  EXPECT_INS_EQ(add_2_exit->InputAt(0), pred_gets[2]);

  EXPECT_EQ(a_noescape->GetEnvironment()->Size(), 2u);
  EXPECT_INS_EQ(a_noescape->GetEnvironment()->GetInstructionAt(0), inst_phi);
  EXPECT_INS_EQ(a_noescape->GetEnvironment()->GetInstructionAt(1), pred_gets[0]);
  EXPECT_EQ(b_noescape->GetEnvironment()->Size(), 3u);
  EXPECT_INS_EQ(b_noescape->GetEnvironment()->GetInstructionAt(0), inst_phi);
  EXPECT_INS_EQ(b_noescape->GetEnvironment()->GetInstructionAt(1), pred_gets[0]);
  EXPECT_INS_EQ(b_noescape->GetEnvironment()->GetInstructionAt(2), pred_gets[1]);
  EXPECT_EQ(c_noescape->GetEnvironment()->Size(), 4u);
  EXPECT_INS_EQ(c_noescape->GetEnvironment()->GetInstructionAt(0), inst_phi);
  EXPECT_INS_EQ(c_noescape->GetEnvironment()->GetInstructionAt(1), pred_gets[0]);
  EXPECT_INS_EQ(c_noescape->GetEnvironment()->GetInstructionAt(2), pred_gets[1]);
  EXPECT_INS_EQ(c_noescape->GetEnvironment()->GetInstructionAt(3), pred_gets[2]);
}

// // ENTRY
// // To be moved
// obj = new Obj();
// obj.foo = 12;
// int a = obj.foo;
// obj.foo = 13;
// noescape();
// int b = obj.foo;
// obj.foo = 14;
// noescape();
// int c = obj.foo;
// obj.foo = 15;
// noescape();
// if (parameter_value) {
//   // LEFT
//   escape(obj);
// }
// EXIT
// return a + b + c + obj.foo
TEST_F(LoadStoreEliminationTest, MutiPartialLoadStore2) {
  ScopedObjectAccess soa(Thread::Current());
  VariableSizedHandleScope vshs(soa.Self());
  CreateGraph(&vshs);
  // Need to have an actual entry block since we check env-layout and the way we
  // add constants would screw this up otherwise.
  AdjacencyListGraph blks(SetupFromAdjacencyList("start",
                                                 "exit",
                                                 {{"start", "entry"},
                                                  {"entry", "left"},
                                                  {"entry", "right"},
                                                  {"right", "breturn"},
                                                  {"left", "breturn"},
                                                  {"breturn", "exit"}}));
#define GET_BLOCK(name) HBasicBlock* name = blks.Get(#name)
  GET_BLOCK(start);
  GET_BLOCK(entry);
  GET_BLOCK(exit);
  GET_BLOCK(breturn);
  GET_BLOCK(left);
  GET_BLOCK(right);
#undef GET_BLOCK
  EnsurePredecessorOrder(breturn, {left, right});
  HInstruction* bool_value = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kBool);
  HInstruction* c12 = graph_->GetIntConstant(12);
  HInstruction* c13 = graph_->GetIntConstant(13);
  HInstruction* c14 = graph_->GetIntConstant(14);
  HInstruction* c15 = graph_->GetIntConstant(15);
  HInstruction* start_suspend = new (GetAllocator()) HSuspendCheck();
  HInstruction* start_goto = new (GetAllocator()) HGoto();

  start->AddInstruction(bool_value);
  start->AddInstruction(start_suspend);
  start->AddInstruction(start_goto);
  ArenaVector<HInstruction*> current_locals_suspend(
      {}, GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(start_suspend, &current_locals_suspend);

  HInstruction* cls = new (GetAllocator()) HLoadClass(graph_->GetCurrentMethod(),
                                                      dex::TypeIndex(10),
                                                      graph_->GetDexFile(),
                                                      ScopedNullHandle<mirror::Class>(),
                                                      false,
                                                      0,
                                                      false);
  HInstruction* new_inst =
      new (GetAllocator()) HNewInstance(cls,
                                        0,
                                        dex::TypeIndex(10),
                                        graph_->GetDexFile(),
                                        false,
                                        QuickEntrypointEnum::kQuickAllocObjectInitialized);
  HInstruction* store = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                               c12,
                                                               nullptr,
                                                               DataType::Type::kInt32,
                                                               MemberOffset(10),
                                                               false,
                                                               0,
                                                               0,
                                                               graph_->GetDexFile(),
                                                               0);

  HInstruction* a_val = new (GetAllocator()) HInstanceFieldGet(new_inst,
                                                               nullptr,
                                                               DataType::Type::kInt32,
                                                               MemberOffset(10),
                                                               false,
                                                               0,
                                                               0,
                                                               graph_->GetDexFile(),
                                                               0);
  HInstruction* a_reset = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                 c13,
                                                                 nullptr,
                                                                 DataType::Type::kInt32,
                                                                 MemberOffset(10),
                                                                 false,
                                                                 0,
                                                                 0,
                                                                 graph_->GetDexFile(),
                                                                 0);
  HInstruction* a_noescape = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            0,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* b_val = new (GetAllocator()) HInstanceFieldGet(new_inst,
                                                               nullptr,
                                                               DataType::Type::kInt32,
                                                               MemberOffset(10),
                                                               false,
                                                               0,
                                                               0,
                                                               graph_->GetDexFile(),
                                                               0);
  HInstruction* b_reset = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                 c14,
                                                                 nullptr,
                                                                 DataType::Type::kInt32,
                                                                 MemberOffset(10),
                                                                 false,
                                                                 0,
                                                                 0,
                                                                 graph_->GetDexFile(),
                                                                 0);
  HInstruction* b_noescape = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            0,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* c_val = new (GetAllocator()) HInstanceFieldGet(new_inst,
                                                               nullptr,
                                                               DataType::Type::kInt32,
                                                               MemberOffset(10),
                                                               false,
                                                               0,
                                                               0,
                                                               graph_->GetDexFile(),
                                                               0);
  HInstruction* c_reset = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                 c15,
                                                                 nullptr,
                                                                 DataType::Type::kInt32,
                                                                 MemberOffset(10),
                                                                 false,
                                                                 0,
                                                                 0,
                                                                 graph_->GetDexFile(),
                                                                 0);
  HInstruction* c_noescape = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            0,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* if_inst = new (GetAllocator()) HIf(bool_value);
  entry->AddInstruction(cls);
  entry->AddInstruction(new_inst);
  entry->AddInstruction(store);
  entry->AddInstruction(a_val);
  entry->AddInstruction(a_reset);
  entry->AddInstruction(a_noescape);
  entry->AddInstruction(b_val);
  entry->AddInstruction(b_reset);
  entry->AddInstruction(b_noescape);
  entry->AddInstruction(c_val);
  entry->AddInstruction(c_reset);
  entry->AddInstruction(c_noescape);
  entry->AddInstruction(if_inst);
  ArenaVector<HInstruction*> current_locals_cls({},
                                                GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(cls, &current_locals_cls);
  new_inst->CopyEnvironmentFrom(cls->GetEnvironment());
  ArenaVector<HInstruction*> current_locals_a({new_inst, a_val},
                                              GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(a_noescape, &current_locals_a);
  ArenaVector<HInstruction*> current_locals_b({new_inst, a_val, b_val},
                                              GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(b_noescape, &current_locals_b);
  ArenaVector<HInstruction*> current_locals_c({new_inst, a_val, b_val, c_val},
                                              GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(c_noescape, &current_locals_c);

  HInstruction* call_left = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            1,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* goto_left = new (GetAllocator()) HGoto();
  call_left->AsInvoke()->SetRawInputAt(0, new_inst);
  left->AddInstruction(call_left);
  left->AddInstruction(goto_left);
  call_left->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* goto_right = new (GetAllocator()) HGoto();
  right->AddInstruction(goto_right);

  HInstruction* val_exit = new (GetAllocator()) HInstanceFieldGet(new_inst,
                                                                  nullptr,
                                                                  DataType::Type::kInt32,
                                                                  MemberOffset(10),
                                                                  false,
                                                                  0,
                                                                  0,
                                                                  graph_->GetDexFile(),
                                                                  0);
  HInstruction* add_1_exit = new (GetAllocator()) HAdd(DataType::Type::kInt32, a_val, b_val);
  HInstruction* add_2_exit = new (GetAllocator()) HAdd(DataType::Type::kInt32, c_val, add_1_exit);
  HInstruction* add_3_exit =
      new (GetAllocator()) HAdd(DataType::Type::kInt32, val_exit, add_2_exit);
  HInstruction* return_exit = new (GetAllocator()) HReturn(add_3_exit);
  breturn->AddInstruction(val_exit);
  breturn->AddInstruction(add_1_exit);
  breturn->AddInstruction(add_2_exit);
  breturn->AddInstruction(add_3_exit);
  breturn->AddInstruction(return_exit);

  HInstruction* exit_instruction = new (GetAllocator()) HExit();
  exit->AddInstruction(exit_instruction);
  // PerformLSE expects this to be empty.
  graph_->ClearDominanceInformation();
  LOG(INFO) << "Pre LSE " << blks;
  PerformLSEWithPartial();
  LOG(INFO) << "Post LSE " << blks;

  HNewInstance* moved_new_inst = nullptr;
  HInstanceFieldSet* moved_set = nullptr;
  std::tie(moved_new_inst, moved_set) =
      FindSingleInstructions<HNewInstance, HInstanceFieldSet>(graph_, left->GetSinglePredecessor());
  std::vector<HPredicatedInstanceFieldGet*> pred_gets;
  std::vector<HInstanceFieldSet*> pred_sets;
  std::vector<HPhi*> return_phis;
  std::tie(return_phis, pred_gets, pred_sets) =
      FindAllInstructions<HPhi, HPredicatedInstanceFieldGet, HInstanceFieldSet>(graph_, breturn);
  ASSERT_EQ(return_phis.size(), 2u);
  HPhi* inst_phi = return_phis[0];
  HPhi* val_phi = return_phis[1];
  if (inst_phi->GetType() != DataType::Type::kReference) {
    std::swap(inst_phi, val_phi);
  }
  EXPECT_INS_EQ(inst_phi->InputAt(0), moved_new_inst);
  EXPECT_INS_EQ(inst_phi->InputAt(1), graph_->GetNullConstant());
  EXPECT_INS_EQ(val_phi->InputAt(0), graph_->GetIntConstant(0));
  EXPECT_INS_EQ(val_phi->InputAt(1), c15);
  ASSERT_EQ(pred_gets.size(), 1u);
  ASSERT_EQ(pred_sets.size(), 0u);
  EXPECT_NE(moved_new_inst, nullptr);
  EXPECT_NE(moved_set, nullptr);
  EXPECT_INS_EQ(moved_set->InputAt(0), moved_new_inst);
  EXPECT_INS_EQ(moved_set->InputAt(1), c15);
  EXPECT_FALSE(IsRemoved(call_left));
  // store removed or moved.
  EXPECT_NE(store->GetBlock(), entry);
  // New-inst removed or moved.
  EXPECT_NE(new_inst->GetBlock(), entry);
  EXPECT_TRUE(IsRemoved(a_val));
  EXPECT_TRUE(IsRemoved(b_val));
  EXPECT_TRUE(IsRemoved(c_val));
  EXPECT_FALSE(IsRemoved(a_noescape));
  EXPECT_FALSE(IsRemoved(b_noescape));
  EXPECT_FALSE(IsRemoved(c_noescape));
  EXPECT_INS_EQ(add_1_exit->InputAt(0), c12);
  EXPECT_INS_EQ(add_1_exit->InputAt(1), c13);
  EXPECT_INS_EQ(add_2_exit->InputAt(0), c14);
  EXPECT_INS_EQ(add_2_exit->InputAt(1), add_1_exit);
  EXPECT_INS_EQ(add_3_exit->InputAt(0), pred_gets[0]);
  EXPECT_INS_EQ(pred_gets[0]->InputAt(1), val_phi);
  EXPECT_INS_EQ(add_3_exit->InputAt(1), add_2_exit);
  EXPECT_EQ(a_noescape->GetEnvironment()->Size(), 2u);
  EXPECT_INS_EQ(a_noescape->GetEnvironment()->GetInstructionAt(0), graph_->GetNullConstant());
  EXPECT_INS_EQ(a_noescape->GetEnvironment()->GetInstructionAt(1), c12);
  EXPECT_EQ(b_noescape->GetEnvironment()->Size(), 3u);
  EXPECT_INS_EQ(b_noescape->GetEnvironment()->GetInstructionAt(0), graph_->GetNullConstant());
  EXPECT_INS_EQ(b_noescape->GetEnvironment()->GetInstructionAt(1), c12);
  EXPECT_INS_EQ(b_noescape->GetEnvironment()->GetInstructionAt(2), c13);
  EXPECT_EQ(c_noescape->GetEnvironment()->Size(), 4u);
  EXPECT_INS_EQ(c_noescape->GetEnvironment()->GetInstructionAt(0), graph_->GetNullConstant());
  EXPECT_INS_EQ(c_noescape->GetEnvironment()->GetInstructionAt(1), c12);
  EXPECT_INS_EQ(c_noescape->GetEnvironment()->GetInstructionAt(2), c13);
  EXPECT_INS_EQ(c_noescape->GetEnvironment()->GetInstructionAt(3), c14);
}

// // ENTRY
// // To be moved
// obj = new Obj();
// // Transforms required for creation non-trivial and unimportant
// if (parameter_value) {
//   obj.foo = 10
// } else {
//   obj.foo = 12;
// }
// if (parameter_value_2) {
//   escape(obj);
// }
// EXIT
TEST_F(LoadStoreEliminationTest, MovePredicatedAlloc2) {
  ScopedObjectAccess soa(Thread::Current());
  VariableSizedHandleScope vshs(soa.Self());
  CreateGraph(&vshs);
  AdjacencyListGraph blks(SetupFromAdjacencyList("entry",
                                                 "exit",
                                                 {{"entry", "left_set"},
                                                  {"entry", "right_set"},
                                                  {"left_set", "merge"},
                                                  {"right_set", "merge"},
                                                  {"merge", "escape"},
                                                  {"escape", "breturn"},
                                                  {"merge", "breturn"},
                                                  {"breturn", "exit"}}));
#define GET_BLOCK(name) HBasicBlock* name = blks.Get(#name)
  GET_BLOCK(entry);
  GET_BLOCK(exit);
  GET_BLOCK(breturn);
  GET_BLOCK(left_set);
  GET_BLOCK(right_set);
  GET_BLOCK(merge);
  GET_BLOCK(escape);
#undef GET_BLOCK
  EnsurePredecessorOrder(breturn, {merge, escape});
  HInstruction* bool_value = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kBool);
  HInstruction* bool_value_2 = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 2, DataType::Type::kBool);
  HInstruction* c10 = graph_->GetIntConstant(10);
  HInstruction* c12 = graph_->GetIntConstant(12);
  HInstruction* cls = new (GetAllocator()) HLoadClass(graph_->GetCurrentMethod(),
                                                      dex::TypeIndex(10),
                                                      graph_->GetDexFile(),
                                                      ScopedNullHandle<mirror::Class>(),
                                                      false,
                                                      0,
                                                      false);
  HInstruction* new_inst =
      new (GetAllocator()) HNewInstance(cls,
                                        0,
                                        dex::TypeIndex(10),
                                        graph_->GetDexFile(),
                                        false,
                                        QuickEntrypointEnum::kQuickAllocObjectInitialized);
  HInstruction* if_inst = new (GetAllocator()) HIf(bool_value);
  entry->AddInstruction(bool_value);
  entry->AddInstruction(bool_value_2);
  entry->AddInstruction(cls);
  entry->AddInstruction(new_inst);
  entry->AddInstruction(if_inst);
  ArenaVector<HInstruction*> current_locals({}, GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(cls, &current_locals);
  new_inst->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* store_left = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                    c10,
                                                                    nullptr,
                                                                    DataType::Type::kInt32,
                                                                    MemberOffset(10),
                                                                    false,
                                                                    0,
                                                                    0,
                                                                    graph_->GetDexFile(),
                                                                    0);
  HInstruction* goto_left = new (GetAllocator()) HGoto();
  left_set->AddInstruction(store_left);
  left_set->AddInstruction(goto_left);

  HInstruction* store_right = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                     c12,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* goto_right = new (GetAllocator()) HGoto();
  right_set->AddInstruction(store_right);
  right_set->AddInstruction(goto_right);

  HInstruction* if_merge = new (GetAllocator()) HIf(bool_value_2);
  merge->AddInstruction(if_merge);

  HInstruction* escape_instruction = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            1,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* escape_goto = new (GetAllocator()) HGoto();
  escape_instruction->AsInvoke()->SetRawInputAt(0, new_inst);
  escape->AddInstruction(escape_instruction);
  escape->AddInstruction(escape_goto);
  escape_instruction->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* return_exit = new (GetAllocator()) HReturnVoid();
  breturn->AddInstruction(return_exit);

  HInstruction* exit_instruction = new (GetAllocator()) HExit();
  exit->AddInstruction(exit_instruction);
  // PerformLSE expects this to be empty.
  graph_->ClearDominanceInformation();
  LOG(INFO) << "Pre LSE " << blks;
  PerformLSEWithPartial();
  LOG(INFO) << "Post LSE " << blks;

  std::vector<HNewInstance*> moved_new_insts;
  std::vector<HInstanceFieldSet*> moved_sets;
  std::tie(moved_new_insts, moved_sets) =
      FindAllInstructions<HNewInstance, HInstanceFieldSet>(graph_);
  HPhi* merge_phi = FindSingleInstruction<HPhi>(graph_, merge);
  HPhi* alloc_phi = FindSingleInstruction<HPhi>(graph_, breturn);
  EXPECT_EQ(moved_new_insts.size(), 1u);
  EXPECT_EQ(moved_sets.size(), 1u);
  EXPECT_TRUE(std::all_of(moved_sets.begin(), moved_sets.end(), [&](HInstanceFieldSet* set) {
    return std::find(moved_new_insts.begin(), moved_new_insts.end(), set->InputAt(0)) !=
           moved_new_insts.end();
  }));
  ASSERT_NE(alloc_phi, nullptr);
  EXPECT_EQ(alloc_phi->InputAt(0), graph_->GetNullConstant())
      << alloc_phi->GetBlock()->GetPredecessors()[0]->GetBlockId() << " " << *alloc_phi;
  EXPECT_TRUE(alloc_phi->InputAt(1)->IsNewInstance()) << *alloc_phi;
  ASSERT_NE(merge_phi, nullptr);
  EXPECT_EQ(merge_phi->InputCount(), 2u);
  EXPECT_TRUE(std::all_of(merge_phi->GetInputs().begin(),
                          merge_phi->GetInputs().end(),
                          [&](HInstruction* ins) { return ins == c10 || ins == c12; }));
  EXPECT_TRUE(merge_phi->GetUses().HasExactlyOneElement());
  EXPECT_TRUE(merge_phi->GetUses().front().GetUser() == moved_sets[0]);
  EXPECT_FALSE(IsRemoved(escape_instruction));
  EXPECT_EQ(escape_instruction->InputAt(0), moved_new_insts[0]);
  // store removed or moved.
  EXPECT_NE(store_left->GetBlock(), left_set);
  EXPECT_NE(store_right->GetBlock(), left_set);
  // New-inst removed or moved.
  EXPECT_NE(new_inst->GetBlock(), entry);
}

// // ENTRY
// // To be moved
// obj = new Obj();
// switch(args) {
//   case a:
//     return obj.a;
//   case b:
//     obj.a = 5; break;
//   case c:
//     obj.b = 4; break;
// }
// escape(obj);
// return obj.a;
// EXIT
TEST_F(LoadStoreEliminationTest, MovePredicatedAlloc3) {
  ScopedObjectAccess soa(Thread::Current());
  VariableSizedHandleScope vshs(soa.Self());
  CreateGraph(&vshs);
  AdjacencyListGraph blks(SetupFromAdjacencyList("entry",
                                                 "exit",
                                                 {{"entry", "early_return"},
                                                  {"entry", "set_one"},
                                                  {"entry", "set_two"},
                                                  {"early_return", "exit"},
                                                  {"set_one", "escape"},
                                                  {"set_two", "escape"},
                                                  {"escape", "exit"}}));
#define GET_BLOCK(name) HBasicBlock* name = blks.Get(#name)
  GET_BLOCK(entry);
  GET_BLOCK(exit);
  GET_BLOCK(escape);
  GET_BLOCK(early_return);
  GET_BLOCK(set_one);
  GET_BLOCK(set_two);
#undef GET_BLOCK
  EnsurePredecessorOrder(escape, {set_one, set_two});
  HInstruction* int_val = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kInt32);
  HInstruction* c0 = graph_->GetIntConstant(0);
  HInstruction* c4 = graph_->GetIntConstant(4);
  HInstruction* c5 = graph_->GetIntConstant(5);
  HInstruction* cls = new (GetAllocator()) HLoadClass(graph_->GetCurrentMethod(),
                                                      dex::TypeIndex(10),
                                                      graph_->GetDexFile(),
                                                      ScopedNullHandle<mirror::Class>(),
                                                      false,
                                                      0,
                                                      false);
  HInstruction* new_inst =
      new (GetAllocator()) HNewInstance(cls,
                                        0,
                                        dex::TypeIndex(10),
                                        graph_->GetDexFile(),
                                        false,
                                        QuickEntrypointEnum::kQuickAllocObjectInitialized);
  HInstruction* switch_inst = new (GetAllocator()) HPackedSwitch(0, 2, int_val);
  entry->AddInstruction(int_val);
  entry->AddInstruction(cls);
  entry->AddInstruction(new_inst);
  entry->AddInstruction(switch_inst);
  ArenaVector<HInstruction*> current_locals({}, GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(cls, &current_locals);
  new_inst->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* store_one = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                   c4,
                                                                   nullptr,
                                                                   DataType::Type::kInt32,
                                                                   MemberOffset(10),
                                                                   false,
                                                                   0,
                                                                   0,
                                                                   graph_->GetDexFile(),
                                                                   0);
  HInstruction* goto_one = new (GetAllocator()) HGoto();
  set_one->AddInstruction(store_one);
  set_one->AddInstruction(goto_one);

  HInstruction* store_two = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                   c5,
                                                                   nullptr,
                                                                   DataType::Type::kInt32,
                                                                   MemberOffset(10),
                                                                   false,
                                                                   0,
                                                                   0,
                                                                   graph_->GetDexFile(),
                                                                   0);
  HInstruction* goto_two = new (GetAllocator()) HGoto();
  set_two->AddInstruction(store_two);
  set_two->AddInstruction(goto_two);

  HInstruction* read_early = new (GetAllocator()) HInstanceFieldGet(new_inst,
                                                                    nullptr,
                                                                    DataType::Type::kInt32,
                                                                    MemberOffset(10),
                                                                    false,
                                                                    0,
                                                                    0,
                                                                    graph_->GetDexFile(),
                                                                    0);
  HInstruction* return_early = new (GetAllocator()) HReturn(read_early);
  early_return->AddInstruction(read_early);
  early_return->AddInstruction(return_early);

  HInstruction* escape_instruction = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            1,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* read_escape = new (GetAllocator()) HInstanceFieldGet(new_inst,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* return_escape = new (GetAllocator()) HReturn(read_escape);
  escape_instruction->AsInvoke()->SetRawInputAt(0, new_inst);
  escape->AddInstruction(escape_instruction);
  escape->AddInstruction(read_escape);
  escape->AddInstruction(return_escape);
  escape_instruction->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* exit_instruction = new (GetAllocator()) HExit();
  exit->AddInstruction(exit_instruction);
  // PerformLSE expects this to be empty.
  graph_->ClearDominanceInformation();
  LOG(INFO) << "Pre LSE " << blks;
  PerformLSEWithPartial();
  LOG(INFO) << "Post LSE " << blks;

  // Each escaping switch path gets its own materialization block.
  // Blocks:
  //   early_return(5) -> [exit(4)]
  //   entry(3) -> [early_return(5), <Unnamed>(9), <Unnamed>(10)]
  //   escape(8) -> [exit(4)]
  //   exit(4) -> []
  //   set_one(6) -> [escape(8)]
  //   set_two(7) -> [escape(8)]
  //   <Unnamed>(10) -> [set_two(7)]
  //   <Unnamed>(9) -> [set_one(6)]
  HBasicBlock* materialize_one = set_one->GetSinglePredecessor();
  HBasicBlock* materialize_two = set_two->GetSinglePredecessor();
  HNewInstance* materialization_ins_one =
      FindSingleInstruction<HNewInstance>(graph_, materialize_one);
  HNewInstance* materialization_ins_two =
      FindSingleInstruction<HNewInstance>(graph_, materialize_two);
  HPhi* new_phi = FindSingleInstruction<HPhi>(graph_, escape);
  EXPECT_NE(materialization_ins_one, nullptr);
  EXPECT_NE(materialization_ins_two, nullptr);
  EXPECT_EQ(materialization_ins_one, new_phi->InputAt(0))
      << *materialization_ins_one << " vs " << *new_phi;
  EXPECT_EQ(materialization_ins_two, new_phi->InputAt(1))
      << *materialization_ins_two << " vs " << *new_phi;

  EXPECT_FALSE(IsRemoved(escape_instruction));
  EXPECT_FALSE(IsRemoved(read_escape));
  EXPECT_EQ(read_escape->InputAt(0), new_phi) << *new_phi << " vs " << *read_escape->InputAt(0);
  EXPECT_EQ(store_one->InputAt(0), materialization_ins_one);
  EXPECT_EQ(store_two->InputAt(0), materialization_ins_two);
  EXPECT_EQ(escape_instruction->InputAt(0), new_phi);
  EXPECT_TRUE(IsRemoved(read_early));
  EXPECT_EQ(return_early->InputAt(0), c0);
}

// // ENTRY
// // To be moved
// obj = new Obj();
// switch(args) {
//   case a:
//     // set_one
//     obj.a = 5;
//     escape(obj);
//   case c:
//     // set_two
//     obj.b = 4; break;
//   default:
//     return obj.a;
// }
// escape(obj);
// return obj.a;
// EXIT
TEST_F(LoadStoreEliminationTest, MovePredicatedAlloc4) {
  ScopedObjectAccess soa(Thread::Current());
  VariableSizedHandleScope vshs(soa.Self());
  CreateGraph(&vshs);
  // Break the critical edge between entry and set_two with the
  // set_two_critical_break node. Graph simplification would do this for us if
  // we didn't do it manually. This way we have a nice-name for debugging and
  // testing.
  AdjacencyListGraph blks(SetupFromAdjacencyList("entry",
                                                 "exit",
                                                 {{"entry", "early_return"},
                                                  {"entry", "set_one"},
                                                  {"entry", "set_two_critical_break"},
                                                  {"set_two_critical_break", "set_two"},
                                                  {"early_return", "exit"},
                                                  {"set_one", "set_two"},
                                                  {"set_two", "escape"},
                                                  {"escape", "exit"}}));
#define GET_BLOCK(name) HBasicBlock* name = blks.Get(#name)
  GET_BLOCK(entry);
  GET_BLOCK(exit);
  GET_BLOCK(escape);
  GET_BLOCK(early_return);
  GET_BLOCK(set_one);
  GET_BLOCK(set_two);
  GET_BLOCK(set_two_critical_break);
#undef GET_BLOCK
  EnsurePredecessorOrder(set_two, {set_one, set_two_critical_break});
  HInstruction* int_val = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kInt32);
  HInstruction* c0 = graph_->GetIntConstant(0);
  HInstruction* c4 = graph_->GetIntConstant(4);
  HInstruction* c5 = graph_->GetIntConstant(5);
  HInstruction* cls = new (GetAllocator()) HLoadClass(graph_->GetCurrentMethod(),
                                                      dex::TypeIndex(10),
                                                      graph_->GetDexFile(),
                                                      ScopedNullHandle<mirror::Class>(),
                                                      false,
                                                      0,
                                                      false);
  HInstruction* new_inst =
      new (GetAllocator()) HNewInstance(cls,
                                        0,
                                        dex::TypeIndex(10),
                                        graph_->GetDexFile(),
                                        false,
                                        QuickEntrypointEnum::kQuickAllocObjectInitialized);
  HInstruction* switch_inst = new (GetAllocator()) HPackedSwitch(0, 2, int_val);
  entry->AddInstruction(int_val);
  entry->AddInstruction(cls);
  entry->AddInstruction(new_inst);
  entry->AddInstruction(switch_inst);
  ArenaVector<HInstruction*> current_locals({}, GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(cls, &current_locals);
  new_inst->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* store_one = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                   c4,
                                                                   nullptr,
                                                                   DataType::Type::kInt32,
                                                                   MemberOffset(10),
                                                                   false,
                                                                   0,
                                                                   0,
                                                                   graph_->GetDexFile(),
                                                                   0);

  HInstruction* escape_one = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            1,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* goto_one = new (GetAllocator()) HGoto();
  escape_one->AsInvoke()->SetRawInputAt(0, new_inst);
  set_one->AddInstruction(store_one);
  set_one->AddInstruction(escape_one);
  set_one->AddInstruction(goto_one);
  escape_one->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* goto_crit_break = new (GetAllocator()) HGoto();
  set_two_critical_break->AddInstruction(goto_crit_break);

  HInstruction* store_two = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                   c5,
                                                                   nullptr,
                                                                   DataType::Type::kInt32,
                                                                   MemberOffset(10),
                                                                   false,
                                                                   0,
                                                                   0,
                                                                   graph_->GetDexFile(),
                                                                   0);
  HInstruction* goto_two = new (GetAllocator()) HGoto();
  set_two->AddInstruction(store_two);
  set_two->AddInstruction(goto_two);

  HInstruction* read_early = new (GetAllocator()) HInstanceFieldGet(new_inst,
                                                                    nullptr,
                                                                    DataType::Type::kInt32,
                                                                    MemberOffset(10),
                                                                    false,
                                                                    0,
                                                                    0,
                                                                    graph_->GetDexFile(),
                                                                    0);
  HInstruction* return_early = new (GetAllocator()) HReturn(read_early);
  early_return->AddInstruction(read_early);
  early_return->AddInstruction(return_early);

  HInstruction* escape_instruction = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            1,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* read_escape = new (GetAllocator()) HInstanceFieldGet(new_inst,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* return_escape = new (GetAllocator()) HReturn(read_escape);
  escape_instruction->AsInvoke()->SetRawInputAt(0, new_inst);
  escape->AddInstruction(escape_instruction);
  escape->AddInstruction(read_escape);
  escape->AddInstruction(return_escape);
  escape_instruction->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* exit_instruction = new (GetAllocator()) HExit();
  exit->AddInstruction(exit_instruction);
  // PerformLSE expects this to be empty.
  graph_->ClearDominanceInformation();
  LOG(INFO) << "Pre LSE " << blks;
  PerformLSEWithPartial();
  LOG(INFO) << "Post LSE " << blks;

  EXPECT_TRUE(IsRemoved(read_early));
  EXPECT_EQ(return_early->InputAt(0), c0);
  // Each escaping switch path gets its own materialization block.
  // Blocks:
  //   early_return(5) -> [exit(4)]
  //   entry(3) -> [early_return(5), <Unnamed>(10), <Unnamed>(11)]
  //   escape(9) -> [exit(4)]
  //   exit(4) -> []
  //   set_one(6) -> [set_two(8)]
  //   set_two(8) -> [escape(9)]
  //   set_two_critical_break(7) -> [set_two(8)]
  //   <Unnamed>(11) -> [set_two_critical_break(7)]
  //   <Unnamed>(10) -> [set_one(6)]
  HBasicBlock* materialize_one = set_one->GetSinglePredecessor();
  HBasicBlock* materialize_two = set_two_critical_break->GetSinglePredecessor();
  HNewInstance* materialization_ins_one =
      FindSingleInstruction<HNewInstance>(graph_, materialize_one);
  HNewInstance* materialization_ins_two =
      FindSingleInstruction<HNewInstance>(graph_, materialize_two);
  HPhi* new_phi = FindSingleInstruction<HPhi>(graph_, set_two);
  EXPECT_NE(materialization_ins_one, nullptr);
  EXPECT_NE(materialization_ins_two, nullptr);
  EXPECT_NE(new_phi, nullptr);
  EXPECT_EQ(materialization_ins_one, new_phi->InputAt(0))
      << *materialization_ins_one << " vs " << *new_phi;
  EXPECT_EQ(materialization_ins_two, new_phi->InputAt(1))
      << *materialization_ins_two << " vs " << *new_phi;

  EXPECT_EQ(store_one->InputAt(0), materialization_ins_one);
  EXPECT_EQ(store_two->InputAt(0), new_phi) << *store_two << " vs " << *new_phi;
  EXPECT_EQ(escape_instruction->InputAt(0), new_phi);
  EXPECT_FALSE(IsRemoved(escape_one));
  EXPECT_EQ(escape_one->InputAt(0), materialization_ins_one)
      << *escape_one << " vs " << *materialization_ins_one;
  EXPECT_FALSE(IsRemoved(escape_instruction));
  EXPECT_FALSE(IsRemoved(read_escape));
  EXPECT_EQ(read_escape->InputAt(0), new_phi) << *new_phi << " vs " << *read_escape->InputAt(0);
}

// // ENTRY
// obj = new Obj();
// if (parameter_value) {
//   // LEFT
//   // DO NOT ELIMINATE
//   obj.field = 1;
//   escape(obj);
//   return obj.field;
// } else {
//   // RIGHT
//   // ELIMINATE
//   obj.field = 2;
//   return obj.field;
// }
// EXIT
TEST_F(LoadStoreEliminationTest, PartialLoadElimination3) {
  ScopedObjectAccess soa(Thread::Current());
  VariableSizedHandleScope vshs(soa.Self());
  CreateGraph(&vshs);
  AdjacencyListGraph blks(SetupFromAdjacencyList(
      "entry",
      "exit",
      {{"entry", "left"}, {"entry", "right"}, {"left", "exit"}, {"right", "exit"}}));
#define GET_BLOCK(name) HBasicBlock* name = blks.Get(#name)
  GET_BLOCK(entry);
  GET_BLOCK(exit);
  GET_BLOCK(left);
  GET_BLOCK(right);
#undef GET_BLOCK
  HInstruction* bool_value = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kBool);
  HInstruction* c1 = graph_->GetIntConstant(1);
  HInstruction* c2 = graph_->GetIntConstant(2);
  HInstruction* cls = new (GetAllocator()) HLoadClass(graph_->GetCurrentMethod(),
                                                      dex::TypeIndex(10),
                                                      graph_->GetDexFile(),
                                                      ScopedNullHandle<mirror::Class>(),
                                                      false,
                                                      0,
                                                      false);
  HInstruction* new_inst =
      new (GetAllocator()) HNewInstance(cls,
                                        0,
                                        dex::TypeIndex(10),
                                        graph_->GetDexFile(),
                                        false,
                                        QuickEntrypointEnum::kQuickAllocObjectInitialized);
  HInstruction* if_inst = new (GetAllocator()) HIf(bool_value);
  entry->AddInstruction(bool_value);
  entry->AddInstruction(cls);
  entry->AddInstruction(new_inst);
  entry->AddInstruction(if_inst);
  ArenaVector<HInstruction*> current_locals({}, GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(cls, &current_locals);
  new_inst->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* write_left = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                    c1,
                                                                    nullptr,
                                                                    DataType::Type::kInt32,
                                                                    MemberOffset(10),
                                                                    false,
                                                                    0,
                                                                    0,
                                                                    graph_->GetDexFile(),
                                                                    0);
  HInstruction* call_left = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            1,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* read_left = new (GetAllocator()) HInstanceFieldGet(new_inst,
                                                                   nullptr,
                                                                   DataType::Type::kInt32,
                                                                   MemberOffset(10),
                                                                   false,
                                                                   0,
                                                                   0,
                                                                   graph_->GetDexFile(),
                                                                   0);
  HInstruction* return_left = new (GetAllocator()) HReturn(read_left);
  call_left->AsInvoke()->SetRawInputAt(0, new_inst);
  left->AddInstruction(write_left);
  left->AddInstruction(call_left);
  left->AddInstruction(read_left);
  left->AddInstruction(return_left);
  call_left->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* write_right = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                     c2,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* read_right = new (GetAllocator()) HInstanceFieldGet(new_inst,
                                                                    nullptr,
                                                                    DataType::Type::kInt32,
                                                                    MemberOffset(10),
                                                                    false,
                                                                    0,
                                                                    0,
                                                                    graph_->GetDexFile(),
                                                                    0);
  HInstruction* return_right = new (GetAllocator()) HReturn(read_right);
  right->AddInstruction(write_right);
  right->AddInstruction(read_right);
  right->AddInstruction(return_right);

  HInstruction* exit_instruction = new (GetAllocator()) HExit();
  exit->AddInstruction(exit_instruction);
  // PerformLSE expects this to be empty.
  graph_->ClearDominanceInformation();
  PerformLSE();

  EXPECT_TRUE(IsRemoved(read_right));
  EXPECT_TRUE(IsRemoved(write_right));
  EXPECT_FALSE(IsRemoved(write_left));
  EXPECT_FALSE(IsRemoved(call_left));
  EXPECT_FALSE(IsRemoved(read_left));
}

// // ENTRY
// obj = new Obj();
// if (parameter_value) {
//   // LEFT
//   // DO NOT ELIMINATE
//   obj.field = 1;
//   while (true) {
//     bool esc = escape(obj);
//     // DO NOT ELIMINATE
//     obj.field = 3;
//     if (esc) break;
//   }
//   // ELIMINATE.
//   return obj.field;
// } else {
//   // RIGHT
//   // ELIMINATE
//   obj.field = 2;
//   return obj.field;
// }
// EXIT
TEST_F(LoadStoreEliminationTest, PartialLoadElimination4) {
  ScopedObjectAccess soa(Thread::Current());
  VariableSizedHandleScope vshs(soa.Self());
  CreateGraph(&vshs);
  AdjacencyListGraph blks(SetupFromAdjacencyList("entry",
                                                 "exit",
                                                 {{"entry", "entry_post"},
                                                  {"entry_post", "right"},
                                                  {"right", "exit"},
                                                  {"entry_post", "left_pre"},
                                                  {"left_pre", "left_loop"},
                                                  {"left_loop", "left_loop"},
                                                  {"left_loop", "left_finish"},
                                                  {"left_finish", "exit"}}));
#define GET_BLOCK(name) HBasicBlock* name = blks.Get(#name)
  GET_BLOCK(entry);
  GET_BLOCK(entry_post);
  GET_BLOCK(exit);
  GET_BLOCK(left_pre);
  GET_BLOCK(left_loop);
  GET_BLOCK(left_finish);
  GET_BLOCK(right);
#undef GET_BLOCK
  // Left-loops first successor is the break.
  if (left_loop->GetSuccessors()[0] != left_finish) {
    left_loop->SwapSuccessors();
  }
  HInstruction* bool_value = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kBool);
  HInstruction* c1 = graph_->GetIntConstant(1);
  HInstruction* c2 = graph_->GetIntConstant(2);
  HInstruction* c3 = graph_->GetIntConstant(3);
  HInstruction* cls = new (GetAllocator()) HLoadClass(graph_->GetCurrentMethod(),
                                                      dex::TypeIndex(10),
                                                      graph_->GetDexFile(),
                                                      ScopedNullHandle<mirror::Class>(),
                                                      false,
                                                      0,
                                                      false);
  HInstruction* new_inst =
      new (GetAllocator()) HNewInstance(cls,
                                        0,
                                        dex::TypeIndex(10),
                                        graph_->GetDexFile(),
                                        false,
                                        QuickEntrypointEnum::kQuickAllocObjectInitialized);
  HInstruction* goto_entry = new (GetAllocator()) HGoto();
  entry->AddInstruction(bool_value);
  entry->AddInstruction(cls);
  entry->AddInstruction(new_inst);
  entry->AddInstruction(goto_entry);
  ArenaVector<HInstruction*> current_locals({}, GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(cls, &current_locals);
  new_inst->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* if_inst = new (GetAllocator()) HIf(bool_value);
  entry_post->AddInstruction(if_inst);

  HInstruction* write_left_pre = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                        c1,
                                                                        nullptr,
                                                                        DataType::Type::kInt32,
                                                                        MemberOffset(10),
                                                                        false,
                                                                        0,
                                                                        0,
                                                                        graph_->GetDexFile(),
                                                                        0);
  HInstruction* goto_left_pre = new (GetAllocator()) HGoto();
  left_pre->AddInstruction(write_left_pre);
  left_pre->AddInstruction(goto_left_pre);

  HInstruction* suspend_left_loop = new (GetAllocator()) HSuspendCheck();
  HInstruction* call_left_loop = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            1,
                            DataType::Type::kBool,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* write_left_loop = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                         c3,
                                                                         nullptr,
                                                                         DataType::Type::kInt32,
                                                                         MemberOffset(10),
                                                                         false,
                                                                         0,
                                                                         0,
                                                                         graph_->GetDexFile(),
                                                                         0);
  HInstruction* if_left_loop = new (GetAllocator()) HIf(call_left_loop);
  call_left_loop->AsInvoke()->SetRawInputAt(0, new_inst);
  left_loop->AddInstruction(suspend_left_loop);
  left_loop->AddInstruction(call_left_loop);
  left_loop->AddInstruction(write_left_loop);
  left_loop->AddInstruction(if_left_loop);
  suspend_left_loop->CopyEnvironmentFrom(cls->GetEnvironment());
  call_left_loop->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* read_left_end = new (GetAllocator()) HInstanceFieldGet(new_inst,
                                                                       nullptr,
                                                                       DataType::Type::kInt32,
                                                                       MemberOffset(10),
                                                                       false,
                                                                       0,
                                                                       0,
                                                                       graph_->GetDexFile(),
                                                                       0);
  HInstruction* return_left_end = new (GetAllocator()) HReturn(read_left_end);
  left_finish->AddInstruction(read_left_end);
  left_finish->AddInstruction(return_left_end);

  HInstruction* write_right = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                     c2,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* read_right = new (GetAllocator()) HInstanceFieldGet(new_inst,
                                                                    nullptr,
                                                                    DataType::Type::kInt32,
                                                                    MemberOffset(10),
                                                                    false,
                                                                    0,
                                                                    0,
                                                                    graph_->GetDexFile(),
                                                                    0);
  HInstruction* return_right = new (GetAllocator()) HReturn(read_right);
  right->AddInstruction(write_right);
  right->AddInstruction(read_right);
  right->AddInstruction(return_right);

  HInstruction* exit_instruction = new (GetAllocator()) HExit();
  exit->AddInstruction(exit_instruction);
  // PerformLSE expects this to be empty.
  graph_->ClearDominanceInformation();
  PerformLSE();

  EXPECT_FALSE(IsRemoved(write_left_pre));
  EXPECT_TRUE(IsRemoved(read_right));
  EXPECT_TRUE(IsRemoved(write_right));
  EXPECT_FALSE(IsRemoved(write_left_loop));
  EXPECT_FALSE(IsRemoved(call_left_loop));
  EXPECT_TRUE(IsRemoved(read_left_end));
}

// // ENTRY
// obj = new Obj();
// if (parameter_value) {
//   // LEFT
//   // DO NOT ELIMINATE
//   escape(obj);
//   obj.field = 1;
// } else {
//   // RIGHT
//   // obj hasn't escaped so it's invisible.
//   // ELIMINATE
//   obj.field = 2;
//   noescape();
// }
// EXIT
// ELIMINATE
// return obj.field
TEST_F(LoadStoreEliminationTest, PartialLoadElimination5) {
  ScopedObjectAccess soa(Thread::Current());
  VariableSizedHandleScope vshs(soa.Self());
  CreateGraph(&vshs);
  AdjacencyListGraph blks(SetupFromAdjacencyList("entry",
                                                 "exit",
                                                 {{"entry", "left"},
                                                  {"entry", "right"},
                                                  {"left", "breturn"},
                                                  {"right", "breturn"},
                                                  {"breturn", "exit"}}));
#define GET_BLOCK(name) HBasicBlock* name = blks.Get(#name)
  GET_BLOCK(entry);
  GET_BLOCK(exit);
  GET_BLOCK(breturn);
  GET_BLOCK(left);
  GET_BLOCK(right);
#undef GET_BLOCK
  HInstruction* bool_value = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kBool);
  HInstruction* c1 = graph_->GetIntConstant(1);
  HInstruction* c2 = graph_->GetIntConstant(2);
  HInstruction* cls = new (GetAllocator()) HLoadClass(graph_->GetCurrentMethod(),
                                                      dex::TypeIndex(10),
                                                      graph_->GetDexFile(),
                                                      ScopedNullHandle<mirror::Class>(),
                                                      false,
                                                      0,
                                                      false);
  HInstruction* new_inst =
      new (GetAllocator()) HNewInstance(cls,
                                        0,
                                        dex::TypeIndex(10),
                                        graph_->GetDexFile(),
                                        false,
                                        QuickEntrypointEnum::kQuickAllocObjectInitialized);
  HInstruction* if_inst = new (GetAllocator()) HIf(bool_value);
  entry->AddInstruction(bool_value);
  entry->AddInstruction(cls);
  entry->AddInstruction(new_inst);
  entry->AddInstruction(if_inst);
  ArenaVector<HInstruction*> current_locals({}, GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(cls, &current_locals);
  new_inst->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* call_left = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            1,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* write_left = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                    c1,
                                                                    nullptr,
                                                                    DataType::Type::kInt32,
                                                                    MemberOffset(10),
                                                                    false,
                                                                    0,
                                                                    0,
                                                                    graph_->GetDexFile(),
                                                                    0);
  HInstruction* goto_left = new (GetAllocator()) HGoto();
  call_left->AsInvoke()->SetRawInputAt(0, new_inst);
  left->AddInstruction(call_left);
  left->AddInstruction(write_left);
  left->AddInstruction(goto_left);
  call_left->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* write_right = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                     c2,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* call_right = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            0,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* goto_right = new (GetAllocator()) HGoto();
  right->AddInstruction(write_right);
  right->AddInstruction(call_right);
  right->AddInstruction(goto_right);
  call_right->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* read_bottom = new (GetAllocator()) HInstanceFieldGet(new_inst,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* return_exit = new (GetAllocator()) HReturn(read_bottom);
  breturn->AddInstruction(read_bottom);
  breturn->AddInstruction(return_exit);

  HInstruction* exit_instruction = new (GetAllocator()) HExit();
  exit->AddInstruction(exit_instruction);
  // PerformLSE expects this to be empty.
  graph_->ClearDominanceInformation();
  PerformLSE();

  EXPECT_TRUE(IsRemoved(read_bottom));
  EXPECT_TRUE(IsRemoved(write_right));
  EXPECT_FALSE(IsRemoved(write_left));
  EXPECT_FALSE(IsRemoved(call_left));
  EXPECT_FALSE(IsRemoved(call_right));
}

// // ENTRY
// obj = new Obj();
// // Eliminate this one. Object hasn't escaped yet so it's safe.
// obj.field = 3;
// noescape();
// if (parameter_value) {
//   // LEFT
//   // DO NOT ELIMINATE
//   obj.field = 5;
//   escape(obj);
//   obj.field = 1;
// } else {
//   // RIGHT
//   // ELIMINATE
//   obj.field = 2;
// }
// EXIT
// ELIMINATE
// return obj.field
TEST_F(LoadStoreEliminationTest, PartialLoadElimination6) {
  ScopedObjectAccess soa(Thread::Current());
  VariableSizedHandleScope vshs(soa.Self());
  CreateGraph(&vshs);
  AdjacencyListGraph blks(SetupFromAdjacencyList("entry",
                                                 "exit",
                                                 {{"entry", "left"},
                                                  {"entry", "right"},
                                                  {"left", "breturn"},
                                                  {"right", "breturn"},
                                                  {"breturn", "exit"}}));
#define GET_BLOCK(name) HBasicBlock* name = blks.Get(#name)
  GET_BLOCK(entry);
  GET_BLOCK(exit);
  GET_BLOCK(breturn);
  GET_BLOCK(left);
  GET_BLOCK(right);
#undef GET_BLOCK
  HInstruction* bool_value = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kBool);
  HInstruction* c1 = graph_->GetIntConstant(1);
  HInstruction* c2 = graph_->GetIntConstant(2);
  HInstruction* c3 = graph_->GetIntConstant(3);
  HInstruction* c5 = graph_->GetIntConstant(5);
  HInstruction* cls = new (GetAllocator()) HLoadClass(graph_->GetCurrentMethod(),
                                                      dex::TypeIndex(10),
                                                      graph_->GetDexFile(),
                                                      ScopedNullHandle<mirror::Class>(),
                                                      false,
                                                      0,
                                                      false);
  HInstruction* new_inst =
      new (GetAllocator()) HNewInstance(cls,
                                        0,
                                        dex::TypeIndex(10),
                                        graph_->GetDexFile(),
                                        false,
                                        QuickEntrypointEnum::kQuickAllocObjectInitialized);
  HInstruction* write_entry = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                     c3,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* call_entry = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            0,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* if_inst = new (GetAllocator()) HIf(bool_value);
  entry->AddInstruction(bool_value);
  entry->AddInstruction(cls);
  entry->AddInstruction(new_inst);
  entry->AddInstruction(write_entry);
  entry->AddInstruction(call_entry);
  entry->AddInstruction(if_inst);
  ArenaVector<HInstruction*> current_locals({}, GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(cls, &current_locals);
  new_inst->CopyEnvironmentFrom(cls->GetEnvironment());
  call_entry->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* write_left_start = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                          c5,
                                                                          nullptr,
                                                                          DataType::Type::kInt32,
                                                                          MemberOffset(10),
                                                                          false,
                                                                          0,
                                                                          0,
                                                                          graph_->GetDexFile(),
                                                                          0);
  HInstruction* call_left = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            1,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* write_left = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                    c1,
                                                                    nullptr,
                                                                    DataType::Type::kInt32,
                                                                    MemberOffset(10),
                                                                    false,
                                                                    0,
                                                                    0,
                                                                    graph_->GetDexFile(),
                                                                    0);
  HInstruction* goto_left = new (GetAllocator()) HGoto();
  call_left->AsInvoke()->SetRawInputAt(0, new_inst);
  left->AddInstruction(write_left_start);
  left->AddInstruction(call_left);
  left->AddInstruction(write_left);
  left->AddInstruction(goto_left);
  call_left->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* write_right = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                     c2,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* goto_right = new (GetAllocator()) HGoto();
  right->AddInstruction(write_right);
  right->AddInstruction(goto_right);

  HInstruction* read_bottom = new (GetAllocator()) HInstanceFieldGet(new_inst,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* return_exit = new (GetAllocator()) HReturn(read_bottom);
  breturn->AddInstruction(read_bottom);
  breturn->AddInstruction(return_exit);

  HInstruction* exit_instruction = new (GetAllocator()) HExit();
  exit->AddInstruction(exit_instruction);
  // PerformLSE expects this to be empty.
  graph_->ClearDominanceInformation();
  PerformLSE();

  EXPECT_TRUE(IsRemoved(read_bottom));
  EXPECT_TRUE(IsRemoved(write_right));
  EXPECT_TRUE(IsRemoved(write_entry));
  EXPECT_FALSE(IsRemoved(write_left_start));
  EXPECT_FALSE(IsRemoved(write_left));
  EXPECT_FALSE(IsRemoved(call_left));
  EXPECT_FALSE(IsRemoved(call_entry));
}

// // ENTRY
// obj = new Obj();
// if (parameter_value) {
//   // LEFT
//   // DO NOT ELIMINATE
//   obj.field = 1;
//   while (true) {
//     bool esc = escape(obj);
//     if (esc) break;
//     // DO NOT ELIMINATE
//     obj.field = 3;
//   }
// } else {
//   // RIGHT
//   // DO NOT ELIMINATE
//   obj.field = 2;
// }
// // DO NOT ELIMINATE
// return obj.field;
// EXIT
TEST_F(LoadStoreEliminationTest, PartialLoadPreserved3) {
  ScopedObjectAccess soa(Thread::Current());
  VariableSizedHandleScope vshs(soa.Self());
  CreateGraph(&vshs);
  AdjacencyListGraph blks(SetupFromAdjacencyList("entry",
                                                 "exit",
                                                 {{"entry", "entry_post"},
                                                  {"entry_post", "right"},
                                                  {"right", "return_block"},
                                                  {"entry_post", "left_pre"},
                                                  {"left_pre", "left_loop"},
                                                  {"left_loop", "left_loop_post"},
                                                  {"left_loop_post", "left_loop"},
                                                  {"left_loop", "return_block"},
                                                  {"return_block", "exit"}}));
#define GET_BLOCK(name) HBasicBlock* name = blks.Get(#name)
  GET_BLOCK(entry);
  GET_BLOCK(entry_post);
  GET_BLOCK(exit);
  GET_BLOCK(return_block);
  GET_BLOCK(left_pre);
  GET_BLOCK(left_loop);
  GET_BLOCK(left_loop_post);
  GET_BLOCK(right);
#undef GET_BLOCK
  // Left-loops first successor is the break.
  if (left_loop->GetSuccessors()[0] != return_block) {
    left_loop->SwapSuccessors();
  }
  HInstruction* bool_value = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kBool);
  HInstruction* c1 = graph_->GetIntConstant(1);
  HInstruction* c2 = graph_->GetIntConstant(2);
  HInstruction* c3 = graph_->GetIntConstant(3);
  HInstruction* cls = new (GetAllocator()) HLoadClass(graph_->GetCurrentMethod(),
                                                      dex::TypeIndex(10),
                                                      graph_->GetDexFile(),
                                                      ScopedNullHandle<mirror::Class>(),
                                                      false,
                                                      0,
                                                      false);
  HInstruction* new_inst =
      new (GetAllocator()) HNewInstance(cls,
                                        0,
                                        dex::TypeIndex(10),
                                        graph_->GetDexFile(),
                                        false,
                                        QuickEntrypointEnum::kQuickAllocObjectInitialized);
  HInstruction* goto_entry = new (GetAllocator()) HGoto();
  entry->AddInstruction(bool_value);
  entry->AddInstruction(cls);
  entry->AddInstruction(new_inst);
  entry->AddInstruction(goto_entry);
  ArenaVector<HInstruction*> current_locals({}, GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(cls, &current_locals);
  new_inst->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* if_inst = new (GetAllocator()) HIf(bool_value);
  entry_post->AddInstruction(if_inst);

  HInstruction* write_left_pre = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                        c1,
                                                                        nullptr,
                                                                        DataType::Type::kInt32,
                                                                        MemberOffset(10),
                                                                        false,
                                                                        0,
                                                                        0,
                                                                        graph_->GetDexFile(),
                                                                        0);
  HInstruction* goto_left_pre = new (GetAllocator()) HGoto();
  left_pre->AddInstruction(write_left_pre);
  left_pre->AddInstruction(goto_left_pre);

  HInstruction* suspend_left_loop = new (GetAllocator()) HSuspendCheck();
  HInstruction* call_left_loop = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            1,
                            DataType::Type::kBool,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* if_left_loop = new (GetAllocator()) HIf(call_left_loop);
  call_left_loop->AsInvoke()->SetRawInputAt(0, new_inst);
  left_loop->AddInstruction(suspend_left_loop);
  left_loop->AddInstruction(call_left_loop);
  left_loop->AddInstruction(if_left_loop);
  suspend_left_loop->CopyEnvironmentFrom(cls->GetEnvironment());
  call_left_loop->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* write_left_loop = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                         c3,
                                                                         nullptr,
                                                                         DataType::Type::kInt32,
                                                                         MemberOffset(10),
                                                                         false,
                                                                         0,
                                                                         0,
                                                                         graph_->GetDexFile(),
                                                                         0);
  HInstruction* goto_left_loop = new (GetAllocator()) HGoto();
  left_loop_post->AddInstruction(write_left_loop);
  left_loop_post->AddInstruction(goto_left_loop);

  HInstruction* write_right = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                     c2,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* goto_right = new (GetAllocator()) HGoto();
  right->AddInstruction(write_right);
  right->AddInstruction(goto_right);

  HInstruction* read_return = new (GetAllocator()) HInstanceFieldGet(new_inst,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* return_final = new (GetAllocator()) HReturn(read_return);
  return_block->AddInstruction(read_return);
  return_block->AddInstruction(return_final);

  HInstruction* exit_instruction = new (GetAllocator()) HExit();
  exit->AddInstruction(exit_instruction);
  // PerformLSE expects this to be empty.
  graph_->ClearDominanceInformation();
  PerformLSENoPartial();

  EXPECT_FALSE(IsRemoved(write_left_pre)) << *write_left_pre;
  EXPECT_FALSE(IsRemoved(read_return)) << *read_return;
  EXPECT_FALSE(IsRemoved(write_right)) << *write_right;
  EXPECT_FALSE(IsRemoved(write_left_loop)) << *write_left_loop;
  EXPECT_FALSE(IsRemoved(call_left_loop)) << *call_left_loop;
}

// // ENTRY
// obj = new Obj();
// if (parameter_value) {
//   // LEFT
//   // ELIMINATE (not visible since always overridden by obj.field = 3)
//   obj.field = 1;
//   while (true) {
//     bool stop = should_stop();
//     // DO NOT ELIMINATE (visible by read at end)
//     obj.field = 3;
//     if (stop) break;
//   }
// } else {
//   // RIGHT
//   // DO NOT ELIMINATE
//   obj.field = 2;
//   escape(obj);
// }
// // DO NOT ELIMINATE
// return obj.field;
// EXIT
TEST_F(LoadStoreEliminationTest, PartialLoadPreserved4) {
  ScopedObjectAccess soa(Thread::Current());
  VariableSizedHandleScope vshs(soa.Self());
  CreateGraph(&vshs);
  AdjacencyListGraph blks(SetupFromAdjacencyList("entry",
                                                 "exit",
                                                 {{"entry", "entry_post"},
                                                  {"entry_post", "right"},
                                                  {"right", "return_block"},
                                                  {"entry_post", "left_pre"},
                                                  {"left_pre", "left_loop"},
                                                  {"left_loop", "left_loop"},
                                                  {"left_loop", "return_block"},
                                                  {"return_block", "exit"}}));
#define GET_BLOCK(name) HBasicBlock* name = blks.Get(#name)
  GET_BLOCK(entry);
  GET_BLOCK(entry_post);
  GET_BLOCK(exit);
  GET_BLOCK(return_block);
  GET_BLOCK(left_pre);
  GET_BLOCK(left_loop);
  GET_BLOCK(right);
#undef GET_BLOCK
  // Left-loops first successor is the break.
  if (left_loop->GetSuccessors()[0] != return_block) {
    left_loop->SwapSuccessors();
  }
  HInstruction* bool_value = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kBool);
  HInstruction* c1 = graph_->GetIntConstant(1);
  HInstruction* c2 = graph_->GetIntConstant(2);
  HInstruction* c3 = graph_->GetIntConstant(3);
  HInstruction* cls = new (GetAllocator()) HLoadClass(graph_->GetCurrentMethod(),
                                                      dex::TypeIndex(10),
                                                      graph_->GetDexFile(),
                                                      ScopedNullHandle<mirror::Class>(),
                                                      false,
                                                      0,
                                                      false);
  HInstruction* new_inst =
      new (GetAllocator()) HNewInstance(cls,
                                        0,
                                        dex::TypeIndex(10),
                                        graph_->GetDexFile(),
                                        false,
                                        QuickEntrypointEnum::kQuickAllocObjectInitialized);
  HInstruction* goto_entry = new (GetAllocator()) HGoto();
  entry->AddInstruction(bool_value);
  entry->AddInstruction(cls);
  entry->AddInstruction(new_inst);
  entry->AddInstruction(goto_entry);
  ArenaVector<HInstruction*> current_locals({}, GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(cls, &current_locals);
  new_inst->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* if_inst = new (GetAllocator()) HIf(bool_value);
  entry_post->AddInstruction(if_inst);

  HInstruction* write_left_pre = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                        c1,
                                                                        nullptr,
                                                                        DataType::Type::kInt32,
                                                                        MemberOffset(10),
                                                                        false,
                                                                        0,
                                                                        0,
                                                                        graph_->GetDexFile(),
                                                                        0);
  HInstruction* goto_left_pre = new (GetAllocator()) HGoto();
  left_pre->AddInstruction(write_left_pre);
  left_pre->AddInstruction(goto_left_pre);

  HInstruction* suspend_left_loop = new (GetAllocator()) HSuspendCheck();
  HInstruction* call_left_loop = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            0,
                            DataType::Type::kBool,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* write_left_loop = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                         c3,
                                                                         nullptr,
                                                                         DataType::Type::kInt32,
                                                                         MemberOffset(10),
                                                                         false,
                                                                         0,
                                                                         0,
                                                                         graph_->GetDexFile(),
                                                                         0);
  HInstruction* if_left_loop = new (GetAllocator()) HIf(call_left_loop);
  left_loop->AddInstruction(suspend_left_loop);
  left_loop->AddInstruction(call_left_loop);
  left_loop->AddInstruction(write_left_loop);
  left_loop->AddInstruction(if_left_loop);
  suspend_left_loop->CopyEnvironmentFrom(cls->GetEnvironment());
  call_left_loop->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* write_right = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                     c2,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* call_right = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            1,
                            DataType::Type::kBool,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* goto_right = new (GetAllocator()) HGoto();
  call_right->AsInvoke()->SetRawInputAt(0, new_inst);
  right->AddInstruction(write_right);
  right->AddInstruction(call_right);
  right->AddInstruction(goto_right);
  call_right->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* read_return = new (GetAllocator()) HInstanceFieldGet(new_inst,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* return_final = new (GetAllocator()) HReturn(read_return);
  return_block->AddInstruction(read_return);
  return_block->AddInstruction(return_final);

  HInstruction* exit_instruction = new (GetAllocator()) HExit();
  exit->AddInstruction(exit_instruction);
  // PerformLSE expects this to be empty.
  graph_->ClearDominanceInformation();
  PerformLSENoPartial();

  EXPECT_FALSE(IsRemoved(read_return));
  EXPECT_FALSE(IsRemoved(write_right));
  EXPECT_FALSE(IsRemoved(write_left_loop));
  EXPECT_FALSE(IsRemoved(call_left_loop));
  EXPECT_TRUE(IsRemoved(write_left_pre));
  EXPECT_FALSE(IsRemoved(call_right));
}

// // ENTRY
// obj = new Obj();
// if (parameter_value) {
//   // LEFT
//   // DO NOT ELIMINATE
//   escape(obj);
//   obj.field = 1;
//   // obj has already escaped so can't use field = 1 for value
//   noescape();
// } else {
//   // RIGHT
//   // obj is needed for read since we don't know what the left value is
//   // DO NOT ELIMINATE
//   obj.field = 2;
//   noescape();
// }
// EXIT
// ELIMINATE
// return obj.field
TEST_F(LoadStoreEliminationTest, PartialLoadPreserved5) {
  ScopedObjectAccess soa(Thread::Current());
  VariableSizedHandleScope vshs(soa.Self());
  CreateGraph(&vshs);
  AdjacencyListGraph blks(SetupFromAdjacencyList("entry",
                                                 "exit",
                                                 {{"entry", "left"},
                                                  {"entry", "right"},
                                                  {"left", "breturn"},
                                                  {"right", "breturn"},
                                                  {"breturn", "exit"}}));
#define GET_BLOCK(name) HBasicBlock* name = blks.Get(#name)
  GET_BLOCK(entry);
  GET_BLOCK(exit);
  GET_BLOCK(breturn);
  GET_BLOCK(left);
  GET_BLOCK(right);
#undef GET_BLOCK
  HInstruction* bool_value = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kBool);
  HInstruction* c1 = graph_->GetIntConstant(1);
  HInstruction* c2 = graph_->GetIntConstant(2);
  HInstruction* cls = new (GetAllocator()) HLoadClass(graph_->GetCurrentMethod(),
                                                      dex::TypeIndex(10),
                                                      graph_->GetDexFile(),
                                                      ScopedNullHandle<mirror::Class>(),
                                                      false,
                                                      0,
                                                      false);
  HInstruction* new_inst =
      new (GetAllocator()) HNewInstance(cls,
                                        0,
                                        dex::TypeIndex(10),
                                        graph_->GetDexFile(),
                                        false,
                                        QuickEntrypointEnum::kQuickAllocObjectInitialized);
  HInstruction* if_inst = new (GetAllocator()) HIf(bool_value);
  entry->AddInstruction(bool_value);
  entry->AddInstruction(cls);
  entry->AddInstruction(new_inst);
  entry->AddInstruction(if_inst);
  ArenaVector<HInstruction*> current_locals({}, GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(cls, &current_locals);
  new_inst->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* call_left = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            1,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* write_left = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                    c1,
                                                                    nullptr,
                                                                    DataType::Type::kInt32,
                                                                    MemberOffset(10),
                                                                    false,
                                                                    0,
                                                                    0,
                                                                    graph_->GetDexFile(),
                                                                    0);
  HInstruction* call2_left = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            0,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* goto_left = new (GetAllocator()) HGoto();
  call_left->AsInvoke()->SetRawInputAt(0, new_inst);
  left->AddInstruction(call_left);
  left->AddInstruction(write_left);
  left->AddInstruction(call2_left);
  left->AddInstruction(goto_left);
  call_left->CopyEnvironmentFrom(cls->GetEnvironment());
  call2_left->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* write_right = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                     c2,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* call_right = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            0,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* goto_right = new (GetAllocator()) HGoto();
  right->AddInstruction(write_right);
  right->AddInstruction(call_right);
  right->AddInstruction(goto_right);
  call_right->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* read_bottom = new (GetAllocator()) HInstanceFieldGet(new_inst,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* return_exit = new (GetAllocator()) HReturn(read_bottom);
  breturn->AddInstruction(read_bottom);
  breturn->AddInstruction(return_exit);

  HInstruction* exit_instruction = new (GetAllocator()) HExit();
  exit->AddInstruction(exit_instruction);
  // PerformLSE expects this to be empty.
  graph_->ClearDominanceInformation();
  PerformLSENoPartial();

  EXPECT_FALSE(IsRemoved(read_bottom));
  EXPECT_FALSE(IsRemoved(write_right));
  EXPECT_FALSE(IsRemoved(write_left));
  EXPECT_FALSE(IsRemoved(call_left));
  EXPECT_FALSE(IsRemoved(call_right));
}

// // ENTRY
// obj = new Obj();
// DO NOT ELIMINATE. Kept by escape.
// obj.field = 3;
// noescape();
// if (parameter_value) {
//   // LEFT
//   // DO NOT ELIMINATE
//   escape(obj);
//   obj.field = 1;
// } else {
//   // RIGHT
//   // ELIMINATE
//   obj.field = 2;
// }
// EXIT
// ELIMINATE
// return obj.field
TEST_F(LoadStoreEliminationTest, PartialLoadPreserved6) {
  CreateGraph();
  AdjacencyListGraph blks(SetupFromAdjacencyList("entry",
                                                 "exit",
                                                 {{"entry", "left"},
                                                  {"entry", "right"},
                                                  {"left", "breturn"},
                                                  {"right", "breturn"},
                                                  {"breturn", "exit"}}));
#define GET_BLOCK(name) HBasicBlock* name = blks.Get(#name)
  GET_BLOCK(entry);
  GET_BLOCK(exit);
  GET_BLOCK(breturn);
  GET_BLOCK(left);
  GET_BLOCK(right);
#undef GET_BLOCK
  HInstruction* bool_value = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kBool);
  HInstruction* c1 = graph_->GetIntConstant(1);
  HInstruction* c2 = graph_->GetIntConstant(2);
  HInstruction* c3 = graph_->GetIntConstant(3);
  HInstruction* cls = new (GetAllocator()) HLoadClass(graph_->GetCurrentMethod(),
                                                      dex::TypeIndex(10),
                                                      graph_->GetDexFile(),
                                                      ScopedNullHandle<mirror::Class>(),
                                                      false,
                                                      0,
                                                      false);
  HInstruction* new_inst =
      new (GetAllocator()) HNewInstance(cls,
                                        0,
                                        dex::TypeIndex(10),
                                        graph_->GetDexFile(),
                                        false,
                                        QuickEntrypointEnum::kQuickAllocObjectInitialized);
  HInstruction* write_entry = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                     c3,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* call_entry = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            0,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* if_inst = new (GetAllocator()) HIf(bool_value);
  entry->AddInstruction(bool_value);
  entry->AddInstruction(cls);
  entry->AddInstruction(new_inst);
  entry->AddInstruction(write_entry);
  entry->AddInstruction(call_entry);
  entry->AddInstruction(if_inst);
  ArenaVector<HInstruction*> current_locals({}, GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(cls, &current_locals);
  new_inst->CopyEnvironmentFrom(cls->GetEnvironment());
  call_entry->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* call_left = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            1,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* write_left = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                    c1,
                                                                    nullptr,
                                                                    DataType::Type::kInt32,
                                                                    MemberOffset(10),
                                                                    false,
                                                                    0,
                                                                    0,
                                                                    graph_->GetDexFile(),
                                                                    0);
  HInstruction* goto_left = new (GetAllocator()) HGoto();
  call_left->AsInvoke()->SetRawInputAt(0, new_inst);
  left->AddInstruction(call_left);
  left->AddInstruction(write_left);
  left->AddInstruction(goto_left);
  call_left->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* write_right = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                     c2,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* goto_right = new (GetAllocator()) HGoto();
  right->AddInstruction(write_right);
  right->AddInstruction(goto_right);

  HInstruction* read_bottom = new (GetAllocator()) HInstanceFieldGet(new_inst,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* return_exit = new (GetAllocator()) HReturn(read_bottom);
  breturn->AddInstruction(read_bottom);
  breturn->AddInstruction(return_exit);

  HInstruction* exit_instruction = new (GetAllocator()) HExit();
  exit->AddInstruction(exit_instruction);
  // PerformLSE expects this to be empty.
  graph_->ClearDominanceInformation();

  LOG(INFO) << "Pre LSE " << blks;
  PerformLSENoPartial();
  LOG(INFO) << "Post LSE " << blks;

  EXPECT_TRUE(IsRemoved(read_bottom));
  EXPECT_TRUE(IsRemoved(write_right));
  EXPECT_FALSE(IsRemoved(write_entry));
  EXPECT_FALSE(IsRemoved(write_left));
  EXPECT_FALSE(IsRemoved(call_left));
  EXPECT_FALSE(IsRemoved(call_entry));
}

// // ENTRY
// // MOVED TO MATERIALIZATION BLOCK
// obj = new Obj();
// ELIMINATE, moved to materialization block. Kept by escape.
// obj.field = 3;
// // Make sure this graph isn't broken
// if (obj ==/!= (STATIC.VALUE|obj|null)) {
//   // partial_BLOCK
//   // REMOVE (either from unreachable or normal PHI creation)
//   obj.field = 4;
// }
// if (parameter_value) {
//   // LEFT
//   // DO NOT ELIMINATE
//   escape(obj);
// } else {
//   // RIGHT
//   // ELIMINATE
//   obj.field = 2;
// }
// EXIT
// PREDICATED GET
// return obj.field
TEST_P(PartialComparisonTestGroup, PartialComparisonBeforeCohort) {
  // PartialComparisonKind kind = GetParam();
  ScopedObjectAccess soa(Thread::Current());
  VariableSizedHandleScope vshs(soa.Self());
  CreateGraph(/*handles=*/&vshs);
  AdjacencyListGraph blks(SetupFromAdjacencyList("entry",
                                                 "exit",
                                                 {{"entry", "critical_break"},
                                                  {"entry", "partial"},
                                                  {"partial", "merge"},
                                                  {"critical_break", "merge"},
                                                  {"merge", "left"},
                                                  {"merge", "right"},
                                                  {"left", "breturn"},
                                                  {"right", "breturn"},
                                                  {"breturn", "exit"}}));
#define GET_BLOCK(name) HBasicBlock* name = blks.Get(#name)
  GET_BLOCK(entry);
  GET_BLOCK(merge);
  GET_BLOCK(partial);
  GET_BLOCK(critical_break);
  GET_BLOCK(exit);
  GET_BLOCK(breturn);
  GET_BLOCK(left);
  GET_BLOCK(right);
#undef GET_BLOCK
  HInstruction* bool_value = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kBool);
  // HInstruction* c1 = graph_->GetIntConstant(1);
  HInstruction* c2 = graph_->GetIntConstant(2);
  HInstruction* c3 = graph_->GetIntConstant(3);
  HInstruction* c4 = graph_->GetIntConstant(4);
  HInstruction* cls = new (GetAllocator()) HLoadClass(graph_->GetCurrentMethod(),
                                                      dex::TypeIndex(10),
                                                      graph_->GetDexFile(),
                                                      ScopedNullHandle<mirror::Class>(),
                                                      false,
                                                      0,
                                                      false);
  HInstruction* new_inst =
      new (GetAllocator()) HNewInstance(cls,
                                        0,
                                        dex::TypeIndex(10),
                                        graph_->GetDexFile(),
                                        false,
                                        QuickEntrypointEnum::kQuickAllocObjectInitialized);
  HInstruction* write_entry = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                     c3,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  ComparisonInstructions cmp_instructions = GetComparisonInstructions(new_inst);
  HInstruction* if_inst = new (GetAllocator()) HIf(cmp_instructions.cmp_);
  entry->AddInstruction(bool_value);
  entry->AddInstruction(cls);
  entry->AddInstruction(new_inst);
  entry->AddInstruction(write_entry);
  cmp_instructions.AddSetup(entry);
  entry->AddInstruction(cmp_instructions.cmp_);
  entry->AddInstruction(if_inst);
  ArenaVector<HInstruction*> current_locals({}, GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(cls, &current_locals);
  cmp_instructions.AddEnvironment(cls->GetEnvironment());
  new_inst->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* write_partial = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                       c4,
                                                                       nullptr,
                                                                       DataType::Type::kInt32,
                                                                       MemberOffset(10),
                                                                       false,
                                                                       0,
                                                                       0,
                                                                       graph_->GetDexFile(),
                                                                       0);
  HInstruction* goto_partial = new (GetAllocator()) HGoto();
  partial->AddInstruction(write_partial);
  partial->AddInstruction(goto_partial);

  HInstruction* goto_crit_break = new (GetAllocator()) HGoto();
  critical_break->AddInstruction(goto_crit_break);

  HInstruction* if_merge = new (GetAllocator()) HIf(bool_value);
  merge->AddInstruction(if_merge);

  HInstruction* call_left = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            1,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* goto_left = new (GetAllocator()) HGoto();
  call_left->AsInvoke()->SetRawInputAt(0, new_inst);
  left->AddInstruction(call_left);
  left->AddInstruction(goto_left);
  call_left->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* write_right = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                     c2,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* goto_right = new (GetAllocator()) HGoto();
  right->AddInstruction(write_right);
  right->AddInstruction(goto_right);

  HInstruction* read_bottom = new (GetAllocator()) HInstanceFieldGet(new_inst,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* return_exit = new (GetAllocator()) HReturn(read_bottom);
  breturn->AddInstruction(read_bottom);
  breturn->AddInstruction(return_exit);

  HInstruction* exit_instruction = new (GetAllocator()) HExit();
  exit->AddInstruction(exit_instruction);
  // PerformLSE expects this to be empty.
  graph_->ClearDominanceInformation();

  LOG(INFO) << "Pre LSE " << blks;
  PerformLSEWithPartial();
  LOG(INFO) << "Post LSE " << blks;

  std::vector<HPhi*> merges;
  HPredicatedInstanceFieldGet* pred_get;
  HInstanceFieldSet* init_set;
  std::tie(pred_get, init_set) =
      FindSingleInstructions<HPredicatedInstanceFieldGet, HInstanceFieldSet>(graph_);
  std::tie(merges) = FindAllInstructions<HPhi>(graph_);
  ASSERT_EQ(merges.size(), 3u);
  HPhi* merge_value_return = *std::find_if(merges.begin(), merges.end(), [&](HPhi* p) {
    return p->GetType() == DataType::Type::kInt32 && p->GetBlock() == breturn;
  });
  HPhi* merge_value_top = *std::find_if(merges.begin(), merges.end(), [&](HPhi* p) {
    return p->GetType() == DataType::Type::kInt32 && p->GetBlock() != breturn;
  });
  HPhi* merge_alloc = *std::find_if(merges.begin(), merges.end(), [](HPhi* p) {
    return p->GetType() == DataType::Type::kReference;
  });
  EXPECT_TRUE(IsRemoved(read_bottom));
  EXPECT_TRUE(IsRemoved(write_entry));
  EXPECT_TRUE(IsRemoved(write_partial));
  EXPECT_FALSE(IsRemoved(call_left));
  CheckFinalInstruction(if_inst->InputAt(0), ComparisonPlacement::kBeforeEscape);
  EXPECT_INS_EQ(init_set->InputAt(1), merge_value_top);
  EXPECT_INS_EQ(pred_get->InputAt(0), merge_alloc);
  EXPECT_INS_EQ(pred_get->InputAt(1), merge_value_return);
}

// // ENTRY
// // MOVED TO MATERIALIZATION BLOCK
// obj = new Obj();
// ELIMINATE, moved to materialization block. Kept by escape.
// obj.field = 3;
// // Make sure this graph isn't broken
// if (parameter_value) {
//   if (obj ==/!= (STATIC.VALUE|obj|null)) {
//     // partial_BLOCK
//     obj.field = 4;
//   }
//   // LEFT
//   // DO NOT ELIMINATE
//   escape(obj);
// } else {
//   // RIGHT
//   // ELIMINATE
//   obj.field = 2;
// }
// EXIT
// PREDICATED GET
// return obj.field
TEST_P(PartialComparisonTestGroup, PartialComparisonInCohortBeforeEscape) {
  // PartialComparisonKind kind = GetParam();
  ScopedObjectAccess soa(Thread::Current());
  VariableSizedHandleScope vshs(soa.Self());
  CreateGraph(/*handles=*/&vshs);
  AdjacencyListGraph blks(SetupFromAdjacencyList("entry",
                                                 "exit",
                                                 {{"entry", "left_begin"},
                                                  {"left_begin", "partial"},
                                                  {"left_begin", "left"},
                                                  {"partial", "left"},
                                                  {"entry", "right"},
                                                  {"left", "breturn"},
                                                  {"right", "breturn"},
                                                  {"breturn", "exit"}}));
#define GET_BLOCK(name) HBasicBlock* name = blks.Get(#name)
  GET_BLOCK(entry);
  GET_BLOCK(partial);
  GET_BLOCK(left_begin);
  GET_BLOCK(exit);
  GET_BLOCK(breturn);
  GET_BLOCK(left);
  GET_BLOCK(right);
#undef GET_BLOCK
  HInstruction* bool_value = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kBool);
  // HInstruction* c1 = graph_->GetIntConstant(1);
  HInstruction* c2 = graph_->GetIntConstant(2);
  HInstruction* c3 = graph_->GetIntConstant(3);
  HInstruction* c4 = graph_->GetIntConstant(4);
  HInstruction* cls = new (GetAllocator()) HLoadClass(graph_->GetCurrentMethod(),
                                                      dex::TypeIndex(10),
                                                      graph_->GetDexFile(),
                                                      ScopedNullHandle<mirror::Class>(),
                                                      false,
                                                      0,
                                                      false);
  HInstruction* new_inst =
      new (GetAllocator()) HNewInstance(cls,
                                        0,
                                        dex::TypeIndex(10),
                                        graph_->GetDexFile(),
                                        false,
                                        QuickEntrypointEnum::kQuickAllocObjectInitialized);
  HInstruction* write_entry = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                     c3,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* if_inst = new (GetAllocator()) HIf(bool_value);
  entry->AddInstruction(bool_value);
  entry->AddInstruction(cls);
  entry->AddInstruction(new_inst);
  entry->AddInstruction(write_entry);
  entry->AddInstruction(if_inst);
  ArenaVector<HInstruction*> current_locals({}, GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(cls, &current_locals);
  new_inst->CopyEnvironmentFrom(cls->GetEnvironment());

  ComparisonInstructions cmp_instructions = GetComparisonInstructions(new_inst);
  HInstruction* if_left_begin = new (GetAllocator()) HIf(cmp_instructions.cmp_);
  cmp_instructions.AddSetup(left_begin);
  left_begin->AddInstruction(cmp_instructions.cmp_);
  left_begin->AddInstruction(if_left_begin);
  cmp_instructions.AddEnvironment(cls->GetEnvironment());
  if (if_left_begin->AsIf()->IfTrueSuccessor() != partial) {
    left_begin->SwapSuccessors();
  }

  HInstruction* write_partial = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                       c4,
                                                                       nullptr,
                                                                       DataType::Type::kInt32,
                                                                       MemberOffset(10),
                                                                       false,
                                                                       0,
                                                                       0,
                                                                       graph_->GetDexFile(),
                                                                       0);
  HInstruction* goto_partial = new (GetAllocator()) HGoto();
  partial->AddInstruction(write_partial);
  partial->AddInstruction(goto_partial);

  HInstruction* call_left = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            1,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* goto_left = new (GetAllocator()) HGoto();
  call_left->AsInvoke()->SetRawInputAt(0, new_inst);
  left->AddInstruction(call_left);
  left->AddInstruction(goto_left);
  call_left->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* write_right = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                     c2,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* goto_right = new (GetAllocator()) HGoto();
  right->AddInstruction(write_right);
  right->AddInstruction(goto_right);

  HInstruction* read_bottom = new (GetAllocator()) HInstanceFieldGet(new_inst,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* return_exit = new (GetAllocator()) HReturn(read_bottom);
  breturn->AddInstruction(read_bottom);
  breturn->AddInstruction(return_exit);

  HInstruction* exit_instruction = new (GetAllocator()) HExit();
  exit->AddInstruction(exit_instruction);
  // PerformLSE expects this to be empty.
  graph_->ClearDominanceInformation();
  LOG(INFO) << "Pre LSE " << blks;
  PerformLSEWithPartial();
  LOG(INFO) << "Post LSE " << blks;

  std::vector<HPhi*> merges;
  HInstanceFieldSet* init_set =
      FindSingleInstruction<HInstanceFieldSet>(graph_, left_begin->GetSinglePredecessor());
  HInstanceFieldSet* partial_set = FindSingleInstruction<HInstanceFieldSet>(graph_, partial);
  HPredicatedInstanceFieldGet* pred_get =
      FindSingleInstruction<HPredicatedInstanceFieldGet>(graph_);
  std::tie(merges) = FindAllInstructions<HPhi>(graph_);
  ASSERT_EQ(merges.size(), 2u);
  HPhi* merge_value_return = *std::find_if(merges.begin(), merges.end(), [&](HPhi* p) {
    return p->GetType() == DataType::Type::kInt32 && p->GetBlock() == breturn;
  });
  HPhi* merge_alloc = *std::find_if(merges.begin(), merges.end(), [](HPhi* p) {
    return p->GetType() == DataType::Type::kReference;
  });
  EXPECT_TRUE(IsRemoved(read_bottom));
  EXPECT_TRUE(IsRemoved(write_entry));
  EXPECT_FALSE(IsRemoved(write_partial));
  EXPECT_FALSE(IsRemoved(call_left));
  CheckFinalInstruction(if_left_begin->InputAt(0), ComparisonPlacement::kInEscape);
  EXPECT_INS_EQ(init_set->InputAt(1), c3);
  EXPECT_INS_EQ(partial_set->InputAt(0), init_set->InputAt(0));
  EXPECT_INS_EQ(partial_set->InputAt(1), c4);
  EXPECT_INS_EQ(pred_get->InputAt(0), merge_alloc);
  EXPECT_INS_EQ(pred_get->InputAt(1), merge_value_return);
}

// // ENTRY
// // MOVED TO MATERIALIZATION BLOCK
// obj = new Obj();
// ELIMINATE, moved to materialization block. Kept by escape.
// obj.field = 3;
// // Make sure this graph isn't broken
// if (parameter_value) {
//   // LEFT
//   // DO NOT ELIMINATE
//   escape(obj);
// } else {
//   // RIGHT
//   // ELIMINATE
//   obj.field = 2;
// }
// if (obj ==/!= (STATIC.VALUE|obj|null)) {
//   // partial_BLOCK
//   obj.field = 4;
// }
// EXIT
// PREDICATED GET
// return obj.field
TEST_P(PartialComparisonTestGroup, PartialComparisonAfterCohort) {
  // PartialComparisonKind kind = GetParam();
  ScopedObjectAccess soa(Thread::Current());
  VariableSizedHandleScope vshs(soa.Self());
  CreateGraph(/*handles=*/&vshs);
  AdjacencyListGraph blks(SetupFromAdjacencyList("entry",
                                                 "exit",
                                                 {{"entry", "left"},
                                                  {"entry", "right"},
                                                  {"left", "critical_break"},
                                                  {"right", "critical_break"},
                                                  {"critical_break", "merge"},
                                                  {"merge", "breturn"},
                                                  {"merge", "partial"},
                                                  {"partial", "breturn"},
                                                  {"breturn", "exit"}}));
#define GET_BLOCK(name) HBasicBlock* name = blks.Get(#name)
  GET_BLOCK(entry);
  GET_BLOCK(partial);
  GET_BLOCK(critical_break);
  GET_BLOCK(merge);
  GET_BLOCK(exit);
  GET_BLOCK(breturn);
  GET_BLOCK(left);
  GET_BLOCK(right);
#undef GET_BLOCK
  HInstruction* bool_value = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kBool);
  // HInstruction* c1 = graph_->GetIntConstant(1);
  HInstruction* c2 = graph_->GetIntConstant(2);
  HInstruction* c3 = graph_->GetIntConstant(3);
  HInstruction* c4 = graph_->GetIntConstant(4);
  HInstruction* cls = new (GetAllocator()) HLoadClass(graph_->GetCurrentMethod(),
                                                      dex::TypeIndex(10),
                                                      graph_->GetDexFile(),
                                                      ScopedNullHandle<mirror::Class>(),
                                                      false,
                                                      0,
                                                      false);
  HInstruction* new_inst =
      new (GetAllocator()) HNewInstance(cls,
                                        0,
                                        dex::TypeIndex(10),
                                        graph_->GetDexFile(),
                                        false,
                                        QuickEntrypointEnum::kQuickAllocObjectInitialized);
  HInstruction* write_entry = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                     c3,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* if_inst = new (GetAllocator()) HIf(bool_value);
  entry->AddInstruction(bool_value);
  entry->AddInstruction(cls);
  entry->AddInstruction(new_inst);
  entry->AddInstruction(write_entry);
  entry->AddInstruction(if_inst);
  ArenaVector<HInstruction*> current_locals({}, GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(cls, &current_locals);
  new_inst->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* call_left = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            1,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* goto_left = new (GetAllocator()) HGoto();
  call_left->AsInvoke()->SetRawInputAt(0, new_inst);
  left->AddInstruction(call_left);
  left->AddInstruction(goto_left);
  call_left->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* write_right = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                     c2,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* goto_right = new (GetAllocator()) HGoto();
  right->AddInstruction(write_right);
  right->AddInstruction(goto_right);

  HInstruction* goto_crit_break = new (GetAllocator()) HGoto();
  critical_break->AddInstruction(goto_crit_break);

  ComparisonInstructions cmp_instructions = GetComparisonInstructions(new_inst);
  HInstruction* if_merge = new (GetAllocator()) HIf(cmp_instructions.cmp_);
  cmp_instructions.AddSetup(merge);
  merge->AddInstruction(cmp_instructions.cmp_);
  merge->AddInstruction(if_merge);
  cmp_instructions.AddEnvironment(cls->GetEnvironment());
  if (if_merge->AsIf()->IfTrueSuccessor() != partial) {
    merge->SwapSuccessors();
  }

  HInstruction* write_partial = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                       c4,
                                                                       nullptr,
                                                                       DataType::Type::kInt32,
                                                                       MemberOffset(10),
                                                                       false,
                                                                       0,
                                                                       0,
                                                                       graph_->GetDexFile(),
                                                                       0);
  HInstruction* goto_partial = new (GetAllocator()) HGoto();
  partial->AddInstruction(write_partial);
  partial->AddInstruction(goto_partial);

  HInstruction* read_bottom = new (GetAllocator()) HInstanceFieldGet(new_inst,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* return_exit = new (GetAllocator()) HReturn(read_bottom);
  breturn->AddInstruction(read_bottom);
  breturn->AddInstruction(return_exit);

  HInstruction* exit_instruction = new (GetAllocator()) HExit();
  exit->AddInstruction(exit_instruction);
  // PerformLSE expects this to be empty.
  graph_->ClearDominanceInformation();
  LOG(INFO) << "Pre LSE " << blks;
  PerformLSEWithPartial();
  LOG(INFO) << "Post LSE " << blks;

  std::vector<HPhi*> merges;
  HInstanceFieldSet* init_set =
      FindSingleInstruction<HInstanceFieldSet>(graph_, left->GetSinglePredecessor());
  HPredicatedInstanceFieldGet* pred_get =
      FindSingleInstruction<HPredicatedInstanceFieldGet>(graph_);
  std::tie(merges) = FindAllInstructions<HPhi>(graph_);
  ASSERT_EQ(merges.size(), 3u);
  HPhi* merge_value_return = *std::find_if(merges.begin(), merges.end(), [&](HPhi* p) {
    return p->GetType() == DataType::Type::kInt32 && p->GetBlock() == breturn;
  });
  HPhi* merge_alloc = *std::find_if(merges.begin(), merges.end(), [](HPhi* p) {
    return p->GetType() == DataType::Type::kReference;
  });
  EXPECT_TRUE(IsRemoved(read_bottom));
  EXPECT_TRUE(IsRemoved(write_entry));
  EXPECT_FALSE(IsRemoved(write_partial));
  EXPECT_TRUE(write_partial->AsInstanceFieldSet()->GetIsPredicatedSet());
  EXPECT_FALSE(IsRemoved(call_left));
  CheckFinalInstruction(if_merge->InputAt(0), ComparisonPlacement::kAfterEscape);
  EXPECT_INS_EQ(init_set->InputAt(1), c3);
  EXPECT_INS_EQ(write_partial->InputAt(0)->AsPhi()->InputAt(0), init_set->InputAt(0));
  EXPECT_INS_EQ(write_partial->InputAt(1), c4);
  EXPECT_INS_EQ(pred_get->InputAt(0), merge_alloc);
  EXPECT_INS_EQ(pred_get->InputAt(1), merge_value_return);
}

// // ENTRY
// // MOVED TO MATERIALIZATION BLOCK
// obj = new Obj();
// ELIMINATE, moved to materialization block. Kept by escape.
// obj.field = 3;
// // Make sure this graph isn't broken
// if (parameter_value) {
//   // LEFT
//   // DO NOT ELIMINATE
//   escape(obj);
//   if (obj ==/!= (STATIC.VALUE|obj|null)) {
//     // partial_BLOCK
//     obj.field = 4;
//   }
// } else {
//   // RIGHT
//   // ELIMINATE
//   obj.field = 2;
// }
// EXIT
// PREDICATED GET
// return obj.field
TEST_P(PartialComparisonTestGroup, PartialComparisonInCohortAfterEscape) {
  PartialComparisonKind kind = GetParam();
  ScopedObjectAccess soa(Thread::Current());
  VariableSizedHandleScope vshs(soa.Self());
  CreateGraph(/*handles=*/&vshs);
  AdjacencyListGraph blks(SetupFromAdjacencyList("entry",
                                                 "exit",
                                                 {{"entry", "left"},
                                                  {"left", "partial"},
                                                  {"partial", "left_end"},
                                                  {"left", "left_end"},
                                                  {"left_end", "breturn"},
                                                  {"entry", "right"},
                                                  {"right", "breturn"},
                                                  {"breturn", "exit"}}));
#define GET_BLOCK(name) HBasicBlock* name = blks.Get(#name)
  GET_BLOCK(entry);
  GET_BLOCK(partial);
  GET_BLOCK(left_end);
  GET_BLOCK(exit);
  GET_BLOCK(breturn);
  GET_BLOCK(left);
  GET_BLOCK(right);
#undef GET_BLOCK
  HInstruction* bool_value = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kBool);
  // HInstruction* c1 = graph_->GetIntConstant(1);
  HInstruction* c2 = graph_->GetIntConstant(2);
  HInstruction* c3 = graph_->GetIntConstant(3);
  HInstruction* c4 = graph_->GetIntConstant(4);
  HInstruction* cls = new (GetAllocator()) HLoadClass(graph_->GetCurrentMethod(),
                                                      dex::TypeIndex(10),
                                                      graph_->GetDexFile(),
                                                      ScopedNullHandle<mirror::Class>(),
                                                      false,
                                                      0,
                                                      false);
  HInstruction* new_inst =
      new (GetAllocator()) HNewInstance(cls,
                                        0,
                                        dex::TypeIndex(10),
                                        graph_->GetDexFile(),
                                        false,
                                        QuickEntrypointEnum::kQuickAllocObjectInitialized);
  HInstruction* write_entry = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                     c3,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* if_inst = new (GetAllocator()) HIf(bool_value);
  entry->AddInstruction(bool_value);
  entry->AddInstruction(cls);
  entry->AddInstruction(new_inst);
  entry->AddInstruction(write_entry);
  entry->AddInstruction(if_inst);
  ArenaVector<HInstruction*> current_locals({}, GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(cls, &current_locals);
  new_inst->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* call_left = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            1,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  ComparisonInstructions cmp_instructions = GetComparisonInstructions(new_inst);
  HInstruction* if_left = new (GetAllocator()) HIf(cmp_instructions.cmp_);
  call_left->AsInvoke()->SetRawInputAt(0, new_inst);
  left->AddInstruction(call_left);
  cmp_instructions.AddSetup(left);
  left->AddInstruction(cmp_instructions.cmp_);
  left->AddInstruction(if_left);
  call_left->CopyEnvironmentFrom(cls->GetEnvironment());
  cmp_instructions.AddEnvironment(cls->GetEnvironment());
  if (if_left->AsIf()->IfTrueSuccessor() != partial) {
    left->SwapSuccessors();
  }

  HInstruction* write_partial = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                       c4,
                                                                       nullptr,
                                                                       DataType::Type::kInt32,
                                                                       MemberOffset(10),
                                                                       false,
                                                                       0,
                                                                       0,
                                                                       graph_->GetDexFile(),
                                                                       0);
  HInstruction* goto_partial = new (GetAllocator()) HGoto();
  partial->AddInstruction(write_partial);
  partial->AddInstruction(goto_partial);

  HInstruction* goto_left_end = new (GetAllocator()) HGoto();
  left_end->AddInstruction(goto_left_end);

  HInstruction* write_right = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                     c2,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* goto_right = new (GetAllocator()) HGoto();
  right->AddInstruction(write_right);
  right->AddInstruction(goto_right);

  HInstruction* read_bottom = new (GetAllocator()) HInstanceFieldGet(new_inst,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* return_exit = new (GetAllocator()) HReturn(read_bottom);
  breturn->AddInstruction(read_bottom);
  breturn->AddInstruction(return_exit);

  HInstruction* exit_instruction = new (GetAllocator()) HExit();
  exit->AddInstruction(exit_instruction);
  // PerformLSE expects this to be empty.
  graph_->ClearDominanceInformation();

  LOG(INFO) << "Pre LSE " << blks;
  PerformLSEWithPartial();
  LOG(INFO) << "Post LSE " << blks;

  std::vector<HPhi*> merges;
  std::vector<HInstanceFieldSet*> sets;
  HPredicatedInstanceFieldGet* pred_get =
      FindSingleInstruction<HPredicatedInstanceFieldGet>(graph_);
  std::tie(merges, sets) = FindAllInstructions<HPhi, HInstanceFieldSet>(graph_);
  ASSERT_EQ(merges.size(), 2u);
  ASSERT_GE(sets.size(), 1u);
  HInstanceFieldSet* init_set = *std::find_if(sets.begin(), sets.end(), [&](HInstanceFieldSet* s) {
    return s->GetBlock()->GetSingleSuccessor() == left;
  });
  EXPECT_INS_EQ(init_set->InputAt(1), c3);
  HPhi* merge_value_return = *std::find_if(merges.begin(), merges.end(), [&](HPhi* p) {
    return p->GetType() == DataType::Type::kInt32 && p->GetBlock() == breturn;
  });
  HPhi* merge_alloc = *std::find_if(merges.begin(), merges.end(), [](HPhi* p) {
    return p->GetType() == DataType::Type::kReference;
  });
  EXPECT_TRUE(IsRemoved(read_bottom));
  EXPECT_TRUE(IsRemoved(write_entry));
  if (kind.IsPossiblyTrue()) {
    EXPECT_FALSE(IsRemoved(write_partial));
    EXPECT_TRUE(std::find(sets.begin(), sets.end(), write_partial) != sets.end());
  }
  EXPECT_FALSE(IsRemoved(call_left));
  CheckFinalInstruction(if_left->InputAt(0), ComparisonPlacement::kInEscape);
  EXPECT_INS_EQ(pred_get->InputAt(0), merge_alloc);
  EXPECT_INS_EQ(pred_get->InputAt(1), merge_value_return);
}

INSTANTIATE_TEST_SUITE_P(
    LoadStoreEliminationTest,
    PartialComparisonTestGroup,
    testing::Values(PartialComparisonKind{PartialComparisonKind::Type::kEquals,
                                          PartialComparisonKind::Target::kNull,
                                          PartialComparisonKind::Position::kLeft},
                    PartialComparisonKind{PartialComparisonKind::Type::kEquals,
                                          PartialComparisonKind::Target::kNull,
                                          PartialComparisonKind::Position::kRight},
                    PartialComparisonKind{PartialComparisonKind::Type::kEquals,
                                          PartialComparisonKind::Target::kValue,
                                          PartialComparisonKind::Position::kLeft},
                    PartialComparisonKind{PartialComparisonKind::Type::kEquals,
                                          PartialComparisonKind::Target::kValue,
                                          PartialComparisonKind::Position::kRight},
                    PartialComparisonKind{PartialComparisonKind::Type::kEquals,
                                          PartialComparisonKind::Target::kSelf,
                                          PartialComparisonKind::Position::kLeft},
                    PartialComparisonKind{PartialComparisonKind::Type::kNotEquals,
                                          PartialComparisonKind::Target::kNull,
                                          PartialComparisonKind::Position::kLeft},
                    PartialComparisonKind{PartialComparisonKind::Type::kNotEquals,
                                          PartialComparisonKind::Target::kNull,
                                          PartialComparisonKind::Position::kRight},
                    PartialComparisonKind{PartialComparisonKind::Type::kNotEquals,
                                          PartialComparisonKind::Target::kSelf,
                                          PartialComparisonKind::Position::kLeft},
                    PartialComparisonKind{PartialComparisonKind::Type::kNotEquals,
                                          PartialComparisonKind::Target::kValue,
                                          PartialComparisonKind::Position::kLeft},
                    PartialComparisonKind{PartialComparisonKind::Type::kNotEquals,
                                          PartialComparisonKind::Target::kValue,
                                          PartialComparisonKind::Position::kRight}));

// // ENTRY
// obj = new Obj();
// if (parameter_value) {
//   // LEFT
//   escape(obj);
// } else {
//   // RIGHT
//   // ELIMINATE
//   obj.field = 2;
// }
// EXIT
// predicated-ELIMINATE
// obj.field = 3;
TEST_F(LoadStoreEliminationTest, PredicatedStore1) {
  ScopedObjectAccess soa(Thread::Current());
  VariableSizedHandleScope vshs(soa.Self());
  InitGraph(/*handles=*/&vshs);
  AdjacencyListGraph blks(SetupFromAdjacencyList("entry",
                                                 "exit",
                                                 {{"entry", "left"},
                                                  {"entry", "right"},
                                                  {"left", "breturn"},
                                                  {"right", "breturn"},
                                                  {"breturn", "exit"}}));
#define GET_BLOCK(name) HBasicBlock* name = blks.Get(#name)
  GET_BLOCK(entry);
  GET_BLOCK(exit);
  GET_BLOCK(breturn);
  GET_BLOCK(left);
  GET_BLOCK(right);
#undef GET_BLOCK
  EnsurePredecessorOrder(breturn, {left, right});
  HInstruction* bool_value = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kBool);
  HInstruction* null_const = graph_->GetNullConstant();
  HInstruction* c2 = graph_->GetIntConstant(2);
  HInstruction* c3 = graph_->GetIntConstant(3);
  HInstruction* cls = new (GetAllocator()) HLoadClass(graph_->GetCurrentMethod(),
                                                      dex::TypeIndex(10),
                                                      graph_->GetDexFile(),
                                                      ScopedNullHandle<mirror::Class>(),
                                                      false,
                                                      0,
                                                      false);
  HInstruction* new_inst =
      new (GetAllocator()) HNewInstance(cls,
                                        0,
                                        dex::TypeIndex(10),
                                        graph_->GetDexFile(),
                                        false,
                                        QuickEntrypointEnum::kQuickAllocObjectInitialized);
  HInstruction* if_inst = new (GetAllocator()) HIf(bool_value);
  entry->AddInstruction(bool_value);
  entry->AddInstruction(cls);
  entry->AddInstruction(new_inst);
  entry->AddInstruction(if_inst);
  ArenaVector<HInstruction*> current_locals({}, GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(cls, &current_locals);
  new_inst->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* call_left = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            1,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* goto_left = new (GetAllocator()) HGoto();
  call_left->AsInvoke()->SetRawInputAt(0, new_inst);
  left->AddInstruction(call_left);
  left->AddInstruction(goto_left);
  call_left->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* write_right = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                     c2,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* goto_right = new (GetAllocator()) HGoto();
  right->AddInstruction(write_right);
  right->AddInstruction(goto_right);

  HInstruction* write_bottom = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                      c3,
                                                                      nullptr,
                                                                      DataType::Type::kInt32,
                                                                      MemberOffset(10),
                                                                      false,
                                                                      0,
                                                                      0,
                                                                      graph_->GetDexFile(),
                                                                      0);
  HInstruction* return_exit = new (GetAllocator()) HReturnVoid();
  breturn->AddInstruction(write_bottom);
  breturn->AddInstruction(return_exit);

  HInstruction* exit_instruction = new (GetAllocator()) HExit();
  exit->AddInstruction(exit_instruction);
  // PerformLSE expects this to be empty.
  graph_->ClearDominanceInformation();

  LOG(INFO) << "Pre LSE " << blks;
  PerformLSEWithPartial();
  LOG(INFO) << "Post LSE " << blks;

  EXPECT_FALSE(IsRemoved(write_bottom));
  EXPECT_TRUE(write_bottom->AsInstanceFieldSet()->GetIsPredicatedSet());
  EXPECT_TRUE(IsRemoved(write_right));
  EXPECT_FALSE(IsRemoved(call_left));
  HPhi* merge_alloc = FindSingleInstruction<HPhi>(graph_, breturn);
  ASSERT_NE(merge_alloc, nullptr);
  EXPECT_TRUE(merge_alloc->InputAt(0)->IsNewInstance()) << *merge_alloc;
  EXPECT_EQ(merge_alloc->InputAt(0)->InputAt(0), cls) << *merge_alloc << " cls? " << *cls;
  EXPECT_EQ(merge_alloc->InputAt(1), null_const);
}

// // ENTRY
// obj = new Obj();
// obj.field = 3;
// if (parameter_value) {
//   // LEFT
//   escape(obj);
// } else {
//   // RIGHT
//   // ELIMINATE
//   obj.field = 2;
// }
// // MERGE
// if (second_param) {
//   // NON_ESCAPE
//   obj.field = 1;
//   noescape();
// }
// EXIT
// predicated-ELIMINATE
// obj.field = 4;
TEST_F(LoadStoreEliminationTest, PredicatedStore2) {
  ScopedObjectAccess soa(Thread::Current());
  VariableSizedHandleScope vshs(soa.Self());
  CreateGraph(/*handles=*/&vshs);
  AdjacencyListGraph blks(SetupFromAdjacencyList("entry",
                                                 "exit",
                                                 {{"entry", "left"},
                                                  {"entry", "right"},
                                                  {"left", "merge"},
                                                  {"right", "merge"},
                                                  {"merge", "non_escape"},
                                                  {"non_escape", "breturn"},
                                                  {"merge", "breturn"},
                                                  {"breturn", "exit"}}));
#define GET_BLOCK(name) HBasicBlock* name = blks.Get(#name)
  GET_BLOCK(entry);
  GET_BLOCK(exit);
  GET_BLOCK(breturn);
  GET_BLOCK(left);
  GET_BLOCK(right);
  GET_BLOCK(merge);
  GET_BLOCK(non_escape);
#undef GET_BLOCK
  EnsurePredecessorOrder(merge, {left, right});
  EnsurePredecessorOrder(breturn, {merge, non_escape});
  HInstruction* bool_value = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kBool);
  HInstruction* bool_value2 = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 2, DataType::Type::kBool);
  HInstruction* null_const = graph_->GetNullConstant();
  HInstruction* c1 = graph_->GetIntConstant(3);
  HInstruction* c2 = graph_->GetIntConstant(2);
  HInstruction* c3 = graph_->GetIntConstant(3);
  HInstruction* c4 = graph_->GetIntConstant(4);
  HInstruction* cls = new (GetAllocator()) HLoadClass(graph_->GetCurrentMethod(),
                                                      dex::TypeIndex(10),
                                                      graph_->GetDexFile(),
                                                      ScopedNullHandle<mirror::Class>(),
                                                      false,
                                                      0,
                                                      false);
  HInstruction* new_inst =
      new (GetAllocator()) HNewInstance(cls,
                                        0,
                                        dex::TypeIndex(10),
                                        graph_->GetDexFile(),
                                        false,
                                        QuickEntrypointEnum::kQuickAllocObjectInitialized);
  HInstruction* write_entry = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                     c3,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* if_inst = new (GetAllocator()) HIf(bool_value);
  entry->AddInstruction(bool_value);
  entry->AddInstruction(bool_value2);
  entry->AddInstruction(cls);
  entry->AddInstruction(new_inst);
  entry->AddInstruction(write_entry);
  entry->AddInstruction(if_inst);
  ArenaVector<HInstruction*> current_locals({}, GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(cls, &current_locals);
  new_inst->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* call_left = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            1,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* goto_left = new (GetAllocator()) HGoto();
  call_left->AsInvoke()->SetRawInputAt(0, new_inst);
  left->AddInstruction(call_left);
  left->AddInstruction(goto_left);
  call_left->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* write_right = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                     c2,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* goto_right = new (GetAllocator()) HGoto();
  right->AddInstruction(write_right);
  right->AddInstruction(goto_right);

  HInstruction* merge_if = new (GetAllocator()) HIf(bool_value2);
  merge->AddInstruction(merge_if);

  HInstruction* write_non_escape = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                          c1,
                                                                          nullptr,
                                                                          DataType::Type::kInt32,
                                                                          MemberOffset(10),
                                                                          false,
                                                                          0,
                                                                          0,
                                                                          graph_->GetDexFile(),
                                                                          0);
  HInstruction* non_escape_call = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            0,
                            DataType::Type::kVoid,
                            0u,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* non_escape_goto = new (GetAllocator()) HGoto();
  non_escape->AddInstruction(write_non_escape);
  non_escape->AddInstruction(non_escape_call);
  non_escape->AddInstruction(non_escape_goto);
  non_escape_call->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* write_bottom = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                      c4,
                                                                      nullptr,
                                                                      DataType::Type::kInt32,
                                                                      MemberOffset(10),
                                                                      false,
                                                                      0,
                                                                      0,
                                                                      graph_->GetDexFile(),
                                                                      0);
  HInstruction* return_exit = new (GetAllocator()) HReturnVoid();
  breturn->AddInstruction(write_bottom);
  breturn->AddInstruction(return_exit);

  HInstruction* exit_instruction = new (GetAllocator()) HExit();
  exit->AddInstruction(exit_instruction);
  // PerformLSE expects this to be empty.
  graph_->ClearDominanceInformation();
  LOG(INFO) << "Pre LSE " << blks;
  PerformLSEWithPartial();
  LOG(INFO) << "Post LSE " << blks;

  EXPECT_FALSE(IsRemoved(write_bottom));
  EXPECT_TRUE(write_bottom->AsInstanceFieldSet()->GetIsPredicatedSet()) << *write_bottom;
  EXPECT_TRUE(IsRemoved(write_right));
  EXPECT_FALSE(IsRemoved(call_left));
  HInstanceFieldSet* pred_set = FindSingleInstruction<HInstanceFieldSet>(graph_, breturn);
  HPhi* merge_alloc = FindSingleInstruction<HPhi>(graph_);
  ASSERT_NE(merge_alloc, nullptr);
  EXPECT_TRUE(merge_alloc->InputAt(0)->IsNewInstance()) << *merge_alloc;
  EXPECT_INS_EQ(merge_alloc->InputAt(0)->InputAt(0), cls) << " phi is: " << *merge_alloc;
  EXPECT_INS_EQ(merge_alloc->InputAt(1), null_const);
  ASSERT_NE(pred_set, nullptr);
  EXPECT_TRUE(pred_set->GetIsPredicatedSet()) << *pred_set;
  EXPECT_INS_EQ(pred_set->InputAt(0), merge_alloc);
}

// // ENTRY
// obj = new Obj();
// obj.field = 3;
// if (parameter_value) {
//   // LEFT
//   escape(obj);
// } else {
//   // RIGHT
//   // ELIMINATE
//   obj.field = 2;
// }
// EXIT
// predicated-ELIMINATE
// return obj.field
TEST_F(LoadStoreEliminationTest, PredicatedLoad1) {
  ScopedObjectAccess soa(Thread::Current());
  VariableSizedHandleScope vshs(soa.Self());
  CreateGraph(/*handles=*/&vshs);
  AdjacencyListGraph blks(SetupFromAdjacencyList("entry",
                                                 "exit",
                                                 {{"entry", "left"},
                                                  {"entry", "right"},
                                                  {"left", "breturn"},
                                                  {"right", "breturn"},
                                                  {"breturn", "exit"}}));
#define GET_BLOCK(name) HBasicBlock* name = blks.Get(#name)
  GET_BLOCK(entry);
  GET_BLOCK(exit);
  GET_BLOCK(breturn);
  GET_BLOCK(left);
  GET_BLOCK(right);
#undef GET_BLOCK
  EnsurePredecessorOrder(breturn, {left, right});
  HInstruction* bool_value = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kBool);
  HInstruction* null_const = graph_->GetNullConstant();
  HInstruction* c2 = graph_->GetIntConstant(2);
  HInstruction* c3 = graph_->GetIntConstant(3);
  HInstruction* cls = new (GetAllocator()) HLoadClass(graph_->GetCurrentMethod(),
                                                      dex::TypeIndex(10),
                                                      graph_->GetDexFile(),
                                                      ScopedNullHandle<mirror::Class>(),
                                                      false,
                                                      0,
                                                      false);
  HInstruction* new_inst =
      new (GetAllocator()) HNewInstance(cls,
                                        0,
                                        dex::TypeIndex(10),
                                        graph_->GetDexFile(),
                                        false,
                                        QuickEntrypointEnum::kQuickAllocObjectInitialized);
  HInstruction* write_entry = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                     c3,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* if_inst = new (GetAllocator()) HIf(bool_value);
  entry->AddInstruction(bool_value);
  entry->AddInstruction(cls);
  entry->AddInstruction(new_inst);
  entry->AddInstruction(write_entry);
  entry->AddInstruction(if_inst);
  ArenaVector<HInstruction*> current_locals({}, GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(cls, &current_locals);
  new_inst->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* call_left = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            1,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* goto_left = new (GetAllocator()) HGoto();
  call_left->AsInvoke()->SetRawInputAt(0, new_inst);
  left->AddInstruction(call_left);
  left->AddInstruction(goto_left);
  call_left->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* write_right = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                     c2,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* goto_right = new (GetAllocator()) HGoto();
  right->AddInstruction(write_right);
  right->AddInstruction(goto_right);

  HInstruction* read_bottom = new (GetAllocator()) HInstanceFieldGet(new_inst,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* return_exit = new (GetAllocator()) HReturn(read_bottom);
  breturn->AddInstruction(read_bottom);
  breturn->AddInstruction(return_exit);

  HInstruction* exit_instruction = new (GetAllocator()) HExit();
  exit->AddInstruction(exit_instruction);
  // PerformLSE expects this to be empty.
  graph_->ClearDominanceInformation();
  LOG(INFO) << "Pre LSE " << blks;
  PerformLSEWithPartial();
  LOG(INFO) << "Post LSE " << blks;

  EXPECT_TRUE(IsRemoved(read_bottom));
  EXPECT_TRUE(IsRemoved(write_right));
  EXPECT_FALSE(IsRemoved(call_left));
  std::vector<HPhi*> merges;
  HPredicatedInstanceFieldGet* pred_get =
      FindSingleInstruction<HPredicatedInstanceFieldGet>(graph_, breturn);
  std::tie(merges) = FindAllInstructions<HPhi>(graph_, breturn);
  ASSERT_EQ(merges.size(), 2u);
  HPhi* merge_value_return = *std::find_if(
      merges.begin(), merges.end(), [](HPhi* p) { return p->GetType() == DataType::Type::kInt32; });
  HPhi* merge_alloc = *std::find_if(merges.begin(), merges.end(), [](HPhi* p) {
    return p->GetType() == DataType::Type::kReference;
  });
  ASSERT_NE(merge_alloc, nullptr);
  EXPECT_TRUE(merge_alloc->InputAt(0)->IsNewInstance()) << *merge_alloc;
  EXPECT_EQ(merge_alloc->InputAt(0)->InputAt(0), cls) << *merge_alloc << " cls? " << *cls;
  EXPECT_EQ(merge_alloc->InputAt(1), null_const);
  ASSERT_NE(pred_get, nullptr);
  EXPECT_INS_EQ(pred_get->InputAt(0), merge_alloc);
  EXPECT_INS_EQ(pred_get->InputAt(1), merge_value_return) << " pred-get is: " << *pred_get;
  EXPECT_INS_EQ(merge_value_return->InputAt(0), graph_->GetIntConstant(0))
      << " merge val is: " << *merge_value_return;
  EXPECT_INS_EQ(merge_value_return->InputAt(1), c2) << " merge val is: " << *merge_value_return;
}

// // ENTRY
// obj1 = new Obj1();
// obj2 = new Obj2();
// obj1.field = 3;
// obj2.field = 13;
// if (parameter_value) {
//   // LEFT
//   escape(obj1);
//   escape(obj2);
// } else {
//   // RIGHT
//   // ELIMINATE
//   obj1.field = 2;
//   obj2.field = 12;
// }
// EXIT
// predicated-ELIMINATE
// return obj1.field + obj2.field
TEST_F(LoadStoreEliminationTest, MultiPredicatedLoad1) {
  ScopedObjectAccess soa(Thread::Current());
  VariableSizedHandleScope vshs(soa.Self());
  CreateGraph(/*handles=*/&vshs);
  AdjacencyListGraph blks(SetupFromAdjacencyList("entry",
                                                 "exit",
                                                 {{"entry", "left"},
                                                  {"entry", "right"},
                                                  {"left", "breturn"},
                                                  {"right", "breturn"},
                                                  {"breturn", "exit"}}));
#define GET_BLOCK(name) HBasicBlock* name = blks.Get(#name)
  GET_BLOCK(entry);
  GET_BLOCK(exit);
  GET_BLOCK(breturn);
  GET_BLOCK(left);
  GET_BLOCK(right);
#undef GET_BLOCK
  EnsurePredecessorOrder(breturn, {left, right});
  HInstruction* bool_value = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kBool);
  // HInstruction* null_const = graph_->GetNullConstant();
  HInstruction* c2 = graph_->GetIntConstant(2);
  HInstruction* c3 = graph_->GetIntConstant(3);
  HInstruction* c12 = graph_->GetIntConstant(12);
  HInstruction* c13 = graph_->GetIntConstant(13);
  HInstruction* cls1 = new (GetAllocator()) HLoadClass(graph_->GetCurrentMethod(),
                                                       dex::TypeIndex(10),
                                                       graph_->GetDexFile(),
                                                       ScopedNullHandle<mirror::Class>(),
                                                       false,
                                                       0,
                                                       false);
  HInstruction* cls2 = new (GetAllocator()) HLoadClass(graph_->GetCurrentMethod(),
                                                       dex::TypeIndex(20),
                                                       graph_->GetDexFile(),
                                                       ScopedNullHandle<mirror::Class>(),
                                                       false,
                                                       0,
                                                       false);
  HInstruction* new_inst1 =
      new (GetAllocator()) HNewInstance(cls1,
                                        0,
                                        dex::TypeIndex(10),
                                        graph_->GetDexFile(),
                                        false,
                                        QuickEntrypointEnum::kQuickAllocObjectInitialized);
  HInstruction* new_inst2 =
      new (GetAllocator()) HNewInstance(cls2,
                                        0,
                                        dex::TypeIndex(20),
                                        graph_->GetDexFile(),
                                        false,
                                        QuickEntrypointEnum::kQuickAllocObjectInitialized);
  HInstruction* write_entry1 = new (GetAllocator()) HInstanceFieldSet(new_inst1,
                                                                      c3,
                                                                      nullptr,
                                                                      DataType::Type::kInt32,
                                                                      MemberOffset(10),
                                                                      false,
                                                                      0,
                                                                      0,
                                                                      graph_->GetDexFile(),
                                                                      0);
  HInstruction* write_entry2 = new (GetAllocator()) HInstanceFieldSet(new_inst2,
                                                                      c13,
                                                                      nullptr,
                                                                      DataType::Type::kInt32,
                                                                      MemberOffset(10),
                                                                      false,
                                                                      0,
                                                                      0,
                                                                      graph_->GetDexFile(),
                                                                      0);
  HInstruction* if_inst = new (GetAllocator()) HIf(bool_value);
  entry->AddInstruction(bool_value);
  entry->AddInstruction(cls1);
  entry->AddInstruction(cls2);
  entry->AddInstruction(new_inst1);
  entry->AddInstruction(new_inst2);
  entry->AddInstruction(write_entry1);
  entry->AddInstruction(write_entry2);
  entry->AddInstruction(if_inst);
  ArenaVector<HInstruction*> current_locals({}, GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(cls1, &current_locals);
  cls2->CopyEnvironmentFrom(cls1->GetEnvironment());
  new_inst1->CopyEnvironmentFrom(cls1->GetEnvironment());
  new_inst2->CopyEnvironmentFrom(cls1->GetEnvironment());

  HInstruction* call_left1 = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            1,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* call_left2 = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            1,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* goto_left = new (GetAllocator()) HGoto();
  call_left1->AsInvoke()->SetRawInputAt(0, new_inst1);
  call_left2->AsInvoke()->SetRawInputAt(0, new_inst2);
  left->AddInstruction(call_left1);
  left->AddInstruction(call_left2);
  left->AddInstruction(goto_left);
  call_left1->CopyEnvironmentFrom(cls1->GetEnvironment());
  call_left2->CopyEnvironmentFrom(cls1->GetEnvironment());

  HInstruction* write_right1 = new (GetAllocator()) HInstanceFieldSet(new_inst1,
                                                                      c2,
                                                                      nullptr,
                                                                      DataType::Type::kInt32,
                                                                      MemberOffset(10),
                                                                      false,
                                                                      0,
                                                                      0,
                                                                      graph_->GetDexFile(),
                                                                      0);
  HInstruction* write_right2 = new (GetAllocator()) HInstanceFieldSet(new_inst2,
                                                                      c12,
                                                                      nullptr,
                                                                      DataType::Type::kInt32,
                                                                      MemberOffset(10),
                                                                      false,
                                                                      0,
                                                                      0,
                                                                      graph_->GetDexFile(),
                                                                      0);
  HInstruction* goto_right = new (GetAllocator()) HGoto();
  right->AddInstruction(write_right1);
  right->AddInstruction(write_right2);
  right->AddInstruction(goto_right);

  HInstruction* read_bottom1 = new (GetAllocator()) HInstanceFieldGet(new_inst1,
                                                                      nullptr,
                                                                      DataType::Type::kInt32,
                                                                      MemberOffset(10),
                                                                      false,
                                                                      0,
                                                                      0,
                                                                      graph_->GetDexFile(),
                                                                      0);
  HInstruction* read_bottom2 = new (GetAllocator()) HInstanceFieldGet(new_inst2,
                                                                      nullptr,
                                                                      DataType::Type::kInt32,
                                                                      MemberOffset(10),
                                                                      false,
                                                                      0,
                                                                      0,
                                                                      graph_->GetDexFile(),
                                                                      0);
  HInstruction* combine =
      new (GetAllocator()) HAdd(DataType::Type::kInt32, read_bottom1, read_bottom2);
  HInstruction* return_exit = new (GetAllocator()) HReturn(combine);
  breturn->AddInstruction(read_bottom1);
  breturn->AddInstruction(read_bottom2);
  breturn->AddInstruction(combine);
  breturn->AddInstruction(return_exit);

  HInstruction* exit_instruction = new (GetAllocator()) HExit();
  exit->AddInstruction(exit_instruction);
  // PerformLSE expects this to be empty.
  graph_->ClearDominanceInformation();
  LOG(INFO) << "Pre LSE " << blks;
  PerformLSEWithPartial();
  LOG(INFO) << "Post LSE " << blks;

  EXPECT_TRUE(IsRemoved(read_bottom1));
  EXPECT_TRUE(IsRemoved(read_bottom2));
  EXPECT_TRUE(IsRemoved(write_right1));
  EXPECT_TRUE(IsRemoved(write_right2));
  EXPECT_FALSE(IsRemoved(call_left1));
  EXPECT_FALSE(IsRemoved(call_left2));
  std::vector<HPhi*> merges;
  std::vector<HPredicatedInstanceFieldGet*> pred_gets;
  std::tie(merges, pred_gets) =
      FindAllInstructions<HPhi, HPredicatedInstanceFieldGet>(graph_, breturn);
  ASSERT_EQ(merges.size(), 4u);
  ASSERT_EQ(pred_gets.size(), 2u);
  HPhi* merge_value_return1 = *std::find_if(merges.begin(), merges.end(), [&](HPhi* p) {
    return p->GetType() == DataType::Type::kInt32 && p->InputAt(1) == c2;
  });
  HPhi* merge_value_return2 = *std::find_if(merges.begin(), merges.end(), [&](HPhi* p) {
    return p->GetType() == DataType::Type::kInt32 && p->InputAt(1) == c12;
  });
  HPhi* merge_alloc1 = *std::find_if(merges.begin(), merges.end(), [&](HPhi* p) {
    return p->GetType() == DataType::Type::kReference && p->InputAt(0)->InputAt(0) == cls1;
  });
  HPhi* merge_alloc2 = *std::find_if(merges.begin(), merges.end(), [&](HPhi* p) {
    return p->GetType() == DataType::Type::kReference && p->InputAt(0)->InputAt(0) == cls2;
  });
  HPredicatedInstanceFieldGet* pred_get1 =
      *std::find_if(pred_gets.begin(), pred_gets.end(), [&](HPredicatedInstanceFieldGet* pg) {
        return pg->InputAt(0) == merge_alloc1;
      });
  HPredicatedInstanceFieldGet* pred_get2 =
      *std::find_if(pred_gets.begin(), pred_gets.end(), [&](HPredicatedInstanceFieldGet* pg) {
        return pg->InputAt(0) == merge_alloc2;
      });
  ASSERT_NE(merge_alloc1, nullptr);
  EXPECT_TRUE(merge_alloc1->InputAt(0)->IsNewInstance()) << *merge_alloc1;
  EXPECT_EQ(merge_alloc1->InputAt(1), graph_->GetNullConstant());
  ASSERT_NE(merge_alloc2, nullptr);
  EXPECT_TRUE(merge_alloc2->InputAt(0)->IsNewInstance()) << *merge_alloc2;
  EXPECT_EQ(merge_alloc2->InputAt(1), graph_->GetNullConstant());
  ASSERT_NE(pred_get1, nullptr);
  EXPECT_INS_EQ(pred_get1->InputAt(0), merge_alloc1);
  EXPECT_INS_EQ(pred_get1->InputAt(1), merge_value_return1) << " pred-get is: " << *pred_get1;
  EXPECT_INS_EQ(merge_value_return1->InputAt(0), graph_->GetIntConstant(0))
      << " merge val is: " << *merge_value_return1;
  EXPECT_INS_EQ(merge_value_return1->InputAt(1), c2) << " merge val is: " << *merge_value_return1;
  ASSERT_NE(pred_get2, nullptr);
  EXPECT_INS_EQ(pred_get2->InputAt(0), merge_alloc2);
  EXPECT_INS_EQ(pred_get2->InputAt(1), merge_value_return2) << " pred-get is: " << *pred_get2;
  EXPECT_INS_EQ(merge_value_return2->InputAt(0), graph_->GetIntConstant(0))
      << " merge val is: " << *merge_value_return1;
  EXPECT_INS_EQ(merge_value_return2->InputAt(1), c12) << " merge val is: " << *merge_value_return1;
}
// // ENTRY
// obj1 = new Obj1();
// obj2 = new Obj2();
// obj1.field = 3;
// obj2.field = 13;
// if (parameter_value) {
//   // LEFT
//   escape(obj1);
//   // ELIMINATE
//   obj2.field = 12;
// } else {
//   // RIGHT
//   // ELIMINATE
//   obj1.field = 2;
//   escape(obj2);
// }
// EXIT
// predicated-ELIMINATE
// return obj1.field + obj2.field
TEST_F(LoadStoreEliminationTest, MultiPredicatedLoad2) {
  ScopedObjectAccess soa(Thread::Current());
  VariableSizedHandleScope vshs(soa.Self());
  CreateGraph(/*handles=*/&vshs);
  AdjacencyListGraph blks(SetupFromAdjacencyList("entry",
                                                 "exit",
                                                 {{"entry", "left"},
                                                  {"entry", "right"},
                                                  {"left", "breturn"},
                                                  {"right", "breturn"},
                                                  {"breturn", "exit"}}));
#define GET_BLOCK(name) HBasicBlock* name = blks.Get(#name)
  GET_BLOCK(entry);
  GET_BLOCK(exit);
  GET_BLOCK(breturn);
  GET_BLOCK(left);
  GET_BLOCK(right);
#undef GET_BLOCK
  EnsurePredecessorOrder(breturn, {left, right});
  HInstruction* bool_value = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kBool);
  // HInstruction* null_const = graph_->GetNullConstant();
  HInstruction* c2 = graph_->GetIntConstant(2);
  HInstruction* c3 = graph_->GetIntConstant(3);
  HInstruction* c12 = graph_->GetIntConstant(12);
  HInstruction* c13 = graph_->GetIntConstant(13);
  HInstruction* cls1 = new (GetAllocator()) HLoadClass(graph_->GetCurrentMethod(),
                                                       dex::TypeIndex(10),
                                                       graph_->GetDexFile(),
                                                       ScopedNullHandle<mirror::Class>(),
                                                       false,
                                                       0,
                                                       false);
  HInstruction* cls2 = new (GetAllocator()) HLoadClass(graph_->GetCurrentMethod(),
                                                       dex::TypeIndex(20),
                                                       graph_->GetDexFile(),
                                                       ScopedNullHandle<mirror::Class>(),
                                                       false,
                                                       0,
                                                       false);
  HInstruction* new_inst1 =
      new (GetAllocator()) HNewInstance(cls1,
                                        0,
                                        dex::TypeIndex(10),
                                        graph_->GetDexFile(),
                                        false,
                                        QuickEntrypointEnum::kQuickAllocObjectInitialized);
  HInstruction* new_inst2 =
      new (GetAllocator()) HNewInstance(cls2,
                                        0,
                                        dex::TypeIndex(20),
                                        graph_->GetDexFile(),
                                        false,
                                        QuickEntrypointEnum::kQuickAllocObjectInitialized);
  HInstruction* write_entry1 = new (GetAllocator()) HInstanceFieldSet(new_inst1,
                                                                      c3,
                                                                      nullptr,
                                                                      DataType::Type::kInt32,
                                                                      MemberOffset(10),
                                                                      false,
                                                                      0,
                                                                      0,
                                                                      graph_->GetDexFile(),
                                                                      0);
  HInstruction* write_entry2 = new (GetAllocator()) HInstanceFieldSet(new_inst2,
                                                                      c13,
                                                                      nullptr,
                                                                      DataType::Type::kInt32,
                                                                      MemberOffset(10),
                                                                      false,
                                                                      0,
                                                                      0,
                                                                      graph_->GetDexFile(),
                                                                      0);
  HInstruction* if_inst = new (GetAllocator()) HIf(bool_value);
  entry->AddInstruction(bool_value);
  entry->AddInstruction(cls1);
  entry->AddInstruction(cls2);
  entry->AddInstruction(new_inst1);
  entry->AddInstruction(new_inst2);
  entry->AddInstruction(write_entry1);
  entry->AddInstruction(write_entry2);
  entry->AddInstruction(if_inst);
  ArenaVector<HInstruction*> current_locals({}, GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(cls1, &current_locals);
  cls2->CopyEnvironmentFrom(cls1->GetEnvironment());
  new_inst1->CopyEnvironmentFrom(cls1->GetEnvironment());
  new_inst2->CopyEnvironmentFrom(cls1->GetEnvironment());

  HInstruction* call_left1 = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            1,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* write_left2 = new (GetAllocator()) HInstanceFieldSet(new_inst2,
                                                                     c12,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* goto_left = new (GetAllocator()) HGoto();
  call_left1->AsInvoke()->SetRawInputAt(0, new_inst1);
  left->AddInstruction(call_left1);
  left->AddInstruction(write_left2);
  left->AddInstruction(goto_left);
  call_left1->CopyEnvironmentFrom(cls1->GetEnvironment());

  HInstruction* write_right1 = new (GetAllocator()) HInstanceFieldSet(new_inst1,
                                                                      c2,
                                                                      nullptr,
                                                                      DataType::Type::kInt32,
                                                                      MemberOffset(10),
                                                                      false,
                                                                      0,
                                                                      0,
                                                                      graph_->GetDexFile(),
                                                                      0);
  HInstruction* call_right2 = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            1,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* goto_right = new (GetAllocator()) HGoto();
  call_right2->AsInvoke()->SetRawInputAt(0, new_inst2);
  right->AddInstruction(write_right1);
  right->AddInstruction(call_right2);
  right->AddInstruction(goto_right);
  call_right2->CopyEnvironmentFrom(cls1->GetEnvironment());

  HInstruction* read_bottom1 = new (GetAllocator()) HInstanceFieldGet(new_inst1,
                                                                      nullptr,
                                                                      DataType::Type::kInt32,
                                                                      MemberOffset(10),
                                                                      false,
                                                                      0,
                                                                      0,
                                                                      graph_->GetDexFile(),
                                                                      0);
  HInstruction* read_bottom2 = new (GetAllocator()) HInstanceFieldGet(new_inst2,
                                                                      nullptr,
                                                                      DataType::Type::kInt32,
                                                                      MemberOffset(10),
                                                                      false,
                                                                      0,
                                                                      0,
                                                                      graph_->GetDexFile(),
                                                                      0);
  HInstruction* combine =
      new (GetAllocator()) HAdd(DataType::Type::kInt32, read_bottom1, read_bottom2);
  HInstruction* return_exit = new (GetAllocator()) HReturn(combine);
  breturn->AddInstruction(read_bottom1);
  breturn->AddInstruction(read_bottom2);
  breturn->AddInstruction(combine);
  breturn->AddInstruction(return_exit);

  HInstruction* exit_instruction = new (GetAllocator()) HExit();
  exit->AddInstruction(exit_instruction);
  // PerformLSE expects this to be empty.
  graph_->ClearDominanceInformation();
  LOG(INFO) << "Pre LSE " << blks;
  PerformLSEWithPartial();
  LOG(INFO) << "Post LSE " << blks;

  EXPECT_TRUE(IsRemoved(read_bottom1));
  EXPECT_TRUE(IsRemoved(read_bottom2));
  EXPECT_TRUE(IsRemoved(write_right1));
  EXPECT_TRUE(IsRemoved(write_left2));
  EXPECT_FALSE(IsRemoved(call_left1));
  EXPECT_FALSE(IsRemoved(call_right2));
  std::vector<HPhi*> merges;
  std::vector<HPredicatedInstanceFieldGet*> pred_gets;
  std::tie(merges, pred_gets) =
      FindAllInstructions<HPhi, HPredicatedInstanceFieldGet>(graph_, breturn);
  ASSERT_EQ(merges.size(), 4u);
  ASSERT_EQ(pred_gets.size(), 2u);
  HPhi* merge_value_return1 = *std::find_if(merges.begin(), merges.end(), [&](HPhi* p) {
    return p->GetType() == DataType::Type::kInt32 && p->InputAt(1) == c2;
  });
  HPhi* merge_value_return2 = *std::find_if(merges.begin(), merges.end(), [&](HPhi* p) {
    return p->GetType() == DataType::Type::kInt32 && p->InputAt(0) == c12;
  });
  HPhi* merge_alloc1 = *std::find_if(merges.begin(), merges.end(), [&](HPhi* p) {
    return p->GetType() == DataType::Type::kReference && p->InputAt(1) == graph_->GetNullConstant();
  });
  HPhi* merge_alloc2 = *std::find_if(merges.begin(), merges.end(), [&](HPhi* p) {
    return p->GetType() == DataType::Type::kReference && p->InputAt(0) == graph_->GetNullConstant();
  });
  HPredicatedInstanceFieldGet* pred_get1 =
      *std::find_if(pred_gets.begin(), pred_gets.end(), [&](HPredicatedInstanceFieldGet* pg) {
        return pg->InputAt(0) == merge_alloc1;
      });
  HPredicatedInstanceFieldGet* pred_get2 =
      *std::find_if(pred_gets.begin(), pred_gets.end(), [&](HPredicatedInstanceFieldGet* pg) {
        return pg->InputAt(0) == merge_alloc2;
      });
  ASSERT_NE(merge_alloc1, nullptr);
  EXPECT_TRUE(merge_alloc1->InputAt(0)->IsNewInstance()) << *merge_alloc1;
  EXPECT_INS_EQ(merge_alloc1->InputAt(0)->InputAt(0), cls1) << *merge_alloc1;
  EXPECT_INS_EQ(merge_alloc1->InputAt(1), graph_->GetNullConstant());
  ASSERT_NE(merge_alloc2, nullptr);
  EXPECT_TRUE(merge_alloc2->InputAt(1)->IsNewInstance()) << *merge_alloc2;
  EXPECT_INS_EQ(merge_alloc2->InputAt(1)->InputAt(0), cls2) << *merge_alloc2;
  EXPECT_INS_EQ(merge_alloc2->InputAt(0), graph_->GetNullConstant());
  ASSERT_NE(pred_get1, nullptr);
  EXPECT_INS_EQ(pred_get1->InputAt(0), merge_alloc1);
  EXPECT_INS_EQ(pred_get1->InputAt(1), merge_value_return1) << " pred-get is: " << *pred_get1;
  EXPECT_INS_EQ(merge_value_return1->InputAt(0), graph_->GetIntConstant(0))
      << " merge val is: " << *merge_value_return1;
  EXPECT_INS_EQ(merge_value_return1->InputAt(1), c2) << " merge val is: " << *merge_value_return1;
  ASSERT_NE(pred_get2, nullptr);
  EXPECT_INS_EQ(pred_get2->InputAt(0), merge_alloc2);
  EXPECT_INS_EQ(pred_get2->InputAt(1), merge_value_return2) << " pred-get is: " << *pred_get2;
  EXPECT_INS_EQ(merge_value_return2->InputAt(1), graph_->GetIntConstant(0))
      << " merge val is: " << *merge_value_return1;
  EXPECT_INS_EQ(merge_value_return2->InputAt(0), c12) << " merge val is: " << *merge_value_return1;
}

// Based on structure seen in `java.util.List
// java.util.Collections.checkedList(java.util.List, java.lang.Class)`
// Incorrect accounting would cause attempts to materialize both obj1 and obj2
// in each of the materialization blocks.
// // ENTRY
// Obj obj;
// if (param1) {
//   // needs to be moved after param2 check
//   obj1 = new Obj1();
//   obj1.foo = 33;
//   if (param2) {
//     return;
//   }
//   obj = obj1;
// } else {
//   obj2 = new Obj2();
//   obj2.foo = 44;
//   if (param2) {
//     return;
//   }
//   obj = obj2;
// }
// EXIT
// // obj = PHI[obj1, obj2]
// // NB The phi acts as an escape for both obj1 and obj2 meaning as far as the
// LSA is concerned the escape frontier is left_crit_break->breturn and
// right_crit_break->breturn for both even though only one of the objects is
// actually live at each edge.
// return obj.foo;
TEST_F(LoadStoreEliminationTest, MultiPredicatedLoad3) {
  ScopedObjectAccess soa(Thread::Current());
  VariableSizedHandleScope vshs(soa.Self());
  CreateGraph(/*handles=*/&vshs);
  AdjacencyListGraph blks(SetupFromAdjacencyList("entry",
                                                 "exit",
                                                 {{"entry", "left"},
                                                  {"left", "left_crit_break"},
                                                  {"left_crit_break", "breturn"},
                                                  {"left", "left_exit_early"},
                                                  {"left_exit_early", "exit"},
                                                  {"entry", "right"},
                                                  {"right", "right_crit_break"},
                                                  {"right_crit_break", "breturn"},
                                                  {"right", "right_exit_early"},
                                                  {"right_exit_early", "exit"},
                                                  {"breturn", "exit"}}));
#define GET_BLOCK(name) HBasicBlock* name = blks.Get(#name)
  GET_BLOCK(entry);
  GET_BLOCK(exit);
  GET_BLOCK(breturn);
  GET_BLOCK(left);
  GET_BLOCK(left_crit_break);
  GET_BLOCK(left_exit_early);
  GET_BLOCK(right);
  GET_BLOCK(right_crit_break);
  GET_BLOCK(right_exit_early);
#undef GET_BLOCK
  EnsurePredecessorOrder(breturn, {left_crit_break, right_crit_break});
  HInstruction* param1 = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kBool);
  HInstruction* param2 = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 2, DataType::Type::kBool);
  // HInstruction* null_const = graph_->GetNullConstant();
  HInstruction* c33 = graph_->GetIntConstant(33);
  HInstruction* c44 = graph_->GetIntConstant(44);
  HInstruction* if_inst = new (GetAllocator()) HIf(param1);
  entry->AddInstruction(param1);
  entry->AddInstruction(param2);
  entry->AddInstruction(if_inst);

  HInstruction* cls1 = new (GetAllocator()) HLoadClass(graph_->GetCurrentMethod(),
                                                       dex::TypeIndex(10),
                                                       graph_->GetDexFile(),
                                                       ScopedNullHandle<mirror::Class>(),
                                                       false,
                                                       0,
                                                       false);
  HInstruction* new_inst1 =
      new (GetAllocator()) HNewInstance(cls1,
                                        0,
                                        dex::TypeIndex(10),
                                        graph_->GetDexFile(),
                                        false,
                                        QuickEntrypointEnum::kQuickAllocObjectInitialized);

  HInstruction* write1 = new (GetAllocator()) HInstanceFieldSet(new_inst1,
                                                                c33,
                                                                nullptr,
                                                                DataType::Type::kInt32,
                                                                MemberOffset(10),
                                                                false,
                                                                0,
                                                                0,
                                                                graph_->GetDexFile(),
                                                                0);
  HInstruction* if_left = new (GetAllocator()) HIf(param2);
  left->AddInstruction(cls1);
  left->AddInstruction(new_inst1);
  left->AddInstruction(write1);
  left->AddInstruction(if_left);
  ArenaVector<HInstruction*> current_locals({}, GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(cls1, &current_locals);
  new_inst1->CopyEnvironmentFrom(cls1->GetEnvironment());

  left_crit_break->AddInstruction(new (GetAllocator()) HGoto());

  HInstruction* early_exit_left_read =
      new (GetAllocator()) HInstanceFieldGet(new_inst1,
                                             nullptr,
                                             DataType::Type::kInt32,
                                             MemberOffset(10),
                                             false,
                                             0,
                                             0,
                                             graph_->GetDexFile(),
                                             0);
  HInstruction* early_exit_left_return = new (GetAllocator()) HReturn(early_exit_left_read);
  left_exit_early->AddInstruction(early_exit_left_read);
  left_exit_early->AddInstruction(early_exit_left_return);

  HInstruction* cls2 = new (GetAllocator()) HLoadClass(graph_->GetCurrentMethod(),
                                                       dex::TypeIndex(20),
                                                       graph_->GetDexFile(),
                                                       ScopedNullHandle<mirror::Class>(),
                                                       false,
                                                       0,
                                                       false);
  HInstruction* new_inst2 =
      new (GetAllocator()) HNewInstance(cls2,
                                        0,
                                        dex::TypeIndex(20),
                                        graph_->GetDexFile(),
                                        false,
                                        QuickEntrypointEnum::kQuickAllocObjectInitialized);

  HInstruction* write2 = new (GetAllocator()) HInstanceFieldSet(new_inst2,
                                                                c44,
                                                                nullptr,
                                                                DataType::Type::kInt32,
                                                                MemberOffset(10),
                                                                false,
                                                                0,
                                                                0,
                                                                graph_->GetDexFile(),
                                                                0);
  HInstruction* if_right = new (GetAllocator()) HIf(param2);
  right->AddInstruction(cls2);
  right->AddInstruction(new_inst2);
  right->AddInstruction(write2);
  right->AddInstruction(if_right);
  cls2->CopyEnvironmentFrom(cls1->GetEnvironment());
  new_inst2->CopyEnvironmentFrom(cls2->GetEnvironment());

  right_crit_break->AddInstruction(new (GetAllocator()) HGoto());

  HInstruction* early_exit_right_read =
      new (GetAllocator()) HInstanceFieldGet(new_inst2,
                                             nullptr,
                                             DataType::Type::kInt32,
                                             MemberOffset(10),
                                             false,
                                             0,
                                             0,
                                             graph_->GetDexFile(),
                                             0);
  HInstruction* early_exit_right_return = new (GetAllocator()) HReturn(early_exit_right_read);
  right_exit_early->AddInstruction(early_exit_right_read);
  right_exit_early->AddInstruction(early_exit_right_return);

  HPhi* bottom_phi =
      new (GetAllocator()) HPhi(GetAllocator(), kNoRegNumber, 2, DataType::Type::kReference);
  bottom_phi->SetRawInputAt(0, new_inst1);
  bottom_phi->SetRawInputAt(1, new_inst2);
  HInstruction* read_bottom = new (GetAllocator()) HInstanceFieldGet(bottom_phi,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* return_exit = new (GetAllocator()) HReturn(read_bottom);
  breturn->AddPhi(bottom_phi);
  breturn->AddInstruction(read_bottom);
  breturn->AddInstruction(return_exit);

  HInstruction* exit_instruction = new (GetAllocator()) HExit();
  exit->AddInstruction(exit_instruction);
  // PerformLSE expects this to be empty.
  graph_->ClearDominanceInformation();
  LOG(INFO) << "Pre LSE " << blks;
  PerformLSEWithPartial();
  LOG(INFO) << "Post LSE " << blks;

  EXPECT_INS_REMOVED(early_exit_left_read);
  EXPECT_INS_REMOVED(early_exit_right_read);
  EXPECT_INS_RETAINED(bottom_phi);
  EXPECT_INS_RETAINED(read_bottom);
  EXPECT_INS_EQ(early_exit_left_return->InputAt(0), c33);
  EXPECT_INS_EQ(early_exit_right_return->InputAt(0), c44);
  // These assert there is only 1 HNewInstance in the given blocks.
  HNewInstance* moved_ni1 =
      FindSingleInstruction<HNewInstance>(graph_, left_crit_break->GetSinglePredecessor());
  HNewInstance* moved_ni2 =
      FindSingleInstruction<HNewInstance>(graph_, right_crit_break->GetSinglePredecessor());
  ASSERT_NE(moved_ni1, nullptr);
  ASSERT_NE(moved_ni2, nullptr);
  EXPECT_INS_EQ(bottom_phi->InputAt(0), moved_ni1);
  EXPECT_INS_EQ(bottom_phi->InputAt(1), moved_ni2);
}


// Based on structure seen in `java.util.Set java.util.Collections$UnmodifiableMap.entrySet()`
// We end up having to update a PHI generated by normal LSE.
// // ENTRY
// Obj obj_init = param_obj.BAR;
// if (param1) {
//   Obj other = new Obj();
//   other.foo = 42;
//   if (param2) {
//     return other.foo;
//   } else {
//     param1.BAR = other;
//   }
// } else { }
// EXIT
// LSE Turns this into PHI[obj_init, other]
// final_read = param1.BAR;
// // won't be changed. The escape happens with .BAR set so this is in escaping cohort.
// return final_read.foo;
TEST_F(LoadStoreEliminationTest, MultiPredicatedLoad4) {
  ScopedObjectAccess soa(Thread::Current());
  VariableSizedHandleScope vshs(soa.Self());
  CreateGraph(/*handles=*/&vshs);
  AdjacencyListGraph blks(SetupFromAdjacencyList("entry",
                                                 "exit",
                                                 {{"entry", "left"},
                                                  {"left", "left_early_return"},
                                                  {"left_early_return", "exit"},
                                                  {"left", "left_write_escape"},
                                                  {"left_write_escape", "breturn"},
                                                  {"entry", "right"},
                                                  {"right", "breturn"},
                                                  {"breturn", "exit"}}));
#define GET_BLOCK(name) HBasicBlock* name = blks.Get(#name)
  GET_BLOCK(entry);
  GET_BLOCK(exit);
  GET_BLOCK(breturn);
  GET_BLOCK(left);
  GET_BLOCK(left_early_return);
  GET_BLOCK(left_write_escape);
  GET_BLOCK(right);
#undef GET_BLOCK
  MemberOffset foo_offset = MemberOffset(10);
  MemberOffset bar_offset = MemberOffset(20);
  EnsurePredecessorOrder(breturn, {left_write_escape, right});
  HInstruction* c42 = graph_->GetIntConstant(42);
  HInstruction* param1 = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kBool);
  HInstruction* param2 = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 2, DataType::Type::kBool);
  HInstruction* param_obj = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(2), 3, DataType::Type::kReference);
  HInstruction* get_initial = new (GetAllocator()) HInstanceFieldGet(param_obj, nullptr, DataType::Type::kReference, bar_offset, false, 0, 0, graph_->GetDexFile(), 0);
  HInstruction* if_inst = new (GetAllocator()) HIf(param1);
  entry->AddInstruction(param1);
  entry->AddInstruction(param2);
  entry->AddInstruction(param_obj);
  entry->AddInstruction(get_initial);
  entry->AddInstruction(if_inst);

  HInstruction* cls1 = new (GetAllocator()) HLoadClass(graph_->GetCurrentMethod(),
                                                       dex::TypeIndex(10),
                                                       graph_->GetDexFile(),
                                                       ScopedNullHandle<mirror::Class>(),
                                                       false,
                                                       0,
                                                       false);
  HInstruction* new_inst1 =
      new (GetAllocator()) HNewInstance(cls1,
                                        0,
                                        dex::TypeIndex(10),
                                        graph_->GetDexFile(),
                                        false,
                                        QuickEntrypointEnum::kQuickAllocObjectInitialized);

  HInstruction* write1 = new (GetAllocator()) HInstanceFieldSet(new_inst1,
                                                                c42,
                                                                nullptr,
                                                                DataType::Type::kInt32,
                                                                foo_offset,
                                                                false,
                                                                0,
                                                                0,
                                                                graph_->GetDexFile(),
                                                                0);
  HInstruction* if_left = new (GetAllocator()) HIf(param2);
  left->AddInstruction(cls1);
  left->AddInstruction(new_inst1);
  left->AddInstruction(write1);
  left->AddInstruction(if_left);
  ArenaVector<HInstruction*> current_locals({}, GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(cls1, &current_locals);
  new_inst1->CopyEnvironmentFrom(cls1->GetEnvironment());

  HInstruction* read_early_return = new (GetAllocator()) HInstanceFieldGet(new_inst1, nullptr, DataType::Type::kInt32, foo_offset, false, 0, 0, graph_->GetDexFile(), 0);
  HInstruction* return_early = new (GetAllocator()) HReturn(read_early_return);
  left_early_return->AddInstruction(read_early_return);
  left_early_return->AddInstruction(return_early);

  HInstruction* write_escape = new (GetAllocator()) HInstanceFieldSet(param_obj, new_inst1, nullptr, DataType::Type::kReference, bar_offset, false, 0, 0, graph_->GetDexFile(), 0);
  HInstruction* write_goto = new (GetAllocator()) HGoto();
  left_write_escape->AddInstruction(write_escape);
  left_write_escape->AddInstruction(write_goto);

  right->AddInstruction(new (GetAllocator()) HGoto());

  HInstruction* read_bottom = new (GetAllocator()) HInstanceFieldGet(param_obj,
                                                                     nullptr,
                                                                     DataType::Type::kReference,
                                                                     bar_offset,
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* final_read = new (GetAllocator()) HInstanceFieldGet(read_bottom,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     foo_offset,
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* return_exit = new (GetAllocator()) HReturn(final_read);
  breturn->AddInstruction(read_bottom);
  breturn->AddInstruction(final_read);
  breturn->AddInstruction(return_exit);

  HInstruction* exit_instruction = new (GetAllocator()) HExit();
  exit->AddInstruction(exit_instruction);
  // PerformLSE expects this to be empty.
  graph_->ClearDominanceInformation();
  LOG(INFO) << "Pre LSE " << blks;
  PerformLSEWithPartial();
  LOG(INFO) << "Post LSE " << blks;

  EXPECT_INS_REMOVED(read_bottom);
  EXPECT_INS_REMOVED(read_early_return);
  EXPECT_INS_EQ(return_early->InputAt(0), c42);
  EXPECT_INS_RETAINED(final_read);
  HNewInstance* moved_ni = FindSingleInstruction<HNewInstance>(graph_, left_write_escape->GetSinglePredecessor());
  EXPECT_TRUE(final_read->InputAt(0)->IsPhi());
  EXPECT_INS_EQ(final_read->InputAt(0)->InputAt(0), moved_ni);
  EXPECT_INS_EQ(final_read->InputAt(0)->InputAt(1), get_initial);
}

// // ENTRY
// obj = new Obj();
// obj.field = 3;
// if (parameter_value) {
//   // LEFT
//   escape(obj);
// } else {
//   // RIGHT
//   // ELIMINATE
//   obj.field = 2;
// }
// // MERGE
// if (second_param) {
//   // NON_ESCAPE
//   obj.field = 1;
//   noescape();
// }
// EXIT
// predicated-ELIMINATE
// return obj.field
TEST_F(LoadStoreEliminationTest, PredicatedLoad2) {
  ScopedObjectAccess soa(Thread::Current());
  VariableSizedHandleScope vshs(soa.Self());
  CreateGraph(/*handles=*/&vshs);
  AdjacencyListGraph blks(SetupFromAdjacencyList("entry",
                                                 "exit",
                                                 {{"entry", "left"},
                                                  {"entry", "right"},
                                                  {"left", "merge"},
                                                  {"right", "merge"},
                                                  {"merge", "non_escape"},
                                                  {"non_escape", "breturn"},
                                                  {"merge", "breturn"},
                                                  {"breturn", "exit"}}));
#define GET_BLOCK(name) HBasicBlock* name = blks.Get(#name)
  GET_BLOCK(entry);
  GET_BLOCK(exit);
  GET_BLOCK(breturn);
  GET_BLOCK(left);
  GET_BLOCK(right);
  GET_BLOCK(merge);
  GET_BLOCK(non_escape);
#undef GET_BLOCK
  EnsurePredecessorOrder(merge, {left, right});
  EnsurePredecessorOrder(breturn, {merge, non_escape});
  HInstruction* bool_value = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kBool);
  HInstruction* bool_value2 = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 2, DataType::Type::kBool);
  HInstruction* null_const = graph_->GetNullConstant();
  HInstruction* c1 = graph_->GetIntConstant(3);
  HInstruction* c2 = graph_->GetIntConstant(2);
  HInstruction* c3 = graph_->GetIntConstant(3);
  HInstruction* cls = new (GetAllocator()) HLoadClass(graph_->GetCurrentMethod(),
                                                      dex::TypeIndex(10),
                                                      graph_->GetDexFile(),
                                                      ScopedNullHandle<mirror::Class>(),
                                                      false,
                                                      0,
                                                      false);
  HInstruction* new_inst =
      new (GetAllocator()) HNewInstance(cls,
                                        0,
                                        dex::TypeIndex(10),
                                        graph_->GetDexFile(),
                                        false,
                                        QuickEntrypointEnum::kQuickAllocObjectInitialized);
  HInstruction* write_entry = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                     c3,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* if_inst = new (GetAllocator()) HIf(bool_value);
  entry->AddInstruction(bool_value);
  entry->AddInstruction(bool_value2);
  entry->AddInstruction(cls);
  entry->AddInstruction(new_inst);
  entry->AddInstruction(write_entry);
  entry->AddInstruction(if_inst);
  ArenaVector<HInstruction*> current_locals({}, GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(cls, &current_locals);
  new_inst->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* call_left = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            1,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* goto_left = new (GetAllocator()) HGoto();
  call_left->AsInvoke()->SetRawInputAt(0, new_inst);
  left->AddInstruction(call_left);
  left->AddInstruction(goto_left);
  call_left->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* write_right = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                     c2,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* goto_right = new (GetAllocator()) HGoto();
  right->AddInstruction(write_right);
  right->AddInstruction(goto_right);

  HInstruction* merge_if = new (GetAllocator()) HIf(bool_value2);
  merge->AddInstruction(merge_if);

  HInstruction* write_non_escape = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                          c1,
                                                                          nullptr,
                                                                          DataType::Type::kInt32,
                                                                          MemberOffset(10),
                                                                          false,
                                                                          0,
                                                                          0,
                                                                          graph_->GetDexFile(),
                                                                          0);
  HInstruction* non_escape_call = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            0,
                            DataType::Type::kVoid,
                            0u,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* non_escape_goto = new (GetAllocator()) HGoto();
  non_escape->AddInstruction(write_non_escape);
  non_escape->AddInstruction(non_escape_call);
  non_escape->AddInstruction(non_escape_goto);
  non_escape_call->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* read_bottom = new (GetAllocator()) HInstanceFieldGet(new_inst,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* return_exit = new (GetAllocator()) HReturn(read_bottom);
  breturn->AddInstruction(read_bottom);
  breturn->AddInstruction(return_exit);

  HInstruction* exit_instruction = new (GetAllocator()) HExit();
  exit->AddInstruction(exit_instruction);
  // PerformLSE expects this to be empty.
  graph_->ClearDominanceInformation();
  LOG(INFO) << "Pre LSE " << blks;
  PerformLSEWithPartial();
  LOG(INFO) << "Post LSE " << blks;

  EXPECT_TRUE(IsRemoved(read_bottom));
  EXPECT_TRUE(IsRemoved(write_right));
  EXPECT_FALSE(IsRemoved(call_left));
  std::vector<HPhi*> merges;
  HPredicatedInstanceFieldGet* pred_get =
      FindSingleInstruction<HPredicatedInstanceFieldGet>(graph_, breturn);
  std::tie(merges) = FindAllInstructions<HPhi>(graph_);
  ASSERT_EQ(merges.size(), 3u);
  HPhi* merge_value_return = *std::find_if(merges.begin(), merges.end(), [&](HPhi* p) {
    return p->GetType() == DataType::Type::kInt32 && p->GetBlock() == breturn;
  });
  HPhi* merge_value_merge = *std::find_if(merges.begin(), merges.end(), [&](HPhi* p) {
    return p->GetType() == DataType::Type::kInt32 && p->GetBlock() != breturn;
  });
  HPhi* merge_alloc = *std::find_if(merges.begin(), merges.end(), [](HPhi* p) {
    return p->GetType() == DataType::Type::kReference;
  });
  ASSERT_NE(merge_alloc, nullptr);
  EXPECT_TRUE(merge_alloc->InputAt(0)->IsNewInstance()) << *merge_alloc;
  EXPECT_INS_EQ(merge_alloc->InputAt(0)->InputAt(0), cls) << " phi is: " << *merge_alloc;
  EXPECT_INS_EQ(merge_alloc->InputAt(1), null_const);
  ASSERT_NE(pred_get, nullptr);
  EXPECT_INS_EQ(pred_get->InputAt(0), merge_alloc);
  EXPECT_INS_EQ(pred_get->InputAt(1), merge_value_return) << "get is " << *pred_get;
  EXPECT_INS_EQ(merge_value_return->InputAt(0), merge_value_merge)
      << " phi is: " << *merge_value_return;
  EXPECT_INS_EQ(merge_value_return->InputAt(1), c3) << " phi is: " << *merge_value_return;
  EXPECT_INS_EQ(merge_value_merge->InputAt(0), graph_->GetIntConstant(0))
      << " phi is: " << *merge_value_merge;
  EXPECT_INS_EQ(merge_value_merge->InputAt(1), c2) << " phi is: " << *merge_value_merge;
}

// // ENTRY
// obj = new Obj();
// // ALL should be kept
// switch (parameter_value) {
//   case 1:
//     // Case1
//     obj.field = 1;
//     call_func(obj);
//     break;
//   case 2:
//     // Case2
//     obj.field = 2;
//     call_func(obj);
//     // We don't know what obj.field is now we aren't able to eliminate the read below!
//     break;
//   default:
//     // Case3
//     obj.field = 3;
//     do {
//       if (test2()) { } else { obj.field = 5; }
//     } while (test());
//     break;
// }
// EXIT
// return obj.field
TEST_F(LoadStoreEliminationTest, PartialLoopPhis1) {
  ScopedObjectAccess soa(Thread::Current());
  VariableSizedHandleScope vshs(soa.Self());
  CreateGraph(/*handles=*/&vshs);
  AdjacencyListGraph blks(SetupFromAdjacencyList("entry",
                                                 "exit",
                                                 {{"entry", "bswitch"},
                                                  {"bswitch", "case1"},
                                                  {"bswitch", "case2"},
                                                  {"bswitch", "case3"},
                                                  {"case1", "breturn"},
                                                  {"case2", "breturn"},
                                                  {"case3", "loop_pre_header"},
                                                  {"loop_pre_header", "loop_header"},
                                                  {"loop_header", "loop_body"},
                                                  {"loop_body", "loop_if_left"},
                                                  {"loop_body", "loop_if_right"},
                                                  {"loop_if_left", "loop_merge"},
                                                  {"loop_if_right", "loop_merge"},
                                                  {"loop_merge", "loop_end"},
                                                  {"loop_end", "loop_header"},
                                                  {"loop_end", "critical_break"},
                                                  {"critical_break", "breturn"},
                                                  {"breturn", "exit"}}));
#define GET_BLOCK(name) HBasicBlock* name = blks.Get(#name)
  GET_BLOCK(entry);
  GET_BLOCK(bswitch);
  GET_BLOCK(exit);
  GET_BLOCK(breturn);
  GET_BLOCK(case1);
  GET_BLOCK(case2);
  GET_BLOCK(case3);

  GET_BLOCK(loop_pre_header);
  GET_BLOCK(loop_header);
  GET_BLOCK(loop_body);
  GET_BLOCK(loop_if_left);
  GET_BLOCK(loop_if_right);
  GET_BLOCK(loop_merge);
  GET_BLOCK(loop_end);
  GET_BLOCK(critical_break);
#undef GET_BLOCK
  EnsurePredecessorOrder(breturn, {case1, case2, critical_break});
  EnsurePredecessorOrder(loop_header, {loop_pre_header, loop_end});
  EnsurePredecessorOrder(loop_merge, {loop_if_left, loop_if_right});
  CHECK_SUBROUTINE_FAILURE();
  HInstruction* switch_val = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kInt32);
  HInstruction* c1 = graph_->GetIntConstant(1);
  HInstruction* c2 = graph_->GetIntConstant(2);
  HInstruction* c3 = graph_->GetIntConstant(3);
  HInstruction* c5 = graph_->GetIntConstant(5);
  HInstruction* cls = new (GetAllocator()) HLoadClass(graph_->GetCurrentMethod(),
                                                      dex::TypeIndex(10),
                                                      graph_->GetDexFile(),
                                                      ScopedNullHandle<mirror::Class>(),
                                                      false,
                                                      0,
                                                      false);
  HInstruction* new_inst =
      new (GetAllocator()) HNewInstance(cls,
                                        0,
                                        dex::TypeIndex(10),
                                        graph_->GetDexFile(),
                                        false,
                                        QuickEntrypointEnum::kQuickAllocObjectInitialized);
  HInstruction* entry_goto = new (GetAllocator()) HGoto();
  entry->AddInstruction(switch_val);
  entry->AddInstruction(cls);
  entry->AddInstruction(new_inst);
  entry->AddInstruction(entry_goto);
  ArenaVector<HInstruction*> current_locals({}, GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(cls, &current_locals);
  new_inst->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* switch_inst = new (GetAllocator()) HPackedSwitch(0, 2, switch_val);
  bswitch->AddInstruction(switch_inst);

  HInstruction* write_c1 = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                  c1,
                                                                  nullptr,
                                                                  DataType::Type::kInt32,
                                                                  MemberOffset(10),
                                                                  false,
                                                                  0,
                                                                  0,
                                                                  graph_->GetDexFile(),
                                                                  0);
  HInstruction* call_c1 = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            1,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* goto_c1 = new (GetAllocator()) HGoto();
  call_c1->AsInvoke()->SetRawInputAt(0, new_inst);
  case1->AddInstruction(write_c1);
  case1->AddInstruction(call_c1);
  case1->AddInstruction(goto_c1);
  call_c1->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* write_c2 = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                  c2,
                                                                  nullptr,
                                                                  DataType::Type::kInt32,
                                                                  MemberOffset(10),
                                                                  false,
                                                                  0,
                                                                  0,
                                                                  graph_->GetDexFile(),
                                                                  0);
  HInstruction* call_c2 = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            1,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* goto_c2 = new (GetAllocator()) HGoto();
  call_c2->AsInvoke()->SetRawInputAt(0, new_inst);
  case2->AddInstruction(write_c2);
  case2->AddInstruction(call_c2);
  case2->AddInstruction(goto_c2);
  call_c2->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* write_c3 = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                  c3,
                                                                  nullptr,
                                                                  DataType::Type::kInt32,
                                                                  MemberOffset(10),
                                                                  false,
                                                                  0,
                                                                  0,
                                                                  graph_->GetDexFile(),
                                                                  0);
  HInstruction* goto_c3 = new (GetAllocator()) HGoto();
  case3->AddInstruction(write_c3);
  case3->AddInstruction(goto_c3);

  HInstruction* goto_preheader = new (GetAllocator()) HGoto();
  loop_pre_header->AddInstruction(goto_preheader);

  HInstruction* suspend_check_header = new (GetAllocator()) HSuspendCheck();
  HInstruction* goto_header = new (GetAllocator()) HGoto();
  loop_header->AddInstruction(suspend_check_header);
  loop_header->AddInstruction(goto_header);
  suspend_check_header->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* call_loop_body = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            0,
                            DataType::Type::kBool,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* if_loop_body = new (GetAllocator()) HIf(call_loop_body);
  loop_body->AddInstruction(call_loop_body);
  loop_body->AddInstruction(if_loop_body);
  call_loop_body->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* goto_loop_left = new (GetAllocator()) HGoto();
  loop_if_left->AddInstruction(goto_loop_left);

  HInstruction* write_loop_right = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                          c5,
                                                                          nullptr,
                                                                          DataType::Type::kInt32,
                                                                          MemberOffset(10),
                                                                          false,
                                                                          0,
                                                                          0,
                                                                          graph_->GetDexFile(),
                                                                          0);
  HInstruction* goto_loop_right = new (GetAllocator()) HGoto();
  loop_if_right->AddInstruction(write_loop_right);
  loop_if_right->AddInstruction(goto_loop_right);

  HInstruction* goto_loop_merge = new (GetAllocator()) HGoto();
  loop_merge->AddInstruction(goto_loop_merge);

  HInstruction* call_end = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            0,
                            DataType::Type::kBool,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* if_end = new (GetAllocator()) HIf(call_end);
  loop_end->AddInstruction(call_end);
  loop_end->AddInstruction(if_end);
  call_end->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* goto_critical_break = new (GetAllocator()) HGoto();
  critical_break->AddInstruction(goto_critical_break);

  HInstruction* read_bottom = new (GetAllocator()) HInstanceFieldGet(new_inst,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* return_exit = new (GetAllocator()) HReturn(read_bottom);
  breturn->AddInstruction(read_bottom);
  breturn->AddInstruction(return_exit);

  HInstruction* exit_ins = new (GetAllocator()) HExit();
  exit->AddInstruction(exit_ins);
  // PerformLSE expects this to be empty.
  graph_->ClearDominanceInformation();
  LOG(INFO) << "Pre LSE " << blks;
  PerformLSEWithPartial();
  LOG(INFO) << "Post LSE " << blks;

  HPredicatedInstanceFieldGet* pred_get =
      FindSingleInstruction<HPredicatedInstanceFieldGet>(graph_, breturn);
  EXPECT_TRUE(IsRemoved(read_bottom)) << *read_bottom;
  ASSERT_TRUE(pred_get != nullptr);
  HPhi* inst_return_phi = pred_get->InputAt(0)->AsPhi();
  ASSERT_TRUE(inst_return_phi != nullptr) << *pred_get->InputAt(0);
  EXPECT_INS_EQ(inst_return_phi->InputAt(0),
                FindSingleInstruction<HNewInstance>(graph_, case1->GetSinglePredecessor()));
  EXPECT_INS_EQ(inst_return_phi->InputAt(1),
                FindSingleInstruction<HNewInstance>(graph_, case2->GetSinglePredecessor()));
  EXPECT_INS_EQ(inst_return_phi->InputAt(2), graph_->GetNullConstant());
  HPhi* inst_value_phi = pred_get->InputAt(1)->AsPhi();
  ASSERT_TRUE(inst_value_phi != nullptr) << *pred_get->InputAt(1);
  EXPECT_INS_EQ(inst_value_phi->InputAt(0), graph_->GetIntConstant(0));
  EXPECT_INS_EQ(inst_value_phi->InputAt(1), graph_->GetIntConstant(0));
  HPhi* loop_merge_phi = FindSingleInstruction<HPhi>(graph_, loop_merge);
  ASSERT_TRUE(loop_merge_phi != nullptr);
  HPhi* loop_header_phi = FindSingleInstruction<HPhi>(graph_, loop_header);
  ASSERT_TRUE(loop_header_phi != nullptr);
  EXPECT_INS_EQ(loop_header_phi->InputAt(0), c3);
  EXPECT_INS_EQ(loop_header_phi->InputAt(1), loop_merge_phi);
  EXPECT_INS_EQ(loop_merge_phi->InputAt(0), loop_header_phi);
  EXPECT_INS_EQ(loop_merge_phi->InputAt(1), c5);
  EXPECT_INS_EQ(inst_value_phi->InputAt(2), loop_merge_phi);
  EXPECT_FALSE(IsRemoved(write_c1)) << *write_c1;
  EXPECT_FALSE(IsRemoved(write_c2)) << *write_c2;
  EXPECT_TRUE(IsRemoved(write_c3)) << *write_c3;
  EXPECT_TRUE(IsRemoved(write_loop_right)) << *write_loop_right;
}

// // ENTRY
// obj = new Obj();
// switch (parameter_value) {
//   case 1:
//     // Case1
//     obj.field = 1;
//     call_func(obj);
//     break;
//   case 2:
//     // Case2
//     obj.field = 2;
//     call_func(obj);
//     // We don't know what obj.field is now we aren't able to eliminate the read below!
//     break;
//   default:
//     // Case3
//     obj.field = 3;
//     while (true) {
//       if (test()) { break; }
//       if (test2()) { } else { obj.field = 5; }
//     }
//     break;
// }
// EXIT
// return obj.field
TEST_F(LoadStoreEliminationTest, PartialLoopPhis2) {
  ScopedObjectAccess soa(Thread::Current());
  VariableSizedHandleScope vshs(soa.Self());
  CreateGraph(/*handles=*/&vshs);
  AdjacencyListGraph blks(SetupFromAdjacencyList("entry",
                                                 "exit",
                                                 {{"entry", "bswitch"},
                                                  {"bswitch", "case1"},
                                                  {"bswitch", "case2"},
                                                  {"bswitch", "case3"},
                                                  {"case1", "breturn"},
                                                  {"case2", "breturn"},
                                                  {"case3", "loop_pre_header"},

                                                  {"loop_pre_header", "loop_header"},
                                                  {"loop_header", "critical_break"},
                                                  {"loop_header", "loop_body"},
                                                  {"loop_body", "loop_if_left"},
                                                  {"loop_body", "loop_if_right"},
                                                  {"loop_if_left", "loop_merge"},
                                                  {"loop_if_right", "loop_merge"},
                                                  {"loop_merge", "loop_header"},

                                                  {"critical_break", "breturn"},
                                                  {"breturn", "exit"}}));
#define GET_BLOCK(name) HBasicBlock* name = blks.Get(#name)
  GET_BLOCK(entry);
  GET_BLOCK(bswitch);
  GET_BLOCK(exit);
  GET_BLOCK(breturn);
  GET_BLOCK(case1);
  GET_BLOCK(case2);
  GET_BLOCK(case3);

  GET_BLOCK(loop_pre_header);
  GET_BLOCK(loop_header);
  GET_BLOCK(loop_body);
  GET_BLOCK(loop_if_left);
  GET_BLOCK(loop_if_right);
  GET_BLOCK(loop_merge);
  GET_BLOCK(critical_break);
#undef GET_BLOCK
  EnsurePredecessorOrder(breturn, {case1, case2, critical_break});
  EnsurePredecessorOrder(loop_header, {loop_pre_header, loop_merge});
  EnsurePredecessorOrder(loop_merge, {loop_if_left, loop_if_right});
  CHECK_SUBROUTINE_FAILURE();
  HInstruction* switch_val = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kInt32);
  HInstruction* c1 = graph_->GetIntConstant(1);
  HInstruction* c2 = graph_->GetIntConstant(2);
  HInstruction* c3 = graph_->GetIntConstant(3);
  HInstruction* c5 = graph_->GetIntConstant(5);
  HInstruction* cls = new (GetAllocator()) HLoadClass(graph_->GetCurrentMethod(),
                                                      dex::TypeIndex(10),
                                                      graph_->GetDexFile(),
                                                      ScopedNullHandle<mirror::Class>(),
                                                      false,
                                                      0,
                                                      false);
  HInstruction* new_inst =
      new (GetAllocator()) HNewInstance(cls,
                                        0,
                                        dex::TypeIndex(10),
                                        graph_->GetDexFile(),
                                        false,
                                        QuickEntrypointEnum::kQuickAllocObjectInitialized);
  HInstruction* entry_goto = new (GetAllocator()) HGoto();
  entry->AddInstruction(switch_val);
  entry->AddInstruction(cls);
  entry->AddInstruction(new_inst);
  entry->AddInstruction(entry_goto);
  ArenaVector<HInstruction*> current_locals({}, GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(cls, &current_locals);
  new_inst->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* switch_inst = new (GetAllocator()) HPackedSwitch(0, 2, switch_val);
  bswitch->AddInstruction(switch_inst);

  HInstruction* write_c1 = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                  c1,
                                                                  nullptr,
                                                                  DataType::Type::kInt32,
                                                                  MemberOffset(10),
                                                                  false,
                                                                  0,
                                                                  0,
                                                                  graph_->GetDexFile(),
                                                                  0);
  HInstruction* call_c1 = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            1,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* goto_c1 = new (GetAllocator()) HGoto();
  call_c1->AsInvoke()->SetRawInputAt(0, new_inst);
  case1->AddInstruction(write_c1);
  case1->AddInstruction(call_c1);
  case1->AddInstruction(goto_c1);
  call_c1->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* write_c2 = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                  c2,
                                                                  nullptr,
                                                                  DataType::Type::kInt32,
                                                                  MemberOffset(10),
                                                                  false,
                                                                  0,
                                                                  0,
                                                                  graph_->GetDexFile(),
                                                                  0);
  HInstruction* call_c2 = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            1,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* goto_c2 = new (GetAllocator()) HGoto();
  call_c2->AsInvoke()->SetRawInputAt(0, new_inst);
  case2->AddInstruction(write_c2);
  case2->AddInstruction(call_c2);
  case2->AddInstruction(goto_c2);
  call_c2->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* write_c3 = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                  c3,
                                                                  nullptr,
                                                                  DataType::Type::kInt32,
                                                                  MemberOffset(10),
                                                                  false,
                                                                  0,
                                                                  0,
                                                                  graph_->GetDexFile(),
                                                                  0);
  HInstruction* goto_c3 = new (GetAllocator()) HGoto();
  case3->AddInstruction(write_c3);
  case3->AddInstruction(goto_c3);

  HInstruction* goto_preheader = new (GetAllocator()) HGoto();
  loop_pre_header->AddInstruction(goto_preheader);

  HInstruction* suspend_check_header = new (GetAllocator()) HSuspendCheck();
  HInstruction* call_header = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            0,
                            DataType::Type::kBool,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* if_header = new (GetAllocator()) HIf(call_header);
  loop_header->AddInstruction(suspend_check_header);
  loop_header->AddInstruction(call_header);
  loop_header->AddInstruction(if_header);
  call_header->CopyEnvironmentFrom(cls->GetEnvironment());
  suspend_check_header->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* call_loop_body = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            0,
                            DataType::Type::kBool,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* if_loop_body = new (GetAllocator()) HIf(call_loop_body);
  loop_body->AddInstruction(call_loop_body);
  loop_body->AddInstruction(if_loop_body);
  call_loop_body->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* goto_loop_left = new (GetAllocator()) HGoto();
  loop_if_left->AddInstruction(goto_loop_left);

  HInstruction* write_loop_right = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                          c5,
                                                                          nullptr,
                                                                          DataType::Type::kInt32,
                                                                          MemberOffset(10),
                                                                          false,
                                                                          0,
                                                                          0,
                                                                          graph_->GetDexFile(),
                                                                          0);
  HInstruction* goto_loop_right = new (GetAllocator()) HGoto();
  loop_if_right->AddInstruction(write_loop_right);
  loop_if_right->AddInstruction(goto_loop_right);

  HInstruction* goto_loop_merge = new (GetAllocator()) HGoto();
  loop_merge->AddInstruction(goto_loop_merge);

  HInstruction* goto_critical_break = new (GetAllocator()) HGoto();
  critical_break->AddInstruction(goto_critical_break);

  HInstruction* read_bottom = new (GetAllocator()) HInstanceFieldGet(new_inst,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* return_exit = new (GetAllocator()) HReturn(read_bottom);
  breturn->AddInstruction(read_bottom);
  breturn->AddInstruction(return_exit);

  HInstruction* exit_ins = new (GetAllocator()) HExit();
  exit->AddInstruction(exit_ins);
  // PerformLSE expects this to be empty.
  graph_->ClearDominanceInformation();
  LOG(INFO) << "Pre LSE " << blks;
  PerformLSEWithPartial();
  LOG(INFO) << "Post LSE " << blks;

  HPredicatedInstanceFieldGet* pred_get =
      FindSingleInstruction<HPredicatedInstanceFieldGet>(graph_, breturn);
  EXPECT_TRUE(IsRemoved(read_bottom)) << *read_bottom;
  ASSERT_TRUE(pred_get != nullptr);
  HPhi* inst_return_phi = pred_get->InputAt(0)->AsPhi();
  ASSERT_TRUE(inst_return_phi != nullptr) << *pred_get->InputAt(0);
  EXPECT_INS_EQ(inst_return_phi->InputAt(0),
                FindSingleInstruction<HNewInstance>(graph_, case1->GetSinglePredecessor()));
  EXPECT_INS_EQ(inst_return_phi->InputAt(1),
                FindSingleInstruction<HNewInstance>(graph_, case2->GetSinglePredecessor()));
  EXPECT_INS_EQ(inst_return_phi->InputAt(2), graph_->GetNullConstant());
  HPhi* inst_value_phi = pred_get->InputAt(1)->AsPhi();
  ASSERT_TRUE(inst_value_phi != nullptr) << *pred_get->InputAt(1);
  EXPECT_INS_EQ(inst_value_phi->InputAt(0), graph_->GetIntConstant(0));
  EXPECT_INS_EQ(inst_value_phi->InputAt(1), graph_->GetIntConstant(0));
  HPhi* loop_merge_phi = FindSingleInstruction<HPhi>(graph_, loop_merge);
  ASSERT_TRUE(loop_merge_phi != nullptr);
  HPhi* loop_header_phi = FindSingleInstruction<HPhi>(graph_, loop_header);
  ASSERT_TRUE(loop_header_phi != nullptr);
  EXPECT_INS_EQ(loop_header_phi->InputAt(0), c3);
  EXPECT_INS_EQ(loop_header_phi->InputAt(1), loop_merge_phi);
  EXPECT_INS_EQ(loop_merge_phi->InputAt(0), loop_header_phi);
  EXPECT_INS_EQ(loop_merge_phi->InputAt(1), c5);
  EXPECT_INS_EQ(inst_value_phi->InputAt(2), loop_header_phi);
  EXPECT_FALSE(IsRemoved(write_c1)) << *write_c1;
  EXPECT_FALSE(IsRemoved(write_c2)) << *write_c2;
  EXPECT_TRUE(IsRemoved(write_c3)) << *write_c3;
  EXPECT_TRUE(IsRemoved(write_loop_right)) << *write_loop_right;
}

// // ENTRY
// obj = new Obj();
// obj.field = 3;
// while (true) {
//   if (test()) { break; }
//   if (test2()) { } else { obj.field = 5; }
// }
// if (bool) {
//   escape(obj);
// }
// EXIT
// return obj.field
TEST_F(LoadStoreEliminationTest, PartialLoopPhis3) {
  ScopedObjectAccess soa(Thread::Current());
  VariableSizedHandleScope vshs(soa.Self());
  CreateGraph(/*handles=*/&vshs);
  AdjacencyListGraph blks(SetupFromAdjacencyList("entry",
                                                 "exit",
                                                 {{"entry", "loop_pre_header"},

                                                  {"loop_pre_header", "loop_header"},
                                                  {"loop_header", "critical_break"},
                                                  {"loop_header", "loop_body"},
                                                  {"loop_body", "loop_if_left"},
                                                  {"loop_body", "loop_if_right"},
                                                  {"loop_if_left", "loop_merge"},
                                                  {"loop_if_right", "loop_merge"},
                                                  {"loop_merge", "loop_header"},

                                                  {"critical_break", "escape_check"},
                                                  {"escape_check", "escape"},
                                                  {"escape_check", "no_escape"},
                                                  {"no_escape", "breturn"},
                                                  {"escape", "breturn"},
                                                  {"breturn", "exit"}}));
#define GET_BLOCK(name) HBasicBlock* name = blks.Get(#name)
  GET_BLOCK(entry);
  GET_BLOCK(exit);
  GET_BLOCK(breturn);
  GET_BLOCK(no_escape);
  GET_BLOCK(escape);
  GET_BLOCK(escape_check);

  GET_BLOCK(loop_pre_header);
  GET_BLOCK(loop_header);
  GET_BLOCK(loop_body);
  GET_BLOCK(loop_if_left);
  GET_BLOCK(loop_if_right);
  GET_BLOCK(loop_merge);
  GET_BLOCK(critical_break);
#undef GET_BLOCK
  EnsurePredecessorOrder(breturn, {no_escape, escape});
  EnsurePredecessorOrder(loop_header, {loop_pre_header, loop_merge});
  EnsurePredecessorOrder(loop_merge, {loop_if_left, loop_if_right});
  CHECK_SUBROUTINE_FAILURE();
  HInstruction* bool_val = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kBool);
  HInstruction* c3 = graph_->GetIntConstant(3);
  HInstruction* c5 = graph_->GetIntConstant(5);
  HInstruction* cls = new (GetAllocator()) HLoadClass(graph_->GetCurrentMethod(),
                                                      dex::TypeIndex(10),
                                                      graph_->GetDexFile(),
                                                      ScopedNullHandle<mirror::Class>(),
                                                      false,
                                                      0,
                                                      false);
  HInstruction* new_inst =
      new (GetAllocator()) HNewInstance(cls,
                                        0,
                                        dex::TypeIndex(10),
                                        graph_->GetDexFile(),
                                        false,
                                        QuickEntrypointEnum::kQuickAllocObjectInitialized);
  HInstruction* entry_goto = new (GetAllocator()) HGoto();
  entry->AddInstruction(bool_val);
  entry->AddInstruction(cls);
  entry->AddInstruction(new_inst);
  entry->AddInstruction(entry_goto);
  ArenaVector<HInstruction*> current_locals({}, GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(cls, &current_locals);
  new_inst->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* write_pre_header = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                          c3,
                                                                          nullptr,
                                                                          DataType::Type::kInt32,
                                                                          MemberOffset(10),
                                                                          false,
                                                                          0,
                                                                          0,
                                                                          graph_->GetDexFile(),
                                                                          0);
  HInstruction* goto_preheader = new (GetAllocator()) HGoto();
  loop_pre_header->AddInstruction(write_pre_header);
  loop_pre_header->AddInstruction(goto_preheader);

  HInstruction* suspend_check_header = new (GetAllocator()) HSuspendCheck();
  HInstruction* call_header = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            0,
                            DataType::Type::kBool,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* if_header = new (GetAllocator()) HIf(call_header);
  loop_header->AddInstruction(suspend_check_header);
  loop_header->AddInstruction(call_header);
  loop_header->AddInstruction(if_header);
  call_header->CopyEnvironmentFrom(cls->GetEnvironment());
  suspend_check_header->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* call_loop_body = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            0,
                            DataType::Type::kBool,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* if_loop_body = new (GetAllocator()) HIf(call_loop_body);
  loop_body->AddInstruction(call_loop_body);
  loop_body->AddInstruction(if_loop_body);
  call_loop_body->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* goto_loop_left = new (GetAllocator()) HGoto();
  loop_if_left->AddInstruction(goto_loop_left);

  HInstruction* write_loop_right = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                          c5,
                                                                          nullptr,
                                                                          DataType::Type::kInt32,
                                                                          MemberOffset(10),
                                                                          false,
                                                                          0,
                                                                          0,
                                                                          graph_->GetDexFile(),
                                                                          0);
  HInstruction* goto_loop_right = new (GetAllocator()) HGoto();
  loop_if_right->AddInstruction(write_loop_right);
  loop_if_right->AddInstruction(goto_loop_right);

  HInstruction* goto_loop_merge = new (GetAllocator()) HGoto();
  loop_merge->AddInstruction(goto_loop_merge);

  HInstruction* goto_critical_break = new (GetAllocator()) HGoto();
  critical_break->AddInstruction(goto_critical_break);

  HInstruction* if_esc_check = new (GetAllocator()) HIf(bool_val);
  escape_check->AddInstruction(if_esc_check);

  HInstruction* call_escape = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            1,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* goto_escape = new (GetAllocator()) HGoto();
  call_escape->AsInvoke()->SetRawInputAt(0, new_inst);
  escape->AddInstruction(call_escape);
  escape->AddInstruction(goto_escape);
  call_escape->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* goto_no_escape = new (GetAllocator()) HGoto();
  no_escape->AddInstruction(goto_no_escape);

  HInstruction* read_bottom = new (GetAllocator()) HInstanceFieldGet(new_inst,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* return_exit = new (GetAllocator()) HReturn(read_bottom);
  breturn->AddInstruction(read_bottom);
  breturn->AddInstruction(return_exit);

  HInstruction* exit_ins = new (GetAllocator()) HExit();
  exit->AddInstruction(exit_ins);
  // PerformLSE expects this to be empty.
  graph_->ClearDominanceInformation();
  LOG(INFO) << "Pre LSE " << blks;
  PerformLSEWithPartial();
  LOG(INFO) << "Post LSE " << blks;

  HPredicatedInstanceFieldGet* pred_get =
      FindSingleInstruction<HPredicatedInstanceFieldGet>(graph_, breturn);
  EXPECT_TRUE(IsRemoved(read_bottom)) << *read_bottom;
  ASSERT_TRUE(pred_get != nullptr);
  HPhi* inst_return_phi = pred_get->InputAt(0)->AsPhi();
  ASSERT_TRUE(inst_return_phi != nullptr) << *pred_get->InputAt(0);
  // The one we don't escape
  EXPECT_INS_EQ(inst_return_phi->InputAt(0), graph_->GetNullConstant());
  EXPECT_INS_EQ(inst_return_phi->InputAt(1),
                FindSingleInstruction<HNewInstance>(graph_, escape->GetSinglePredecessor()));
  HPhi* inst_value_phi = pred_get->InputAt(1)->AsPhi();
  ASSERT_TRUE(inst_value_phi != nullptr) << *pred_get->InputAt(1);
  HPhi* loop_header_phi = FindSingleInstruction<HPhi>(graph_, loop_header);
  HPhi* loop_merge_phi = FindSingleInstruction<HPhi>(graph_, loop_merge);
  EXPECT_INS_EQ(inst_value_phi->InputAt(0), loop_header_phi);
  EXPECT_INS_EQ(inst_value_phi->InputAt(1), graph_->GetIntConstant(0));
  EXPECT_INS_EQ(loop_header_phi->InputAt(0), c3);
  EXPECT_INS_EQ(loop_header_phi->InputAt(1), loop_merge_phi);
  EXPECT_INS_EQ(loop_merge_phi->InputAt(0), loop_header_phi);
  EXPECT_INS_EQ(loop_merge_phi->InputAt(1), c5);
  EXPECT_INS_EQ(
      FindSingleInstruction<HInstanceFieldSet>(graph_, escape->GetSinglePredecessor())->InputAt(1),
      loop_header_phi);
  EXPECT_TRUE(IsRemoved(write_loop_right)) << *write_loop_right;
  EXPECT_TRUE(IsRemoved(write_pre_header)) << *write_pre_header;
}

// // ENTRY
// obj = new Obj();
// if (bool) {
//   escape(obj);
// }
// obj.field = 3;
// while (true) {
//   if (test()) { break; }
//   if (test2()) { } else { obj.field = 5; }
// }
// EXIT
// return obj.field
TEST_F(LoadStoreEliminationTest, PartialLoopPhis4) {
  ScopedObjectAccess soa(Thread::Current());
  VariableSizedHandleScope vshs(soa.Self());
  CreateGraph(/*handles=*/&vshs);
  AdjacencyListGraph blks(SetupFromAdjacencyList("entry",
                                                 "exit",
                                                 {{"entry", "escape_check"},
                                                  {"escape_check", "escape"},
                                                  {"escape_check", "no_escape"},
                                                  {"no_escape", "loop_pre_header"},
                                                  {"escape", "loop_pre_header"},

                                                  {"loop_pre_header", "loop_header"},
                                                  {"loop_header", "critical_break"},
                                                  {"loop_header", "loop_body"},
                                                  {"loop_body", "loop_if_left"},
                                                  {"loop_body", "loop_if_right"},
                                                  {"loop_if_left", "loop_merge"},
                                                  {"loop_if_right", "loop_merge"},
                                                  {"loop_merge", "loop_header"},

                                                  {"critical_break", "breturn"},
                                                  {"breturn", "exit"}}));
#define GET_BLOCK(name) HBasicBlock* name = blks.Get(#name)
  GET_BLOCK(entry);
  GET_BLOCK(exit);
  GET_BLOCK(breturn);
  GET_BLOCK(no_escape);
  GET_BLOCK(escape);
  GET_BLOCK(escape_check);

  GET_BLOCK(loop_pre_header);
  GET_BLOCK(loop_header);
  GET_BLOCK(loop_body);
  GET_BLOCK(loop_if_left);
  GET_BLOCK(loop_if_right);
  GET_BLOCK(loop_merge);
  GET_BLOCK(critical_break);
#undef GET_BLOCK
  EnsurePredecessorOrder(loop_pre_header, {no_escape, escape});
  EnsurePredecessorOrder(loop_header, {loop_pre_header, loop_merge});
  EnsurePredecessorOrder(loop_merge, {loop_if_left, loop_if_right});
  CHECK_SUBROUTINE_FAILURE();
  HInstruction* bool_val = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kBool);
  HInstruction* c3 = graph_->GetIntConstant(3);
  HInstruction* c5 = graph_->GetIntConstant(5);
  HInstruction* cls = new (GetAllocator()) HLoadClass(graph_->GetCurrentMethod(),
                                                      dex::TypeIndex(10),
                                                      graph_->GetDexFile(),
                                                      ScopedNullHandle<mirror::Class>(),
                                                      false,
                                                      0,
                                                      false);
  HInstruction* new_inst =
      new (GetAllocator()) HNewInstance(cls,
                                        0,
                                        dex::TypeIndex(10),
                                        graph_->GetDexFile(),
                                        false,
                                        QuickEntrypointEnum::kQuickAllocObjectInitialized);
  HInstruction* entry_goto = new (GetAllocator()) HGoto();
  entry->AddInstruction(bool_val);
  entry->AddInstruction(cls);
  entry->AddInstruction(new_inst);
  entry->AddInstruction(entry_goto);
  ArenaVector<HInstruction*> current_locals({}, GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(cls, &current_locals);
  new_inst->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* write_pre_header = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                          c3,
                                                                          nullptr,
                                                                          DataType::Type::kInt32,
                                                                          MemberOffset(10),
                                                                          false,
                                                                          0,
                                                                          0,
                                                                          graph_->GetDexFile(),
                                                                          0);
  HInstruction* goto_preheader = new (GetAllocator()) HGoto();
  loop_pre_header->AddInstruction(write_pre_header);
  loop_pre_header->AddInstruction(goto_preheader);

  HInstruction* suspend_check_header = new (GetAllocator()) HSuspendCheck();
  HInstruction* call_header = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            0,
                            DataType::Type::kBool,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* if_header = new (GetAllocator()) HIf(call_header);
  loop_header->AddInstruction(suspend_check_header);
  loop_header->AddInstruction(call_header);
  loop_header->AddInstruction(if_header);
  call_header->CopyEnvironmentFrom(cls->GetEnvironment());
  suspend_check_header->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* call_loop_body = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            0,
                            DataType::Type::kBool,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* if_loop_body = new (GetAllocator()) HIf(call_loop_body);
  loop_body->AddInstruction(call_loop_body);
  loop_body->AddInstruction(if_loop_body);
  call_loop_body->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* goto_loop_left = new (GetAllocator()) HGoto();
  loop_if_left->AddInstruction(goto_loop_left);

  HInstruction* write_loop_right = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                          c5,
                                                                          nullptr,
                                                                          DataType::Type::kInt32,
                                                                          MemberOffset(10),
                                                                          false,
                                                                          0,
                                                                          0,
                                                                          graph_->GetDexFile(),
                                                                          0);
  HInstruction* goto_loop_right = new (GetAllocator()) HGoto();
  loop_if_right->AddInstruction(write_loop_right);
  loop_if_right->AddInstruction(goto_loop_right);

  HInstruction* goto_loop_merge = new (GetAllocator()) HGoto();
  loop_merge->AddInstruction(goto_loop_merge);

  HInstruction* goto_critical_break = new (GetAllocator()) HGoto();
  critical_break->AddInstruction(goto_critical_break);

  HInstruction* if_esc_check = new (GetAllocator()) HIf(bool_val);
  escape_check->AddInstruction(if_esc_check);

  HInstruction* call_escape = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            1,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* goto_escape = new (GetAllocator()) HGoto();
  call_escape->AsInvoke()->SetRawInputAt(0, new_inst);
  escape->AddInstruction(call_escape);
  escape->AddInstruction(goto_escape);
  call_escape->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* goto_no_escape = new (GetAllocator()) HGoto();
  no_escape->AddInstruction(goto_no_escape);

  HInstruction* read_bottom = new (GetAllocator()) HInstanceFieldGet(new_inst,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* return_exit = new (GetAllocator()) HReturn(read_bottom);
  breturn->AddInstruction(read_bottom);
  breturn->AddInstruction(return_exit);

  HInstruction* exit_ins = new (GetAllocator()) HExit();
  exit->AddInstruction(exit_ins);
  // PerformLSE expects this to be empty.
  graph_->ClearDominanceInformation();
  LOG(INFO) << "Pre LSE " << blks;
  PerformLSEWithPartial();
  LOG(INFO) << "Post LSE " << blks;

  HPredicatedInstanceFieldGet* pred_get =
      FindSingleInstruction<HPredicatedInstanceFieldGet>(graph_, breturn);
  EXPECT_TRUE(IsRemoved(read_bottom)) << *read_bottom;
  ASSERT_TRUE(pred_get != nullptr);
  HPhi* inst_return_phi = pred_get->InputAt(0)->AsPhi();
  ASSERT_TRUE(inst_return_phi != nullptr) << *pred_get->InputAt(0);
  // The one we don't escape
  EXPECT_INS_EQ(inst_return_phi->InputAt(0), graph_->GetNullConstant());
  EXPECT_INS_EQ(inst_return_phi->InputAt(1),
                FindSingleInstruction<HNewInstance>(graph_, escape->GetSinglePredecessor()));
  HPhi* inst_value_phi = pred_get->InputAt(1)->AsPhi();
  ASSERT_TRUE(inst_value_phi != nullptr) << *pred_get->InputAt(1);
  HPhi* loop_header_phi = FindSingleInstruction<HPhi>(graph_, loop_header);
  HPhi* loop_merge_phi = FindSingleInstruction<HPhi>(graph_, loop_merge);
  EXPECT_INS_EQ(inst_value_phi, loop_header_phi);
  EXPECT_INS_EQ(loop_header_phi->InputAt(0), c3);
  EXPECT_INS_EQ(loop_header_phi->InputAt(1), loop_merge_phi);
  EXPECT_INS_EQ(loop_merge_phi->InputAt(0), loop_header_phi);
  EXPECT_INS_EQ(loop_merge_phi->InputAt(1), c5);
  EXPECT_FALSE(IsRemoved(write_loop_right)) << *write_loop_right;
  EXPECT_TRUE(write_loop_right->AsInstanceFieldSet()->GetIsPredicatedSet()) << *write_loop_right;
  EXPECT_FALSE(IsRemoved(write_pre_header)) << *write_pre_header;
  EXPECT_TRUE(write_pre_header->AsInstanceFieldSet()->GetIsPredicatedSet()) << *write_pre_header;
}

// // ENTRY
// obj = new Obj();
// obj.field = 3;
// while (true) {
//   if (test()) { break; }
//   if (test2()) { } else { obj.field += 5; }
// }
// if (bool) {
//   escape(obj);
// }
// EXIT
// return obj.field
TEST_F(LoadStoreEliminationTest, PartialLoopPhis5) {
  ScopedObjectAccess soa(Thread::Current());
  VariableSizedHandleScope vshs(soa.Self());
  CreateGraph(/*handles=*/&vshs);
  AdjacencyListGraph blks(SetupFromAdjacencyList("entry",
                                                 "exit",
                                                 {{"entry", "loop_pre_header"},

                                                  {"loop_pre_header", "loop_header"},
                                                  {"loop_header", "critical_break"},
                                                  {"loop_header", "loop_body"},
                                                  {"loop_body", "loop_if_left"},
                                                  {"loop_body", "loop_if_right"},
                                                  {"loop_if_left", "loop_merge"},
                                                  {"loop_if_right", "loop_merge"},
                                                  {"loop_merge", "loop_header"},

                                                  {"critical_break", "escape_check"},
                                                  {"escape_check", "escape"},
                                                  {"escape_check", "no_escape"},
                                                  {"no_escape", "breturn"},
                                                  {"escape", "breturn"},
                                                  {"breturn", "exit"}}));
#define GET_BLOCK(name) HBasicBlock* name = blks.Get(#name)
  GET_BLOCK(entry);
  GET_BLOCK(exit);
  GET_BLOCK(breturn);
  GET_BLOCK(no_escape);
  GET_BLOCK(escape);
  GET_BLOCK(escape_check);

  GET_BLOCK(loop_pre_header);
  GET_BLOCK(loop_header);
  GET_BLOCK(loop_body);
  GET_BLOCK(loop_if_left);
  GET_BLOCK(loop_if_right);
  GET_BLOCK(loop_merge);
  GET_BLOCK(critical_break);
#undef GET_BLOCK
  EnsurePredecessorOrder(breturn, {no_escape, escape});
  EnsurePredecessorOrder(loop_header, {loop_pre_header, loop_merge});
  EnsurePredecessorOrder(loop_merge, {loop_if_left, loop_if_right});
  CHECK_SUBROUTINE_FAILURE();
  HInstruction* bool_val = new (GetAllocator())
      HParameterValue(graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kBool);
  HInstruction* c3 = graph_->GetIntConstant(3);
  HInstruction* c5 = graph_->GetIntConstant(5);
  HInstruction* cls = new (GetAllocator()) HLoadClass(graph_->GetCurrentMethod(),
                                                      dex::TypeIndex(10),
                                                      graph_->GetDexFile(),
                                                      ScopedNullHandle<mirror::Class>(),
                                                      false,
                                                      0,
                                                      false);
  HInstruction* new_inst =
      new (GetAllocator()) HNewInstance(cls,
                                        0,
                                        dex::TypeIndex(10),
                                        graph_->GetDexFile(),
                                        false,
                                        QuickEntrypointEnum::kQuickAllocObjectInitialized);
  HInstruction* entry_goto = new (GetAllocator()) HGoto();
  entry->AddInstruction(bool_val);
  entry->AddInstruction(cls);
  entry->AddInstruction(new_inst);
  entry->AddInstruction(entry_goto);
  ArenaVector<HInstruction*> current_locals({}, GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(cls, &current_locals);
  new_inst->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* write_pre_header = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                          c3,
                                                                          nullptr,
                                                                          DataType::Type::kInt32,
                                                                          MemberOffset(10),
                                                                          false,
                                                                          0,
                                                                          0,
                                                                          graph_->GetDexFile(),
                                                                          0);
  HInstruction* goto_preheader = new (GetAllocator()) HGoto();
  loop_pre_header->AddInstruction(write_pre_header);
  loop_pre_header->AddInstruction(goto_preheader);

  HInstruction* suspend_check_header = new (GetAllocator()) HSuspendCheck();
  HInstruction* call_header = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            0,
                            DataType::Type::kBool,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* if_header = new (GetAllocator()) HIf(call_header);
  loop_header->AddInstruction(suspend_check_header);
  loop_header->AddInstruction(call_header);
  loop_header->AddInstruction(if_header);
  call_header->CopyEnvironmentFrom(cls->GetEnvironment());
  suspend_check_header->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* call_loop_body = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            0,
                            DataType::Type::kBool,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* if_loop_body = new (GetAllocator()) HIf(call_loop_body);
  loop_body->AddInstruction(call_loop_body);
  loop_body->AddInstruction(if_loop_body);
  call_loop_body->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* goto_loop_left = new (GetAllocator()) HGoto();
  loop_if_left->AddInstruction(goto_loop_left);

  HInstruction* read_loop_right = new (GetAllocator()) HInstanceFieldGet(new_inst,
                                                                         nullptr,
                                                                         DataType::Type::kInt32,
                                                                         MemberOffset(10),
                                                                         false,
                                                                         0,
                                                                         0,
                                                                         graph_->GetDexFile(),
                                                                         0);
  HInstruction* add_loop_right =
      new (GetAllocator()) HAdd(DataType::Type::kInt32, c5, read_loop_right);
  HInstruction* write_loop_right = new (GetAllocator()) HInstanceFieldSet(new_inst,
                                                                          add_loop_right,
                                                                          nullptr,
                                                                          DataType::Type::kInt32,
                                                                          MemberOffset(10),
                                                                          false,
                                                                          0,
                                                                          0,
                                                                          graph_->GetDexFile(),
                                                                          0);
  HInstruction* goto_loop_right = new (GetAllocator()) HGoto();
  loop_if_right->AddInstruction(read_loop_right);
  loop_if_right->AddInstruction(add_loop_right);
  loop_if_right->AddInstruction(write_loop_right);
  loop_if_right->AddInstruction(goto_loop_right);

  HInstruction* goto_loop_merge = new (GetAllocator()) HGoto();
  loop_merge->AddInstruction(goto_loop_merge);

  HInstruction* goto_critical_break = new (GetAllocator()) HGoto();
  critical_break->AddInstruction(goto_critical_break);

  HInstruction* if_esc_check = new (GetAllocator()) HIf(bool_val);
  escape_check->AddInstruction(if_esc_check);

  HInstruction* call_escape = new (GetAllocator())
      HInvokeStaticOrDirect(GetAllocator(),
                            1,
                            DataType::Type::kVoid,
                            0,
                            {nullptr, 0},
                            nullptr,
                            {},
                            InvokeType::kStatic,
                            {nullptr, 0},
                            HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  HInstruction* goto_escape = new (GetAllocator()) HGoto();
  call_escape->AsInvoke()->SetRawInputAt(0, new_inst);
  escape->AddInstruction(call_escape);
  escape->AddInstruction(goto_escape);
  call_escape->CopyEnvironmentFrom(cls->GetEnvironment());

  HInstruction* goto_no_escape = new (GetAllocator()) HGoto();
  no_escape->AddInstruction(goto_no_escape);

  HInstruction* read_bottom = new (GetAllocator()) HInstanceFieldGet(new_inst,
                                                                     nullptr,
                                                                     DataType::Type::kInt32,
                                                                     MemberOffset(10),
                                                                     false,
                                                                     0,
                                                                     0,
                                                                     graph_->GetDexFile(),
                                                                     0);
  HInstruction* return_exit = new (GetAllocator()) HReturn(read_bottom);
  breturn->AddInstruction(read_bottom);
  breturn->AddInstruction(return_exit);

  HInstruction* exit_ins = new (GetAllocator()) HExit();
  exit->AddInstruction(exit_ins);
  // PerformLSE expects this to be empty.
  graph_->ClearDominanceInformation();
  LOG(INFO) << "Pre LSE " << blks;
  PerformLSEWithPartial();
  LOG(INFO) << "Post LSE " << blks;

  HPredicatedInstanceFieldGet* pred_get =
      FindSingleInstruction<HPredicatedInstanceFieldGet>(graph_, breturn);
  EXPECT_TRUE(IsRemoved(read_bottom)) << *read_bottom;
  ASSERT_TRUE(pred_get != nullptr);
  HPhi* inst_return_phi = pred_get->InputAt(0)->AsPhi();
  ASSERT_TRUE(inst_return_phi != nullptr) << *pred_get->InputAt(0);
  // The one we don't escape
  EXPECT_INS_EQ(inst_return_phi->InputAt(0), graph_->GetNullConstant());
  EXPECT_INS_EQ(inst_return_phi->InputAt(1),
                FindSingleInstruction<HNewInstance>(graph_, escape->GetSinglePredecessor()));
  HPhi* inst_value_phi = pred_get->InputAt(1)->AsPhi();
  ASSERT_TRUE(inst_value_phi != nullptr) << *pred_get->InputAt(1);
  HPhi* loop_header_phi = FindSingleInstruction<HPhi>(graph_, loop_header);
  HPhi* loop_merge_phi = FindSingleInstruction<HPhi>(graph_, loop_merge);
  EXPECT_INS_EQ(inst_value_phi->InputAt(0), loop_header_phi);
  EXPECT_INS_EQ(inst_value_phi->InputAt(1), graph_->GetIntConstant(0));
  EXPECT_INS_EQ(loop_header_phi->InputAt(0), c3);
  EXPECT_INS_EQ(loop_header_phi->InputAt(1), loop_merge_phi);
  EXPECT_INS_EQ(loop_merge_phi->InputAt(0), loop_header_phi);
  EXPECT_INS_EQ(loop_merge_phi->InputAt(1), add_loop_right);
  EXPECT_INS_EQ(add_loop_right->InputAt(0), c5);
  EXPECT_INS_EQ(add_loop_right->InputAt(1), loop_header_phi);
  EXPECT_INS_EQ(
      FindSingleInstruction<HInstanceFieldSet>(graph_, escape->GetSinglePredecessor())->InputAt(1),
      loop_header_phi);
  EXPECT_TRUE(IsRemoved(write_loop_right)) << *write_loop_right;
  EXPECT_TRUE(IsRemoved(write_pre_header)) << *write_pre_header;
}
}  // namespace art
