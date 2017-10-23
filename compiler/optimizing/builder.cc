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

#include "builder.h"

#include "art_field-inl.h"
#include "base/arena_bit_vector.h"
#include "base/bit_vector-inl.h"
#include "base/logging.h"
#include "block_builder.h"
#include "data_type-inl.h"
#include "dex/verified_method.h"
#include "driver/compiler_options.h"
#include "instruction_builder.h"
#include "mirror/class_loader.h"
#include "mirror/dex_cache.h"
#include "nodes.h"
#include "optimizing_compiler_stats.h"
#include "ssa_builder.h"
#include "thread.h"
#include "utils/dex_cache_arrays_layout-inl.h"

namespace art {

HGraphBuilder::HGraphBuilder(HGraph* graph,
                             const DexCompilationUnit* dex_compilation_unit,
                             const DexCompilationUnit* outer_compilation_unit,
                             CompilerDriver* driver,
                             CodeGenerator* code_generator,
                             OptimizingCompilerStats* compiler_stats,
                             const uint8_t* interpreter_metadata,
                             VariableSizedHandleScope* handles)
    : graph_(graph),
      dex_file_(&graph->GetDexFile()),
      code_item_(*dex_compilation_unit->GetCodeItem()),
      dex_compilation_unit_(dex_compilation_unit),
      outer_compilation_unit_(outer_compilation_unit),
      compiler_driver_(driver),
      code_generator_(code_generator),
      compilation_stats_(compiler_stats),
      interpreter_metadata_(interpreter_metadata),
      handles_(handles),
      return_type_(DataType::FromShorty(dex_compilation_unit_->GetShorty()[0])) {}

bool HGraphBuilder::SkipCompilation(size_t number_of_branches) {
  if (compiler_driver_ == nullptr) {
    // Note that the compiler driver is null when unit testing.
    return false;
  }

  const CompilerOptions& compiler_options = compiler_driver_->GetCompilerOptions();
  CompilerFilter::Filter compiler_filter = compiler_options.GetCompilerFilter();
  if (compiler_filter == CompilerFilter::kEverything) {
    return false;
  }

  if (compiler_options.IsHugeMethod(code_item_.insns_size_in_code_units_)) {
    VLOG(compiler) << "Skip compilation of huge method "
                   << dex_file_->PrettyMethod(dex_compilation_unit_->GetDexMethodIndex())
                   << ": " << code_item_.insns_size_in_code_units_ << " code units";
    MaybeRecordStat(compilation_stats_,
                    MethodCompilationStat::kNotCompiledHugeMethod);
    return true;
  }

  // If it's large and contains no branches, it's likely to be machine generated initialization.
  if (compiler_options.IsLargeMethod(code_item_.insns_size_in_code_units_)
      && (number_of_branches == 0)) {
    VLOG(compiler) << "Skip compilation of large method with no branch "
                   << dex_file_->PrettyMethod(dex_compilation_unit_->GetDexMethodIndex())
                   << ": " << code_item_.insns_size_in_code_units_ << " code units";
    MaybeRecordStat(compilation_stats_,
                    MethodCompilationStat::kNotCompiledLargeMethodNoBranches);
    return true;
  }

  return false;
}

GraphAnalysisResult HGraphBuilder::AttemptIntrinsicCallGraph() {
  ArtMethod* method = graph_->GetArtMethod();
  CHECK(method != nullptr);
  CHECK(method->IsIntrinsic());

  Intrinsics intrinsic = static_cast<Intrinsics>(method->GetIntrinsic());
  switch (intrinsic) {
    case Intrinsics::kStringEquals:
      break;

    // Unsupported/unknown intrinsics.
    default:
      return kAnalysisSkipped;
  }

  // TODO: How to ensure ISA support?

  graph_->SetNumberOfVRegs(code_item_.registers_size_);
  graph_->SetNumberOfInVRegs(code_item_.ins_size_);
  graph_->SetMaximumNumberOfOutVRegs(code_item_.outs_size_);
  graph_->SetHasTryCatch(false);

  // Use ScopedArenaAllocator for all local allocations.
  ArenaAllocator* allocator = graph_->GetAllocator();

  const char* descriptor = dex_file_->GetMethodShorty(dex_compilation_unit_->GetDexMethodIndex());
  size_t number_of_arguments = strlen(descriptor);

  // Emulation as:
  // 0x00: invoke-virtual-range ...
  // 0x03: move-result v0
  // 0x04: return v0
  // ---------------------------
  //    5 dex bytecode units
  //
  // Add for complete code_item: 8 units data.

  constexpr size_t header_units = 8u;
  constexpr size_t code_units = 5u;
  uint16_t* code_item_emu = allocator->AllocArray<uint16_t>(header_units + code_units);
  DexFile::CodeItem* code_item = reinterpret_cast<DexFile::CodeItem*>(code_item_emu);

  code_item->registers_size_ = code_item_.registers_size_;
  code_item->ins_size_ = code_item_.ins_size_;
  code_item->outs_size_ = code_item_.outs_size_;
  code_item->tries_size_ = 0;
  code_item->debug_info_off_ = 0;
  code_item->insns_size_in_code_units_ = 5;

  {
    uint16_t* dex_emu = &code_item_emu[header_units];
    dex_emu[0] = 0x0074 | (number_of_arguments << 8);                // invoke-range + arg-count
    dex_emu[1] = dex_compilation_unit_->GetDexMethodIndex();         //              + target
    dex_emu[2] = code_item_.registers_size_ - code_item_.ins_size_;  //              + first arg
    dex_emu[3] = 0x000a;                                             // move-result v0
    dex_emu[4] = 0x000f;                                             // return v0
  }

  // Use ScopedArenaAllocator for all local allocations.
  ScopedArenaAllocator local_allocator(graph_->GetArenaStack());
  HBasicBlockBuilder block_builder(graph_, dex_file_, *code_item, &local_allocator);
  SsaBuilder ssa_builder(graph_,
                         dex_compilation_unit_->GetClassLoader(),
                         dex_compilation_unit_->GetDexCache(),
                         handles_,
                         &local_allocator);
  HInstructionBuilder instruction_builder(graph_,
                                          &block_builder,
                                          &ssa_builder,
                                          dex_file_,
                                          *code_item,
                                          return_type_,
                                          dex_compilation_unit_,
                                          outer_compilation_unit_,
                                          compiler_driver_,
                                          code_generator_,
                                          interpreter_metadata_,
                                          compilation_stats_,
                                          handles_,
                                          &local_allocator);

  // 1) Create basic blocks and link them together. Basic blocks are left
  //    unpopulated with the exception of synthetic blocks, e.g. HTryBoundaries.
  if (!block_builder.Build()) {
    return kAnalysisInvalidBytecode;
  }

  // 2) Decide whether to skip this method based on its code size and number
  //    of branches.
  if (SkipCompilation(block_builder.GetNumberOfBranches())) {
    return kAnalysisSkipped;
  }

  // 3) Build the dominator tree and fill in loop and try/catch metadata.
  GraphAnalysisResult result = graph_->BuildDominatorTree();
  if (result != kAnalysisSuccess) {
    return result;
  }

  // 4) Populate basic blocks with instructions.
  if (!instruction_builder.Build()) {
    return kAnalysisInvalidBytecode;
  }

  // 5) Type the graph and eliminate dead/redundant phis.
  result = ssa_builder.BuildSsa();
  if (result == kAnalysisSuccess) {
    graph_->SetCodeItemOverride(code_item);
  }
  return result;
}

GraphAnalysisResult HGraphBuilder::BuildGraph(bool attempt_boot_intrinsic) {
  DCHECK(graph_->GetBlocks().empty());


  GraphAnalysisResult result;
  if (attempt_boot_intrinsic) {
    result = AttemptIntrinsicCallGraph();
    if (result == kAnalysisSuccess) {
      return result;
    }
    DCHECK(graph_->GetBlocks().empty());
  }

  graph_->SetNumberOfVRegs(code_item_.registers_size_);
  graph_->SetNumberOfInVRegs(code_item_.ins_size_);
  graph_->SetMaximumNumberOfOutVRegs(code_item_.outs_size_);
  graph_->SetHasTryCatch(code_item_.tries_size_ != 0);

  // Use ScopedArenaAllocator for all local allocations.
  ScopedArenaAllocator local_allocator(graph_->GetArenaStack());
  HBasicBlockBuilder block_builder(graph_, dex_file_, code_item_, &local_allocator);
  SsaBuilder ssa_builder(graph_,
                         dex_compilation_unit_->GetClassLoader(),
                         dex_compilation_unit_->GetDexCache(),
                         handles_,
                         &local_allocator);
  HInstructionBuilder instruction_builder(graph_,
                                          &block_builder,
                                          &ssa_builder,
                                          dex_file_,
                                          code_item_,
                                          return_type_,
                                          dex_compilation_unit_,
                                          outer_compilation_unit_,
                                          compiler_driver_,
                                          code_generator_,
                                          interpreter_metadata_,
                                          compilation_stats_,
                                          handles_,
                                          &local_allocator);

  // 1) Create basic blocks and link them together. Basic blocks are left
  //    unpopulated with the exception of synthetic blocks, e.g. HTryBoundaries.
  if (!block_builder.Build()) {
    return kAnalysisInvalidBytecode;
  }

  // 2) Decide whether to skip this method based on its code size and number
  //    of branches.
  if (SkipCompilation(block_builder.GetNumberOfBranches())) {
    return kAnalysisSkipped;
  }

  // 3) Build the dominator tree and fill in loop and try/catch metadata.
  result = graph_->BuildDominatorTree();
  if (result != kAnalysisSuccess) {
    return result;
  }

  // 4) Populate basic blocks with instructions.
  if (!instruction_builder.Build()) {
    return kAnalysisInvalidBytecode;
  }

  // 5) Type the graph and eliminate dead/redundant phis.
  return ssa_builder.BuildSsa();
}

}  // namespace art
