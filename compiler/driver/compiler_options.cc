/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include "compiler_options.h"

#include <fstream>

namespace art {

CompilerOptions::CompilerOptions()
    : compiler_filter_(CompilerFilter::kDefaultCompilerFilter),
      huge_method_threshold_(kDefaultHugeMethodThreshold),
      large_method_threshold_(kDefaultLargeMethodThreshold),
      small_method_threshold_(kDefaultSmallMethodThreshold),
      tiny_method_threshold_(kDefaultTinyMethodThreshold),
      num_dex_methods_threshold_(kDefaultNumDexMethodsThreshold),
      inline_max_code_units_(kUnsetInlineMaxCodeUnits),
      no_inline_from_(nullptr),
      boot_image_(false),
      app_image_(false),
      top_k_profile_threshold_(kDefaultTopKProfileThreshold),
      debuggable_(false),
      generate_debug_info_(kDefaultGenerateDebugInfo),
      generate_mini_debug_info_(kDefaultGenerateMiniDebugInfo),
      generate_build_id_(false),
      implicit_null_checks_(true),
      implicit_so_checks_(true),
      implicit_suspend_checks_(false),
      compile_pic_(false),
      verbose_methods_(nullptr),
      abort_on_hard_verifier_failure_(false),
      init_failure_output_(nullptr),
      dump_cfg_file_name_(""),
      dump_cfg_append_(false),
      force_determinism_(false),
      scheduler_strength_(kSchedulerStrength),
      scheduler_ArmIntegerOpLatency_(kArmIntegerOpLatency),
      scheduler_ArmFloatingPointOpLatency_(kArmFloatingPointOpLatency),
      scheduler_ArmDataProcWithShifterOpLatency_(kArmDataProcWithShifterOpLatency),
      scheduler_ArmMulIntegerLatency_(kArmMulIntegerLatency),
      scheduler_ArmMulFloatingPointLatency_(kArmMulFloatingPointLatency),
      scheduler_ArmDivIntegerLatency_(kArmDivIntegerLatency),
      scheduler_ArmDivFloatLatency_(kArmDivFloatLatency),
      scheduler_ArmDivDoubleLatency_(kArmDivDoubleLatency),
      scheduler_ArmTypeConversionFloatingPointIntegerLatency_(kArmTypeConversionFloatingPointIntegerLatency),
      scheduler_ArmMemoryLoadLatency_(kArmMemoryLoadLatency),
      scheduler_ArmMemoryStoreLatency_(kArmMemoryStoreLatency),
      scheduler_ArmMemoryBarrierLatency_(kArmMemoryBarrierLatency),
      scheduler_ArmBranchLatency_(kArmBranchLatency),
      scheduler_ArmCallLatency_(kArmCallLatency),
      scheduler_ArmCallInternalLatency_(kArmCallInternalLatency),
      scheduler_ArmLoadStringInternalLatency_(kArmLoadStringInternalLatency),
      scheduler_ArmNopLatency_(kArmNopLatency),
      scheduler_ArmLoadWithBakerReadBarrierLatency_(kArmLoadWithBakerReadBarrierLatency),
      scheduler_ArmRuntimeTypeCheckLatency_(kArmRuntimeTypeCheckLatency),
      register_allocation_strategy_(RegisterAllocator::kRegisterAllocatorDefault),
      passes_to_run_(nullptr) {
}

CompilerOptions::~CompilerOptions() {
  // The destructor looks empty but it destroys a PassManagerOptions object. We keep it here
  // because we don't want to include the PassManagerOptions definition from the header file.
}

CompilerOptions::CompilerOptions(CompilerFilter::Filter compiler_filter,
                                 size_t huge_method_threshold,
                                 size_t large_method_threshold,
                                 size_t small_method_threshold,
                                 size_t tiny_method_threshold,
                                 size_t num_dex_methods_threshold,
                                 size_t inline_max_code_units,
                                 const std::vector<const DexFile*>* no_inline_from,
                                 double top_k_profile_threshold,
                                 bool debuggable,
                                 bool generate_debug_info,
                                 bool implicit_null_checks,
                                 bool implicit_so_checks,
                                 bool implicit_suspend_checks,
                                 bool compile_pic,
                                 const std::vector<std::string>* verbose_methods,
                                 std::ostream* init_failure_output,
                                 bool abort_on_hard_verifier_failure,
                                 const std::string& dump_cfg_file_name,
                                 bool dump_cfg_append,
                                 bool force_determinism,
                                 size_t scheduler_strength,
                                 size_t scheduler_ArmIntegerOpLatency,
                                 size_t scheduler_ArmFloatingPointOpLatency,
                                 size_t scheduler_ArmDataProcWithShifterOpLatency,
                                 size_t scheduler_ArmMulIntegerLatency,
                                 size_t scheduler_ArmMulFloatingPointLatency,
                                 size_t scheduler_ArmDivIntegerLatency,
                                 size_t scheduler_ArmDivFloatLatency,
                                 size_t scheduler_ArmDivDoubleLatency,
                                 size_t scheduler_ArmTypeConversionFloatingPointIntegerLatency,
                                 size_t scheduler_ArmMemoryLoadLatency,
                                 size_t scheduler_ArmMemoryStoreLatency,
                                 size_t scheduler_ArmMemoryBarrierLatency,
                                 size_t scheduler_ArmBranchLatency,
                                 size_t scheduler_ArmCallLatency,
                                 size_t scheduler_ArmCallInternalLatency,
                                 size_t scheduler_ArmLoadStringInternalLatency,
                                 size_t scheduler_ArmNopLatency,
                                 size_t scheduler_ArmLoadWithBakerReadBarrierLatency,
                                 size_t scheduler_ArmRuntimeTypeCheckLatency,
                                 RegisterAllocator::Strategy regalloc_strategy,
                                 const std::vector<std::string>* passes_to_run)
    : compiler_filter_(compiler_filter),
      huge_method_threshold_(huge_method_threshold),
      large_method_threshold_(large_method_threshold),
      small_method_threshold_(small_method_threshold),
      tiny_method_threshold_(tiny_method_threshold),
      num_dex_methods_threshold_(num_dex_methods_threshold),
      inline_max_code_units_(inline_max_code_units),
      no_inline_from_(no_inline_from),
      boot_image_(false),
      app_image_(false),
      top_k_profile_threshold_(top_k_profile_threshold),
      debuggable_(debuggable),
      generate_debug_info_(generate_debug_info),
      generate_mini_debug_info_(kDefaultGenerateMiniDebugInfo),
      generate_build_id_(false),
      implicit_null_checks_(implicit_null_checks),
      implicit_so_checks_(implicit_so_checks),
      implicit_suspend_checks_(implicit_suspend_checks),
      compile_pic_(compile_pic),
      verbose_methods_(verbose_methods),
      abort_on_hard_verifier_failure_(abort_on_hard_verifier_failure),
      init_failure_output_(init_failure_output),
      dump_cfg_file_name_(dump_cfg_file_name),
      dump_cfg_append_(dump_cfg_append),
      force_determinism_(force_determinism),
      scheduler_strength_(scheduler_strength),
      scheduler_ArmIntegerOpLatency_(scheduler_ArmIntegerOpLatency),
      scheduler_ArmFloatingPointOpLatency_(scheduler_ArmFloatingPointOpLatency),
      scheduler_ArmDataProcWithShifterOpLatency_(scheduler_ArmDataProcWithShifterOpLatency),
      scheduler_ArmMulIntegerLatency_(scheduler_ArmMulIntegerLatency),
      scheduler_ArmMulFloatingPointLatency_(scheduler_ArmMulFloatingPointLatency),
      scheduler_ArmDivIntegerLatency_(scheduler_ArmDivIntegerLatency),
      scheduler_ArmDivFloatLatency_(scheduler_ArmDivFloatLatency),
      scheduler_ArmDivDoubleLatency_(scheduler_ArmDivDoubleLatency),
      scheduler_ArmTypeConversionFloatingPointIntegerLatency_(scheduler_ArmTypeConversionFloatingPointIntegerLatency),
      scheduler_ArmMemoryLoadLatency_(scheduler_ArmMemoryLoadLatency),
      scheduler_ArmMemoryStoreLatency_(scheduler_ArmMemoryStoreLatency),
      scheduler_ArmMemoryBarrierLatency_(scheduler_ArmMemoryBarrierLatency),
      scheduler_ArmBranchLatency_(scheduler_ArmBranchLatency),
      scheduler_ArmCallLatency_(scheduler_ArmCallLatency),
      scheduler_ArmCallInternalLatency_(scheduler_ArmCallInternalLatency),
      scheduler_ArmLoadStringInternalLatency_(scheduler_ArmLoadStringInternalLatency),
      scheduler_ArmNopLatency_(scheduler_ArmNopLatency),
      scheduler_ArmLoadWithBakerReadBarrierLatency_(scheduler_ArmLoadWithBakerReadBarrierLatency),
      scheduler_ArmRuntimeTypeCheckLatency_(scheduler_ArmRuntimeTypeCheckLatency),
      register_allocation_strategy_(regalloc_strategy),
      passes_to_run_(passes_to_run) {
}

void CompilerOptions::ParseSchedulerStrength(const StringPiece& option, UsageFn Usage) {
  ParseUintOption(option, "--scheduler-strength", &scheduler_strength_, Usage);
}

// auto generated using string: void CompilerOptions::parse_scheduler_Arm&(const StringPiece\& Option, UsageFn Usage) { ParseUintOption(option, "--scheduler-Arm&", \&scheduler_Arm&_, Usage); }
void CompilerOptions::Parse_scheduler_ArmIntegerOpLatency(const StringPiece& Option, UsageFn Usage) { ParseUintOption(Option, "--scheduler-ArmIntegerOpLatency", &scheduler_ArmIntegerOpLatency_, Usage); }
void CompilerOptions::Parse_scheduler_ArmFloatingPointOpLatency(const StringPiece& Option, UsageFn Usage) { ParseUintOption(Option, "--scheduler-ArmFloatingPointOpLatency", &scheduler_ArmFloatingPointOpLatency_, Usage); }
void CompilerOptions::Parse_scheduler_ArmDataProcWithShifterOpLatency(const StringPiece& Option, UsageFn Usage) { ParseUintOption(Option, "--scheduler-ArmDataProcWithShifterOpLatency", &scheduler_ArmDataProcWithShifterOpLatency_, Usage); }
void CompilerOptions::Parse_scheduler_ArmMulIntegerLatency(const StringPiece& Option, UsageFn Usage) { ParseUintOption(Option, "--scheduler-ArmMulIntegerLatency", &scheduler_ArmMulIntegerLatency_, Usage); }
void CompilerOptions::Parse_scheduler_ArmMulFloatingPointLatency(const StringPiece& Option, UsageFn Usage) { ParseUintOption(Option, "--scheduler-ArmMulFloatingPointLatency", &scheduler_ArmMulFloatingPointLatency_, Usage); }
void CompilerOptions::Parse_scheduler_ArmDivIntegerLatency(const StringPiece& Option, UsageFn Usage) { ParseUintOption(Option, "--scheduler-ArmDivIntegerLatency", &scheduler_ArmDivIntegerLatency_, Usage); }
void CompilerOptions::Parse_scheduler_ArmDivFloatLatency(const StringPiece& Option, UsageFn Usage) { ParseUintOption(Option, "--scheduler-ArmDivFloatLatency", &scheduler_ArmDivFloatLatency_, Usage); }
void CompilerOptions::Parse_scheduler_ArmDivDoubleLatency(const StringPiece& Option, UsageFn Usage) { ParseUintOption(Option, "--scheduler-ArmDivDoubleLatency", &scheduler_ArmDivDoubleLatency_, Usage); }
void CompilerOptions::Parse_scheduler_ArmTypeConversionFloatingPointIntegerLatency(const StringPiece& Option, UsageFn Usage) { ParseUintOption(Option, "--scheduler-ArmTypeConversionFloatingPointIntegerLatency", &scheduler_ArmTypeConversionFloatingPointIntegerLatency_, Usage); }
void CompilerOptions::Parse_scheduler_ArmMemoryLoadLatency(const StringPiece& Option, UsageFn Usage) { ParseUintOption(Option, "--scheduler-ArmMemoryLoadLatency", &scheduler_ArmMemoryLoadLatency_, Usage); }
void CompilerOptions::Parse_scheduler_ArmMemoryStoreLatency(const StringPiece& Option, UsageFn Usage) { ParseUintOption(Option, "--scheduler-ArmMemoryStoreLatency", &scheduler_ArmMemoryStoreLatency_, Usage); }
void CompilerOptions::Parse_scheduler_ArmMemoryBarrierLatency(const StringPiece& Option, UsageFn Usage) { ParseUintOption(Option, "--scheduler-ArmMemoryBarrierLatency", &scheduler_ArmMemoryBarrierLatency_, Usage); }
void CompilerOptions::Parse_scheduler_ArmBranchLatency(const StringPiece& Option, UsageFn Usage) { ParseUintOption(Option, "--scheduler-ArmBranchLatency", &scheduler_ArmBranchLatency_, Usage); }
void CompilerOptions::Parse_scheduler_ArmCallLatency(const StringPiece& Option, UsageFn Usage) { ParseUintOption(Option, "--scheduler-ArmCallLatency", &scheduler_ArmCallLatency_, Usage); }
void CompilerOptions::Parse_scheduler_ArmCallInternalLatency(const StringPiece& Option, UsageFn Usage) { ParseUintOption(Option, "--scheduler-ArmCallInternalLatency", &scheduler_ArmCallInternalLatency_, Usage); }
void CompilerOptions::Parse_scheduler_ArmLoadStringInternalLatency(const StringPiece& Option, UsageFn Usage) { ParseUintOption(Option, "--scheduler-ArmLoadStringInternalLatency", &scheduler_ArmLoadStringInternalLatency_, Usage); }
void CompilerOptions::Parse_scheduler_ArmNopLatency(const StringPiece& Option, UsageFn Usage) { ParseUintOption(Option, "--scheduler-ArmNopLatency", &scheduler_ArmNopLatency_, Usage); }
void CompilerOptions::Parse_scheduler_ArmLoadWithBakerReadBarrierLatency(const StringPiece& Option, UsageFn Usage) { ParseUintOption(Option, "--scheduler-ArmLoadWithBakerReadBarrierLatency", &scheduler_ArmLoadWithBakerReadBarrierLatency_, Usage); }
void CompilerOptions::Parse_scheduler_ArmRuntimeTypeCheckLatency(const StringPiece& Option, UsageFn Usage) { ParseUintOption(Option, "--scheduler-ArmRuntimeTypeCheckLatency", &scheduler_ArmRuntimeTypeCheckLatency_, Usage); }

void CompilerOptions::ParseHugeMethodMax(const StringPiece& option, UsageFn Usage) {
  ParseUintOption(option, "--huge-method-max", &huge_method_threshold_, Usage);
}

void CompilerOptions::ParseLargeMethodMax(const StringPiece& option, UsageFn Usage) {
  ParseUintOption(option, "--large-method-max", &large_method_threshold_, Usage);
}

void CompilerOptions::ParseSmallMethodMax(const StringPiece& option, UsageFn Usage) {
  ParseUintOption(option, "--small-method-max", &small_method_threshold_, Usage);
}

void CompilerOptions::ParseTinyMethodMax(const StringPiece& option, UsageFn Usage) {
  ParseUintOption(option, "--tiny-method-max", &tiny_method_threshold_, Usage);
}

void CompilerOptions::ParseNumDexMethods(const StringPiece& option, UsageFn Usage) {
  ParseUintOption(option, "--num-dex-methods", &num_dex_methods_threshold_, Usage);
}

void CompilerOptions::ParseInlineMaxCodeUnits(const StringPiece& option, UsageFn Usage) {
  ParseUintOption(option, "--inline-max-code-units", &inline_max_code_units_, Usage);
}

void CompilerOptions::ParseDumpInitFailures(const StringPiece& option,
                                            UsageFn Usage ATTRIBUTE_UNUSED) {
  DCHECK(option.starts_with("--dump-init-failures="));
  std::string file_name = option.substr(strlen("--dump-init-failures=")).data();
  init_failure_output_.reset(new std::ofstream(file_name));
  if (init_failure_output_.get() == nullptr) {
    LOG(ERROR) << "Failed to allocate ofstream";
  } else if (init_failure_output_->fail()) {
    LOG(ERROR) << "Failed to open " << file_name << " for writing the initialization "
               << "failures.";
    init_failure_output_.reset();
  }
}

void CompilerOptions::ParseRegisterAllocationStrategy(const StringPiece& option,
                                                      UsageFn Usage) {
  DCHECK(option.starts_with("--register-allocation-strategy="));
  StringPiece choice = option.substr(strlen("--register-allocation-strategy=")).data();
  if (choice == "linear-scan") {
    register_allocation_strategy_ = RegisterAllocator::Strategy::kRegisterAllocatorLinearScan;
  } else if (choice == "graph-color") {
    register_allocation_strategy_ = RegisterAllocator::Strategy::kRegisterAllocatorGraphColor;
  } else {
    Usage("Unrecognized register allocation strategy. Try linear-scan, or graph-color.");
  }
}

bool CompilerOptions::ParseCompilerOption(const StringPiece& option, UsageFn Usage) {
  if (option.starts_with("--compiler-filter=")) {
    const char* compiler_filter_string = option.substr(strlen("--compiler-filter=")).data();
    if (!CompilerFilter::ParseCompilerFilter(compiler_filter_string, &compiler_filter_)) {
      Usage("Unknown --compiler-filter value %s", compiler_filter_string);
    }
  } else if (option == "--compile-pic") {
    compile_pic_ = true;
  } else if (option.starts_with("--scheduler-strength=")) {
    ParseSchedulerStrength(option, Usage);
    // auto generated using string: } else if (option.starts_with("--scheduler-Arm&=")) { Parse_scheduler_Arm&(option, Usage);
  } else if (option.starts_with("--scheduler-ArmIntegerOpLatency=")) { Parse_scheduler_ArmIntegerOpLatency(option, Usage);
  } else if (option.starts_with("--scheduler-ArmFloatingPointOpLatency=")) { Parse_scheduler_ArmFloatingPointOpLatency(option, Usage);
  } else if (option.starts_with("--scheduler-ArmDataProcWithShifterOpLatency=")) { Parse_scheduler_ArmDataProcWithShifterOpLatency(option, Usage);
  } else if (option.starts_with("--scheduler-ArmMulIntegerLatency=")) { Parse_scheduler_ArmMulIntegerLatency(option, Usage);
  } else if (option.starts_with("--scheduler-ArmMulFloatingPointLatency=")) { Parse_scheduler_ArmMulFloatingPointLatency(option, Usage);
  } else if (option.starts_with("--scheduler-ArmDivIntegerLatency=")) { Parse_scheduler_ArmDivIntegerLatency(option, Usage);
  } else if (option.starts_with("--scheduler-ArmDivFloatLatency=")) { Parse_scheduler_ArmDivFloatLatency(option, Usage);
  } else if (option.starts_with("--scheduler-ArmDivDoubleLatency=")) { Parse_scheduler_ArmDivDoubleLatency(option, Usage);
  } else if (option.starts_with("--scheduler-ArmTypeConversionFloatingPointIntegerLatency=")) { Parse_scheduler_ArmTypeConversionFloatingPointIntegerLatency(option, Usage);
  } else if (option.starts_with("--scheduler-ArmMemoryLoadLatency=")) { Parse_scheduler_ArmMemoryLoadLatency(option, Usage);
  } else if (option.starts_with("--scheduler-ArmMemoryStoreLatency=")) { Parse_scheduler_ArmMemoryStoreLatency(option, Usage);
  } else if (option.starts_with("--scheduler-ArmMemoryBarrierLatency=")) { Parse_scheduler_ArmMemoryBarrierLatency(option, Usage);
  } else if (option.starts_with("--scheduler-ArmBranchLatency=")) { Parse_scheduler_ArmBranchLatency(option, Usage);
  } else if (option.starts_with("--scheduler-ArmCallLatency=")) { Parse_scheduler_ArmCallLatency(option, Usage);
  } else if (option.starts_with("--scheduler-ArmCallInternalLatency=")) { Parse_scheduler_ArmCallInternalLatency(option, Usage);
  } else if (option.starts_with("--scheduler-ArmLoadStringInternalLatency=")) { Parse_scheduler_ArmLoadStringInternalLatency(option, Usage);
  } else if (option.starts_with("--scheduler-ArmNopLatency=")) { Parse_scheduler_ArmNopLatency(option, Usage);
  } else if (option.starts_with("--scheduler-ArmLoadWithBakerReadBarrierLatency=")) { Parse_scheduler_ArmLoadWithBakerReadBarrierLatency(option, Usage);
  } else if (option.starts_with("--scheduler-ArmRuntimeTypeCheckLatency=")) { Parse_scheduler_ArmRuntimeTypeCheckLatency(option, Usage);
  } else if (option.starts_with("--huge-method-max=")) {
    ParseHugeMethodMax(option, Usage);
  } else if (option.starts_with("--large-method-max=")) {
    ParseLargeMethodMax(option, Usage);
  } else if (option.starts_with("--small-method-max=")) {
    ParseSmallMethodMax(option, Usage);
  } else if (option.starts_with("--tiny-method-max=")) {
    ParseTinyMethodMax(option, Usage);
  } else if (option.starts_with("--num-dex-methods=")) {
    ParseNumDexMethods(option, Usage);
  } else if (option.starts_with("--inline-max-code-units=")) {
    ParseInlineMaxCodeUnits(option, Usage);
  } else if (option == "--generate-debug-info" || option == "-g") {
    generate_debug_info_ = true;
  } else if (option == "--no-generate-debug-info") {
    generate_debug_info_ = false;
  } else if (option == "--generate-mini-debug-info") {
    generate_mini_debug_info_ = true;
  } else if (option == "--no-generate-mini-debug-info") {
    generate_mini_debug_info_ = false;
  } else if (option == "--generate-build-id") {
    generate_build_id_ = true;
  } else if (option == "--no-generate-build-id") {
    generate_build_id_ = false;
  } else if (option == "--debuggable") {
    debuggable_ = true;
  } else if (option.starts_with("--top-k-profile-threshold=")) {
    ParseDouble(option.data(), '=', 0.0, 100.0, &top_k_profile_threshold_, Usage);
  } else if (option == "--abort-on-hard-verifier-error") {
    abort_on_hard_verifier_failure_ = true;
  } else if (option.starts_with("--dump-init-failures=")) {
    ParseDumpInitFailures(option, Usage);
  } else if (option.starts_with("--dump-cfg=")) {
    dump_cfg_file_name_ = option.substr(strlen("--dump-cfg=")).data();
  } else if (option == "--dump-cfg-append") {
    dump_cfg_append_ = true;
  } else if (option.starts_with("--register-allocation-strategy=")) {
    ParseRegisterAllocationStrategy(option, Usage);
  } else {
    // Option not recognized.
    return false;
  }
  return true;
}

}  // namespace art
