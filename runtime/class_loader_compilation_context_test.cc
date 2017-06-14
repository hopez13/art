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

#include <gtest/gtest.h>


#include "class_loader_compilation_context.h"
#include "common_runtime_test.h"

#include "base/dchecked_vector.h"
#include "base/stl_util.h"
#include "class_linker.h"
#include "dex_file.h"
#include "oat_file_assistant.h"
#include "runtime.h"
#include "scoped_thread_state_change-inl.h"
#include "thread.h"

namespace art {

class ClassLoaderCompilationContextTest : public CommonRuntimeTest {
 public:
  void verifyContextSize(ClassLoaderCompilationContext* context, size_t expected_size) {
    ASSERT_TRUE(context != nullptr);
    ASSERT_EQ(expected_size, context->class_loader_chain_.size());
  }

  void verifyClassLoaderPCL(ClassLoaderCompilationContext* context,
                            size_t index,
                            std::string classpath) {
    verifyClassLoaderInfo(
        context, index, ClassLoaderCompilationContext::kPathClassLoader, classpath);
  }

  void verifyClassLoaderDLC(ClassLoaderCompilationContext* context,
                            size_t index,
                            std::string classpath) {
    verifyClassLoaderInfo(
        context, index, ClassLoaderCompilationContext::kDelegateLastClassLoader, classpath);
  }

  void verifyOpenDexFiles(
      ClassLoaderCompilationContext* context,
      size_t index,
      std::vector<std::vector<std::unique_ptr<const DexFile>>*>& all_dex_files) {
    ASSERT_TRUE(context != nullptr);
    ASSERT_TRUE(context->dex_files_open_attempted_);
    ASSERT_TRUE(context->dex_files_open_result_);
    ClassLoaderCompilationContext::ClassLoaderInfo& info = context->class_loader_chain_[index];
    ASSERT_EQ(all_dex_files.size(), info.classpath.size());
    size_t cur_open_dex_index = 0;
    for (size_t k = 0; k < all_dex_files.size(); k++) {
      std::vector<std::unique_ptr<const DexFile>>& dex_files_for_cp_elem = *(all_dex_files[k]);
      for (size_t i = 0; i < dex_files_for_cp_elem.size(); i++) {
        ASSERT_LT(cur_open_dex_index, info.opened_dex_files.size());

        std::unique_ptr<const DexFile>& opened_dex_file =
            info.opened_dex_files[cur_open_dex_index++];
        std::unique_ptr<const DexFile>& expected_dex_file = dex_files_for_cp_elem[i];

        ASSERT_EQ(expected_dex_file->GetLocation(), opened_dex_file->GetLocation());
        ASSERT_EQ(expected_dex_file->GetLocationChecksum(), opened_dex_file->GetLocationChecksum());
        ASSERT_EQ(info.classpath[k], opened_dex_file->GetBaseLocation());
      }
    }
  }

 private:
  void verifyClassLoaderInfo(ClassLoaderCompilationContext* context,
                             size_t index,
                             ClassLoaderCompilationContext::ClassLoaderType type,
                             std::string classpath) {
    ASSERT_TRUE(context != nullptr);
    ASSERT_GT(context->class_loader_chain_.size(), index);
    ClassLoaderCompilationContext::ClassLoaderInfo& info = context->class_loader_chain_[index];
    ASSERT_EQ(type, info.type);
    std::vector<std::string> expected_classpath;
    Split(classpath, ':', &expected_classpath);
    ASSERT_EQ(expected_classpath, info.classpath);
  }
};

TEST_F(ClassLoaderCompilationContextTest, ParseValidContextPCL) {
  std::unique_ptr<ClassLoaderCompilationContext> context =
      ClassLoaderCompilationContext::Create("PCL[a.dex]");
  verifyContextSize(context.get(), 1);
  verifyClassLoaderPCL(context.get(), 0, "a.dex");
}

TEST_F(ClassLoaderCompilationContextTest, ParseValidContextDLC) {
  std::unique_ptr<ClassLoaderCompilationContext> context =
      ClassLoaderCompilationContext::Create("DLC[a.dex]");
  verifyContextSize(context.get(), 1);
  verifyClassLoaderDLC(context.get(), 0, "a.dex");
}

TEST_F(ClassLoaderCompilationContextTest, ParseValidContextChain) {
  std::unique_ptr<ClassLoaderCompilationContext> context =
      ClassLoaderCompilationContext::Create("PCL[a.dex:b.dex];DLC[c.dex:d.dex];PCL[e.dex]");
  verifyContextSize(context.get(), 3);
  verifyClassLoaderPCL(context.get(), 0, "a.dex:b.dex");
  verifyClassLoaderDLC(context.get(), 1, "c.dex:d.dex");
  verifyClassLoaderPCL(context.get(), 2, "e.dex");
}

TEST_F(ClassLoaderCompilationContextTest, ParseValidContextSpecialSymbol) {
  std::unique_ptr<ClassLoaderCompilationContext> context =
    ClassLoaderCompilationContext::Create(OatFile::kSpecialSharedLibrary);
  verifyContextSize(context.get(), 0);
}

TEST_F(ClassLoaderCompilationContextTest, ParseInvalidValidContexts) {
  ASSERT_TRUE(nullptr == ClassLoaderCompilationContext::Create("ABC[a.dex]"));
  ASSERT_TRUE(nullptr == ClassLoaderCompilationContext::Create("PCL"));
  ASSERT_TRUE(nullptr == ClassLoaderCompilationContext::Create("PCL[a.dex"));
  ASSERT_TRUE(nullptr == ClassLoaderCompilationContext::Create("PCLa.dex]"));
  ASSERT_TRUE(nullptr == ClassLoaderCompilationContext::Create("PCL{a.dex}"));
  ASSERT_TRUE(nullptr == ClassLoaderCompilationContext::Create("PCL[a.dex];DLC[b.dex"));

  // Empty classpath is not allowed.
  ASSERT_TRUE(nullptr == ClassLoaderCompilationContext::Create("PCL[]"));
  ASSERT_TRUE(nullptr == ClassLoaderCompilationContext::Create("PCL[a.dex];DLC[]"));
}

TEST_F(ClassLoaderCompilationContextTest, OpenInvalidDexFiles) {
  std::unique_ptr<ClassLoaderCompilationContext> context =
      ClassLoaderCompilationContext::Create("PCL[does_not_exist.dex]");
  verifyContextSize(context.get(), 1);
  ASSERT_FALSE(context->OpenDexFiles(InstructionSet::kArm, "."));
}

TEST_F(ClassLoaderCompilationContextTest, OpenValidDexFiles) {
  std::string multidex_name = GetTestDexFileName("MultiDex");
  std::vector<std::unique_ptr<const DexFile>> multidex_files = OpenTestDexFiles("MultiDex");
  std::string myclass_dex_name = GetTestDexFileName("MyClass");
  std::vector<std::unique_ptr<const DexFile>> myclass_dex_files = OpenTestDexFiles("MyClass");
  std::string dex_name = GetTestDexFileName("Main");
  std::vector<std::unique_ptr<const DexFile>> dex_files = OpenTestDexFiles("Main");


  std::unique_ptr<ClassLoaderCompilationContext> context =
      ClassLoaderCompilationContext::Create(
          "PCL[" + multidex_name + ":" + myclass_dex_name + "];" +
          "DLC[" + dex_name + "]");

  ASSERT_TRUE(context->OpenDexFiles(InstructionSet::kArm, /*classpath_dir*/ ""));

  verifyContextSize(context.get(), 2);
  std::vector<std::vector<std::unique_ptr<const DexFile>>*> all_dex_files0;
  all_dex_files0.push_back(&multidex_files);
  all_dex_files0.push_back(&myclass_dex_files);
  std::vector<std::vector<std::unique_ptr<const DexFile>>*> all_dex_files1;
  all_dex_files1.push_back(&dex_files);

  verifyOpenDexFiles(context.get(), 0, all_dex_files0);
  verifyOpenDexFiles(context.get(), 1, all_dex_files1);
}

}  // namespace art
