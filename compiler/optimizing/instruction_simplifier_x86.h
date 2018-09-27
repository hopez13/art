/*Copyright (C) 2018 The Android Open Source Project
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

#ifndef ART_COMPILER_OPTIMIZING_INSTRUCTION_SIMPLIFIER_X86_H_
#define ART_COMPILER_OPTIMIZING_INSTRUCTION_SIMPLIFIER_X86_H_

#include "nodes.h"
#include "optimization.h"
#include "code_generator_x86.h"

namespace art {

namespace x86 {

class InstructionSimplifierX86Visitor : public HGraphVisitor {
 public:
  InstructionSimplifierX86Visitor(HGraph* graph, CodeGeneratorX86* codegen, OptimizingCompilerStats* stats)
      : HGraphVisitor(graph),
        codegen_(codegen),
        stats_(stats) {}

 private:
  void RecordSimplification() {
    MaybeRecordStat(stats_, MethodCompilationStat::kInstructionSimplificationsArch);
  }

  bool HasCpuFeatureFlag() {
    return (codegen_->GetInstructionSetFeatures().HasAVX2());
  }

  void VisitBasicBlock(HBasicBlock* block) override {
    for (HInstructionIterator it(block->GetInstructions()); !it.Done(); it.Advance()) {
      HInstruction* instruction = it.Current();
      if (instruction->IsInBlock()) {
        instruction->Accept(this);
      }
    }
  }

  void VisitNot(HNot * instruction) override;
  void VisitNeg(HNeg* instruction) override;
  void VisitAdd(HAdd* instruction) override;

  CodeGeneratorX86* codegen_;
  OptimizingCompilerStats* stats_;
};

class InstructionSimplifierX86 : public HOptimization {
 public:
  InstructionSimplifierX86(HGraph* graph, CodeGenerator* codegen, OptimizingCompilerStats* stats)
      : HOptimization(graph, kInstructionSimplifierX86PassName, stats),
        codegen_(down_cast<CodeGeneratorX86*>(codegen)) {}

  static constexpr const char* kInstructionSimplifierX86PassName = "instruction_simplifier_x86";

  bool Run() override {
    InstructionSimplifierX86Visitor visitor(graph_, codegen_, stats_);
    visitor.VisitReversePostOrder();
    return true;
  }

 private:
  CodeGeneratorX86* codegen_;
};

}  // namespace x86
}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_INSTRUCTION_SIMPLIFIER_X86_H_
