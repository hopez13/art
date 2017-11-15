/*
 * Copyright (C) 2014 The Android Open Source Project
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

#include "optimization.h"

#ifdef ART_ENABLE_CODEGEN_arm
#include "instruction_simplifier_arm.h"
#endif
#ifdef ART_ENABLE_CODEGEN_arm64
#include "instruction_simplifier_arm64.h"
#endif
#ifdef ART_ENABLE_CODEGEN_mips
#include "instruction_simplifier_mips.h"
#include "pc_relative_fixups_mips.h"
#endif
#ifdef ART_ENABLE_CODEGEN_x86
#include "pc_relative_fixups_x86.h"
#endif
#if defined(ART_ENABLE_CODEGEN_x86) || defined(ART_ENABLE_CODEGEN_x86_64)
#include "x86_memory_gen.h"
#endif

#include "bounds_check_elimination.h"
#include "cha_guard_optimization.h"
#include "code_sinking.h"
#include "constant_folding.h"
#include "constructor_fence_redundancy_elimination.h"
#include "dead_code_elimination.h"
#include "driver/dex_compilation_unit.h"
#include "gvn.h"
#include "induction_var_analysis.h"
#include "inliner.h"
#include "instruction_simplifier.h"
#include "intrinsics.h"
#include "licm.h"
#include "load_store_analysis.h"
#include "load_store_elimination.h"
#include "loop_optimization.h"
#include "scheduler.h"
#include "select_generator.h"
#include "sharpening.h"
#include "side_effects_analysis.h"

// Decide between default or alternative pass name.

namespace art {

const char* OptimizationPassName(OptimizationPass pass) {
  switch (pass) {
    // Analysis.
    case kSideEffectsAnalysis:
      return SideEffectsAnalysis::kSideEffectsAnalysisPassName;
    case kInductionVarAnalysis:
      return HInductionVarAnalysis::kInductionPassName;
    case kLoadStoreAnalysis:
      return LoadStoreAnalysis::kLoadStoreAnalysisPassName;
    // Need prior.
    case kGlobalValueNumbering:
      return GVNOptimization::kGlobalValueNumberingPassName;
    case kInvariantCodeMotion:
      return LICM::kLoopInvariantCodeMotionPassName;
    case kLoopOptimization:
      return HLoopOptimization::kLoopOptimizationPassName;
    case kBoundsCheckElimination:
      return BoundsCheckElimination::kBoundsCheckEliminationPassName;
    case kLoadStoreElimination:
      return LoadStoreElimination::kLoadStoreEliminationPassName;
    // Regular.
    case kConstantFolding:
      return HConstantFolding::kConstantFoldingPassName;
    case kDeadCodeElimination:
      return HDeadCodeElimination::kDeadCodeEliminationPassName;
    case kInliner:
      return HInliner::kInlinerPassName;
    case kSharpening:
      return HSharpening::kSharpeningPassName;
    case kSelectGenerator:
      return HSelectGenerator::kSelectGeneratorPassName;
    case kInstructionSimplifier:
      return InstructionSimplifier::kInstructionSimplifierPassName;
    case kIntrinsicsRecognizer:
      return IntrinsicsRecognizer::kIntrinsicsRecognizerPassName;
    case kCHAGuardOptimization:
      return CHAGuardOptimization::kCHAGuardOptimizationPassName;
    case kCodeSinking:
      return CodeSinking::kCodeSinkingPassName;
    case kConstructorFenceRedundancyElimination:
      return ConstructorFenceRedundancyElimination::kCFREPassName;
    case kScheduling:
      return HInstructionScheduling::kInstructionSchedulingPassName;
    // Arch-specific.
#ifdef ART_ENABLE_CODEGEN_arm
    case kInstructionSimplifierArm:
      return arm::InstructionSimplifierArm::kInstructionSimplifierArmPassName;
#endif
#ifdef ART_ENABLE_CODEGEN_arm64
    case kInstructionSimplifierArm64:
      return arm64::InstructionSimplifierArm64::kInstructionSimplifierArm64PassName;
#endif
#ifdef ART_ENABLE_CODEGEN_mips
    case kPcRelativeFixupsMips:
      return mips::PcRelativeFixups::kPcRelativeFixupsMipsPassName;
    case kInstructionSimplifierMips:
      return mips::InstructionSimplifierMips::kInstructionSimplifierMipsPassName;
#endif
#ifdef ART_ENABLE_CODEGEN_x86
    case kPcRelativeFixupsX86:
      return x86::PcRelativeFixups::kPcRelativeFixupsX86PassName;
#endif
#if defined(ART_ENABLE_CODEGEN_x86) || defined(ART_ENABLE_CODEGEN_x86_64)
    case kX86MemoryOperandGeneration:
      return x86::X86MemoryOperandGeneration::kX86MemoryOperandGenerationPassName;
#endif
  }
}

#define X(x) if (name == OptimizationPassName((x))) return (x)

OptimizationPass OptimizationPassByName(const std::string& name) {
  X(kBoundsCheckElimination);
  X(kCHAGuardOptimization);
  X(kCodeSinking);
  X(kConstantFolding);
  X(kConstructorFenceRedundancyElimination);
  X(kDeadCodeElimination);
  X(kGlobalValueNumbering);
  X(kInductionVarAnalysis);
  X(kInliner);
  X(kInstructionSimplifier);
  X(kIntrinsicsRecognizer);
  X(kInvariantCodeMotion);
  X(kLoadStoreAnalysis);
  X(kLoadStoreElimination);
  X(kLoopOptimization);
  X(kScheduling);
  X(kSelectGenerator);
  X(kSharpening);
  X(kSideEffectsAnalysis);
#ifdef ART_ENABLE_CODEGEN_arm
  X(kInstructionSimplifierArm);
#endif
#ifdef ART_ENABLE_CODEGEN_arm64
  X(kInstructionSimplifierArm64);
#endif
#ifdef ART_ENABLE_CODEGEN_mips
  X(kPcRelativeFixupsMips);
  X(kInstructionSimplifierMips);
#endif
#ifdef ART_ENABLE_CODEGEN_x86
  X(kPcRelativeFixupsX86);
  X(kX86MemoryOperandGeneration);
#endif
  LOG(FATAL) << "Cannot find optimization " << name;
  UNREACHABLE();
}

#undef X

ArenaVector<HOptimization*> ConstructOptimizations(
    std::pair<OptimizationPass, const char*> definitions[],
    size_t length,
    ArenaAllocator* allocator,
    HGraph* graph,
    OptimizingCompilerStats* stats,
    CodeGenerator* codegen,
    CompilerDriver* driver,
    const DexCompilationUnit& dex_compilation_unit,
    VariableSizedHandleScope* handles) {
  ArenaVector<HOptimization*> optimizations(allocator->Adapter());

  // Some optimizations require SideEffectsAnalysis or HInductionVarAnalysis
  // instances. This method uses the nearest instance preceeding it in the pass
  // name list or fails fatally if no such analysis can be found.
  SideEffectsAnalysis* most_recent_side_effects = nullptr;
  HInductionVarAnalysis* most_recent_induction = nullptr;
  LoadStoreAnalysis* most_recent_lsa = nullptr;

  // Loop over the requested optimizations.
  for (size_t i = 0; i < length; i++) {
    OptimizationPass pass = definitions[i].first;
    const char* alt_name = definitions[i].second;
    const char* name = alt_name != nullptr
        ? alt_name
        : OptimizationPassName(pass);
    HOptimization* opt = nullptr;

    switch (pass) {
      //
      // Analysis passes (kept in most recent for subsequent passes).
      //
      case kSideEffectsAnalysis:
        opt = most_recent_side_effects = new (allocator) SideEffectsAnalysis(graph, name);
        break;
      case kInductionVarAnalysis:
        opt = most_recent_induction = new (allocator) HInductionVarAnalysis(graph, name);
        break;
      case kLoadStoreAnalysis:
        opt = most_recent_lsa = new (allocator) LoadStoreAnalysis(graph, name);
        break;
      //
      // Passes that need prior analysis.
      //
      case kGlobalValueNumbering:
        CHECK(most_recent_side_effects != nullptr);
        opt = new (allocator) GVNOptimization(graph, *most_recent_side_effects, name);
        break;
      case kInvariantCodeMotion:
        CHECK(most_recent_side_effects != nullptr);
        opt = new (allocator) LICM(graph, *most_recent_side_effects, stats, name);
        break;
      case kLoopOptimization:
        CHECK(most_recent_induction != nullptr);
        opt = new (allocator) HLoopOptimization(graph, driver, most_recent_induction, stats, name);
        break;
      case kBoundsCheckElimination:
        CHECK(most_recent_side_effects != nullptr && most_recent_induction != nullptr);
        opt = new (allocator) BoundsCheckElimination(
            graph, *most_recent_side_effects, most_recent_induction, name);
        break;
      case kLoadStoreElimination:
        CHECK(most_recent_side_effects != nullptr && most_recent_induction != nullptr);
        opt = new (allocator) LoadStoreElimination(
            graph, *most_recent_side_effects, *most_recent_lsa, stats, name);
        break;
      //
      // Regular passes.
      //
      case kConstantFolding:
        opt = new (allocator) HConstantFolding(graph, name);
        break;
      case kDeadCodeElimination:
        opt = new (allocator) HDeadCodeElimination(graph, stats, name);
        break;
      case kInliner: {
        size_t number_of_dex_registers = dex_compilation_unit.GetCodeItem()->registers_size_;
        opt = new (allocator) HInliner(graph,                   // outer_graph
                                       graph,                   // outermost_graph
                                       codegen,
                                       dex_compilation_unit,    // outer_compilation_unit
                                       dex_compilation_unit,    // outermost_compilation_unit
                                       driver,
                                       handles,
                                       stats,
                                       number_of_dex_registers,
                                       /* total_number_of_instructions */ 0,
                                       /* parent */ nullptr,
                                       /* depth */ 0,
                                       name);
        break;
      }
      case kSharpening:
        opt = new (allocator) HSharpening(graph, codegen, dex_compilation_unit, driver, handles, name);
        break;
      case kSelectGenerator:
        opt = new (allocator) HSelectGenerator(graph, handles, stats, name);
        break;
      case kInstructionSimplifier:
        opt = new (allocator) InstructionSimplifier(graph, codegen, driver, stats, name);
        break;
      case kIntrinsicsRecognizer:
        opt = new (allocator) IntrinsicsRecognizer(graph, stats, name);
        break;
      case kCHAGuardOptimization:
        opt = new (allocator) CHAGuardOptimization(graph, name);
        break;
      case kCodeSinking:
        opt = new (allocator) CodeSinking(graph, stats, name);
        break;
      case kConstructorFenceRedundancyElimination:
        opt = new (allocator) ConstructorFenceRedundancyElimination(graph, stats, name);
        break;
      case kScheduling:
        opt = new (allocator) HInstructionScheduling(
            graph, driver->GetInstructionSet(), codegen, name);
        break;
      //
      // Arch-specific passes.
      //
#ifdef ART_ENABLE_CODEGEN_arm
      case kInstructionSimplifierArm:
        DCHECK(alt_name == nullptr) << "arch-specific pass does not support alternative name";
        opt = new (allocator) arm::InstructionSimplifierArm(graph, stats);
        break;
#endif
#ifdef ART_ENABLE_CODEGEN_arm64
      case kInstructionSimplifierArm64:
        DCHECK(alt_name == nullptr) << "arch-specific pass does not support alternative name";
        opt = new (allocator) arm64::InstructionSimplifierArm64(graph, stats);
        break;
#endif
#ifdef ART_ENABLE_CODEGEN_mips
      case kPcRelativeFixupsMips:
        DCHECK(alt_name == nullptr) << "arch-specific pass does not support alternative name";
        opt = new (allocator) mips::PcRelativeFixups(graph, codegen, stats);
        break;
      case kInstructionSimplifierMips:
        DCHECK(alt_name == nullptr) << "arch-specific pass does not support alternative name";
        opt = new (allocator) mips::InstructionSimplifierMips(graph, codegen, stats);
        break;
#endif
#ifdef ART_ENABLE_CODEGEN_x86
      case kPcRelativeFixupsX86:
        DCHECK(alt_name == nullptr) << "arch-specific pass does not support alternative name";
        opt = new (allocator) x86::PcRelativeFixups(graph, codegen, stats);
        break;
      case kX86MemoryOperandGeneration:
        DCHECK(alt_name == nullptr) << "arch-specific pass does not support alternative name";
        opt = new (allocator) x86::X86MemoryOperandGeneration(graph, codegen, stats);
        break;
#endif
    }  // switch

    // Add each next optimization to result vector.
    CHECK(opt != nullptr);
    DCHECK_STREQ(name, opt->GetPassName());  // sanity
    optimizations.push_back(opt);
  }

  return optimizations;
}

}  // namespace art
