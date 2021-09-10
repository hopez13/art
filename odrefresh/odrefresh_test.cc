/*
 * Copyright (C) 2021 The Android Open Source Project
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

#include "odrefresh.h"

#include <unistd.h>

#include <functional>
#include <memory>
#include <string_view>
#include <vector>

#include "android-base/stringprintf.h"
#include "base/stl_util.h"
#include "odr_artifacts.h"

#include "android-base/properties.h"
#include "android-base/scopeguard.h"
#include "android-base/strings.h"
#include "arch/instruction_set.h"
#include "base/common_art_test.h"
#include "base/file_utils.h"
#include "exec_utils.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "odr_common.h"
#include "odr_config.h"
#include "odr_dexopt.h"
#include "odr_fs_utils.h"
#include "odr_metrics.h"
#include "odrefresh/odrefresh.h"

#include "aidl/com/android/art/DexoptBcpExtArgs.h"
#include "aidl/com/android/art/DexoptSystemServerArgs.h"
#include "aidl/com/android/art/Isa.h"

namespace art {
namespace odrefresh {

using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Ge;
using ::testing::Return;
using aidl::com::android::art::DexoptBcpExtArgs;
using aidl::com::android::art::DexoptSystemServerArgs;
using aidl::com::android::art::Isa;

constexpr int kReplace = 1;

void CreateEmptyFile(const std::string& name) {
  File* file = OS::CreateEmptyFile(name.c_str());
  ASSERT_TRUE(file != nullptr);
  file->Release();
  delete file;
}

android::base::ScopeGuard<std::function<void()>> ScopedCreateEmptyFile(const std::string& name) {
  CreateEmptyFile(name);
  return android::base::ScopeGuard([=]() { unlink(name.c_str()); });
}

android::base::ScopeGuard<std::function<void()>> ScopedSetProperty(const std::string& key,
                                                                   const std::string& value) {
  std::string old_value = android::base::GetProperty(key, /*default_value=*/{});
  android::base::SetProperty(key, value);
  return android::base::ScopeGuard([=]() { android::base::SetProperty(key, old_value); });
}

class MockOdrDexopt : public OdrDexopt {
 public:
  // A workaround to avoid MOCK_METHOD on a method with an `std::string*` parameter, which will lead
  // to a conflict between gmock and android-base/logging.h (b/132668253).
  int DexoptBcpExtension(const DexoptBcpExtArgs& args,
                         time_t,
                         bool*,
                         std::string*) override {
    return DoDexoptBcpExtension(args);
  }

  int DexoptSystemServer(const DexoptSystemServerArgs& args,
                         time_t,
                         bool*,
                         std::string*) override {
    return DoDexoptSystemServer(args);
  }


  MOCK_METHOD(int, DoDexoptBcpExtension, (const DexoptBcpExtArgs&));
  MOCK_METHOD(int, DoDexoptSystemServer, (const DexoptSystemServerArgs&));
};

// Matches an FD of a file whose path matches `matcher`.
MATCHER_P(FdOf, matcher, "") {
  char path[PATH_MAX];
  int fd = arg;
  std::string proc_path = android::base::StringPrintf("/proc/self/fd/%d", fd);
  ssize_t len = readlink(proc_path.c_str(), path, sizeof(path));
  if (len < 0) {
    return false;
  }
  std::string path_str{path, (size_t)len};
  return ExplainMatchResult(matcher, path_str, result_listener);
}

class OdRefreshTest : public CommonArtTest {
 public:
  OdRefreshTest() : config_("odrefresh") {}

 protected:
  void SetUp() override {
    CommonArtTest::SetUp();

    temp_dir_ = std::make_unique<ScratchDir>();
    std::string_view temp_dir_path = temp_dir_->GetPath();
    android::base::ConsumeSuffix(&temp_dir_path, "/");

    std::string android_root_path = Concatenate({temp_dir_path, "/system"});
    ASSERT_TRUE(EnsureDirectoryExists(android_root_path));
    android_root_env_ = std::make_unique<ScopedUnsetEnvironmentVariable>("ANDROID_ROOT");
    setenv("ANDROID_ROOT", android_root_path.c_str(), kReplace);

    std::string android_art_root_path = Concatenate({temp_dir_path, "/apex/com.android.art"});
    ASSERT_TRUE(EnsureDirectoryExists(android_art_root_path));
    android_art_root_env_ = std::make_unique<ScopedUnsetEnvironmentVariable>("ANDROID_ART_ROOT");
    setenv("ANDROID_ART_ROOT", android_art_root_path.c_str(), kReplace);

    std::string art_apex_data_path = Concatenate({temp_dir_path, kArtApexDataDefaultPath});
    ASSERT_TRUE(EnsureDirectoryExists(art_apex_data_path));
    art_apex_data_env_ = std::make_unique<ScopedUnsetEnvironmentVariable>("ART_APEX_DATA");
    setenv("ART_APEX_DATA", art_apex_data_path.c_str(), kReplace);

    dalvik_cache_dir_ = art_apex_data_path + "/dalvik-cache";
    ASSERT_TRUE(EnsureDirectoryExists(dalvik_cache_dir_ + "/x86_64"));

    framework_dir_ = android_root_path + "/framework";
    framework_jar_ = framework_dir_ + "/framework.jar";
    location_provider_jar_ = framework_dir_ + "/com.android.location.provider.jar";
    services_jar_ = framework_dir_ + "/services.jar";
    std::string services_jar_prof = framework_dir_ + "/services.jar.prof";
    std::string javalib_dir = android_art_root_path + "/javalib";
    std::string boot_art = javalib_dir + "/boot.art";

    // Create placeholder files.
    ASSERT_TRUE(EnsureDirectoryExists(framework_dir_ + "/x86_64"));
    CreateEmptyFile(framework_jar_);
    CreateEmptyFile(location_provider_jar_);
    CreateEmptyFile(services_jar_);
    CreateEmptyFile(services_jar_prof);
    ASSERT_TRUE(EnsureDirectoryExists(javalib_dir));
    CreateEmptyFile(boot_art);

    config_.SetApexInfoListFile(Concatenate({temp_dir_path, "/apex-info-list.xml"}));
    config_.SetArtBinDir(Concatenate({temp_dir_path, "/bin"}));
    config_.SetBootClasspath(framework_jar_);
    config_.SetDex2oatBootclasspath(framework_jar_);
    config_.SetSystemServerClasspath(Concatenate({location_provider_jar_, ":", services_jar_}));
    config_.SetIsa(InstructionSet::kX86_64);
    config_.SetZygoteKind(ZygoteKind::kZygote64_32);

    std::string staging_dir = dalvik_cache_dir_ + "/staging";
    ASSERT_TRUE(EnsureDirectoryExists(staging_dir));
    config_.SetStagingDir(staging_dir);

    auto mock_odr_dexopt = std::make_unique<MockOdrDexopt>();
    mock_odr_dexopt_ = mock_odr_dexopt.get();

    metrics_ = std::make_unique<OdrMetrics>(dalvik_cache_dir_);
    odrefresh_ = std::make_unique<OnDeviceRefresh>(
        config_, dalvik_cache_dir_ + "/cache-info.xml", std::make_unique<ExecUtils>(),
        std::move(mock_odr_dexopt));
  }

  void TearDown() override {
    metrics_.reset();
    temp_dir_.reset();
    android_root_env_.reset();
    android_art_root_env_.reset();
    art_apex_data_env_.reset();

    CommonArtTest::TearDown();
  }

  std::unique_ptr<ScratchDir> temp_dir_;
  std::unique_ptr<ScopedUnsetEnvironmentVariable> android_root_env_;
  std::unique_ptr<ScopedUnsetEnvironmentVariable> android_art_root_env_;
  std::unique_ptr<ScopedUnsetEnvironmentVariable> art_apex_data_env_;
  OdrConfig config_;
  MockOdrDexopt* mock_odr_dexopt_;
  std::unique_ptr<OdrMetrics> metrics_;
  std::unique_ptr<OnDeviceRefresh> odrefresh_;
  std::string framework_jar_;
  std::string location_provider_jar_;
  std::string services_jar_;
  std::string dalvik_cache_dir_;
  std::string framework_dir_;
};

TEST_F(OdRefreshTest, OdrefreshArtifactDirectory) {
  // odrefresh.h defines kOdrefreshArtifactDirectory for external callers of odrefresh. This is
  // where compilation artifacts end up.
  ScopedUnsetEnvironmentVariable no_env("ART_APEX_DATA");
  EXPECT_EQ(kOdrefreshArtifactDirectory, GetArtApexData() + "/dalvik-cache");
}

TEST_F(OdRefreshTest, OutputFilesAndIsa) {
  {
    EXPECT_CALL(
        *mock_odr_dexopt_,
        DoDexoptBcpExtension(AllOf(
            Field(&DexoptBcpExtArgs::isa, Eq(Isa::X86_64)),
            Field(&DexoptBcpExtArgs::imageFd, Ge(0)),
            Field(&DexoptBcpExtArgs::vdexFd, Ge(0)),
            Field(&DexoptBcpExtArgs::oatFd, Ge(0)))))
        .WillOnce(Return(0));

    EXPECT_CALL(
        *mock_odr_dexopt_,
        DoDexoptSystemServer(AllOf(
            Field(&DexoptSystemServerArgs::isa, Eq(Isa::X86_64)),
            Field(&DexoptSystemServerArgs::imageFd, Ge(0)),
            Field(&DexoptSystemServerArgs::vdexFd, Ge(0)),
            Field(&DexoptSystemServerArgs::oatFd, Ge(0)))))
        .Times(2);

    EXPECT_EQ(odrefresh_->Compile(*metrics_,
                                  /*compile_boot_extensions=*/{InstructionSet::kX86_64},
                                  /*compile_system_server=*/true),
              ExitCode::kCompilationSuccess);
  }
}

TEST_F(OdRefreshTest, CompileChoosesBootImage) {
  {
    // Boot image is on /data.
    OdrArtifacts artifacts =
        OdrArtifacts::ForBootImageExtension(dalvik_cache_dir_ + "/x86_64/boot-framework.art");
    auto file1 = ScopedCreateEmptyFile(artifacts.ImagePath());
    auto file2 = ScopedCreateEmptyFile(artifacts.VdexPath());
    auto file3 = ScopedCreateEmptyFile(artifacts.OatPath());

    EXPECT_CALL(
        *mock_odr_dexopt_,
        DoDexoptSystemServer(AllOf(
            Field(&DexoptSystemServerArgs::isBootImageOnSystem, Eq(false)),
            Field(&DexoptSystemServerArgs::bootClasspathImageFds,
                  Contains(FdOf(artifacts.ImagePath()))),
            Field(&DexoptSystemServerArgs::bootClasspathVdexFds,
                  Contains(FdOf(artifacts.VdexPath()))),
            Field(&DexoptSystemServerArgs::bootClasspathOatFds,
                  Contains(FdOf(artifacts.OatPath()))))))
        .Times(2)
        .WillRepeatedly(Return(0));
    EXPECT_EQ(odrefresh_->Compile(
                  *metrics_, /*compile_boot_extensions=*/{}, /*compile_system_server=*/true),
              ExitCode::kCompilationSuccess);
  }

  {
    // Boot image is on /system.
    OdrArtifacts artifacts =
        OdrArtifacts::ForBootImageExtension(framework_dir_ + "/x86_64/boot-framework.art");
    auto file1 = ScopedCreateEmptyFile(artifacts.ImagePath());
    auto file2 = ScopedCreateEmptyFile(artifacts.VdexPath());
    auto file3 = ScopedCreateEmptyFile(artifacts.OatPath());

    EXPECT_CALL(
        *mock_odr_dexopt_,
        DoDexoptSystemServer(AllOf(
            Field(&DexoptSystemServerArgs::isBootImageOnSystem, Eq(true)),
            Field(&DexoptSystemServerArgs::bootClasspathImageFds,
                  Contains(FdOf(artifacts.ImagePath()))),
            Field(&DexoptSystemServerArgs::bootClasspathVdexFds,
                  Contains(FdOf(artifacts.VdexPath()))),
            Field(&DexoptSystemServerArgs::bootClasspathOatFds,
                  Contains(FdOf(artifacts.OatPath()))))))
        .Times(2)
        .WillRepeatedly(Return(0));
    EXPECT_EQ(odrefresh_->Compile(
                  *metrics_, /*compile_boot_extensions=*/{}, /*compile_system_server=*/true),
              ExitCode::kCompilationSuccess);
  }
}

}  // namespace odrefresh
}  // namespace art
