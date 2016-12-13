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

  // Read barrier state kind: either explicit (contained in a (32-bit)
  // core register) or implicit (not materialized in a core register,
  // but stored in a flags register).
  enum class LoadReadBarrierStateKind {
    kExplicit,
    kImplicit
  };

  // Get the read barrier state kind (explicit or implicit) used by
  // the target instruction set.
  LoadReadBarrierStateKind GetLoadReadBarrierStateKind() const;

  // Get the type of a HLoadReadBarrierState instruction.
  static Primitive::Type GetReadBarrierStateType(LoadReadBarrierStateKind load_rb_state_kind);

  // The instruction preceding the current instruction.
  HInstruction* prev_inst_;

  DISALLOW_COPY_AND_ASSIGN(GcOptimizerVisitor);
};

void GcOptimizer::Run() {
  // At the moment, the GC Optimizer pass only contains optimizations
  // pertaining to Baker read barriers.
  if (kEmitCompilerReadBarrier && kUseBakerReadBarrier) {
    // TODO: Remove this condition when read barriers are implemented
    // on all architectures.
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
    //   RBState: LoadReadBarrierState [Obj]
    //   FieldGet1: InstanceFieldGet [Obj] field_name:Field1 generates_own_read_barrier: false
    //   FieldGet2: InstanceFieldGet [Obj] field_name:Field2 generates_own_read_barrier: false
    //   MarkReferencesExplicitRBState [RBState, FieldGet1, FieldGet2]
    //
    // or:
    //
    //   RBState: LoadReadBarrierState [Obj]
    //   FieldGet1: InstanceFieldGet [Obj] field_name:Field1 generates_own_read_barrier: false
    //   FieldGet2: InstanceFieldGet [Obj] field_name:Field2 generates_own_read_barrier: false
    //   MarkReferencesImplicitRBState [FieldGet1, FieldGet2]
    //
    // (depending on the architecture).
    //
    // The code generated for `FieldGet1` and `FieldGet2` depend on
    // the code generated for `RBState` (to avoid a load-load
    // reordering). However, there is no explicit dependency between
    // the former and the latter at the HIR level (because the read
    // barrier state is not actually used by the InstanceFieldGet
    // instructions). To ensure that no optimization (such as GVN)
    // reorders these instructions, HInstanceFieldGet::CanBeMoved
    // returns `false` for InstanceFieldGet instructions transformed
    // by this method. This constraint is also checked by
    // GraphChecker.

    HInstanceFieldGet* prev_field_get = prev_inst_->AsInstanceFieldGet();
    DCHECK_EQ(prev_field_get->GetType(), Primitive::kPrimNot);
    HInstruction* object = field_get->InputAt(0);

    ArenaAllocator* allocator = GetGraph()->GetArena();

    LoadReadBarrierStateKind load_rb_state_kind = GetLoadReadBarrierStateKind();
    HLoadReadBarrierState* rb_state = new (allocator) HLoadReadBarrierState(
        GetReadBarrierStateType(load_rb_state_kind), object, prev_field_get->GetDexPc());
    prev_field_get->ClearGeneratesOwnReadBarrier();
    field_get->ClearGeneratesOwnReadBarrier();
    // The reference marking instruction type depends on whether the
    // read barrier state is explicit (encoded as an integer value) or
    // implicit (a side effect of the read barrier state instruction).
    // See also GcOptimizerVisitor::LoadReadBarrierStateKind.
    HInstruction* mark_references;
    switch (load_rb_state_kind) {
      case LoadReadBarrierStateKind::kExplicit:
        mark_references = new (allocator) HMarkReferencesExplicitRBState(
            rb_state, prev_field_get, field_get, field_get->GetDexPc());
        break;
      case LoadReadBarrierStateKind::kImplicit:
        mark_references = new (allocator) HMarkReferencesImplicitRBState(
            prev_field_get, field_get, field_get->GetDexPc());
        break;
    }

    HBasicBlock* block = field_get->GetBlock();
    block->InsertInstructionBefore(rb_state, prev_field_get);
    block->InsertInstructionAfter(mark_references, field_get);
  }
}

GcOptimizerVisitor::LoadReadBarrierStateKind
GcOptimizerVisitor::GetLoadReadBarrierStateKind() const {
  switch (GetGraph()->GetInstructionSet()) {
    case kArm:
    case kArm64:
    case kThumb2:
      // The read barrier state is contained in the lockword of the
      // base object, which is an int32 value.
      return LoadReadBarrierStateKind::kExplicit;

    case kX86:
    case kX86_64:
      // The read barrier state is contained in EFLAGS/RFLAGS
      // registers, which is not a materialized value.
      return LoadReadBarrierStateKind::kImplicit;

    default:
      LOG(FATAL) << "Unsupported architecture " << GetGraph()->GetInstructionSet();
      UNREACHABLE();
  }
}

Primitive::Type GcOptimizerVisitor::GetReadBarrierStateType(
    GcOptimizerVisitor::LoadReadBarrierStateKind load_rb_state_kind) {
  switch (load_rb_state_kind) {
    case LoadReadBarrierStateKind::kExplicit:
      return Primitive::kPrimInt;
    case LoadReadBarrierStateKind::kImplicit:
      return Primitive::kPrimVoid;
  }
}

}  // namespace art
