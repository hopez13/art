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

#ifndef ART_COMPILER_OPTIMIZING_LOOP_ANALYSIS_H_
#define ART_COMPILER_OPTIMIZING_LOOP_ANALYSIS_H_

#include "base/arena_object.h"
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

// Return whether a HEdge edge correspond to an existing edge in the graph.
inline bool IsEdgeValid(HEdge edge, HGraph* graph) {
  if (!edge.IsValid()) {
    return false;
  }
  uint32_t from = edge.GetFrom();
  uint32_t to = edge.GetTo();
  if (from >= graph->GetBlocks().size() || to >= graph->GetBlocks().size()) {
    return false;
  }

  HBasicBlock* block_from = graph->GetBlocks()[from];
  HBasicBlock* block_to = graph->GetBlocks()[to];
  if (block_from == nullptr || block_to == nullptr) {
    return false;
  }

  return block_from->HasSuccessor(block_to, 0);
}

}  // namespace art

namespace std {
  template <>
  struct hash<art::HEdge> {
    size_t operator()(art::HEdge const& x) const noexcept  {
      // Use Cantor pairing function as the hash function.
      uint32_t a = x.GetFrom();
      uint32_t b = x.GetTo();
      return (a + b) * (a + b + 1) / 2 + b;
    }
  };
  ostream& operator<<(ostream& os, const art::HEdge& e);
}  // namespace std

#endif  // ART_COMPILER_OPTIMIZING_LOOP_ANALYSIS_H_
