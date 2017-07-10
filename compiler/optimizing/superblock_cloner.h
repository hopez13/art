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

#ifndef ART_COMPILER_OPTIMIZING_SUPERBLOCK_CLONER_H_
#define ART_COMPILER_OPTIMIZING_SUPERBLOCK_CLONER_H_

#include "base/arena_bit_vector.h"
#include "base/arena_containers.h"
#include "base/bit_vector-inl.h"
#include "nodes.h"

namespace art {

// Represents an edge between two HBasicBlocks.
class HEdge : public ArenaObject<kArenaAllocSuperblockCloner> {
 public:
  HEdge(HBasicBlock* from, HBasicBlock* to) : from_(from->GetBlockId()), to_(to->GetBlockId()) {
    DCHECK_NE(to_, kInvalidBlockId);
    DCHECK_NE(from_, kInvalidBlockId);
  }
  HEdge(uint32_t from, uint32_t to) : from_(from), to_(to) {
    DCHECK_NE(to_, kInvalidBlockId);
    DCHECK_NE(from_, kInvalidBlockId);
  }
  HEdge() : from_(kInvalidBlockId), to_(kInvalidBlockId) {}

  uint32_t GetFrom() const { return from_; }
  uint32_t GetTo() const { return to_; }

  bool operator==(const HEdge& other) const {
    return this->from_ == other.from_ && this->to_ == other.to_;
  }

  bool operator!=(const HEdge& other) const { return !operator==(other); }
  void Dump(std::ostream& stream) const;

  // Return whether an edge represents a valid edge in CF graph: whether the from_ block
  // has to_ block as a successor.
  bool IsValid() const { return from_ != kInvalidBlockId && to_ != kInvalidBlockId; }

 private:
  // Predecessor block id.
  uint32_t from_;
  // Successor block id.
  uint32_t to_;
};

// TODO: Investigated optimal types for the containers.
using HBasicBlockMap = ArenaSafeMap<HBasicBlock*, HBasicBlock*>;
using HInstructionMap = ArenaSafeMap<HInstruction*, HInstruction*>;
using HBasicBlockSet = ArenaBitVector;
using HEdgeSet = ArenaHashSet<HEdge>;

// Return a clone of a basic block (orig_block) and inserts it into the graph.
//
//  - The copy block will have no successors/predecessors; they should be set up manually.
//  - For each instruction in the orig_block a copy is created and inserted into the copy block;
//    this correspondence is recorded in the map (old instruction, new instruction).
//  - Graph HIR is not valid after this transformation: all of the HIRs have their inputs the same,
//    as in the original block, PHIs do not reflect a correct correspondence between the value and
//    predecessors (as the copy block has no predecessors by now), etc.
HBasicBlock* CloneBasicBlock(const HBasicBlock* orig_block, HInstructionMap *map);

// SuperblockCloner provides a feature of cloning subgraphs in a smart, high level way without
// fine grain manipulation with IR; data flow and graph properties are resolved/adjusted
// automatically. The clone transformation is defined by specifying a set of basic blocks to copy
// and a set of rules how to treat edges, remap their successors.
//
// The idea of the transformation is based on "Superblock cloning" technique described in the book
// "Engineering a Compiler. Second Edition", Keith D. Cooper, Linda Torczon, Rice University
// Houston, Texas. 2nd edition, Morgan Kaufmann. The original paper is "The Superblock: An Efective
// Technique for VLIW and Superscalar Compilation" by Hwu, W.M.W., Mahlke, S.A., Chen, W.Y. et al.
// J Supercomput (1993) 7: 229. doi:10.1007/BF01205185.
//
// By using this approach such optimizations as Branch Target Expansion, Loop Peeling,
// Loop Unrolling can be implemented.
//
// There are two states of the IR graph: original graph (before the transformation) and
// copy graph (after).
//
// Before the transformation:
// Defining a set of basic block to copy (orig_bb_set) partitions all of the edges in the original
// graph into 4 categories/sets (use the following notation for edges: "(pred, succ)",
// where pred, succ - basic blocks):
//  - internal - pred, succ are members of ‘orig_bb_set’.
//  - outside  - pred, succ are not members of ‘orig_bb_set’.
//  - incoming - pred is not a member of ‘orig_bb_set’, succ is.
//  - outgoing - pred is a member of ‘orig_bb_set’, succ is not.
//
// Transformation:
//
// 1. Initial cloning:
//   1.1. For each ‘orig_block’ in orig_bb_set create a copy ‘copy_block’; these new blocks
//        form ‘copy_bb_set’.
//   1.2. For each edge (X, Y) from internal set create an edge (X_1, Y_1) where X_1, Y_1 are the
//        copies of X, Y basic blocks correspondingly; these new edges form ‘copy_internal’ edge
//        set.
//   1.3. For each edge (X, Y) from outgoing set create an edge (X_1, Y_1) where X_1, Y_1 are the
//        copies of X, Y basic blocks correspondingly; these new edges form ‘copy_outgoing’ edge
//        set.
// 2. Successors remapping.
//   2.1. 'remap_orig_internal’ - set of edges (X, Y) from ‘orig_bb_set’ whose successors should
//        be remapped to copy nodes: ((X, Y) will be transformed into (X, Y_1)).
//   2.2. ‘remap_copy_internal’ - set of edges (X_1, Y_1) from ‘copy_bb_set’ whose successors
//        should be remapped to copy nodes: (X_1, Y_1) will be transformed into (X_1, Y)).
//   2.3. 'remap_incoming’ - set of edges (X, Y) from the ‘incoming’ edge set in the original graph
//        whose successors should be remapped to copies nodes: ((X, Y) will be transformed into
//        (X, Y_1)).
// 3. Adjust control flow structures and relations (dominance, reverse post order, loops, etc).
// 4. Fix/resolve data flow.
// 5. Do cleanups (DCE, critical edges splitting, etc).
//
class SuperblockCloner: public ValueObject {
 public:
  SuperblockCloner(HGraph* graph,
                   const HBasicBlockSet* orig_bb_set,
                   HBasicBlockMap* bb_map,
                   HInstructionMap* hir_map);

  void SetSuccessorRemappingInfo(const HEdgeSet* remap_orig_internal,
                                 const HEdgeSet* remap_copy_internal,
                                 const HEdgeSet* remap_incoming);

  // Run the copy algorithm according to the description.
  void Run();

  // Return whether the specified subgraph is copyable.
  // TODO: Start from small range of graph patterns then extend it.
  bool IsSubgraphCopyable() const { return true; }

 private:
  void CloneBasicBlocks();
  void RemapEdgesSuccessors();
  void RecalculateControlFlowInfo();
  void ResolveDataFlow();
  void CleanUp();

  HGraph* graph_;
  ArenaAllocator* arena_;

  // Set of basic block in the original graph to be copied.
  const HBasicBlockSet* orig_bb_set_;

  // Sets of edges which require successors remapping.
  const HEdgeSet* remap_orig_internal_;
  const HEdgeSet* remap_copy_internal_;
  const HEdgeSet* remap_incoming_;

  // Correspondence map for blocks: (original block, copy block).
  HBasicBlockMap* bb_map_;
  // Correspondence map for instructions: (original HInstruction, copy HInstruction).
  HInstructionMap* hir_map_;

  DISALLOW_COPY_AND_ASSIGN(SuperblockCloner);
};

}  // namespace art

namespace std {
  template <>
  struct hash<art::HEdge> {
    size_t operator()(art::HEdge const& x) const noexcept;
  };
  ostream& operator<<(ostream& os, const art::HEdge& e);
}  // namespace std

#endif  // ART_COMPILER_OPTIMIZING_SUPERBLOCK_CLONER_H_
