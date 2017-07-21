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

#include "fixed_up_dex_file.h"

#include "class_linker.h"
#include "compiler/common_compiler_test.h"
#include "compiler/compiled_method.h"
#include "compiler/driver/compiler_options.h"
#include "compiler/driver/compiler_driver.h"
#include "compiler_callbacks.h"
#include "dex_file.h"
#include "handle_scope-inl.h"
#include "verifier/method_verifier-inl.h"
#include "mirror/class_loader.h"
#include "runtime.h"
#include "thread.h"
#include "scoped_thread_state_change-inl.h"

namespace art {

class FixedUpDexFileTest : public CommonCompilerTest {
 public:
  void CompileAll(jobject class_loader) REQUIRES(!Locks::mutator_lock_) {
    TimingLogger timings("CompilerDriverTest::CompileAll", false, false);
    TimingLogger::ScopedTiming t(__FUNCTION__, &timings);
    compiler_options_->SetCompilerFilter(CompilerFilter::kQuicken);
    compiler_driver_->CompileAll(class_loader, GetDexFiles(class_loader), &timings);
  }

  void RunTest(const char* dex_name) NO_THREAD_SAFETY_ANALYSIS {
    Thread* self = Thread::Current();
    // First load the original dex file.
    jobject original_class_loader;
    {
      ScopedObjectAccess soa(self);
      original_class_loader = LoadDex(dex_name);
    }
    const DexFile* original_dex_file = GetDexFiles(original_class_loader)[0];

    // Load the dex file again and make it writable to quicken them.
    jobject class_loader;
    const DexFile* updated_dex_file = nullptr;
    {
      ScopedObjectAccess soa(self);
      class_loader = LoadDex(dex_name);
      updated_dex_file = GetDexFiles(class_loader)[0];
      Runtime::Current()->GetClassLinker()->RegisterDexFile(
          *updated_dex_file, soa.Decode<mirror::ClassLoader>(class_loader).Ptr());
    }
    // The dex files should be identical.
    int cmp = memcmp(original_dex_file->Begin(),
                     updated_dex_file->Begin(),
                     updated_dex_file->Size());
    ASSERT_EQ(0, cmp);

    updated_dex_file->EnableWrite();
    CompileAll(class_loader);
    // The dex files should be different after quickening.
    cmp = memcmp(original_dex_file->Begin(), updated_dex_file->Begin(), updated_dex_file->Size());
    ASSERT_NE(0, cmp);

    std::unique_ptr<openjdkjvmti::FixedUpDexFile> final_dex_file(
        openjdkjvmti::FixedUpDexFile::Create(*updated_dex_file));
    ASSERT_NE(final_dex_file.get(), nullptr);
    // Make sure we didn't touch the updated_dex_file
    cmp = memcmp(original_dex_file->Begin(), updated_dex_file->Begin(), updated_dex_file->Size());
    ASSERT_NE(0, cmp);
    ASSERT_EQ(original_dex_file->Size(), final_dex_file->Size());
    // Make sure we have the original back in final_dex_file
    cmp = memcmp(original_dex_file->Begin(), final_dex_file->Begin(), final_dex_file->Size());
    for (size_t i = 0; i < final_dex_file->Size(); i++) {
      if (original_dex_file->Begin()[i] != updated_dex_file->Begin()[i]) {
        LOG(ERROR) << "Byte " << i << " differs. " << std::hex << "0x"
                   << static_cast<uint32_t>(original_dex_file->Begin()[i]) << " vs 0x"
                   << static_cast<uint32_t>(final_dex_file->Begin()[i]);
      }
    }
    for (size_t i = 0; i < final_dex_file->Size(); i++) {
      if (original_dex_file->Begin()[i] != final_dex_file->Begin()[i]) {
        LOG(ERROR) << "Byte " << i << " differs. " << std::hex << "0x"
                   << static_cast<uint32_t>(original_dex_file->Begin()[i]) << " vs 0x"
                   << static_cast<uint32_t>(final_dex_file->Begin()[i]);
      }
    }
    LOG(ERROR) << "size is " << final_dex_file->Size();
    ASSERT_EQ(0, cmp);
  }
};

TEST_F(FixedUpDexFileTest, FixedUpDexFile) {
  RunTest("DexToDexDecompiler");
}

}  // namespace art
