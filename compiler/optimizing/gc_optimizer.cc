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

#include "gc_optimizer.h"

namespace art {

class GcOptimizerVisitor : public HGraphVisitor {
 public:
  explicit GcOptimizerVisitor(HGraph* graph)
      : HGraphVisitor(graph), prev_inst_(nullptr) {}

 private:
  void VisitBasicBlock(HBasicBlock* block) OVERRIDE;

  // Try to merge read barriers on successive HInstanceFieldGet instructions.
  void VisitInstanceFieldGet(HInstanceFieldGet* field_get) OVERRIDE;

  // The instruction preceding the current instruction.
  HInstruction* prev_inst_;

  DISALLOW_COPY_AND_ASSIGN(GcOptimizerVisitor);
};

void GcOptimizer::Run() {
  // At the moment, the GC Optimizer pass only contains optimizations
  // pertaining to Baker read barriers.
  if (kEmitCompilerReadBarrier && kUseBakerReadBarrier) {
    // TODO: Remove this condition when read barrier merging is
    // supported on all architectures.
    if (graph_->GetInstructionSet() == kArm ||
        graph_->GetInstructionSet() == kArm64 ||
        graph_->GetInstructionSet() == kThumb2 ||
        graph_->GetInstructionSet() == kX86 ||
        graph_->GetInstructionSet() == kX86_64) {
      GcOptimizerVisitor visitor(graph_);
      visitor.VisitReversePostOrder();
    }
  }
}

void GcOptimizerVisitor::VisitBasicBlock(HBasicBlock* block) {
  prev_inst_ = nullptr;
  // Traverse this block's instructions in (forward) order.
  for (HInstructionIterator it(block->GetInstructions()); !it.Done(); it.Advance()) {
    HInstruction* curr_inst = it.Current();
    curr_inst->Accept(this);
    // If `curr_inst` is a HInstanceFieldGet that has been
    // transformed by VisitInstanceFieldGet, ignore it in the next
    // iteration.
    prev_inst_ =
        (curr_inst->IsInstanceFieldGet() &&
         !curr_inst->AsInstanceFieldGet()->GeneratesOwnReadBarrier())
            ? nullptr
            : curr_inst;
  }
}

// Is `inst` an InstanceFieldGet on a reference field?
static bool IsInstanceReferenceFieldGet(HInstruction* inst) {
  return inst != nullptr
      && inst->IsInstanceFieldGet()
      && inst->GetType() == Primitive::kPrimNot;
}

void GcOptimizerVisitor::VisitInstanceFieldGet(HInstanceFieldGet* field_get) {
  // TODO: Implement more complex pattern recognition, e.g. more than
  // two InstanceFieldGet (maybe with an arch-dependent upper bound).
  if (field_get->GetType() == Primitive::kPrimNot
      && IsInstanceReferenceFieldGet(prev_inst_)
      && field_get->InputAt(0) == prev_inst_->InputAt(0)) {
    // Replace:
    //
    //   FieldGet1: InstanceFieldGet [Obj] field_name:Field1 generates_own_read_barrier: true
    //   FieldGet2: InstanceFieldGet [Obj] field_name:Field2 generates_own_read_barrier: true
    //
    // with:
    //
    //   Updated: UpdateFields [Obj] field1_name:Field1 field2_name:Field2
    //   FieldGet1: InstanceFieldGet [Updated] field_name:Field1 generates_own_read_barrier: false
    //   FieldGet2: InstanceFieldGet [Updated] field_name:Field2 generates_own_read_barrier: false
    //
    // Note that the location for `Updated` and `Obj` is the same, as
    // the object isn't moved (but the reference in Obj.Field1 and/or
    // Obj.Field2 may have been updated).

    HInstanceFieldGet* prev_field_get = prev_inst_->AsInstanceFieldGet();
    DCHECK_EQ(prev_field_get->GetType(), Primitive::kPrimNot);
    HInstruction* object = field_get->InputAt(0);

    ArenaAllocator* allocator = GetGraph()->GetArena();

    HUpdateFields* updated = new (allocator) HUpdateFields(object,
                                                           prev_field_get->GetFieldInfo(),
                                                           field_get->GetFieldInfo(),
                                                           prev_field_get->GetDexPc());
    updated->SetReferenceTypeInfo(object->GetReferenceTypeInfo());
    field_get->GetBlock()->InsertInstructionBefore(updated, prev_field_get);
    prev_field_get->ReplaceInput(updated, 0);
    prev_field_get->ClearGeneratesOwnReadBarrier();
    field_get->ReplaceInput(updated, 0);
    field_get->ClearGeneratesOwnReadBarrier();
  }
}

}  // namespace art
