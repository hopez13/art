/*
 * Copyright (C) 2022 The Android Open Source Project
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

#include "write_barrier_elimination.h"

#include "base/arena_allocator.h"
#include "base/scoped_arena_allocator.h"
#include "base/scoped_arena_containers.h"
#include "optimizing/nodes.h"

namespace art HIDDEN {

class WBEVisitor final : public HGraphVisitor {
 public:
  // We store a map of <ObjectInstruction, SetOfInstructionsThatContainTheWriteBarrier>. The value
  // is a set since multiple predecessors might flow into the same block, and we want to know all
  // instructions.
  // TODO(solanes): How to make this a ScopedArenaHashSet? We would need to pass the kArenaAllocWBE
  // in the write_barriers_per_block_ constructor somehow.
  typedef HashSet<HInstruction*> WriteBarrierSet;
  typedef ScopedArenaHashMap<HInstruction*, WriteBarrierSet> CurrentWriteBarriers;

  WBEVisitor(HGraph* graph, OptimizingCompilerStats* stats)
      : HGraphVisitor(graph),
        scoped_allocator_(graph->GetArenaStack()),
        write_barriers_per_block_(graph->GetBlocks().size(),
                                  CurrentWriteBarriers(scoped_allocator_.Adapter(kArenaAllocWBE)),
                                  scoped_allocator_.Adapter(kArenaAllocWBE)),
        stats_(stats) {}

  void VisitBasicBlock(HBasicBlock* block) override {
    // No need to process the entry block as it wouldn't contain relevant instructions.
    if (block->IsEntryBlock()) {
      return;
    }
    DCHECK_LT(block->GetBlockId(), GetGraph()->GetBlocks().size());
    DCHECK(write_barriers_per_block_[block->GetBlockId()].empty())
        << " We shouldn't have filled any data yet.";
    // Catch blocks are special and their predecessor relationships are not the same that a regular
    // block. LoopHeader blocks will be visited before their body, making the computation always
    // empty.
    if (!block->IsCatchBlock() && !block->IsLoopHeader()) {
      ComputeWriteBarriersAtEntry(block);
    }
    HGraphVisitor::VisitBasicBlock(block);
  }

  // Merge the predecessors inputs regarding write barriers.
  void ComputeWriteBarriersAtEntry(HBasicBlock* block) {
    DCHECK_GE(block->GetNumberOfPredecessors(), 1u);
    const ArenaVector<HBasicBlock*>& preds = block->GetPredecessors();
    // Iterate per object.
    for (auto& key_value_pair : write_barriers_per_block_[preds[0]->GetBlockId()]) {
      HInstruction* obj = key_value_pair.first;

      // Find all the write barrier setting instructions for that object.
      WriteBarrierSet set_union;
      for (HBasicBlock* pred : preds) {
        auto it = write_barriers_per_block_[pred->GetBlockId()].find(obj);
        // All predecessors must have set the write barrier for it to be valid.
        if (it == write_barriers_per_block_[pred->GetBlockId()].end()) {
          set_union.clear();
          break;
        }
        for (HInstruction* wb_instruction : it->second) {
          set_union.insert(wb_instruction);
        }
      }

      if (!set_union.empty()) {
        write_barriers_per_block_[block->GetBlockId()].insert({obj, set_union});
      }
    }
  }

  void VisitInstanceFieldSet(HInstanceFieldSet* instruction) override {
    DCHECK(!instruction->GetSideEffects().Includes(SideEffects::CanTriggerGC()));

    if (instruction->GetFieldType() != DataType::Type::kReference ||
        instruction->GetValue()->IsNullConstant()) {
      instruction->SetWriteBarrierKind(WriteBarrierKind::kDontEmit);
      return;
    }

    HBasicBlock* block = instruction->GetBlock();
    MaybeRecordStat(stats_, MethodCompilationStat::kPossibleWriteBarrier);
    HInstruction* obj = HuntForOriginalReference(instruction->InputAt(0));
    auto it = write_barriers_per_block_[block->GetBlockId()].find(obj);
    if (it != write_barriers_per_block_[block->GetBlockId()].end()) {
      for (HInstruction* wb_instruction : it->second) {
        DCHECK(wb_instruction->IsInstanceFieldSet());
        DCHECK(wb_instruction->AsInstanceFieldSet()->GetWriteBarrierKind() !=
               WriteBarrierKind::kDontEmit);
        wb_instruction->AsInstanceFieldSet()->SetWriteBarrierKind(
            WriteBarrierKind::kEmitNoNullCheck);
      }
      instruction->SetWriteBarrierKind(WriteBarrierKind::kDontEmit);
      MaybeRecordStat(stats_, MethodCompilationStat::kRemovedWriteBarrier);
    } else {
      WriteBarrierSet wb_instructions;
      wb_instructions.insert(instruction);
      const bool inserted =
          write_barriers_per_block_[block->GetBlockId()].insert({obj, wb_instructions}).second;
      DCHECK(inserted);
      DCHECK(instruction->GetWriteBarrierKind() != WriteBarrierKind::kDontEmit);
    }
  }

  void VisitStaticFieldSet(HStaticFieldSet* instruction) override {
    DCHECK(!instruction->GetSideEffects().Includes(SideEffects::CanTriggerGC()));

    if (instruction->GetFieldType() != DataType::Type::kReference ||
        instruction->GetValue()->IsNullConstant()) {
      instruction->SetWriteBarrierKind(WriteBarrierKind::kDontEmit);
      return;
    }

    HBasicBlock* block = instruction->GetBlock();
    MaybeRecordStat(stats_, MethodCompilationStat::kPossibleWriteBarrier);
    HInstruction* cls = HuntForOriginalReference(instruction->InputAt(0));
    auto it = write_barriers_per_block_[block->GetBlockId()].find(cls);
    if (it != write_barriers_per_block_[block->GetBlockId()].end()) {
      for (HInstruction* wb_instruction : it->second) {
        DCHECK(wb_instruction->IsStaticFieldSet());
        DCHECK(wb_instruction->AsStaticFieldSet()->GetWriteBarrierKind() !=
               WriteBarrierKind::kDontEmit);
        wb_instruction->AsStaticFieldSet()->SetWriteBarrierKind(WriteBarrierKind::kEmitNoNullCheck);
      }
      instruction->SetWriteBarrierKind(WriteBarrierKind::kDontEmit);
      MaybeRecordStat(stats_, MethodCompilationStat::kRemovedWriteBarrier);
    } else {
      WriteBarrierSet wb_instructions;
      wb_instructions.insert(instruction);
      const bool inserted =
          write_barriers_per_block_[block->GetBlockId()].insert({cls, wb_instructions}).second;
      DCHECK(inserted);
      DCHECK(instruction->GetWriteBarrierKind() != WriteBarrierKind::kDontEmit);
    }
  }

  void VisitArraySet(HArraySet* instruction) override {
    VisitInstruction(instruction);

    if (instruction->GetComponentType() != DataType::Type::kReference ||
        instruction->GetValue()->IsNullConstant()) {
      instruction->SetWriteBarrierKind(WriteBarrierKind::kDontEmit);
      return;
    }

    HBasicBlock* block = instruction->GetBlock();
    HInstruction* arr = HuntForOriginalReference(instruction->InputAt(0));
    MaybeRecordStat(stats_, MethodCompilationStat::kPossibleWriteBarrier);
    auto it = write_barriers_per_block_[block->GetBlockId()].find(arr);
    if (it != write_barriers_per_block_[block->GetBlockId()].end()) {
      for (HInstruction* wb_instruction : it->second) {
        DCHECK(wb_instruction->IsArraySet());
        DCHECK(wb_instruction->AsArraySet()->GetWriteBarrierKind() != WriteBarrierKind::kDontEmit);
        // We never skip the null check in ArraySets so that value is already set.
        DCHECK(wb_instruction->AsArraySet()->GetWriteBarrierKind() ==
               WriteBarrierKind::kEmitNoNullCheck);
      }
      instruction->SetWriteBarrierKind(WriteBarrierKind::kDontEmit);
      MaybeRecordStat(stats_, MethodCompilationStat::kRemovedWriteBarrier);
    } else {
      WriteBarrierSet wb_instructions;
      wb_instructions.insert(instruction);
      const bool inserted =
          write_barriers_per_block_[block->GetBlockId()].insert({arr, wb_instructions}).second;
      DCHECK(inserted);
      DCHECK(instruction->GetWriteBarrierKind() != WriteBarrierKind::kDontEmit);
    }
  }

  void VisitInstruction(HInstruction* instruction) override {
    if (instruction->GetSideEffects().Includes(SideEffects::CanTriggerGC())) {
      write_barriers_per_block_[instruction->GetBlock()->GetBlockId()].clear();
    }
  }

 private:
  HInstruction* HuntForOriginalReference(HInstruction* ref) const {
    // An original reference can be transformed by instructions like:
    //   i0 NewArray
    //   i1 HInstruction(i0)  <-- NullCheck, BoundType, IntermediateAddress.
    //   i2 ArraySet(i1, index, value)
    DCHECK(ref != nullptr);
    while (ref->IsNullCheck() || ref->IsBoundType() || ref->IsIntermediateAddress()) {
      ref = ref->InputAt(0);
    }
    return ref;
  }

  ScopedArenaAllocator scoped_allocator_;

  // Stores a map of <Receiver, Instruction(s)WhereTheWriteBarrierIs>.
  // `Instruction(s)WhereTheWriteBarrierIs` is used for DCHECKs only.
  ScopedArenaVector<CurrentWriteBarriers> write_barriers_per_block_;

  OptimizingCompilerStats* const stats_;

  DISALLOW_COPY_AND_ASSIGN(WBEVisitor);
};

bool WriteBarrierElimination::Run() {
  WBEVisitor wbe_visitor(graph_, stats_);
  wbe_visitor.VisitReversePostOrder();
  return true;
}

}  // namespace art
