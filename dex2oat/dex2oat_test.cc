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

#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <iterator>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "android-base/logging.h"
#include "android-base/macros.h"
#include "android-base/stringprintf.h"
#include "arch/instruction_set_features.h"
#include "base/macros.h"
#include "base/mutex-inl.h"
#include "base/string_view_cpp20.h"
#include "base/utils.h"
#include "base/zip_archive.h"
#include "common_runtime_test.h"
#include "dex/art_dex_file_loader.h"
#include "dex/base64_test_util.h"
#include "dex/bytecode_utils.h"
#include "dex/class_accessor-inl.h"
#include "dex/code_item_accessors-inl.h"
#include "dex/dex_file-inl.h"
#include "dex/dex_file_loader.h"
#include "dex2oat_environment_test.h"
#include "elf_file.h"
#include "elf_file_impl.h"
#include "gc_root-inl.h"
#include "intern_table-inl.h"
#include "oat.h"
#include "oat_file.h"
#include "profile/profile_compilation_info.h"
#include "vdex_file.h"
#include "ziparchive/zip_writer.h"

namespace art {

using android::base::StringPrintf;

class Dex2oatTest : public Dex2oatEnvironmentTest {
 public:
  void TearDown() override {
    Dex2oatEnvironmentTest::TearDown();

    output_ = "";
    error_msg_ = "";
  }

 protected:
  int GenerateOdexForTestWithStatus(const std::vector<std::string>& dex_locations,
                                    const std::string& odex_location,
                                    CompilerFilter::Filter filter,
                                    std::string* error_msg,
                                    const std::vector<std::string>& extra_args = {},
                                    bool use_fd = false) {
    std::unique_ptr<File> oat_file;
    std::vector<std::string> args;
    // Add dex file args.
    for (const std::string& dex_location : dex_locations) {
      args.push_back("--dex-file=" + dex_location);
    }
    if (use_fd) {
      oat_file.reset(OS::CreateEmptyFile(odex_location.c_str()));
      CHECK(oat_file != nullptr) << odex_location;
      args.push_back("--oat-fd=" + std::to_string(oat_file->Fd()));
      args.push_back("--oat-location=" + odex_location);
    } else {
      args.push_back("--oat-file=" + odex_location);
    }
    args.push_back("--compiler-filter=" + CompilerFilter::NameOfFilter(filter));
    args.push_back("--runtime-arg");
    args.push_back("-Xnorelocate");

    // Unless otherwise stated, use a small amount of threads, so that potential aborts are
    // shorter. This can be overridden with extra_args.
    args.push_back("-j4");

    args.insert(args.end(), extra_args.begin(), extra_args.end());

    int status = Dex2Oat(args, &output_, error_msg);
    if (oat_file != nullptr) {
      CHECK_EQ(oat_file->FlushClose(), 0) << "Could not flush and close oat file";
    }
    return status;
  }

  ::testing::AssertionResult GenerateOdexForTest(
      const std::string& dex_location,
      const std::string& odex_location,
      CompilerFilter::Filter filter,
      const std::vector<std::string>& extra_args = {},
      bool expect_success = true,
      bool use_fd = false,
      bool use_zip_fd = false) WARN_UNUSED {
    return GenerateOdexForTest(dex_location,
                               odex_location,
                               filter,
                               extra_args,
                               expect_success,
                               use_fd,
                               use_zip_fd,
                               [](const OatFile&) {});
  }

  bool test_accepts_odex_file_on_failure = false;

  template <typename T>
  ::testing::AssertionResult GenerateOdexForTest(
      const std::string& dex_location,
      const std::string& odex_location,
      CompilerFilter::Filter filter,
      const std::vector<std::string>& extra_args,
      bool expect_success,
      bool use_fd,
      bool use_zip_fd,
      T check_oat) WARN_UNUSED {
    std::vector<std::string> dex_locations;
    if (use_zip_fd) {
      std::string loc_arg = "--zip-location=" + dex_location;
      CHECK(std::any_of(extra_args.begin(),
                        extra_args.end(),
                        [&](const std::string& s) { return s == loc_arg; }));
      CHECK(std::any_of(extra_args.begin(),
                        extra_args.end(),
                        [](const std::string& s) { return StartsWith(s, "--zip-fd="); }));
    } else {
      dex_locations.push_back(dex_location);
    }
    std::string error_msg;
    int status = GenerateOdexForTestWithStatus(dex_locations,
                                               odex_location,
                                               filter,
                                               &error_msg,
                                               extra_args,
                                               use_fd);
    bool success = (WIFEXITED(status) && WEXITSTATUS(status) == 0);
    if (expect_success) {
      if (!success) {
        return ::testing::AssertionFailure()
            << "Failed to compile odex: " << error_msg << std::endl << output_;
      }

      // Verify the odex file was generated as expected.
      std::unique_ptr<OatFile> odex_file(OatFile::Open(/*zip_fd=*/ -1,
                                                       odex_location.c_str(),
                                                       odex_location.c_str(),
                                                       /*executable=*/ false,
                                                       /*low_4gb=*/ false,
                                                       dex_location,
                                                       &error_msg));
      if (odex_file == nullptr) {
        return ::testing::AssertionFailure() << "Could not open odex file: " << error_msg;
      }

      CheckFilter(filter, odex_file->GetCompilerFilter());
      check_oat(*(odex_file.get()));
    } else {
      if (success) {
        return ::testing::AssertionFailure() << "Succeeded to compile odex: " << output_;
      }

      error_msg_ = error_msg;

      if (!test_accepts_odex_file_on_failure) {
        // Verify there's no loadable odex file.
        std::unique_ptr<OatFile> odex_file(OatFile::Open(/*zip_fd=*/ -1,
                                                         odex_location.c_str(),
                                                         odex_location.c_str(),
                                                         /*executable=*/ false,
                                                         /*low_4gb=*/ false,
                                                         dex_location,
                                                         &error_msg));
        if (odex_file != nullptr) {
          return ::testing::AssertionFailure() << "Could open odex file: " << error_msg;
        }
      }
    }
    return ::testing::AssertionSuccess();
  }

  // Check the input compiler filter against the generated oat file's filter. May be overridden
  // in subclasses when equality is not expected.
  virtual void CheckFilter(CompilerFilter::Filter expected, CompilerFilter::Filter actual) {
    EXPECT_EQ(expected, actual);
  }

  std::string output_ = "";
  std::string error_msg_ = "";
};

class Dex2oatSwapTest : public Dex2oatTest {
 protected:
  void RunTest(bool use_fd, bool expect_use, const std::vector<std::string>& extra_args = {}) {
    std::string dex_location = GetScratchDir() + "/Dex2OatSwapTest.jar";
    std::string odex_location = GetOdexDir() + "/Dex2OatSwapTest.odex";

    Copy(GetTestDexFileName(), dex_location);

    std::vector<std::string> copy(extra_args);

    std::unique_ptr<ScratchFile> sf;
    if (use_fd) {
      sf.reset(new ScratchFile());
      copy.push_back(android::base::StringPrintf("--swap-fd=%d", sf->GetFd()));
    } else {
      std::string swap_location = GetOdexDir() + "/Dex2OatSwapTest.odex.swap";
      copy.push_back("--swap-file=" + swap_location);
    }
    ASSERT_TRUE(GenerateOdexForTest(dex_location, odex_location, CompilerFilter::kSpeed, copy));

    CheckValidity();
    CheckResult(expect_use);
  }

  virtual std::string GetTestDexFileName() {
    return Dex2oatEnvironmentTest::GetTestDexFileName("VerifierDeps");
  }

  virtual void CheckResult(bool expect_use) {
    if (kIsTargetBuild) {
      CheckTargetResult(expect_use);
    } else {
      CheckHostResult(expect_use);
    }
  }

  virtual void CheckTargetResult(bool expect_use ATTRIBUTE_UNUSED) {
    // TODO: Ignore for now, as we won't capture any output (it goes to the logcat). We may do
    //       something for variants with file descriptor where we can control the lifetime of
    //       the swap file and thus take a look at it.
  }

  virtual void CheckHostResult(bool expect_use) {
    if (!kIsTargetBuild) {
      if (expect_use) {
        EXPECT_NE(output_.find("Large app, accepted running with swap."), std::string::npos)
            << output_;
      } else {
        EXPECT_EQ(output_.find("Large app, accepted running with swap."), std::string::npos)
            << output_;
      }
    }
  }

  // Check whether the dex2oat run was really successful.
  virtual void CheckValidity() {
    if (kIsTargetBuild) {
      CheckTargetValidity();
    } else {
      CheckHostValidity();
    }
  }

  virtual void CheckTargetValidity() {
    // TODO: Ignore for now, as we won't capture any output (it goes to the logcat). We may do
    //       something for variants with file descriptor where we can control the lifetime of
    //       the swap file and thus take a look at it.
  }

  // On the host, we can get the dex2oat output. Here, look for "dex2oat took."
  virtual void CheckHostValidity() {
    EXPECT_NE(output_.find("dex2oat took"), std::string::npos) << output_;
  }
};

TEST_F(Dex2oatSwapTest, DoNotUseSwapDefaultSingleSmall) {
  RunTest(/*use_fd=*/ false, /*expect_use=*/ false);
  RunTest(/*use_fd=*/ true, /*expect_use=*/ false);
}

}  // namespace art
