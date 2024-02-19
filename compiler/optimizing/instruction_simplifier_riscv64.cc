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

namespace art HIDDEN {

namespace riscv64 {

bool CheckInputs(HAdd* add, HShl* shl) {
  return (add->GetLeft() == shl) || (add->GetRight() == shl);
}

class InstructionSimplifierRiscv64Visitor final : public HGraphVisitor {
 public:
  InstructionSimplifierRiscv64Visitor(
      HGraph* graph, OptimizingCompilerStats* stats)
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

  // %constant = 1
  // %shl = Shl input0, %constant
  // %add = Add %shl, input1
  // --> %sh1add = sh1add input0, input1
  void VisitShl(HShl* shl) override {
    if (shl->GetType() == DataType::Type::kInt32) {
      return;
    }

    if (!shl->GetRight()->IsConstant()) {
      return;
    }

    const auto constant = shl->GetRight();
    const int64_t distance = constant->IsIntConstant() ?
      constant->AsIntConstant()->GetValue() :
      constant->AsLongConstant()->GetValue();
    if (distance != 1 && distance != 2 && distance != 3) {
      return;
    }

    if (!shl->HasOnlyOneNonEnvironmentUse()) {
      return;
    }

    auto add_inst = shl->GetUses().front().GetUser();
    if (!add_inst->IsAdd()) {
      return;
    }
    auto add = add_inst->AsAdd();

    if (!CheckInputs(add, shl)) {
      return;
    }

    auto add_other_input = add->GetLeft() == shl ? add->GetRight() :
      add->GetLeft();
    auto shift_add = new (GetGraph()->GetAllocator())
        HRiscv64ShiftAdd(add->GetType(), shl->GetLeft(), add_other_input,
                         (uint32_t) distance);

    add->GetBlock()->ReplaceAndRemoveInstructionWith(add, shift_add);
    shl->GetBlock()->RemoveInstruction(shl);
  }

  OptimizingCompilerStats* stats_ {nullptr};
};

bool InstructionSimplifierRiscv64::Run() {
  auto visitor = InstructionSimplifierRiscv64Visitor(graph_, stats_);
  visitor.VisitReversePostOrder();
  return true;
}

}  // namespace riscv64
}  // namespace art
