/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_COMPILER_COMMON_COMPILER_TEST_H_
#define ART_COMPILER_COMMON_COMPILER_TEST_H_

#include <list>
#include <vector>

#include "android-base/logging.h"
#include "arch/instruction_set.h"
#include "arch/instruction_set_features.h"
#include "base/macros.h"
#include "common_runtime_test.h"
#include "compiler.h"
#include "driver/compiled_code_storage.h"
#include "jni.h"
#include "linker/linker_patch.h"
#include "oat/oat_file.h"

namespace art HIDDEN {
namespace mirror {
class ClassLoader;
}  // namespace mirror

class CompilerOptions;
class CumulativeLogger;
class DexFile;
class TimingLogger;

template<class T> class Handle;

// Export all symbols in `CommonCompilerTestImpl` for dex2oat tests.
class EXPORT CommonCompilerTestImpl {
 public:
  static std::unique_ptr<CompilerOptions> CreateCompilerOptions(InstructionSet instruction_set,
                                                                const std::string& variant);

  CommonCompilerTestImpl();
  virtual ~CommonCompilerTestImpl();

  // Create an executable copy of the code with given metadata.
  const void* MakeExecutable(ArrayRef<const uint8_t> code,
                             ArrayRef<const uint8_t> vmap_table,
                             InstructionSet instruction_set);

 protected:
  void SetUp();

  void SetUpRuntimeOptionsImpl();

  virtual CompilerFilter::Filter GetCompilerFilter() const {
    return CompilerFilter::kDefaultCompilerFilter;
  }

  void TearDown();

  void CompileMethod(ArtMethod* method) REQUIRES_SHARED(Locks::mutator_lock_);

  void ApplyInstructionSet();
  void OverrideInstructionSetFeatures(InstructionSet instruction_set, const std::string& variant);

  void ClearBootImageOption();

  InstructionSet instruction_set_ =
      (kRuntimeISA == InstructionSet::kArm) ? InstructionSet::kThumb2 : kRuntimeISA;
  // Take the default set of instruction features from the build.
  std::unique_ptr<const InstructionSetFeatures> instruction_set_features_
      = InstructionSetFeatures::FromCppDefines();

  std::unique_ptr<CompilerOptions> compiler_options_;

 protected:
  virtual ClassLinker* GetClassLinker() = 0;
  virtual Runtime* GetRuntime() = 0;

  class OneCompiledMethodStorage final : public CompiledCodeStorage {
   public:
    OneCompiledMethodStorage() {}
    ~OneCompiledMethodStorage() {}

    CompiledMethod* CreateCompiledMethod(InstructionSet instruction_set,
                                         ArrayRef<const uint8_t> code,
                                         ArrayRef<const uint8_t> stack_map,
                                         [[maybe_unused]] ArrayRef<const uint8_t> cfi,
                                         ArrayRef<const linker::LinkerPatch> patches,
                                         [[maybe_unused]] bool is_intrinsic) override {
      // Supports only one method at a time.
      CHECK_EQ(instruction_set_, InstructionSet::kNone);
      CHECK_NE(instruction_set, InstructionSet::kNone);
      instruction_set_ = instruction_set;
      CHECK(code_.empty());
      CHECK(!code.empty());
      code_.assign(code.begin(), code.end());
      CHECK(stack_map_.empty());
      CHECK(!stack_map.empty());
      stack_map_.assign(stack_map.begin(), stack_map.end());
      CHECK(patches.empty()) << "Linker patches are unsupported for compiler gtests.";
      return reinterpret_cast<CompiledMethod*>(this);
    }

    ArrayRef<const uint8_t> GetThunkCode(
        [[maybe_unused]] const linker::LinkerPatch& patch,
        [[maybe_unused]] /*out*/ std::string* debug_name) override {
      LOG(FATAL) << "Unsupported.";
      UNREACHABLE();
    }

    void SetThunkCode([[maybe_unused]] const linker::LinkerPatch& patch,
                      [[maybe_unused]] ArrayRef<const uint8_t> code,
                      [[maybe_unused]] const std::string& debug_name) override {
      LOG(FATAL) << "Unsupported.";
      UNREACHABLE();
    }

    InstructionSet GetInstructionSet() const {
      CHECK_NE(instruction_set_, InstructionSet::kNone);
      return instruction_set_;
    }

    ArrayRef<const uint8_t> GetCode() const {
      CHECK(!code_.empty());
      return ArrayRef<const uint8_t>(code_);
    }

    ArrayRef<const uint8_t> GetStackMap() const {
      CHECK(!stack_map_.empty());
      return ArrayRef<const uint8_t>(stack_map_);
    }

   private:
    InstructionSet instruction_set_ = InstructionSet::kNone;
    std::vector<uint8_t> code_;
    std::vector<uint8_t> stack_map_;
  };

 private:
  class CodeAndMetadata;

  std::vector<CodeAndMetadata> code_and_metadata_;
};

template <typename RuntimeBase>
class CommonCompilerTestBase : public CommonCompilerTestImpl, public RuntimeBase {
 public:
  void SetUp() override {
    RuntimeBase::SetUp();
    CommonCompilerTestImpl::SetUp();
  }
  void SetUpRuntimeOptions(RuntimeOptions* options) override {
    RuntimeBase::SetUpRuntimeOptions(options);
    CommonCompilerTestImpl::SetUpRuntimeOptionsImpl();
  }
  void TearDown() override {
    CommonCompilerTestImpl::TearDown();
    RuntimeBase::TearDown();
  }

 protected:
  ClassLinker* GetClassLinker() override {
    return RuntimeBase::class_linker_;
  }
  Runtime* GetRuntime() override {
    return RuntimeBase::runtime_.get();
  }
};

class CommonCompilerTest : public CommonCompilerTestBase<CommonRuntimeTest> {};

template <typename Param>
class CommonCompilerTestWithParam
    : public CommonCompilerTestBase<CommonRuntimeTestWithParam<Param>> {};

}  // namespace art

#endif  // ART_COMPILER_COMMON_COMPILER_TEST_H_
