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

#include "instruction_simplifier_riscv64.h"

#include "instruction_simplifier.h"

namespace art HIDDEN {

namespace riscv64 {

class InstructionSimplifierRiscv64Visitor final : public HGraphVisitor {
 public:
  InstructionSimplifierRiscv64Visitor(HGraph* graph, OptimizingCompilerStats* stats)
      : HGraphVisitor(graph), stats_(stats) {}

 private:
  void RecordSimplification() {
    MaybeRecordStat(stats_, MethodCompilationStat::kInstructionSimplificationsArch);
  }

  void VisitBasicBlock(HBasicBlock* block) override {
    for (HInstructionIterator it(block->GetInstructions()); !it.Done(); it.Advance()) {
      HInstruction* instruction = it.Current();
      if (instruction->IsInBlock()) {
        instruction->Accept(this);
      }
    }
  }

  // Replace Add which has Shl with distance of 1 or 2 or 3 with Riscv64ShiftAdd
  bool TryReplaceAddsWithShiftAdds(HShl* shl) {
    // There is no reason to replace Int32 Shl+Add with ShiftAdd because of
    // additional sign-extension required.
    if (shl->GetType() != DataType::Type::kInt64) {
      return false;
    }

    if (!shl->GetRight()->IsConstant()) {
      return false;
    }

    // The bytecode does not permit the shift distance to come from a wide variable
    DCHECK(shl->GetRight()->IsIntConstant());

    const int32_t distance = shl->GetRight()->AsIntConstant()->GetValue();
    if ((distance & ~0x3) != 0) {
      return false;
    }

    bool replaced = false;

    for (const HUseListNode<HInstruction*>& use : shl->GetUses()) {
      HInstruction* user = use.GetUser();

      if (!user->IsAdd()) {
        continue;
      }
      HAdd* add = user->AsAdd();
      HInstruction* left = add->GetLeft();
      HInstruction* right = add->GetRight();
      DCHECK_EQ(add->GetType(), DataType::Type::kInt64)
          << "Replaceable Add must be the same 64 bit type as the input";

      // If to replace Add which has 2 the same Shl<1|2|3> inputs with Riscv64ShiftAdd
      // which has this Shl<1|2|3> as an input then any processor will not be able to
      // execute these shifts parallel which is the purpose of the replace below.
      if (left == right) {
        continue;
      }

      auto* const add_other_input = left == shl ? right : left;
      auto* const shift_add = new (GetGraph()->GetAllocator())
          HRiscv64ShiftAdd(shl->GetLeft(), add_other_input, distance);

      add->GetBlock()->ReplaceAndRemoveInstructionWith(add, shift_add);
      replaced = true;
    }

    if (!shl->HasUses()) {
      shl->GetBlock()->RemoveInstruction(shl);
    }

    return replaced;
  }

  void VisitAnd(HAnd* inst) override {
    if (TryMergeNegatedInput(inst)) {
      RecordSimplification();
    }
  }

  void VisitOr(HOr* inst) override {
    if (TryMergeNegatedInput(inst)) {
      RecordSimplification();
    }
  }

  // Replace code looking like
  //    SHL tmp, a, 1 or 2 or 3
  //    ADD dst, tmp, b
  // with
  //    Riscv64ShiftAdd dst, a, b
  void VisitShl(HShl* inst) override {
    if (TryReplaceAddsWithShiftAdds(inst)) {
      RecordSimplification();
    }
  }

  void VisitSub(HSub* inst) override {
    if (TryMergeWithAnd(inst)) {
      RecordSimplification();
    }
  }

  void VisitXor(HXor* inst) override {
    if (TryMergeNegatedInput(inst)) {
      RecordSimplification();
    }
  }

  OptimizingCompilerStats* stats_ = nullptr;
};

bool InstructionSimplifierRiscv64::Run() {
  auto visitor = InstructionSimplifierRiscv64Visitor(graph_, stats_);
  visitor.VisitReversePostOrder();
  return true;
}

}  // namespace riscv64
}  // namespace art
