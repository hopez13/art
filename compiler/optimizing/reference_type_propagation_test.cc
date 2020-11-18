/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "reference_type_propagation.h"


#include <random>

#include "base/arena_allocator.h"
#include "base/transform_array_ref.h"
#include "base/transform_iterator.h"
#include "builder.h"
#include "nodes.h"
#include "object_lock.h"
#include "optimizing_unit_test.h"

namespace art {

/**
 * Fixture class for unit testing the ReferenceTypePropagation phase. Used to verify the
 * functionality of methods and situations that are hard to set up with checker tests.
 */
class ReferenceTypePropagationTest : public CommonCompilerTest, public OptimizingUnitTestHelper {
 public:
  ReferenceTypePropagationTest() : graph_(nullptr), propagation_(nullptr) { }

  ~ReferenceTypePropagationTest() { }

  void SetupPropagation(VariableSizedHandleScope* handles) {
    graph_ = CreateGraph(handles);
    propagation_ = new (GetAllocator()) ReferenceTypePropagation(graph_,
                                                                 Handle<mirror::ClassLoader>(),
                                                                 Handle<mirror::DexCache>(),
                                                                 true,
                                                                 "test_prop");
  }

  template <typename Func>
  void RunVisitListTest(Func mutator);

  void SortTopological(std::vector<HInstruction*>& inst);

  // Relay method to merge type in reference type propagation.
  ReferenceTypeInfo MergeTypes(const ReferenceTypeInfo& a,
                               const ReferenceTypeInfo& b) REQUIRES_SHARED(Locks::mutator_lock_) {
    return propagation_->MergeTypes(a, b, graph_->GetHandleCache());
  }

  // Helper method to construct an invalid type.
  ReferenceTypeInfo InvalidType() {
    return ReferenceTypeInfo::CreateInvalid();
  }

  // Helper method to construct the Object type.
  ReferenceTypeInfo ObjectType(bool is_exact = true) REQUIRES_SHARED(Locks::mutator_lock_) {
    return ReferenceTypeInfo::Create(graph_->GetHandleCache()->GetObjectClassHandle(), is_exact);
  }

  // Helper method to construct the String type.
  ReferenceTypeInfo StringType(bool is_exact = true) REQUIRES_SHARED(Locks::mutator_lock_) {
    return ReferenceTypeInfo::Create(graph_->GetHandleCache()->GetStringClassHandle(), is_exact);
  }

  // General building fields.
  HGraph* graph_;

  ReferenceTypePropagation* propagation_;
};

//
// The actual ReferenceTypePropgation unit tests.
//

TEST_F(ReferenceTypePropagationTest, ProperSetup) {
  ScopedObjectAccess soa(Thread::Current());
  VariableSizedHandleScope handles(soa.Self());
  SetupPropagation(&handles);

  EXPECT_TRUE(propagation_ != nullptr);
  EXPECT_TRUE(graph_->GetInexactObjectRti().IsEqual(ObjectType(false)));
}

TEST_F(ReferenceTypePropagationTest, MergeInvalidTypes) {
  ScopedObjectAccess soa(Thread::Current());
  VariableSizedHandleScope handles(soa.Self());
  SetupPropagation(&handles);

  // Two invalid types.
  ReferenceTypeInfo t1(MergeTypes(InvalidType(), InvalidType()));
  EXPECT_FALSE(t1.IsValid());
  EXPECT_FALSE(t1.IsExact());
  EXPECT_TRUE(t1.IsEqual(InvalidType()));

  // Valid type on right.
  ReferenceTypeInfo t2(MergeTypes(InvalidType(), ObjectType()));
  EXPECT_TRUE(t2.IsValid());
  EXPECT_TRUE(t2.IsExact());
  EXPECT_TRUE(t2.IsEqual(ObjectType()));
  ReferenceTypeInfo t3(MergeTypes(InvalidType(), StringType()));
  EXPECT_TRUE(t3.IsValid());
  EXPECT_TRUE(t3.IsExact());
  EXPECT_TRUE(t3.IsEqual(StringType()));

  // Valid type on left.
  ReferenceTypeInfo t4(MergeTypes(ObjectType(), InvalidType()));
  EXPECT_TRUE(t4.IsValid());
  EXPECT_TRUE(t4.IsExact());
  EXPECT_TRUE(t4.IsEqual(ObjectType()));
  ReferenceTypeInfo t5(MergeTypes(StringType(), InvalidType()));
  EXPECT_TRUE(t5.IsValid());
  EXPECT_TRUE(t5.IsExact());
  EXPECT_TRUE(t5.IsEqual(StringType()));
}

TEST_F(ReferenceTypePropagationTest, MergeValidTypes) {
  ScopedObjectAccess soa(Thread::Current());
  VariableSizedHandleScope handles(soa.Self());
  SetupPropagation(&handles);

  // Same types.
  ReferenceTypeInfo t1(MergeTypes(ObjectType(), ObjectType()));
  EXPECT_TRUE(t1.IsValid());
  EXPECT_TRUE(t1.IsExact());
  EXPECT_TRUE(t1.IsEqual(ObjectType()));
  ReferenceTypeInfo t2(MergeTypes(StringType(), StringType()));
  EXPECT_TRUE(t2.IsValid());
  EXPECT_TRUE(t2.IsExact());
  EXPECT_TRUE(t2.IsEqual(StringType()));

  // Left is super class of right.
  ReferenceTypeInfo t3(MergeTypes(ObjectType(), StringType()));
  EXPECT_TRUE(t3.IsValid());
  EXPECT_FALSE(t3.IsExact());
  EXPECT_TRUE(t3.IsEqual(ObjectType(false)));

  // Right is super class of left.
  ReferenceTypeInfo t4(MergeTypes(StringType(), ObjectType()));
  EXPECT_TRUE(t4.IsValid());
  EXPECT_FALSE(t4.IsExact());
  EXPECT_TRUE(t4.IsEqual(ObjectType(false)));

  // Same types, but one or both are inexact.
  ReferenceTypeInfo t5(MergeTypes(ObjectType(false), ObjectType()));
  EXPECT_TRUE(t5.IsValid());
  EXPECT_FALSE(t5.IsExact());
  EXPECT_TRUE(t5.IsEqual(ObjectType(false)));
  ReferenceTypeInfo t6(MergeTypes(ObjectType(), ObjectType(false)));
  EXPECT_TRUE(t6.IsValid());
  EXPECT_FALSE(t6.IsExact());
  EXPECT_TRUE(t6.IsEqual(ObjectType(false)));
  ReferenceTypeInfo t7(MergeTypes(ObjectType(false), ObjectType(false)));
  EXPECT_TRUE(t7.IsValid());
  EXPECT_FALSE(t7.IsExact());
  EXPECT_TRUE(t7.IsEqual(ObjectType(false)));
}

// This generates a large graph with a ton of phis. It then calls the 'mutator'
// function with the list of all the phis and then tries to propogate the types.
// mutator should reorder the list in some way. We verify everything worked by
// making sure every phi has valid type information.
template <typename Func>
void ReferenceTypePropagationTest::RunVisitListTest(Func mutator) {
  ScopedObjectAccess soa(Thread::Current());
  VariableSizedHandleScope handles(soa.Self());
  SetupPropagation(&handles);
  // Make a well-connected graph with a lot of edges.
  constexpr size_t kNumBlocks = 5000;
  constexpr size_t kTestMaxSuccessors = 3;
  std::vector<std::string> mid_blocks;
  for (auto i : Range(kNumBlocks)) {
    std::ostringstream oss;
    oss << "blk" << i;
    mid_blocks.push_back(oss.str());
  }
  // Create the edge list.
  std::vector<AdjacencyListGraph::Edge> edges;
  for (auto cur : Range(kNumBlocks)) {
    for (auto nxt : Range(cur + 1, std::min(cur + 1 + kTestMaxSuccessors, kNumBlocks))) {
      edges.emplace_back(mid_blocks[cur], mid_blocks[nxt]);
    }
  }
  AdjacencyListGraph alg(graph_, GetAllocator(), mid_blocks.front(), mid_blocks.back(), edges);
  std::unordered_map<HBasicBlock*, HInstruction*> single_value;
  // Setup the entry-block with the type to be propogated.
  HInstruction* cls =
      new (GetAllocator()) HLoadClass(graph_->GetCurrentMethod(),
                                      dex::TypeIndex(10),
                                      graph_->GetDexFile(),
                                      graph_->GetHandleCache()->GetObjectClassHandle(),
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
  single_value[alg.Get(mid_blocks.front())] = new_inst;
  HBasicBlock* start = alg.Get(mid_blocks.front());
  start->AddInstruction(cls);
  start->AddInstruction(new_inst);
  new_inst->SetReferenceTypeInfo(ObjectType(true));

  // Setup all the other blocks with a single PHI
  auto succ_blk_names = MakeIterationRange(mid_blocks.begin() + 1, mid_blocks.end());
  auto succ_blocks =
      MakeTransformRange(succ_blk_names, [&](const auto& sv) { return alg.Get(sv); });
  for (HBasicBlock* blk : succ_blocks) {
    HPhi* phi_inst = new (GetAllocator()) HPhi(
        GetAllocator(), kNoRegNumber, blk->GetPredecessors().size(), DataType::Type::kReference);
    single_value[blk] = phi_inst;
  }
  for (HBasicBlock* blk : succ_blocks) {
    HInstruction* my_val = single_value[blk];
    for (const auto& [pred, index] : ZipCount(MakeIterationRange(blk->GetPredecessors()))) {
      my_val->SetRawInputAt(index, single_value[pred]);
    }
    blk->AddPhi(my_val->AsPhi());
  }
  auto vals = MakeTransformRange(succ_blocks, [&](HBasicBlock* blk) { return single_value[blk]; });
  std::vector<HInstruction*> ins(vals.begin(), vals.end());
  graph_->ClearReachabilityInformation();
  graph_->ComputeReachabilityInformation();
  mutator(ins);
  propagation_->Visit(ArrayRef<HInstruction* const>(ins));
  for (auto [blk, i] : single_value) {
    if (blk == start) {
      continue;
    }
    EXPECT_TRUE(i->GetReferenceTypeInfo().IsValid())
        << i->GetId() << " blk: " << alg.GetName(i->GetBlock());
  }
}

void ReferenceTypePropagationTest::SortTopological(std::vector<HInstruction *>& lst) {
  std::sort(lst.begin(), lst.end(), [&](HInstruction* a, HInstruction* b) {
    return a->GetReferenceTypeInfo().IsValid() ||
            std::any_of(a->GetInputs().begin(),
                        a->GetInputs().end(),
                        [&](HInputsRef::reference ref) {
                          return ref->GetReferenceTypeInfo().IsValid();
                        }) ||
            graph_->PathBetween(a->GetBlock(), b->GetBlock());
  });
}

TEST_F(ReferenceTypePropagationTest, VisitReverseTopological) {
  RunVisitListTest([&](std::vector<HInstruction*>& lst) {
    SortTopological(lst);
    std::reverse(lst.begin(), lst.end());
  });
}
TEST_F(ReferenceTypePropagationTest, VisitTopological) {
  RunVisitListTest([&](std::vector<HInstruction*>& lst) {
    SortTopological(lst);
  });
}

TEST_F(ReferenceTypePropagationTest, VisitAlmostTopological) {
  RunVisitListTest([&](std::vector<HInstruction*>& lst) {
    SortTopological(lst);
    std::swap(lst.front(), lst.back());
  });
}

TEST_F(ReferenceTypePropagationTest, VisitRandom) {
  std::default_random_engine g(std::rand());
  RunVisitListTest(
      [&](std::vector<HInstruction*>& lst) { std::shuffle(lst.begin(), lst.end(), g); });
}

}  // namespace art
