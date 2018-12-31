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

#include "superblock_cloner.h"

#include "common_dominator.h"
#include "induction_var_range.h"
#include "graph_checker.h"

#include <iostream>

namespace art {

using HBasicBlockMap = SuperblockCloner::HBasicBlockMap;
using HInstructionMap = SuperblockCloner::HInstructionMap;
using HBasicBlockSet = SuperblockCloner::HBasicBlockSet;
using HEdgeSet = SuperblockCloner::HEdgeSet;

void HEdge::Dump(std::ostream& stream) const {
  stream << "(" << from_ << "->" << to_ << ")";
}

//
// Static helper methods.
//

// Returns whether instruction has any uses (regular or environmental) outside the region,
// defined by basic block set.
static bool IsUsedOutsideRegion(const HInstruction* instr, const HBasicBlockSet& bb_set) {
  auto& uses = instr->GetUses();
  for (auto use_node = uses.begin(), e = uses.end(); use_node != e; ++use_node) {
    HInstruction* user = use_node->GetUser();
    if (!bb_set.IsBitSet(user->GetBlock()->GetBlockId())) {
      return true;
    }
  }

  auto& env_uses = instr->GetEnvUses();
  for (auto use_node = env_uses.begin(), e = env_uses.end(); use_node != e; ++use_node) {
    HInstruction* user = use_node->GetUser()->GetHolder();
    if (!bb_set.IsBitSet(user->GetBlock()->GetBlockId())) {
      return true;
    }
  }

  return false;
}

// Returns whether the phi's inputs are the same HInstruction.
static bool ArePhiInputsTheSame(const HPhi* phi) {
  HInstruction* first_input = phi->InputAt(0);
  for (size_t i = 1, e = phi->InputCount(); i < e; i++) {
    if (phi->InputAt(i) != first_input) {
      return false;
    }
  }

  return true;
}

// Returns whether two Edge sets are equal (ArenaHashSet doesn't have "Equal" method).
static bool EdgeHashSetsEqual(const HEdgeSet* set1, const HEdgeSet* set2) {
  if (set1->size() != set2->size()) {
    return false;
  }

  for (auto e : *set1) {
    if (set2->find(e) == set2->end()) {
      return false;
    }
  }
  return true;
}

// Calls HGraph::OrderLoopHeaderPredecessors for each loop in the graph.
static void OrderLoopsHeadersPredecessors(HGraph* graph) {
  for (HBasicBlock* block : graph->GetPostOrder()) {
    if (block->IsLoopHeader()) {
      graph->OrderLoopHeaderPredecessors(block);
    }
  }
}

// Performs DFS on the subgraph (specified by 'bb_set') starting from the specified block; while
// traversing the function removes basic blocks from the bb_set (instead of traditional DFS
// 'marking'). So what is left in the 'bb_set' after the traversal is not reachable from the start
// block.
static void TraverseSubgraphForConnectivity(HBasicBlock* block, HBasicBlockSet* bb_set) {
  DCHECK(bb_set->IsBitSet(block->GetBlockId()));
  bb_set->ClearBit(block->GetBlockId());

  for (HBasicBlock* succ : block->GetSuccessors()) {
    if (bb_set->IsBitSet(succ->GetBlockId())) {
      TraverseSubgraphForConnectivity(succ, bb_set);
    }
  }
}

//
// Helpers for CloneBasicBlock.
//

void SuperblockCloner::ReplaceInputsWithCopies(HInstruction* copy_instr) {
  DCHECK(!copy_instr->IsPhi());
  for (size_t i = 0, e = copy_instr->InputCount(); i < e; i++) {
    // Copy instruction holds the same input as the original instruction holds.
    HInstruction* orig_input = copy_instr->InputAt(i);
    if (!IsInOrigBBSet(orig_input->GetBlock())) {
      // Defined outside the subgraph.
      continue;
    }
    HInstruction* copy_input = GetInstrCopy(orig_input);
    // copy_instr will be registered as a user of copy_inputs after returning from this function:
    // 'copy_block->AddInstruction(copy_instr)'.
    copy_instr->SetRawInputAt(i, copy_input);
  }
}

void SuperblockCloner::DeepCloneEnvironmentWithRemapping(HInstruction* copy_instr,
                                                         const HEnvironment* orig_env) {
  if (orig_env->GetParent() != nullptr) {
    DeepCloneEnvironmentWithRemapping(copy_instr, orig_env->GetParent());
  }
  HEnvironment* copy_env = new (arena_) HEnvironment(arena_, *orig_env, copy_instr);

  for (size_t i = 0; i < orig_env->Size(); i++) {
    HInstruction* env_input = orig_env->GetInstructionAt(i);
    if (env_input != nullptr && IsInOrigBBSet(env_input->GetBlock())) {
      env_input = GetInstrCopy(env_input);
      DCHECK(env_input != nullptr && env_input->GetBlock() != nullptr);
    }
    copy_env->SetRawEnvAt(i, env_input);
    if (env_input != nullptr) {
      env_input->AddEnvUseAt(copy_env, i);
    }
  }
  // InsertRawEnvironment assumes that instruction already has an environment that's why we use
  // SetRawEnvironment in the 'else' case.
  // As this function calls itself recursively with the same copy_instr - this copy_instr may
  // have partially copied chain of HEnvironments.
  if (copy_instr->HasEnvironment()) {
    copy_instr->InsertRawEnvironment(copy_env);
  } else {
    copy_instr->SetRawEnvironment(copy_env);
  }
}

//
// Helpers for RemapEdgesSuccessors.
//

void SuperblockCloner::RemapOrigInternalOrIncomingEdge(HBasicBlock* orig_block,
                                                       HBasicBlock* orig_succ) {
  DCHECK(IsInOrigBBSet(orig_succ));
  HBasicBlock* copy_succ = GetBlockCopy(orig_succ);

  size_t this_index = orig_succ->GetPredecessorIndexOf(orig_block);
  size_t phi_input_count = 0;
  // This flag reflects whether the original successor has at least one phi and this phi
  // has been already processed in the loop. Used for validation purposes in DCHECK to check that
  // in the end all of the phis in the copy successor have the same number of inputs - the number
  // of copy successor's predecessors.
  bool first_phi_met = false;
  for (HInstructionIterator it(orig_succ->GetPhis()); !it.Done(); it.Advance()) {
    HPhi* orig_phi = it.Current()->AsPhi();
    HPhi* copy_phi = GetInstrCopy(orig_phi)->AsPhi();
    HInstruction* orig_phi_input = orig_phi->InputAt(this_index);
    // Remove corresponding input for original phi.
    orig_phi->RemoveInputAt(this_index);
    // Copy phi doesn't yet have either orig_block as predecessor or the input that corresponds
    // to orig_block, so add the input at the end of the list.
    copy_phi->AddInput(orig_phi_input);
    if (!first_phi_met) {
      phi_input_count = copy_phi->InputCount();
      first_phi_met = true;
    } else {
      DCHECK_EQ(phi_input_count, copy_phi->InputCount());
    }
  }
  // orig_block will be put at the end of the copy_succ's predecessors list; that corresponds
  // to the previously added phi inputs position.
  orig_block->ReplaceSuccessor(orig_succ, copy_succ);
  DCHECK(!first_phi_met || copy_succ->GetPredecessors().size() == phi_input_count);
}

void SuperblockCloner::AddCopyInternalEdge(HBasicBlock* orig_block,
                                           HBasicBlock* orig_succ) {
  DCHECK(IsInOrigBBSet(orig_succ));
  HBasicBlock* copy_block = GetBlockCopy(orig_block);
  HBasicBlock* copy_succ = GetBlockCopy(orig_succ);
  copy_block->AddSuccessor(copy_succ);

  size_t orig_index = orig_succ->GetPredecessorIndexOf(orig_block);
  for (HInstructionIterator it(orig_succ->GetPhis()); !it.Done(); it.Advance()) {
    HPhi* orig_phi = it.Current()->AsPhi();
    HPhi* copy_phi = GetInstrCopy(orig_phi)->AsPhi();
    HInstruction* orig_phi_input = orig_phi->InputAt(orig_index);
    copy_phi->AddInput(orig_phi_input);
  }
}

void SuperblockCloner::RemapCopyInternalEdge(HBasicBlock* orig_block,
                                             HBasicBlock* orig_succ) {
  DCHECK(IsInOrigBBSet(orig_succ));
  HBasicBlock* copy_block = GetBlockCopy(orig_block);
  copy_block->AddSuccessor(orig_succ);
  DCHECK(copy_block->HasSuccessor(orig_succ));

  size_t orig_index = orig_succ->GetPredecessorIndexOf(orig_block);
  for (HInstructionIterator it(orig_succ->GetPhis()); !it.Done(); it.Advance()) {
    HPhi* orig_phi = it.Current()->AsPhi();
    HInstruction* orig_phi_input = orig_phi->InputAt(orig_index);
    orig_phi->AddInput(orig_phi_input);
  }
}

//
// Local versions of CF calculation/adjustment routines.
//

// TODO: merge with the original version in nodes.cc. The concern is that we don't want to affect
// the performance of the base version by checking the local set.
// TODO: this version works when updating the back edges info for natural loop-based local_set.
// Check which exactly types of subgraphs can be analysed or rename it to
// FindBackEdgesInTheNaturalLoop.
void SuperblockCloner::FindBackEdgesLocal(HBasicBlock* entry_block, ArenaBitVector* local_set) {
  ArenaBitVector visited(arena_, graph_->GetBlocks().size(), false, kArenaAllocSuperblockCloner);
  // "visited" must be empty on entry, it's an output argument for all visited (i.e. live) blocks.
  DCHECK_EQ(visited.GetHighestBitSet(), -1);

  // Nodes that we're currently visiting, indexed by block id.
  ArenaBitVector visiting(arena_, graph_->GetBlocks().size(), false, kArenaAllocGraphBuilder);
  // Number of successors visited from a given node, indexed by block id.
  ArenaVector<size_t> successors_visited(graph_->GetBlocks().size(),
                                         0u,
                                         arena_->Adapter(kArenaAllocGraphBuilder));
  // Stack of nodes that we're currently visiting (same as marked in "visiting" above).
  ArenaVector<HBasicBlock*> worklist(arena_->Adapter(kArenaAllocGraphBuilder));
  constexpr size_t kDefaultWorklistSize = 8;
  worklist.reserve(kDefaultWorklistSize);

  visited.SetBit(entry_block->GetBlockId());
  visiting.SetBit(entry_block->GetBlockId());
  worklist.push_back(entry_block);

  while (!worklist.empty()) {
    HBasicBlock* current = worklist.back();
    uint32_t current_id = current->GetBlockId();
    if (successors_visited[current_id] == current->GetSuccessors().size()) {
      visiting.ClearBit(current_id);
      worklist.pop_back();
    } else {
      HBasicBlock* successor = current->GetSuccessors()[successors_visited[current_id]++];
      uint32_t successor_id = successor->GetBlockId();
      if (!local_set->IsBitSet(successor_id)) {
        continue;
      }

      if (visiting.IsBitSet(successor_id)) {
        DCHECK(ContainsElement(worklist, successor));
        successor->AddBackEdgeWhileUpdating(current);
      } else if (!visited.IsBitSet(successor_id)) {
        visited.SetBit(successor_id);
        visiting.SetBit(successor_id);
        worklist.push_back(successor);
      }
    }
  }
}

// "keep_both_loops" for including both orig and copy loop bits
void SuperblockCloner::RecalculateBackEdgesInfo(ArenaBitVector* outer_loop_bb_set, bool keep_both_loops) {
  HBasicBlock* block_entry = nullptr;

  if (outer_loop_ == nullptr) {
    for (auto block : graph_->GetBlocks()) {
      if (block != nullptr) {
        outer_loop_bb_set->SetBit(block->GetBlockId());
        HLoopInformation* info = block->GetLoopInformation();
        if (info != nullptr) {
          info->ResetBasicBlockData();
        }
      }
    }
    block_entry = graph_->GetEntryBlock();
  } else {
    outer_loop_bb_set->Copy(&outer_loop_bb_set_);
    block_entry = outer_loop_->GetHeader();

    // Add newly created copy blocks.
    for (auto entry : *bb_map_) {
      // Include orig loop as well when keep_both_loops is true
      if (keep_both_loops)
        outer_loop_bb_set->SetBit(entry.first->GetBlockId());

      outer_loop_bb_set->SetBit(entry.second->GetBlockId());
    }

    // Clear loop_info for the whole outer loop.
    for (uint32_t idx : outer_loop_bb_set->Indexes()) {
      HBasicBlock* block = GetBlockById(idx);
      HLoopInformation* info = block->GetLoopInformation();
      if (info != nullptr) {
        info->ResetBasicBlockData();
      }
    }
  }

  FindBackEdgesLocal(block_entry, outer_loop_bb_set);

  for (uint32_t idx : outer_loop_bb_set->Indexes()) {
    HBasicBlock* block = GetBlockById(idx);
    HLoopInformation* info = block->GetLoopInformation();
    // Reset LoopInformation for regular blocks and old headers which are no longer loop headers.
    if (info != nullptr &&
        (info->GetHeader() != block || info->NumberOfBackEdges() == 0)) {
      block->SetLoopInformation(nullptr);
    }
  }
}

// This is a modified version of HGraph::AnalyzeLoops.
GraphAnalysisResult SuperblockCloner::AnalyzeLoopsLocally(ArenaBitVector* outer_loop_bb_set) {
  // We iterate post order to ensure we visit inner loops before outer loops.
  // `PopulateRecursive` needs this guarantee to know whether a natural loop
  // contains an irreducible loop.
  for (HBasicBlock* block : graph_->GetPostOrder()) {
    if (!outer_loop_bb_set->IsBitSet(block->GetBlockId())) {
      continue;
    }
    if (block->IsLoopHeader()) {
      if (block->IsCatchBlock()) {
        // TODO: Dealing with exceptional back edges could be tricky because
        //       they only approximate the real control flow. Bail out for now.
        return kAnalysisFailThrowCatchLoop;
      }
      block->GetLoopInformation()->Populate();
    }
  }

  for (HBasicBlock* block : graph_->GetPostOrder()) {
    if (!outer_loop_bb_set->IsBitSet(block->GetBlockId())) {
      continue;
    }
    if (block->IsLoopHeader()) {
      HLoopInformation* cur_loop = block->GetLoopInformation();
      HLoopInformation* outer_loop = cur_loop->GetPreHeader()->GetLoopInformation();
      if (outer_loop != nullptr) {
        outer_loop->PopulateInnerLoopUpwards(cur_loop);
      }
    }
  }

  return kAnalysisSuccess;
}

void SuperblockCloner::CleanUpControlFlow(bool keep_both_loops) {
  // TODO: full control flow clean up for now, optimize it.
  graph_->ClearDominanceInformation();

  ArenaBitVector outer_loop_bb_set(
      arena_, graph_->GetBlocks().size(), false, kArenaAllocSuperblockCloner);
  RecalculateBackEdgesInfo(&outer_loop_bb_set, keep_both_loops);

  // TODO: do it locally.
  graph_->SimplifyCFG();
  graph_->ComputeDominanceInformation();

  // AnalyzeLoopsLocally requires a correct post-ordering information which was calculated just
  // before in ComputeDominanceInformation.
  GraphAnalysisResult result = AnalyzeLoopsLocally(&outer_loop_bb_set);
  DCHECK_EQ(result, kAnalysisSuccess);

  // TODO: do it locally
  OrderLoopsHeadersPredecessors(graph_);

  graph_->ComputeTryBlockInformation();
}

//
// Helpers for ResolveDataFlow
//

void SuperblockCloner::ResolvePhi(HPhi* phi) {
  HBasicBlock* phi_block = phi->GetBlock();
  for (size_t i = 0, e = phi->InputCount(); i < e; i++) {
    HInstruction* input = phi->InputAt(i);
    HBasicBlock* input_block = input->GetBlock();

    // Originally defined outside the region.
    if (!IsInOrigBBSet(input_block)) {
      continue;
    }
    HBasicBlock* corresponding_block = phi_block->GetPredecessors()[i];
    if (!IsInOrigBBSet(corresponding_block)) {
      phi->ReplaceInput(GetInstrCopy(input), i);
    }
  }
}

//
// Main algorithm methods.
//

void SuperblockCloner::SearchForSubgraphExits(ArenaVector<HBasicBlock*>* exits) const {
  DCHECK(exits->empty());
  for (uint32_t block_id : orig_bb_set_.Indexes()) {
    HBasicBlock* block = GetBlockById(block_id);
    for (HBasicBlock* succ : block->GetSuccessors()) {
      if (!IsInOrigBBSet(succ)) {
        exits->push_back(succ);
      }
    }
  }
}

void SuperblockCloner::FindAndSetLocalAreaForAdjustments() {
  DCHECK(outer_loop_ == nullptr);
  ArenaVector<HBasicBlock*> exits(arena_->Adapter(kArenaAllocSuperblockCloner));
  SearchForSubgraphExits(&exits);

  // For a reducible graph we need to update back-edges and dominance information only for
  // the outermost loop which is affected by the transformation - it can be found by picking
  // the common most outer loop of loops to which the subgraph exits blocks belong.
  // Note: it can a loop or the whole graph (outer_loop_ will be nullptr in this case).
  for (HBasicBlock* exit : exits) {
    HLoopInformation* loop_exit_loop_info = exit->GetLoopInformation();
    if (loop_exit_loop_info == nullptr) {
      outer_loop_ = nullptr;
      break;
    }
    if (outer_loop_ == nullptr) {
      // We should not use the initial outer_loop_ value 'nullptr' when finding the most outer
      // common loop.
      outer_loop_ = loop_exit_loop_info;
    }
    outer_loop_ = FindCommonLoop(outer_loop_, loop_exit_loop_info);
  }

  if (outer_loop_ != nullptr) {
    // Save the loop population info as it will be changed later.
    outer_loop_bb_set_.Copy(&outer_loop_->GetBlocks());
  }
}

void SuperblockCloner::RemapEdgesSuccessors() {
  // Redirect incoming edges.
  for (HEdge e : *remap_incoming_) {
    HBasicBlock* orig_block = GetBlockById(e.GetFrom());
    HBasicBlock* orig_succ = GetBlockById(e.GetTo());
    RemapOrigInternalOrIncomingEdge(orig_block, orig_succ);
  }

  // Redirect internal edges.
  for (uint32_t orig_block_id : orig_bb_set_.Indexes()) {
    HBasicBlock* orig_block = GetBlockById(orig_block_id);

    for (HBasicBlock* orig_succ : orig_block->GetSuccessors()) {
      uint32_t orig_succ_id = orig_succ->GetBlockId();

      // Check for outgoing edge.
      if (!IsInOrigBBSet(orig_succ)) {
        HBasicBlock* copy_block = GetBlockCopy(orig_block);
        copy_block->AddSuccessor(orig_succ);
        continue;
      }

      auto orig_redir = remap_orig_internal_->find(HEdge(orig_block_id, orig_succ_id));
      auto copy_redir = remap_copy_internal_->find(HEdge(orig_block_id, orig_succ_id));

      // Due to construction all successors of copied block were set to original.
      if (copy_redir != remap_copy_internal_->end()) {
        RemapCopyInternalEdge(orig_block, orig_succ);
      } else {
        AddCopyInternalEdge(orig_block, orig_succ);
      }

      if (orig_redir != remap_orig_internal_->end()) {
        RemapOrigInternalOrIncomingEdge(orig_block, orig_succ);
      }
    }
  }
}

void SuperblockCloner::AdjustControlFlowInfo(bool keep_both_loops) {
  ArenaBitVector outer_loop_bb_set(
      arena_, graph_->GetBlocks().size(), false, kArenaAllocSuperblockCloner);
  RecalculateBackEdgesInfo(&outer_loop_bb_set, keep_both_loops);

  graph_->ClearDominanceInformation();
  // TODO: Do it locally.
  graph_->ComputeDominanceInformation();
}

// TODO: Current FastCase restriction guarantees that instructions' inputs are already mapped to
// the valid values; only phis' inputs must be adjusted.
void SuperblockCloner::ResolveDataFlow() {
  for (auto entry : *bb_map_) {
    HBasicBlock* orig_block = entry.first;

    for (HInstructionIterator it(orig_block->GetPhis()); !it.Done(); it.Advance()) {
      HPhi* orig_phi = it.Current()->AsPhi();
      HPhi* copy_phi = GetInstrCopy(orig_phi)->AsPhi();
      ResolvePhi(orig_phi);
      ResolvePhi(copy_phi);
    }
    if (kIsDebugBuild) {
      // Inputs of instruction copies must be already mapped to correspondent inputs copies.
      for (HInstructionIterator it(orig_block->GetInstructions()); !it.Done(); it.Advance()) {
        CheckInstructionInputsRemapping(it.Current());
      }
    }
  }
}

//
// Helpers for live-outs processing and Subgraph-closed SSA.
//

bool SuperblockCloner::CollectLiveOutsAndCheckClonable(HInstructionMap* live_outs) const {
  DCHECK(live_outs->empty());
  for (uint32_t idx : orig_bb_set_.Indexes()) {
    HBasicBlock* block = GetBlockById(idx);

    for (HInstructionIterator it(block->GetPhis()); !it.Done(); it.Advance()) {
      HInstruction* instr = it.Current();
      DCHECK(instr->IsClonable());

      if (IsUsedOutsideRegion(instr, orig_bb_set_)) {
        live_outs->FindOrAdd(instr, instr);
      }
    }

    for (HInstructionIterator it(block->GetInstructions()); !it.Done(); it.Advance()) {
      HInstruction* instr = it.Current();
      if (!instr->IsClonable()) {
        return false;
      }

      if (IsUsedOutsideRegion(instr, orig_bb_set_)) {
        // TODO: Investigate why HNewInstance, HCheckCast has a requirement for the input.
        if (instr->IsLoadClass()) {
          return false;
        }
        live_outs->FindOrAdd(instr, instr);
      }
    }
  }
  return true;
}

void SuperblockCloner::UpdateInductionRangeInfoOf(
      HInstruction* user, HInstruction* old_instruction, HInstruction* replacement) {
  if (induction_range_ != nullptr) {
    induction_range_->Replace(user, old_instruction, replacement);
  }
}

void SuperblockCloner::ConstructSubgraphClosedSSA() {
  if (live_outs_.empty()) {
    return;
  }

  ArenaVector<HBasicBlock*> exits(arena_->Adapter(kArenaAllocSuperblockCloner));
  SearchForSubgraphExits(&exits);
  if (exits.empty()) {
    DCHECK(live_outs_.empty());
    return;
  }

  DCHECK_EQ(exits.size(), 1u);
  HBasicBlock* exit_block = exits[0];
  // There should be no critical edges.
  DCHECK_EQ(exit_block->GetPredecessors().size(), 1u);
  DCHECK(exit_block->GetPhis().IsEmpty());

  // For each live-out value insert a phi into the loop exit and replace all the value's uses
  // external to the loop with this phi. The phi will have the original value as its only input;
  // after copying is done FixSubgraphClosedSSAAfterCloning will add a corresponding copy of the
  // original value as the second input thus merging data flow from the original and copy parts of
  // the subgraph. Also update the record in the live_outs_ map from (value, value) to
  // (value, new_phi).
  for (auto live_out_it = live_outs_.begin(); live_out_it != live_outs_.end(); ++live_out_it) {
    HInstruction* value = live_out_it->first;
    HPhi* phi = new (arena_) HPhi(arena_, kNoRegNumber, 0, value->GetType());

    if (value->GetType() == DataType::Type::kReference) {
      phi->SetReferenceTypeInfo(value->GetReferenceTypeInfo());
    }

    exit_block->AddPhi(phi);
    live_out_it->second = phi;

    const HUseList<HInstruction*>& uses = value->GetUses();
    for (auto it = uses.begin(), end = uses.end(); it != end; /* ++it below */) {
      HInstruction* user = it->GetUser();
      size_t index = it->GetIndex();
      // Increment `it` now because `*it` may disappear thanks to user->ReplaceInput().
      ++it;
      if (!IsInOrigBBSet(user->GetBlock())) {
        user->ReplaceInput(phi, index);
        UpdateInductionRangeInfoOf(user, value, phi);
      }
    }

    const HUseList<HEnvironment*>& env_uses = value->GetEnvUses();
    for (auto it = env_uses.begin(), e = env_uses.end(); it != e; /* ++it below */) {
      HEnvironment* env = it->GetUser();
      size_t index = it->GetIndex();
      ++it;
      if (!IsInOrigBBSet(env->GetHolder()->GetBlock())) {
        env->ReplaceInput(phi, index);
      }
    }

    phi->AddInput(value);
  }
}

void SuperblockCloner::FixSubgraphClosedSSAAfterCloning() {
  for (auto it : live_outs_) {
    DCHECK(it.first != it.second);
    HInstruction* orig_value = it.first;
    HPhi* phi = it.second->AsPhi();
    HInstruction* copy_value = GetInstrCopy(orig_value);
    // Copy edges are inserted after the original so we can just add new input to the phi.
    phi->AddInput(copy_value);
  }
}

//
// Debug and logging methods.
//

// Debug function to dump graph' BasicBlocks info.
void DumpBB(HGraph* graph) {
  for (HBasicBlock* bb : graph->GetBlocks()) {
    if (bb == nullptr) {
      continue;
    }
    std::cout << bb->GetBlockId();
    std::cout << " <- ";
    for (HBasicBlock* pred : bb->GetPredecessors()) {
      std::cout << pred->GetBlockId() << " ";
    }
    std::cout << " -> ";
    for (HBasicBlock* succ : bb->GetSuccessors()) {
      std::cout << succ->GetBlockId()  << " ";
    }

    if (bb->GetDominator()) {
      std::cout << " dom " << bb->GetDominator()->GetBlockId();
    }

    if (bb->GetLoopInformation()) {
      std::cout <<  "\tloop: " << bb->GetLoopInformation()->GetHeader()->GetBlockId();
    }

    std::cout << std::endl;
  }
}

void SuperblockCloner::CheckInstructionInputsRemapping(HInstruction* orig_instr) {
  DCHECK(!orig_instr->IsPhi());
  HInstruction* copy_instr = GetInstrCopy(orig_instr);
  for (size_t i = 0, e = orig_instr->InputCount(); i < e; i++) {
    HInstruction* orig_input = orig_instr->InputAt(i);
    DCHECK(orig_input->GetBlock()->Dominates(orig_instr->GetBlock()));

    // If original input is defined outside the region then it will remain for both original
    // instruction and the copy after the transformation.
    if (!IsInOrigBBSet(orig_input->GetBlock())) {
      continue;
    }
    HInstruction* copy_input = GetInstrCopy(orig_input);
    DCHECK(copy_input->GetBlock()->Dominates(copy_instr->GetBlock()));
  }

  // Resolve environment.
  if (orig_instr->HasEnvironment()) {
    HEnvironment* orig_env = orig_instr->GetEnvironment();

    for (size_t i = 0, e = orig_env->Size(); i < e; ++i) {
      HInstruction* orig_input = orig_env->GetInstructionAt(i);

      // If original input is defined outside the region then it will remain for both original
      // instruction and the copy after the transformation.
      if (orig_input == nullptr || !IsInOrigBBSet(orig_input->GetBlock())) {
        continue;
      }

      HInstruction* copy_input = GetInstrCopy(orig_input);
      DCHECK(copy_input->GetBlock()->Dominates(copy_instr->GetBlock()));
    }
  }
}

bool SuperblockCloner::CheckRemappingInfoIsValid() {
  for (HEdge edge : *remap_orig_internal_) {
    if (!IsEdgeValid(edge, graph_) ||
        !IsInOrigBBSet(edge.GetFrom()) ||
        !IsInOrigBBSet(edge.GetTo())) {
      return false;
    }
  }

  for (auto edge : *remap_copy_internal_) {
    if (!IsEdgeValid(edge, graph_) ||
        !IsInOrigBBSet(edge.GetFrom()) ||
        !IsInOrigBBSet(edge.GetTo())) {
      return false;
    }
  }

  for (auto edge : *remap_incoming_) {
    if (!IsEdgeValid(edge, graph_) ||
        IsInOrigBBSet(edge.GetFrom()) ||
        !IsInOrigBBSet(edge.GetTo())) {
      return false;
    }
  }

  return true;
}

void SuperblockCloner::VerifyGraph() {
  for (auto it : *hir_map_) {
    HInstruction* orig_instr = it.first;
    HInstruction* copy_instr = it.second;
    if (!orig_instr->IsPhi() && !orig_instr->IsSuspendCheck()) {
      DCHECK(it.first->GetBlock() != nullptr);
    }
    if (!copy_instr->IsPhi() && !copy_instr->IsSuspendCheck()) {
      DCHECK(it.second->GetBlock() != nullptr);
    }
  }

  GraphChecker checker(graph_);
  checker.Run();
  if (!checker.IsValid()) {
    for (const std::string& error : checker.GetErrors()) {
      std::cout << error << std::endl;
    }
    LOG(FATAL) << "GraphChecker failed: superblock cloner\n";
  }
}

void DumpBBSet(const ArenaBitVector* set) {
  for (uint32_t idx : set->Indexes()) {
    std::cout << idx << "\n";
  }
}

void SuperblockCloner::DumpInputSets() {
  std::cout << "orig_bb_set:\n";
  for (uint32_t idx : orig_bb_set_.Indexes()) {
    std::cout << idx << "\n";
  }
  std::cout << "remap_orig_internal:\n";
  for (HEdge e : *remap_orig_internal_) {
    std::cout << e << "\n";
  }
  std::cout << "remap_copy_internal:\n";
  for (auto e : *remap_copy_internal_) {
    std::cout << e << "\n";
  }
  std::cout << "remap_incoming:\n";
  for (auto e : *remap_incoming_) {
    std::cout << e << "\n";
  }
}

//
// Public methods.
//

SuperblockCloner::SuperblockCloner(HGraph* graph,
                                   const HBasicBlockSet* orig_bb_set,
                                   HBasicBlockMap* bb_map,
                                   HInstructionMap* hir_map,
                                   InductionVarRange* induction_range)
  : graph_(graph),
    arena_(graph->GetAllocator()),
    orig_bb_set_(arena_, orig_bb_set->GetSizeOf(), true, kArenaAllocSuperblockCloner),
    remap_orig_internal_(nullptr),
    remap_copy_internal_(nullptr),
    remap_incoming_(nullptr),
    bb_map_(bb_map),
    hir_map_(hir_map),
    induction_range_(induction_range),
    outer_loop_(nullptr),
    outer_loop_bb_set_(arena_, orig_bb_set->GetSizeOf(), true, kArenaAllocSuperblockCloner),
    live_outs_(std::less<HInstruction*>(),
        graph->GetAllocator()->Adapter(kArenaAllocSuperblockCloner)) {
  orig_bb_set_.Copy(orig_bb_set);
}

void SuperblockCloner::SetSuccessorRemappingInfo(const HEdgeSet* remap_orig_internal,
                                                 const HEdgeSet* remap_copy_internal,
                                                 const HEdgeSet* remap_incoming) {
  remap_orig_internal_ = remap_orig_internal;
  remap_copy_internal_ = remap_copy_internal;
  remap_incoming_ = remap_incoming;
  DCHECK(CheckRemappingInfoIsValid());
}

bool SuperblockCloner::IsSubgraphClonable() const {
  // TODO: Support irreducible graphs and graphs with try-catch.
  if (graph_->HasIrreducibleLoops() || graph_->HasTryCatch()) {
    return false;
  }

  HInstructionMap live_outs(
      std::less<HInstruction*>(), graph_->GetAllocator()->Adapter(kArenaAllocSuperblockCloner));

  if (!CollectLiveOutsAndCheckClonable(&live_outs)) {
    return false;
  }

  ArenaVector<HBasicBlock*> exits(arena_->Adapter(kArenaAllocSuperblockCloner));
  SearchForSubgraphExits(&exits);

  // The only loops with live-outs which are currently supported are loops with a single exit.
  if (!live_outs.empty() && exits.size() != 1) {
    return false;
  }

  return true;
}

bool SuperblockCloner::IsFastCase() const {
  // Check that loop unrolling/loop peeling is being conducted.
  // Check that all the basic blocks belong to the same loop.
  bool flag = false;
  HLoopInformation* common_loop_info = nullptr;
  for (uint32_t idx : orig_bb_set_.Indexes()) {
    HBasicBlock* block = GetBlockById(idx);
    HLoopInformation* block_loop_info = block->GetLoopInformation();
    if (!flag) {
      common_loop_info = block_loop_info;
    } else {
      if (block_loop_info != common_loop_info) {
        return false;
      }
    }
  }

  // Check that orig_bb_set_ corresponds to loop peeling/unrolling.
  if (common_loop_info == nullptr || !orig_bb_set_.SameBitsSet(&common_loop_info->GetBlocks())) {
    return false;
  }

  bool peeling_or_unrolling = false;
  HEdgeSet remap_orig_internal(graph_->GetAllocator()->Adapter(kArenaAllocSuperblockCloner));
  HEdgeSet remap_copy_internal(graph_->GetAllocator()->Adapter(kArenaAllocSuperblockCloner));
  HEdgeSet remap_incoming(graph_->GetAllocator()->Adapter(kArenaAllocSuperblockCloner));


  // Check whether remapping info corresponds to loop unrolling.
  CollectRemappingInfoForPeelUnroll(/* to_unroll*/ true,
                                    common_loop_info,
                                    &remap_orig_internal,
                                    &remap_copy_internal,
                                    &remap_incoming);

  peeling_or_unrolling |= EdgeHashSetsEqual(&remap_orig_internal, remap_orig_internal_) &&
                          EdgeHashSetsEqual(&remap_copy_internal, remap_copy_internal_) &&
                          EdgeHashSetsEqual(&remap_incoming, remap_incoming_);

  remap_orig_internal.clear();
  remap_copy_internal.clear();
  remap_incoming.clear();

  // Check whether remapping info corresponds to loop peeling.
  CollectRemappingInfoForPeelUnroll(/* to_unroll*/ false,
                                    common_loop_info,
                                    &remap_orig_internal,
                                    &remap_copy_internal,
                                    &remap_incoming);

  peeling_or_unrolling |= EdgeHashSetsEqual(&remap_orig_internal, remap_orig_internal_) &&
                          EdgeHashSetsEqual(&remap_copy_internal, remap_copy_internal_) &&
                          EdgeHashSetsEqual(&remap_incoming, remap_incoming_);

  return peeling_or_unrolling;
}

void SuperblockCloner::Run() {
  DCHECK(bb_map_ != nullptr);
  DCHECK(hir_map_ != nullptr);
  DCHECK(remap_orig_internal_ != nullptr &&
         remap_copy_internal_ != nullptr &&
         remap_incoming_ != nullptr);
  DCHECK(IsSubgraphClonable());
  DCHECK(IsFastCase());

  if (kSuperblockClonerLogging) {
    DumpInputSets();
  }

  CollectLiveOutsAndCheckClonable(&live_outs_);
  // Find an area in the graph for which control flow information should be adjusted.
  FindAndSetLocalAreaForAdjustments();
  ConstructSubgraphClosedSSA();
  // Clone the basic blocks from the orig_bb_set_; data flow is invalid after the call and is to be
  // adjusted.
  CloneBasicBlocks();
  // Connect the blocks together/remap successors and fix phis which are directly affected my the
  // remapping.
  RemapEdgesSuccessors();

  // Check that the subgraph is connected.
  if (kIsDebugBuild) {
    HBasicBlockSet work_set(arena_, orig_bb_set_.GetSizeOf(), true, kArenaAllocSuperblockCloner);

    // Add original and copy blocks of the subgraph to the work set.
    for (auto iter : *bb_map_) {
      work_set.SetBit(iter.first->GetBlockId());   // Original block.
      work_set.SetBit(iter.second->GetBlockId());  // Copy block.
    }
    CHECK(IsSubgraphConnected(&work_set, graph_));
  }

  // Recalculate dominance and backedge information which is required by the next stage.
  AdjustControlFlowInfo(false);
  // Fix data flow of the graph.
  ResolveDataFlow();
  FixSubgraphClosedSSAAfterCloning();
}

void SuperblockCloner::CleanUp(bool keep_both_loops) {
  CleanUpControlFlow(keep_both_loops);

  // Remove phis which have all inputs being same.
  // When a block has a single predecessor it must not have any phis. However after the
  // transformation it could happen that there is such block with a phi with a single input.
  // As this is needed to be processed we also simplify phis with multiple same inputs here.
  for (auto entry : *bb_map_) {
    HBasicBlock* orig_block = entry.first;
    for (HInstructionIterator inst_it(orig_block->GetPhis()); !inst_it.Done(); inst_it.Advance()) {
      HPhi* phi = inst_it.Current()->AsPhi();
      if (ArePhiInputsTheSame(phi)) {
        phi->ReplaceWith(phi->InputAt(0));
        orig_block->RemovePhi(phi);
      }
    }

    HBasicBlock* copy_block = GetBlockCopy(orig_block);
    for (HInstructionIterator inst_it(copy_block->GetPhis()); !inst_it.Done(); inst_it.Advance()) {
      HPhi* phi = inst_it.Current()->AsPhi();
      if (ArePhiInputsTheSame(phi)) {
        phi->ReplaceWith(phi->InputAt(0));
        copy_block->RemovePhi(phi);
      }
    }
  }

  if (kIsDebugBuild) {
    VerifyGraph();
  }
}

HBasicBlock* SuperblockCloner::CloneBasicBlock(const HBasicBlock* orig_block) {
  HGraph* graph = orig_block->GetGraph();
  HBasicBlock* copy_block = new (arena_) HBasicBlock(graph, orig_block->GetDexPc());
  graph->AddBlock(copy_block);

  // Clone all the phis and add them to the map.
  for (HInstructionIterator it(orig_block->GetPhis()); !it.Done(); it.Advance()) {
    HInstruction* orig_instr = it.Current();
    HInstruction* copy_instr = orig_instr->Clone(arena_);
    copy_block->AddPhi(copy_instr->AsPhi());
    copy_instr->AsPhi()->RemoveAllInputs();
    DCHECK(!orig_instr->HasEnvironment());
    hir_map_->Put(orig_instr, copy_instr);
  }

  // Clone all the instructions and add them to the map.
  for (HInstructionIterator it(orig_block->GetInstructions()); !it.Done(); it.Advance()) {
    HInstruction* orig_instr = it.Current();
    HInstruction* copy_instr = orig_instr->Clone(arena_);
    ReplaceInputsWithCopies(copy_instr);
    copy_block->AddInstruction(copy_instr);
    if (orig_instr->HasEnvironment()) {
      DeepCloneEnvironmentWithRemapping(copy_instr, orig_instr->GetEnvironment());
    }
    hir_map_->Put(orig_instr, copy_instr);
  }

  return copy_block;
}

void SuperblockCloner::CloneBasicBlocks() {
  // By this time ReversePostOrder must be valid: in 'CloneBasicBlock' inputs of the copied
  // instructions might be replaced by copies of the original inputs (depending where those inputs
  // are defined). So the definitions of the original inputs must be visited before their original
  // uses. The property of the reducible graphs "if 'A' dom 'B' then rpo_num('A') >= rpo_num('B')"
  // guarantees that.
  for (HBasicBlock* orig_block : graph_->GetReversePostOrder()) {
    if (!IsInOrigBBSet(orig_block)) {
      continue;
    }
    HBasicBlock* copy_block = CloneBasicBlock(orig_block);
    bb_map_->Put(orig_block, copy_block);
    if (kSuperblockClonerLogging) {
      std::cout << "new block :" << copy_block->GetBlockId() << ": " << orig_block->GetBlockId() <<
                   std::endl;
    }
  }
}

// Make internal edges in copy loop as per internal edges in orig loop
void SuperblockCloner::RedirectIntenalEdges() {
  // Redirect internal edges.
  for (uint32_t orig_block_id : orig_bb_set_.Indexes()) {
    HBasicBlock* orig_block = GetBlockById(orig_block_id);

    for (HBasicBlock* orig_succ : orig_block->GetSuccessors()) {
      // Check for outgoing edge.
      if (!IsInOrigBBSet(orig_succ)) {
        continue;
      }

      // Due to construction all successors of copied block were set to original.
      HBasicBlock* copy_block = GetBlockCopy(orig_block);
      HBasicBlock* copy_succ = GetBlockCopy(orig_succ);

      if (copy_block && copy_succ) {
        copy_block->AddSuccessor(copy_succ);

        size_t orig_index = orig_succ->GetPredecessorIndexOf(orig_block);
        for (HInstructionIterator it(orig_succ->GetPhis()); !it.Done(); it.Advance()) {
          HPhi* orig_phi = it.Current()->AsPhi();
          HPhi* copy_phi = GetInstrCopy(orig_phi)->AsPhi();
          HInstruction* orig_phi_input = orig_phi->InputAt(orig_index);
          if (hir_map_->find(orig_phi_input) != hir_map_->end()) {
            HInstruction* copy_phi_input = GetInstrCopy(orig_phi_input);
            copy_phi->AddInput(copy_phi_input);
          } else {
            copy_phi->AddInput(orig_phi_input);
          }
        }
      }
    }
  }
}

// Append copy loop at exit of orig loop
// as of now, we are handling only top tested loops
bool SuperblockCloner::ReArrangeCopyLoop() {
  HBasicBlock* orig_block = nullptr;
  HBasicBlock* copy_block = nullptr;

  for (auto entry : *bb_map_) {
    orig_block = entry.first;
    copy_block = entry.second;

    if (orig_block->IsLoopHeader()) {
      HLoopInformation* loop_info = orig_block->GetLoopInformation();
      DCHECK(loop_info);
      HBasicBlock* orig_back_edge = loop_info->GetBackEdges()[0];
      DCHECK(IsInOrigBBSet(orig_back_edge));

      HBasicBlock* copy_back_edge = GetBlockCopy(orig_back_edge);
      DCHECK(copy_back_edge);

      HInstruction* inst = orig_block->GetLastInstruction();
      HIf* inst_if = inst->AsIf();
      if (inst_if) {
        HBasicBlock* true_succ = inst_if->IfTrueSuccessor();
        HBasicBlock* false_succ = inst_if->IfFalseSuccessor();

        HBasicBlock* orig_loop_exit = (true_succ == orig_back_edge)? false_succ : true_succ;

        // add empty block between orig and copy loop,
        // loop pre-header should not contain more than one successors
        HGraph* graph = orig_block->GetGraph();
        DCHECK(graph);
        ArenaAllocator* allocator = graph->GetAllocator();
        DCHECK(allocator);
        HBasicBlock* new_block = new (allocator) HBasicBlock(graph);
        DCHECK(new_block);
        graph->AddBlock(new_block);
        new_block->AddInstruction(new (allocator) HGoto(kNoDexPc));

        orig_block->ReplaceSuccessor(orig_loop_exit, new_block);
        new_block->SetDominator(orig_block);
        orig_block->AddDominatedBlock(new_block);

        new_block->AddSuccessor(copy_block);
        copy_block->SetDominator(new_block);
        new_block->AddDominatedBlock(copy_block);

        // add exit successor for copy block
        copy_block->AddSuccessor(orig_loop_exit);

        // update predecessor/successor relation beween copy loop back-edge & copy loop_header
        copy_block->SetLoopInformation(nullptr);
        copy_block->AddBackEdge(copy_back_edge);
        HLoopInformation* copy_loop_info = copy_block->GetLoopInformation();
        copy_loop_info->SetHeader(copy_block);

        // set SuspendCheck for copy loop
        HInstruction* orig_sus_check = loop_info->GetSuspendCheck();
        HSuspendCheck* copy_sus_check = GetInstrCopy(orig_sus_check)->AsSuspendCheck();
        DCHECK(copy_sus_check);
        copy_loop_info->SetSuspendCheck(copy_sus_check);

        // first input of all PHIs of copy loop header, will be PHIs in orig loop
        for (HInstructionIterator it(orig_block->GetPhis()); !it.Done(); it.Advance()) {
          HPhi* orig_phi = it.Current()->AsPhi();
          HPhi* copy_phi = GetInstrCopy(orig_phi)->AsPhi();
          DCHECK(copy_phi);

          // Copy phi doesn't yet have either orig_block as predecessor or the input that corresponds
          // to orig_block, so add the input at the end of the list.
          copy_phi->AddInput(orig_phi);
        }

        RedirectIntenalEdges();

        // update dominator of copy loop back edge
        HBasicBlock* orig_back_edge_dominator = orig_back_edge->GetDominator();
        DCHECK(IsInOrigBBSet(orig_back_edge_dominator));
        HBasicBlock* copy_back_edge_dominator = GetBlockCopy(orig_back_edge_dominator);
        DCHECK(copy_back_edge_dominator);
        copy_back_edge->SetDominator(copy_back_edge_dominator);

        return true;
      }
    }
  }

  return false;
}

// Clone a group of connected blocks (i.e. loop) and append
bool SuperblockCloner::CloneAndAppendLoop() {
  DCHECK(bb_map_ != nullptr);
  DCHECK(hir_map_ != nullptr);
  DCHECK(remap_orig_internal_ != nullptr &&
         remap_copy_internal_ != nullptr &&
         remap_incoming_ != nullptr);
  DCHECK(IsSubgraphClonable());
  DCHECK(IsFastCase());

  if (kSuperblockClonerLogging) {
    DumpInputSets();
  }

  CollectLiveOutsAndCheckClonable(&live_outs_);

  // Clone the basic blocks from the orig_bb_set_; data flow is invalid after the call and is to be
  // adjusted.
  CloneBasicBlocks();

  // append copy loop at exit of orig loop
  if (ReArrangeCopyLoop() == false)
    return false;

  // Adjust values of live_outs instructions
  for (auto live_out_it = live_outs_.begin(); live_out_it != live_outs_.end(); ++live_out_it) {
    HInstruction* value = live_out_it->first;

    if (hir_map_->find(value) == hir_map_->end()) {
      continue;
    }
    HInstruction* copy_value = GetInstrCopy(value);

    const HUseList<HInstruction*>& uses = value->GetUses();
    for (auto it = uses.begin(), end = uses.end(); it != end; /* ++it below */) {
      HInstruction* user = it->GetUser();
      size_t index = it->GetIndex();
      // Increment `it` now because `*it` may disappear thanks to user->ReplaceInput().
      ++it;
      if (!IsInOrigBBSet(user->GetBlock()) && (user != copy_value)) {
        user->ReplaceInput(copy_value, index);
      }
    }

    const HUseList<HEnvironment*>& env_uses = value->GetEnvUses();
    for (auto it = env_uses.begin(), e = env_uses.end(); it != e; /* ++it below */) {
      HEnvironment* env = it->GetUser();
      size_t index = it->GetIndex();
      ++it;
      if (!IsInOrigBBSet(env->GetHolder()->GetBlock()) && (env->GetHolder() != copy_value)) {
        env->ReplaceInput(copy_value, index);
      }
    }
  }
  return true;
}

//
// Stand-alone methods.
//

void CollectRemappingInfoForPeelUnroll(bool to_unroll,
                                       HLoopInformation* loop_info,
                                       HEdgeSet* remap_orig_internal,
                                       HEdgeSet* remap_copy_internal,
                                       HEdgeSet* remap_incoming) {
  DCHECK(loop_info != nullptr);
  HBasicBlock* loop_header = loop_info->GetHeader();
  // Set up remap_orig_internal edges set - set is empty.
  // Set up remap_copy_internal edges set.
  for (HBasicBlock* back_edge_block : loop_info->GetBackEdges()) {
    HEdge e = HEdge(back_edge_block, loop_header);
    if (to_unroll) {
      remap_orig_internal->insert(e);
      remap_copy_internal->insert(e);
    } else {
      remap_copy_internal->insert(e);
    }
  }

  // Set up remap_incoming edges set.
  if (!to_unroll) {
    remap_incoming->insert(HEdge(loop_info->GetPreHeader(), loop_header));
  }
}

bool IsSubgraphConnected(SuperblockCloner::HBasicBlockSet* work_set, HGraph* graph) {
  ArenaVector<HBasicBlock*> entry_blocks(
      graph->GetAllocator()->Adapter(kArenaAllocSuperblockCloner));

  // Find subgraph entry blocks.
  for (uint32_t orig_block_id : work_set->Indexes()) {
    HBasicBlock* block = graph->GetBlocks()[orig_block_id];
    for (HBasicBlock* pred : block->GetPredecessors()) {
      if (!work_set->IsBitSet(pred->GetBlockId())) {
        entry_blocks.push_back(block);
        break;
      }
    }
  }

  for (HBasicBlock* entry_block : entry_blocks) {
    if (work_set->IsBitSet(entry_block->GetBlockId())) {
      TraverseSubgraphForConnectivity(entry_block, work_set);
    }
  }

  // Return whether there are unvisited - unreachable - blocks.
  return work_set->NumSetBits() == 0;
}

HLoopInformation* FindCommonLoop(HLoopInformation* loop1, HLoopInformation* loop2) {
  if (loop1 == nullptr || loop2 == nullptr) {
    return nullptr;
  }

  if (loop1->IsIn(*loop2)) {
    return loop2;
  }

  HLoopInformation* current = loop1;
  while (current != nullptr && !loop2->IsIn(*current)) {
    current = current->GetPreHeader()->GetLoopInformation();
  }

  return current;
}

bool PeelUnrollHelper::IsLoopClonable(HLoopInformation* loop_info) {
  PeelUnrollHelper helper(
      loop_info, /* bb_map= */ nullptr, /* hir_map= */ nullptr, /* induction_range= */ nullptr);
  return helper.IsLoopClonable();
}

HBasicBlock* PeelUnrollHelper::DoPeelUnrollImpl(bool to_unroll) {
  // For now do peeling only for natural loops.
  DCHECK(!loop_info_->IsIrreducible());

  HBasicBlock* loop_header = loop_info_->GetHeader();
  // Check that loop info is up-to-date.
  DCHECK(loop_info_ == loop_header->GetLoopInformation());
  HGraph* graph = loop_header->GetGraph();

  if (kSuperblockClonerLogging) {
    std::cout << "Method: " << graph->PrettyMethod() << std::endl;
    std::cout << "Scalar loop " << (to_unroll ? "unrolling" : "peeling") <<
                 " was applied to the loop <" << loop_header->GetBlockId() << ">." << std::endl;
  }

  ArenaAllocator allocator(graph->GetAllocator()->GetArenaPool());

  HEdgeSet remap_orig_internal(graph->GetAllocator()->Adapter(kArenaAllocSuperblockCloner));
  HEdgeSet remap_copy_internal(graph->GetAllocator()->Adapter(kArenaAllocSuperblockCloner));
  HEdgeSet remap_incoming(graph->GetAllocator()->Adapter(kArenaAllocSuperblockCloner));

  CollectRemappingInfoForPeelUnroll(to_unroll,
                                    loop_info_,
                                    &remap_orig_internal,
                                    &remap_copy_internal,
                                    &remap_incoming);

  cloner_.SetSuccessorRemappingInfo(&remap_orig_internal, &remap_copy_internal, &remap_incoming);
  cloner_.Run();
  cloner_.CleanUp(false);

  // Check that loop info is preserved.
  DCHECK(loop_info_ == loop_header->GetLoopInformation());

  return loop_header;
}

// Main function for partial loop unrolling with unknown iterations
// As of now, we are handling 2 blocks for unknown iterations
HBasicBlock* PeelUnrollHelper::DoPartialUnrolling(int iterations ATTRIBUTE_UNUSED, int unroll_factor) {
  // For now do peeling only for natural loops.
  DCHECK(!loop_info_->IsIrreducible());

  HBasicBlock* loop_header = loop_info_->GetHeader();
  // Check that loop info is up-to-date.
  DCHECK(loop_info_ == loop_header->GetLoopInformation());

  HGraph* graph = loop_header->GetGraph();
  DCHECK(graph);

  // Identify loop induction variable
  // Modify  loop iteration value as "(n - m) % unroll_factor"
  // Add new instructions to loop preheader
  HInstruction* phi_induc = AddLoopUnrollEpilogue(graph);
  if (phi_induc == nullptr) {
    return nullptr;
  }

  ArenaAllocator allocator(graph->GetAllocator()->GetArenaPool());

  HEdgeSet remap_orig_internal(graph->GetAllocator()->Adapter(kArenaAllocSuperblockCloner));
  HEdgeSet remap_copy_internal(graph->GetAllocator()->Adapter(kArenaAllocSuperblockCloner));
  HEdgeSet remap_incoming(graph->GetAllocator()->Adapter(kArenaAllocSuperblockCloner));

  CollectRemappingInfoForPeelUnroll(true,
                                    loop_info_,
                                    &remap_orig_internal,
                                    &remap_copy_internal,
                                    &remap_incoming);

  cloner_.SetSuccessorRemappingInfo(&remap_orig_internal, &remap_copy_internal, &remap_incoming);

  if (!(cloner_.CloneAndAppendLoop())) {
    return nullptr;
  }

  // Modify loop induction condition for partial loop unrolling
  ModifyLoopInductionPartialUnroll(graph, phi_induc, unroll_factor);

  // initial value of copy loop's induction variable and
  // replicate orig loop body instructions as per unroll_factor
  AdjustLoops(graph, phi_induc, unroll_factor);

  // cloner cleanup
  cloner_.CleanUpControlFlow(true);

  // Check that loop info is preserved.
  DCHECK(loop_info_ == loop_header->GetLoopInformation());

  return loop_header;
}


// Identify loop induction variable
// Modify  loop iteration value as "(n - m) % unroll_factor"
// Add new instructions to loop preheader
HInstruction* PeelUnrollHelper::AddLoopUnrollEpilogue(HGraph* graph)  {
  DCHECK(loop_info_ != nullptr);

  // get allocator
  ArenaAllocator* allocator = graph->GetAllocator();
  DCHECK(allocator);

  // get loop header & preheader
  HBasicBlock* header = loop_info_->GetHeader();
  HBasicBlock* preheader = loop_info_->GetPreHeader();
  if ((header == nullptr) || (preheader == nullptr)) {
    return nullptr;
  }

  HInstruction* init_induc_val = nullptr;
  HInstruction* phi_induc = nullptr;
  HInstruction* cond_val = nullptr;

  // get index of back-edge block
  HBasicBlock* body_blk = loop_info_->GetBackEdges()[0];
  size_t back_edge_index = header->GetPredecessorIndexOf(body_blk);

  // identify loop induction variable, induction condition and
  // value against which induction variable is being checked
  HIf* hif = header->GetLastInstruction()->AsIf();
  inst_induction_cond = hif->InputAt(0);
  DCHECK(inst_induction_cond);

  HInputsRef inputs = inst_induction_cond->GetInputs();
  for (size_t i = 0; i < inputs.size(); ++i) {
    HInstruction* inst_input = inst_induction_cond->InputAt(i);
    if (inst_input->IsPhi() && (inst_input->GetBlock() == header)) {
      phi_induc = inst_induction_cond->InputAt(i)->AsPhi();
      cond_val = inst_induction_cond->InputAt(1 - i);
      induc_cond_val_index = 1 - i;
      break;
    }
  }

  if ((phi_induc == nullptr) || (cond_val == nullptr)) {
    return nullptr;
  }

  // "cond_val" in header block means that it's  value is somehow
  // dependant (& can't be determined) on previous loop iteration
  if (cond_val->GetBlock() == header) {
    return nullptr;
  }

  // initial value of loop induction variable
  init_induc_val = phi_induc->InputAt(0);
  if (init_induc_val == nullptr) {
    return nullptr;
  }

  // get the type of cond_val and init_induc_val
  DataType::Type cond_val_type = cond_val->GetType();
  DataType::Type init_induc_type = init_induc_val->GetType();

  // Don't support reference and void types for induction var
  // init value and condition
  if ((cond_val_type == DataType::Type::kReference) ||
      (cond_val_type == DataType::Type::kVoid) ||
      (init_induc_type == DataType::Type::kReference) ||
      (init_induc_type == DataType::Type::kVoid) ||
      (phi_induc->GetType() == DataType::Type::kReference) ||
      (phi_induc->GetType() == DataType::Type::kVoid)) {
    return nullptr;
  }

  // get the value of induction advancing variable
  HInstruction* phi_input = phi_induc->InputAt(back_edge_index);
  DCHECK(phi_input);
  size_t iter_phi_ip = 0;
  HInputsRef inputs_back_edge = phi_input->GetInputs();
  for (iter_phi_ip = 0; iter_phi_ip < inputs_back_edge.size(); ++iter_phi_ip) {
    HInstruction* inst_input = phi_input->InputAt(iter_phi_ip);

    if ((inst_input == phi_induc) && (phi_input->IsBinaryOperation())) {
      inst_induction_op = phi_input;
      induc_op_phi_index = iter_phi_ip;
      inst_induction_op_val = phi_input->InputAt(1 - iter_phi_ip);
      break;
    }
  }

  if ((inst_induction_op == nullptr) || (inst_induction_op_val == nullptr)) {
    return nullptr;
  }

  // the value by which induction var is incremented/decremented,
  // should either be in header or block dominating the header
  HBasicBlock* inst_induction_op_val_blk = inst_induction_op_val->GetBlock();
  if ((inst_induction_op_val_blk != header) && !(inst_induction_op_val_blk->Dominates(header))) {
    return nullptr;
  }

  // Support Mul only If induction advancing variable is constant,
  // otherwise support add, sub & Shl operations only
  if (inst_induction_op_val->IsConstant()) {
    if (!(inst_induction_op->IsAdd() || inst_induction_op->IsSub() ||
          inst_induction_op->IsMul() || inst_induction_op->IsShl())) {
      return nullptr;
    }
  } else {
    if (!(inst_induction_op->IsAdd() || inst_induction_op->IsSub() ||
          inst_induction_op->IsShl())) {
      return nullptr;
    }
  }

  return phi_induc;
}

/* Suppose loop looks like "for (i = a; i <comp_op> b; i <advance_op> c)"
   For above loop, Add instructions for computation of "abs(b - a)" and "(abs(c) * unroll_factor)"
   For add & sub operations, Execute partially unrolled loop only if "abs(b - a)"
   (i.e. loop iterations) > "(abs(c) * unroll_factor)".
   For Mul & Shl operations, Excecute partially unrolled loop only if "(c * unroll_factor)" > "b".
*/
void PeelUnrollHelper::ModifyLoopInductionPartialUnroll(HGraph* graph, HInstruction* phi_induc, int unroll_factor) {
  // get allocator
  ArenaAllocator* allocator = graph->GetAllocator();
  DCHECK(allocator);

  // get loop header & preheader
  HBasicBlock* header = loop_info_->GetHeader();
  HBasicBlock* preheader = loop_info_->GetPreHeader();
  HInstruction* inst_cond_val = inst_induction_cond->InputAt(induc_cond_val_index);

  HInstruction* inst_check_val = nullptr;
  HInstruction* inst_unroll_factor2 = nullptr;
  if (inst_induction_op->IsAdd() || inst_induction_op->IsSub() || inst_induction_op->IsShl()) {
    DataType::Type inst_induction_op_val_type = inst_induction_op_val->GetType();
    if (DataType::IsIntegralType(inst_induction_op_val_type)) {
      inst_unroll_factor2 = graph->GetConstant(inst_induction_op_val->GetType(), unroll_factor);
    } else if (inst_induction_op_val_type == DataType::Type::kFloat32) {
      inst_unroll_factor2 = graph->GetFloatConstant(unroll_factor, kNoDexPc);
    } else if (inst_induction_op_val_type == DataType::Type::kFloat64) {
      inst_unroll_factor2 = graph->GetDoubleConstant(unroll_factor, kNoDexPc);
    } else {
      HInstruction* inst_unroll_factor = graph->GetConstant(DataType::Type::kInt32, unroll_factor);
      inst_unroll_factor2 = new (allocator) HTypeConversion(inst_induction_op_val->GetType(), inst_unroll_factor, kNoDexPc);
      // add instruction to loop preheader
      preheader->InsertInstructionBefore(inst_unroll_factor2, preheader->GetLastInstruction());
    }

    HInstruction* inst_abs_induction_op_val = nullptr;
    if (inst_induction_op->IsAdd() || inst_induction_op->IsSub()) {
      inst_abs_induction_op_val = new (allocator) HAbs(inst_induction_op_val->GetType(), inst_induction_op_val, kNoDexPc);
      // add instruction to loop preheader
      preheader->InsertInstructionBefore(inst_abs_induction_op_val, preheader->GetLastInstruction());
    } else {  // "Shl" case
      inst_abs_induction_op_val = inst_induction_op_val;
    }

    inst_check_val = new (allocator) HMul(inst_induction_op_val->GetType(), inst_abs_induction_op_val, inst_unroll_factor2);
    // add instruction to loop preheader
    preheader->InsertInstructionBefore(inst_check_val, preheader->GetLastInstruction());
  }

  // for add, sub operations
  HInstruction* inst_final_sub = nullptr;
  // for mul, shl operations
  HInstruction* inst_unrolled_advance_val = nullptr;

  bool is_induc_advance_positive = false;
  if (inst_induction_op_val->IsConstant()) {
    int64_t induc_advance_val = 0;
    if (inst_induction_op_val->IsIntConstant()) {
      induc_advance_val = inst_induction_op_val->AsIntConstant()->GetValue();
    } else if (inst_induction_op_val->IsLongConstant()) {
      induc_advance_val = inst_induction_op_val->AsLongConstant()->GetValue();
    } else if (inst_induction_op_val->IsFloatConstant()) {
      induc_advance_val = (int64_t) (inst_induction_op_val->AsFloatConstant()->GetValue());
    } else if (inst_induction_op_val->IsDoubleConstant()) {
      induc_advance_val = (int64_t) (inst_induction_op_val->AsDoubleConstant()->GetValue());
    }

    if (induc_advance_val >= 0) {
      if (inst_induction_op->IsAdd() || inst_induction_op->IsMul() || inst_induction_op->IsShl()) {
        is_induc_advance_positive = true;
      }
    } else {
      if (inst_induction_op->IsSub()) {
        is_induc_advance_positive = true;
      }
    }

    if (inst_induction_op->IsAdd() || inst_induction_op->IsSub()) {
      if (is_induc_advance_positive) {
        inst_final_sub = new (allocator) HSub(phi_induc->GetType(), inst_cond_val, phi_induc);
      } else {
        inst_final_sub = new (allocator) HSub(phi_induc->GetType(), phi_induc, inst_cond_val);
      }
    } else if (inst_induction_op->IsMul()) {
      // calculate advance value for partially unrolled loop
      int64_t val_pow = 1;
      for (int iter = 0; iter < unroll_factor; iter++) {
        val_pow = val_pow * induc_advance_val;
      }

      HInstruction* inst_val_pow = nullptr;
      DataType::Type phi_induc_type = phi_induc->GetType();
      if (DataType::IsIntegralType(phi_induc_type)) {
        inst_val_pow = graph->GetConstant(phi_induc_type, val_pow);
      } else if (phi_induc_type == DataType::Type::kFloat32) {
        inst_val_pow = graph->GetFloatConstant(val_pow, kNoDexPc);
      } else if (phi_induc_type == DataType::Type::kFloat64) {
        inst_val_pow = graph->GetDoubleConstant(val_pow, kNoDexPc);
      } else {
        HInstruction* inst_val_pow1 = graph->GetConstant(DataType::Type::kInt32, val_pow);
        inst_val_pow = new (allocator) HTypeConversion(phi_induc_type, inst_val_pow1, kNoDexPc);
        // add instruction to loop preheader
        preheader->InsertInstructionBefore(inst_val_pow, preheader->GetLastInstruction());
      }

      inst_unrolled_advance_val = new (allocator) HMul(phi_induc_type, phi_induc, inst_val_pow);
      DCHECK(inst_unrolled_advance_val);
      // add instruction to loop header
      header->InsertInstructionBefore(inst_unrolled_advance_val, inst_induction_cond);
    } else if (inst_induction_op->IsShl()) {
      inst_unrolled_advance_val = new (allocator) HShl(phi_induc->GetType(), phi_induc, inst_check_val);
      DCHECK(inst_unrolled_advance_val);
      // add instruction to loop header
      header->InsertInstructionBefore(inst_unrolled_advance_val, inst_induction_cond);
    }
  } else {  // only add, sub are supported when induction advancing variable isn't constant
    is_induc_advance_positive = true;

    if (inst_induction_op->IsAdd() || inst_induction_op->IsSub()) {
      HInstruction* inst_new_comp = new (allocator) HGreaterThanOrEqual(phi_induc, inst_cond_val);
      // add instruction to loop header
      header->InsertInstructionBefore(inst_new_comp, inst_induction_cond);

      HInstruction* inst_sub1 = new (allocator) HSub(phi_induc->GetType(), inst_cond_val, phi_induc);
      // add instruction to loop header
      header->InsertInstructionBefore(inst_sub1, inst_induction_cond);

      HInstruction* inst_sub2 = new (allocator) HSub(phi_induc->GetType(), phi_induc, inst_cond_val);
      // add instruction to loop header
      header->InsertInstructionBefore(inst_sub2, inst_induction_cond);

      inst_final_sub = new (allocator) HSelect(inst_new_comp, inst_sub2, inst_sub1, kNoDexPc);
    } else if (inst_induction_op->IsShl()) {
      inst_unrolled_advance_val = new (allocator) HShl(phi_induc->GetType(), phi_induc, inst_check_val);
      DCHECK(inst_unrolled_advance_val);
      // add instruction to loop header
      header->InsertInstructionBefore(inst_unrolled_advance_val, inst_induction_cond);
    }
  }

  if (inst_induction_op->IsAdd() || inst_induction_op->IsSub()) {
    DCHECK(inst_final_sub);
    // add instruction to loop header
    header->InsertInstructionBefore(inst_final_sub, inst_induction_cond);

    HInstruction* inst_check_val2 = nullptr;
    if (inst_check_val->GetType() != inst_final_sub->GetType()) {
      inst_check_val2 = new (allocator) HTypeConversion(inst_final_sub->GetType(), inst_check_val, kNoDexPc);
      // add instruction to loop preheader
      preheader->InsertInstructionBefore(inst_check_val2, preheader->GetLastInstruction());
    } else {
      inst_check_val2 = inst_check_val;
    }

    if (inst_induction_cond->IsEqual() || inst_induction_cond->IsNotEqual()) {
      HInstruction* inst_new_cond = nullptr;
      inst_new_cond = new (allocator) HGreaterThan(inst_final_sub, inst_check_val2);
      DCHECK(inst_new_cond);
      header->InsertInstructionBefore(inst_new_cond, inst_induction_cond);

      // Replace and remove.
      inst_induction_cond->ReplaceWith(inst_new_cond);
      inst_induction_cond->GetBlock()->RemoveInstruction(inst_induction_cond);
    } else {
      // replace old cond val with new cond val
      if (is_induc_advance_positive) {
        inst_induction_cond->ReplaceInput(inst_final_sub, induc_cond_val_index);
        inst_induction_cond->ReplaceInput(inst_check_val2, 1 - induc_cond_val_index);
      } else {
        inst_induction_cond->ReplaceInput(inst_final_sub, 1 - induc_cond_val_index);
        inst_induction_cond->ReplaceInput(inst_check_val2, induc_cond_val_index);
      }
    }
  } else if ((inst_induction_op_val->IsConstant() && inst_induction_op->IsMul()) || inst_induction_op->IsShl()) {
    if (inst_induction_cond->IsEqual() || inst_induction_cond->IsNotEqual()) {
      HInstruction* inst_new_cond = nullptr;
      inst_new_cond = new (allocator) HGreaterThan(inst_cond_val, inst_unrolled_advance_val);
      DCHECK(inst_new_cond);
      header->InsertInstructionBefore(inst_new_cond, inst_induction_cond);

      // Replace and remove.
      inst_induction_cond->ReplaceWith(inst_new_cond);
      inst_induction_cond->GetBlock()->RemoveInstruction(inst_induction_cond);
    } else {
      // replace old cond val with new cond val
      inst_induction_cond->ReplaceInput(inst_unrolled_advance_val, 1 - induc_cond_val_index);
    }
  }
}

// Modify orig loop induction variable condition,
// initial value of copy loop's induction variable and
// replicate orig loop body instructions as per unroll_factor
bool PeelUnrollHelper::AdjustLoops(HGraph* graph, HInstruction* phi_induc, int unroll_factor)  {
  DCHECK(loop_info_ != nullptr);
  DCHECK(phi_induc != nullptr);
  DCHECK(graph != nullptr);

  // get allocator
  ArenaAllocator* allocator = graph->GetAllocator();
  DCHECK(allocator);

  // get loop header
  HBasicBlock* header = loop_info_->GetHeader();
  // only one back edge.
  HBasicBlock* body_blk = loop_info_->GetBackEdges()[0];

  // check the value of induction variable operation i.e. "K" in "inst_induction_op <op> K"
  DCHECK(inst_induction_cond);
  DCHECK(inst_induction_op_val);

  // for orig loop, copy and insert all instructions (insert new instructions in group)
  HBasicBlock* copy_body_blk = cloner_.GetBlockCopy(body_blk);
  DCHECK(copy_body_blk);

  // add instruction mapping into map
  HInstructionMap latest_instr_map(std::less<HInstruction*>(),
      allocator->Adapter(kArenaAllocSuperblockCloner));

  // vector to keep track of already moved instructions (avoid deadlock)
  std::vector<HInstruction*> vec_instr_moved;
  std::vector<HInstruction*> vec_instr_phi;
  for (HInstructionIterator it(copy_body_blk->GetInstructions()); !it.Done(); ) {
    HInstruction* instr = it.Current();
    DCHECK(instr);

    it.Advance();

    if ((instr->GetInputs().size() > 1)) {
      if (std::find(vec_instr_moved.begin(), vec_instr_moved.end(), instr) == vec_instr_moved.end()) {
        vec_instr_moved.push_back(instr);

        // add instruction mapping into map
        // values will be used during resolution of cloned instruction inputs
        HInstruction* instr_orig = cloner_.GetInstrOrig(instr);
        for (HInstructionIterator it1(header->GetPhis()); !it1.Done(); it1.Advance()) {
          HPhi* instr_phi = it1.Current()->AsPhi();
          DCHECK(instr_phi);
          HInputsRef inputs = instr_phi->GetInputs();
          for (size_t i = 0; i < inputs.size(); ++i) {
            HInstruction* phi_input = instr_phi->InputAt(i);
            if (phi_input == instr_orig) {
              latest_instr_map.FindOrAdd(instr_phi, instr_orig);
              if (std::find(vec_instr_phi.begin(), vec_instr_phi.end(), instr_orig) == vec_instr_phi.end()) {
                vec_instr_phi.push_back(instr_orig);
              }
              break;
            }
          }
        }
      }
    }
  }

  // Replicate instuctions in body block "unroll_factor - 1" times
  // For instructions with PHI as 1st input, first store mapping in temp vector
  // and then update in "latest_instr_map" at end of current iteration
  std::map<HInstruction*, HInstruction*> temp_phi_map;
  for (int iter = 1; iter < unroll_factor; iter++) {
    for (HInstructionIterator it(copy_body_blk->GetInstructions()); !it.Done(); it.Advance()) {
      HInstruction* instr = it.Current();
      DCHECK(instr);

      // no need to duplicate Goto instruction
      if (instr->IsGoto()) continue;

      HInstruction* instr_orig = cloner_.GetInstrOrig(instr);
      HInstruction* copy_instr = instr_orig->Clone(allocator);
      DCHECK(copy_instr);

      // insert new instruction before last "Goto" instruction
      body_blk->InsertInstructionBefore(copy_instr, body_blk->GetLastInstruction());
      if (instr_orig->HasEnvironment()) {
        copy_instr->CopyEnvironmentFrom(instr_orig->GetEnvironment());
      }

      // copy_instr also has same inputs as instr_orig.
      // as per our previous checks, "inst_induction_op" is binary operation
      if (instr_orig == inst_induction_op) {
        HInstruction* new_inst_induction_op_val = nullptr;
        HInstruction* cur_unroll_val = nullptr;

        // get constant (for current val of unroll factor) with proper type
        DataType::Type inst_induction_op_val_type = inst_induction_op_val->GetType();
        if (DataType::IsIntegralType(inst_induction_op_val_type)) {
          cur_unroll_val = graph->GetConstant(inst_induction_op_val_type, (iter+1));
        } else if (inst_induction_op_val_type == DataType::Type::kFloat32) {
          cur_unroll_val = graph->GetFloatConstant((iter+1), kNoDexPc);
        } else if (inst_induction_op_val_type == DataType::Type::kFloat64) {
          cur_unroll_val = graph->GetDoubleConstant((iter+1), kNoDexPc);
        } else {
          HInstruction* inst_cur_unroll_val = graph->GetConstant(DataType::Type::kInt32, (iter+1));
          cur_unroll_val = new (allocator) HTypeConversion(inst_induction_op_val_type, inst_cur_unroll_val, kNoDexPc);
          DCHECK(cur_unroll_val);
          body_blk->InsertInstructionBefore(cur_unroll_val, copy_instr);
        }

        new_inst_induction_op_val = new (allocator) HMul(inst_induction_op_val_type, inst_induction_op_val, cur_unroll_val);
        DCHECK(new_inst_induction_op_val);

        body_blk->InsertInstructionBefore(new_inst_induction_op_val, copy_instr);
        // replace new value for induction operation
        copy_instr->ReplaceInput(new_inst_induction_op_val, 1 - induc_op_phi_index);
      } else {
        // Iterate through inputs and replace inputs with latest relevent instruction
        HInputsRef inputs = copy_instr->GetInputs();
        for (size_t i = 0; i < inputs.size(); ++i) {
          HInstruction* inst_input = copy_instr->InputAt(i);
          if (latest_instr_map.find(inst_input) != latest_instr_map.end()) {
            HInstruction* new_inst = latest_instr_map.Get(inst_input);
            copy_instr->ReplaceInput(new_inst, i);
          }
        }
      }

      // Update map with latest value for instr_orig
      if (latest_instr_map.find(instr_orig) != latest_instr_map.end()) {
        latest_instr_map.Overwrite(instr_orig, copy_instr);
      } else {
        latest_instr_map.FindOrAdd(instr_orig, copy_instr);
      }

      if ((instr_orig->GetInputs().size() > 1) && (std::find(vec_instr_phi.begin(), vec_instr_phi.end(), instr_orig) != vec_instr_phi.end())) {
        for (HInstructionIterator it2(header->GetPhis()); !it2.Done(); it2.Advance()) {
          HPhi* instr_phi = it2.Current()->AsPhi();
          DCHECK(instr_phi);
          HInputsRef inputs_phi = instr_phi->GetInputs();
          for (size_t i = 0; i < inputs_phi.size(); ++i) {
            HInstruction* inst_input = instr_phi->InputAt(i);
            if (inst_input == instr_orig) {
              temp_phi_map.insert(std::pair<HInstruction*, HInstruction*>(instr_phi, copy_instr));
              break;
            }
          }
        }
      }
    }

    // update PHI mappings from "temp_phi_map" to "latest_instr_map" at end of iteration
    std::map<HInstruction*, HInstruction*>::iterator phi_map_iter;
    for (phi_map_iter = temp_phi_map.begin(); phi_map_iter != temp_phi_map.end(); ++phi_map_iter) {
      if (latest_instr_map.find(phi_map_iter->first) != latest_instr_map.end()) {
        latest_instr_map.Overwrite(phi_map_iter->first, phi_map_iter->second);
      } else {
        latest_instr_map.FindOrAdd(phi_map_iter->first, phi_map_iter->second);
      }
    }
    temp_phi_map.clear();
  }

  vec_instr_phi.clear();

  // update most recent values (in body block) of PHIs in header block
  for (HInstructionIterator it(header->GetPhis()); !it.Done(); it.Advance()) {
    HPhi* instr_phi = (it.Current())->AsPhi();
    DCHECK(instr_phi);

    size_t index = header->GetPredecessorIndexOf(body_blk);
    HInstruction* old_input = instr_phi->InputAt(index);
    DCHECK(old_input);

    if (latest_instr_map.find(old_input) != latest_instr_map.end()) {
      HInstruction* new_inst = latest_instr_map.Get(old_input);
      DCHECK(new_inst);
      instr_phi->ReplaceInput(new_inst, index);
    }
  }

  // for copy loop, rearrange header's Phi's input as per predecessors order
  HBasicBlock* copy_header_blk = cloner_.GetBlockCopy(header);
  DCHECK(copy_header_blk);
  graph->OrderLoopHeaderPredecessors(copy_header_blk);

  // populate copy loop
  HLoopInformation* loop_info = copy_header_blk->GetLoopInformation();
  if (loop_info != nullptr) {
    loop_info->Populate();
  }

  return true;
}

PeelUnrollSimpleHelper::PeelUnrollSimpleHelper(HLoopInformation* info,
                                               InductionVarRange* induction_range)
  : bb_map_(std::less<HBasicBlock*>(),
            info->GetHeader()->GetGraph()->GetAllocator()->Adapter(kArenaAllocSuperblockCloner)),
    hir_map_(std::less<HInstruction*>(),
             info->GetHeader()->GetGraph()->GetAllocator()->Adapter(kArenaAllocSuperblockCloner)),
    helper_(info, &bb_map_, &hir_map_, induction_range) {}

}  // namespace art

namespace std {

ostream& operator<<(ostream& os, const art::HEdge& e) {
  e.Dump(os);
  return os;
}

}  // namespace std
