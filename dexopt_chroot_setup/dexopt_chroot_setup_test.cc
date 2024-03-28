/*
 * Copyright (C) 2024 The Android Open Source Project
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

#include "dexopt_chroot_setup.h"

#include <filesystem>
#include <string>

#include "aidl/com/android/server/art/BnDexoptChrootSetup.h"
#include "base/common_art_test.h"
#include "base/macros.h"
#include "exec_utils.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "tools/cmdline_builder.h"

namespace art {
namespace dexopt_chroot_setup {
namespace {

using ::art::tools::CmdlineBuilder;

using std::literals::operator""s;  // NOLINT

class DexoptChrootSetupTest : public CommonArtTest {
 protected:
  void SetUp() override {
    CommonArtTest::SetUp();
    dexopt_chroot_setup_ = ndk::SharedRefBase::make<DexoptChrootSetup>();

    scratch_dir_ = std::make_unique<ScratchDir>();
    scratch_path_ = scratch_dir_->GetPath();
    // Remove the trailing '/';
    scratch_path_.resize(scratch_path_.length() - 1);
  }

  void TearDown() override {
    scratch_dir_.reset();
    dexopt_chroot_setup_->tearDown();
    CommonArtTest::TearDown();
  }

  std::shared_ptr<DexoptChrootSetup> dexopt_chroot_setup_;
  std::unique_ptr<ScratchDir> scratch_dir_;
  std::string scratch_path_;
};

TEST_F(DexoptChrootSetupTest, Run) {
  ASSERT_TRUE(dexopt_chroot_setup_->setUp(/*in_otaSlot=*/std::nullopt).isOk());

  // Verify that important directories are there.
  EXPECT_FALSE(std::filesystem::is_empty(DexoptChrootSetup::CHROOT_DIR + "/system"s));
  EXPECT_FALSE(std::filesystem::is_empty(DexoptChrootSetup::CHROOT_DIR + "/system_ext"s));
  EXPECT_FALSE(std::filesystem::is_empty(DexoptChrootSetup::CHROOT_DIR + "/vendor"s));
  EXPECT_FALSE(std::filesystem::is_empty(DexoptChrootSetup::CHROOT_DIR + "/product"s));
  EXPECT_FALSE(std::filesystem::is_empty(DexoptChrootSetup::CHROOT_DIR + "/data"s));
  EXPECT_FALSE(std::filesystem::is_empty(DexoptChrootSetup::CHROOT_DIR + "/dev"s));
  EXPECT_FALSE(std::filesystem::is_empty(DexoptChrootSetup::CHROOT_DIR + "/proc"s));
  EXPECT_FALSE(std::filesystem::is_empty(DexoptChrootSetup::CHROOT_DIR + "/sys"s));
  EXPECT_FALSE(std::filesystem::is_empty(DexoptChrootSetup::CHROOT_DIR + "/metadata"s));
  EXPECT_FALSE(std::filesystem::is_empty(DexoptChrootSetup::CHROOT_DIR + "/apex/com.android.art"s));
  EXPECT_FALSE(
      std::filesystem::is_empty(DexoptChrootSetup::CHROOT_DIR + "/linkerconfig/com.android.art"s));

  // Check that the chroot environment is capable to run programs. `dex2oat` is arbitrarily picked
  // here.
  CmdlineBuilder args;
  args.Add(GetArtBinDir() + "/art_exec")
      .Add("--chroot=%s", DexoptChrootSetup::CHROOT_DIR)
      .Add("--")
      .Add(GetArtBinDir() + "/dex2oat" + (Is64BitInstructionSet(kRuntimeISA) ? "64" : "32"))
      .Add("--dex-file=%s", GetTestDexFileName("Main"))
      .Add("--oat-file=%s", scratch_path_ + "/output.odex")
      .Add("--output-vdex=%s", scratch_path_ + "/output.vdex")
      .Add("--compiler-filter=speed")
      .Add("--boot-image=/nonx/boot.art");
  std::string error_msg;
  EXPECT_TRUE(Exec(args.Get(), &error_msg)) << error_msg;

  // Check that `setUp` can be repetitively called, to simulate the case where an instance of the
  // caller (typically system_server) called `setUp` and crashed later, and a new instance called
  // `setUp` again.
  ASSERT_TRUE(dexopt_chroot_setup_->setUp(/*in_otaSlot=*/std::nullopt).isOk());

  ASSERT_TRUE(dexopt_chroot_setup_->tearDown().isOk());

  EXPECT_FALSE(std::filesystem::exists(DexoptChrootSetup::CHROOT_DIR));
}

}  // namespace
}  // namespace dexopt_chroot_setup
}  // namespace art
