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

#ifndef ART_COMPILER_DRIVER_COMPILER_OPTIONS_H_
#define ART_COMPILER_DRIVER_COMPILER_OPTIONS_H_

#include <ostream>
#include <string>
#include <vector>

#include "base/macros.h"
#include "compiler_filter.h"
#include "globals.h"
#include "optimizing/register_allocator.h"
#include "utils.h"

namespace art {

namespace verifier {
  class VerifierDepsTest;
}

class DexFile;

class CompilerOptions FINAL {
 public:
  // Guide heuristics to determine whether to compile method if profile data not available.
  static const size_t kDefaultHugeMethodThreshold = 10000;
  static const size_t kDefaultLargeMethodThreshold = 600;
  static const size_t kDefaultSmallMethodThreshold = 60;
  static const size_t kDefaultTinyMethodThreshold = 20;
  static const size_t kDefaultNumDexMethodsThreshold = 900;
  static constexpr double kDefaultTopKProfileThreshold = 90.0;
  static const bool kDefaultGenerateDebugInfo = false;
  static const bool kDefaultGenerateMiniDebugInfo = false;
  static const size_t kDefaultInlineMaxCodeUnits = 32;
  static constexpr size_t kUnsetInlineMaxCodeUnits = -1;

  // IntegerOpLatency
  // FloatingPointOpLatency
  // DataProcWithShifterOpLatency
  // MulIntegerLatency
  // MulFloatingPointLatency
  // DivIntegerLatency
  // DivFloatLatency
  // DivDoubleLatency
  // TypeConversionFloatingPointIntegerLatency
  // MemoryLoadLatency
  // MemoryStoreLatency
  // MemoryBarrierLatency
  // BranchLatency
  // CallLatency
  // CallInternalLatency
  // LoadStringInternalLatency
  // NopLatency
  // LoadWithBakerReadBarrierLatency
  // RuntimeTypeCheckLatency
  static constexpr size_t kSchedulerStrength = 10;
  static constexpr size_t kArmIntegerOpLatency = 1;                           // 2;
  static constexpr size_t kArmFloatingPointOpLatency = 1;                     // 15;
  static constexpr size_t kArmDataProcWithShifterOpLatency = 1;               // 4;
  static constexpr size_t kArmMulIntegerLatency = 1;                          // 5;
  static constexpr size_t kArmMulFloatingPointLatency = 1;                    // 11;
  static constexpr size_t kArmDivIntegerLatency = 1;                          // 20;
  static constexpr size_t kArmDivFloatLatency = 1;                            // 27;
  static constexpr size_t kArmDivDoubleLatency = 1;                           // 31;
  static constexpr size_t kArmTypeConversionFloatingPointIntegerLatency = 1;  // 20;
  static constexpr size_t kArmMemoryLoadLatency = 1;                          // 8;
  static constexpr size_t kArmMemoryStoreLatency = 1;                         // 8;
  static constexpr size_t kArmMemoryBarrierLatency = 1;                       // 8;
  static constexpr size_t kArmBranchLatency= 1;                               // 4;
  static constexpr size_t kArmCallLatency = 1;                                // 34;
  static constexpr size_t kArmCallInternalLatency = 1;                        // 30;
  static constexpr size_t kArmLoadStringInternalLatency = 1;                  // 11;
  static constexpr size_t kArmNopLatency = 1;                                 // 3;
  static constexpr size_t kArmLoadWithBakerReadBarrierLatency = 1;            // 7;
  static constexpr size_t kArmRuntimeTypeCheckLatency = 1;                    // 80;

  CompilerOptions();
  ~CompilerOptions();

  CompilerOptions(CompilerFilter::Filter compiler_filter,
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
                  const std::vector<std::string>* passes_to_run);

  CompilerFilter::Filter GetCompilerFilter() const {
    return compiler_filter_;
  }

  void SetCompilerFilter(CompilerFilter::Filter compiler_filter) {
    compiler_filter_ = compiler_filter;
  }

  bool IsAotCompilationEnabled() const {
    return CompilerFilter::IsAotCompilationEnabled(compiler_filter_);
  }

  bool IsJniCompilationEnabled() const {
    return CompilerFilter::IsJniCompilationEnabled(compiler_filter_);
  }

  bool IsQuickeningCompilationEnabled() const {
    return CompilerFilter::IsQuickeningCompilationEnabled(compiler_filter_);
  }

  bool IsVerificationEnabled() const {
    return CompilerFilter::IsVerificationEnabled(compiler_filter_);
  }

  bool AssumeClassesAreVerified() const {
    return compiler_filter_ == CompilerFilter::kAssumeVerified;
  }

  bool VerifyAtRuntime() const {
    return compiler_filter_ == CompilerFilter::kExtract;
  }

  bool IsAnyCompilationEnabled() const {
    return CompilerFilter::IsAnyCompilationEnabled(compiler_filter_);
  }

  size_t GetHugeMethodThreshold() const {
    return huge_method_threshold_;
  }

  size_t GetLargeMethodThreshold() const {
    return large_method_threshold_;
  }

  size_t GetSmallMethodThreshold() const {
    return small_method_threshold_;
  }

  size_t GetTinyMethodThreshold() const {
    return tiny_method_threshold_;
  }

  bool IsHugeMethod(size_t num_dalvik_instructions) const {
    return num_dalvik_instructions > huge_method_threshold_;
  }

  bool IsLargeMethod(size_t num_dalvik_instructions) const {
    return num_dalvik_instructions > large_method_threshold_;
  }

  bool IsSmallMethod(size_t num_dalvik_instructions) const {
    return num_dalvik_instructions > small_method_threshold_;
  }

  bool IsTinyMethod(size_t num_dalvik_instructions) const {
    return num_dalvik_instructions > tiny_method_threshold_;
  }

  size_t GetNumDexMethodsThreshold() const {
    return num_dex_methods_threshold_;
  }

  size_t GetInlineMaxCodeUnits() const {
    return inline_max_code_units_;
  }
  void SetInlineMaxCodeUnits(size_t units) {
    inline_max_code_units_ = units;
  }

  double GetTopKProfileThreshold() const {
    return top_k_profile_threshold_;
  }

  bool GetDebuggable() const {
    return debuggable_;
  }

  bool GetNativeDebuggable() const {
    return GetDebuggable() && GetGenerateDebugInfo();
  }

  // This flag controls whether the compiler collects debugging information.
  // The other flags control how the information is written to disk.
  bool GenerateAnyDebugInfo() const {
    return GetGenerateDebugInfo() || GetGenerateMiniDebugInfo();
  }

  bool GetGenerateDebugInfo() const {
    return generate_debug_info_;
  }

  bool GetGenerateMiniDebugInfo() const {
    return generate_mini_debug_info_;
  }

  bool GetGenerateBuildId() const {
    return generate_build_id_;
  }

  bool GetImplicitNullChecks() const {
    return implicit_null_checks_;
  }

  bool GetImplicitStackOverflowChecks() const {
    return implicit_so_checks_;
  }

  bool GetImplicitSuspendChecks() const {
    return implicit_suspend_checks_;
  }

  bool IsBootImage() const {
    return boot_image_;
  }

  bool IsAppImage() const {
    return app_image_;
  }

  // Should the code be compiled as position independent?
  bool GetCompilePic() const {
    return compile_pic_;
  }

  bool HasVerboseMethods() const {
    return verbose_methods_ != nullptr && !verbose_methods_->empty();
  }

  bool IsVerboseMethod(const std::string& pretty_method) const {
    for (const std::string& cur_method : *verbose_methods_) {
      if (pretty_method.find(cur_method) != std::string::npos) {
        return true;
      }
    }
    return false;
  }

  std::ostream* GetInitFailureOutput() const {
    return init_failure_output_.get();
  }

  bool AbortOnHardVerifierFailure() const {
    return abort_on_hard_verifier_failure_;
  }

  const std::vector<const DexFile*>* GetNoInlineFromDexFile() const {
    return no_inline_from_;
  }

  bool ParseCompilerOption(const StringPiece& option, UsageFn Usage);

  const std::string& GetDumpCfgFileName() const {
    return dump_cfg_file_name_;
  }

  bool GetDumpCfgAppend() const {
    return dump_cfg_append_;
  }

  bool IsForceDeterminism() const {
    return force_determinism_;
  }

  RegisterAllocator::Strategy GetRegisterAllocationStrategy() const {
    return register_allocation_strategy_;
  }

  const std::vector<std::string>* GetPassesToRun() const {
    return passes_to_run_;
  }

  size_t GetSchedulerStrength() const {return scheduler_strength_;}
  // auto generated using string: size_t Get_scheduler_Arm&() const { return scheduler_Arm&_; }
  size_t Get_scheduler_ArmIntegerOpLatency() const { return scheduler_ArmIntegerOpLatency_; }
  size_t Get_scheduler_ArmFloatingPointOpLatency() const { return scheduler_ArmFloatingPointOpLatency_; }
  size_t Get_scheduler_ArmDataProcWithShifterOpLatency() const { return scheduler_ArmDataProcWithShifterOpLatency_; }
  size_t Get_scheduler_ArmMulIntegerLatency() const { return scheduler_ArmMulIntegerLatency_; }
  size_t Get_scheduler_ArmMulFloatingPointLatency() const { return scheduler_ArmMulFloatingPointLatency_; }
  size_t Get_scheduler_ArmDivIntegerLatency() const { return scheduler_ArmDivIntegerLatency_; }
  size_t Get_scheduler_ArmDivFloatLatency() const { return scheduler_ArmDivFloatLatency_; }
  size_t Get_scheduler_ArmDivDoubleLatency() const { return scheduler_ArmDivDoubleLatency_; }
  size_t Get_scheduler_ArmTypeConversionFloatingPointIntegerLatency() const { return scheduler_ArmTypeConversionFloatingPointIntegerLatency_; }
  size_t Get_scheduler_ArmMemoryLoadLatency() const { return scheduler_ArmMemoryLoadLatency_; }
  size_t Get_scheduler_ArmMemoryStoreLatency() const { return scheduler_ArmMemoryStoreLatency_; }
  size_t Get_scheduler_ArmMemoryBarrierLatency() const { return scheduler_ArmMemoryBarrierLatency_; }
  size_t Get_scheduler_ArmBranchLatency() const { return scheduler_ArmBranchLatency_; }
  size_t Get_scheduler_ArmCallLatency() const { return scheduler_ArmCallLatency_; }
  size_t Get_scheduler_ArmCallInternalLatency() const { return scheduler_ArmCallInternalLatency_; }
  size_t Get_scheduler_ArmLoadStringInternalLatency() const { return scheduler_ArmLoadStringInternalLatency_; }
  size_t Get_scheduler_ArmNopLatency() const { return scheduler_ArmNopLatency_; }
  size_t Get_scheduler_ArmLoadWithBakerReadBarrierLatency() const { return scheduler_ArmLoadWithBakerReadBarrierLatency_; }
  size_t Get_scheduler_ArmRuntimeTypeCheckLatency() const { return scheduler_ArmRuntimeTypeCheckLatency_; }

 private:
  void ParseDumpInitFailures(const StringPiece& option, UsageFn Usage);
  void ParseDumpCfgPasses(const StringPiece& option, UsageFn Usage);
  void ParseInlineMaxCodeUnits(const StringPiece& option, UsageFn Usage);
  void ParseNumDexMethods(const StringPiece& option, UsageFn Usage);
  void ParseTinyMethodMax(const StringPiece& option, UsageFn Usage);
  void ParseSmallMethodMax(const StringPiece& option, UsageFn Usage);
  void ParseLargeMethodMax(const StringPiece& option, UsageFn Usage);
  void ParseHugeMethodMax(const StringPiece& option, UsageFn Usage);
  void ParseSchedulerStrength(const StringPiece& option, UsageFn Usage);
  // auto generated using string: void Parse_scheduler_Arm&(const StringPiece\& option, UsageFn Usage);
  void Parse_scheduler_ArmIntegerOpLatency(const StringPiece& option, UsageFn Usage);
  void Parse_scheduler_ArmFloatingPointOpLatency(const StringPiece& option, UsageFn Usage);
  void Parse_scheduler_ArmDataProcWithShifterOpLatency(const StringPiece& option, UsageFn Usage);
  void Parse_scheduler_ArmMulIntegerLatency(const StringPiece& option, UsageFn Usage);
  void Parse_scheduler_ArmMulFloatingPointLatency(const StringPiece& option, UsageFn Usage);
  void Parse_scheduler_ArmDivIntegerLatency(const StringPiece& option, UsageFn Usage);
  void Parse_scheduler_ArmDivFloatLatency(const StringPiece& option, UsageFn Usage);
  void Parse_scheduler_ArmDivDoubleLatency(const StringPiece& option, UsageFn Usage);
  void Parse_scheduler_ArmTypeConversionFloatingPointIntegerLatency(const StringPiece& option, UsageFn Usage);
  void Parse_scheduler_ArmMemoryLoadLatency(const StringPiece& option, UsageFn Usage);
  void Parse_scheduler_ArmMemoryStoreLatency(const StringPiece& option, UsageFn Usage);
  void Parse_scheduler_ArmMemoryBarrierLatency(const StringPiece& option, UsageFn Usage);
  void Parse_scheduler_ArmBranchLatency(const StringPiece& option, UsageFn Usage);
  void Parse_scheduler_ArmCallLatency(const StringPiece& option, UsageFn Usage);
  void Parse_scheduler_ArmCallInternalLatency(const StringPiece& option, UsageFn Usage);
  void Parse_scheduler_ArmLoadStringInternalLatency(const StringPiece& option, UsageFn Usage);
  void Parse_scheduler_ArmNopLatency(const StringPiece& option, UsageFn Usage);
  void Parse_scheduler_ArmLoadWithBakerReadBarrierLatency(const StringPiece& option, UsageFn Usage);
  void Parse_scheduler_ArmRuntimeTypeCheckLatency(const StringPiece& option, UsageFn Usage);
  void ParseRegisterAllocationStrategy(const StringPiece& option, UsageFn Usage);

  CompilerFilter::Filter compiler_filter_;
  size_t huge_method_threshold_;
  size_t large_method_threshold_;
  size_t small_method_threshold_;
  size_t tiny_method_threshold_;
  size_t num_dex_methods_threshold_;
  size_t inline_max_code_units_;

  // Dex files from which we should not inline code.
  // This is usually a very short list (i.e. a single dex file), so we
  // prefer vector<> over a lookup-oriented container, such as set<>.
  const std::vector<const DexFile*>* no_inline_from_;

  bool boot_image_;
  bool app_image_;
  // When using a profile file only the top K% of the profiled samples will be compiled.
  double top_k_profile_threshold_;
  bool debuggable_;
  bool generate_debug_info_;
  bool generate_mini_debug_info_;
  bool generate_build_id_;
  bool implicit_null_checks_;
  bool implicit_so_checks_;
  bool implicit_suspend_checks_;
  bool compile_pic_;

  // Vector of methods to have verbose output enabled for.
  const std::vector<std::string>* verbose_methods_;

  // Abort compilation with an error if we find a class that fails verification with a hard
  // failure.
  bool abort_on_hard_verifier_failure_;

  // Log initialization of initialization failures to this stream if not null.
  std::unique_ptr<std::ostream> init_failure_output_;

  std::string dump_cfg_file_name_;
  bool dump_cfg_append_;

  // Whether the compiler should trade performance for determinism to guarantee exactly reproducible
  // outcomes.
  bool force_determinism_;

  size_t scheduler_strength_;
  size_t scheduler_ArmIntegerOpLatency_;
  size_t scheduler_ArmFloatingPointOpLatency_;
  size_t scheduler_ArmDataProcWithShifterOpLatency_;
  size_t scheduler_ArmMulIntegerLatency_;
  size_t scheduler_ArmMulFloatingPointLatency_;
  size_t scheduler_ArmDivIntegerLatency_;
  size_t scheduler_ArmDivFloatLatency_;
  size_t scheduler_ArmDivDoubleLatency_;
  size_t scheduler_ArmTypeConversionFloatingPointIntegerLatency_;
  size_t scheduler_ArmMemoryLoadLatency_;
  size_t scheduler_ArmMemoryStoreLatency_;
  size_t scheduler_ArmMemoryBarrierLatency_;
  size_t scheduler_ArmBranchLatency_;
  size_t scheduler_ArmCallLatency_;
  size_t scheduler_ArmCallInternalLatency_;
  size_t scheduler_ArmLoadStringInternalLatency_;
  size_t scheduler_ArmNopLatency_;
  size_t scheduler_ArmLoadWithBakerReadBarrierLatency_;
  size_t scheduler_ArmRuntimeTypeCheckLatency_;

  RegisterAllocator::Strategy register_allocation_strategy_;

  // If not null, specifies optimization passes which will be run instead of defaults.
  // Note that passes_to_run_ is not checked for correctness and providing an incorrect
  // list of passes can lead to unexpected compiler behaviour. This is caused by dependencies
  // between passes. Failing to satisfy them can for example lead to compiler crashes.
  // Passing pass names which are not recognized by the compiler will result in
  // compiler-dependant behavior.
  const std::vector<std::string>* passes_to_run_;

  friend class Dex2Oat;
  friend class DexToDexDecompilerTest;
  friend class CommonCompilerTest;
  friend class verifier::VerifierDepsTest;

  DISALLOW_COPY_AND_ASSIGN(CompilerOptions);
};

}  // namespace art

#endif  // ART_COMPILER_DRIVER_COMPILER_OPTIONS_H_
