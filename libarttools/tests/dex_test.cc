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

#include <android-base/file.h>
#include <android-base/logging.h>
#include <binder/Status.h>
#include <cutils/multiuser.h>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <tools/constants.h>
#include <tools/dex.h>
#include <tools/globals.h>

#include <algorithm>
#include <cstdlib>
#include <optional>
#include <string>
#include <tuple>

#include "utils.h"

namespace android {
namespace art {
namespace tools {
namespace tests {

// TODO(calin): try to dedup this code.
#if defined(__arm__)
static const std::string kRuntimeIsa = "arm";
#elif defined(__aarch64__)
static const std::string kRuntimeIsa = "arm64";
#elif defined(__mips__) && !defined(__LP64__)
static const std::string kRuntimeIsa = "mips";
#elif defined(__mips__) && defined(__LP64__)
static const std::string kRuntimeIsa = "mips64";
#elif defined(__i386__)
static const std::string kRuntimeIsa = "x86";
#elif defined(__x86_64__)
static const std::string kRuntimeIsa = "x86_64";
#else
static const std::string kRuntimeIsa = "none";
#endif

static void run_cmd(const std::string& cmd) {
  if (system(cmd.c_str()) == -1) {
    LOG(ERROR) << "Error executing system command: " << strerror(errno);
  }
}

template <typename Visitor>
static void run_cmd_and_process_output(const std::string& cmd, const Visitor& visitor) {
  FILE* file = popen(cmd.c_str(), "r");
  CHECK(file != nullptr) << "Failed to ptrace " << cmd;
  char* line = nullptr;
  while (true) {
    size_t n = 0u;
    ssize_t value = getline(&line, &n, file);
    if (value == -1) {
      break;
    }
    visitor(line);
  }
  free(line);
  fclose(file);
}

static int mkdir(const std::string& path, uid_t owner, gid_t group, mode_t mode) {
  int ret = ::mkdir(path.c_str(), mode);
  if (ret != 0) {
    return ret;
  }
  ret = ::chown(path.c_str(), owner, group);
  if (ret != 0) {
    return ret;
  }
  return ::chmod(path.c_str(), mode);
}

// Base64 encoding of a simple dex files with 2 methods.
static const char kDexFile[] =
    "UEsDBBQAAAAIAOiOYUs9y6BLCgEAABQCAAALABwAY2xhc3Nlcy5kZXhVVAkAA/Ns+lkOHv1ZdXgL"
    "AAEEI+UCAASIEwAAS0mt4DIwNmX4qpn7j/2wA7v7N+ZvoQpCJRlVx5SWa4YaiDAxMBQwMDBUhJkI"
    "MUBBDyMDAzsDRJwFxAdioBDDHAYEYAbiFUAM1M5wAIhFGCGKDIDYAogdgNgDiH2BOAiI0xghekDm"
    "sQIxGxQzM6ACRijNhCbOhCZfyohdPYyuh8szgtVkMkLsLhAAqeCDi+ejibPZZOZlltgxsDnqZSWW"
    "JTKwOUFoZh9HayDhZM0g5AMS0M9JzEvX90/KSk0usWZgDAMaws5nAyXBzmpoYGlgAjsAyJoBMp0b"
    "zQ8gGhbOTEhhzYwU3qxIYc2GFN6MClC/AhUyKUDMAYU9M1Qc5F8GKBscVgIQM0FxCwBQSwECHgMU"
    "AAAACADojmFLPcugSwoBAAAUAgAACwAYAAAAAAAAAAAAoIEAAAAAY2xhc3Nlcy5kZXhVVAUAA/Ns"
    "+ll1eAsAAQQj5QIABIgTAABQSwUGAAAAAAEAAQBRAAAATwEAAAAA";

class DexTest : public testing::Test {
 protected:
  static constexpr bool kDebug = false;
  static constexpr uid_t kSystemUid = 1000;
  static constexpr uid_t kSystemGid = 1000;
  static constexpr int32_t kOSdkVersion = 25;
  static constexpr int32_t kAppDataFlags =
      ::art::tools::FLAG_STORAGE_CE | ::art::tools::FLAG_STORAGE_DE;
  static constexpr int32_t kTestUserId = 0;
  static constexpr uid_t kTestAppId = 19999;

  const gid_t kTestAppUid = multiuser_get_uid(kTestUserId, kTestAppId);
  const uid_t kTestAppGid = multiuser_get_shared_gid(kTestUserId, kTestAppId);

  std::optional<std::string> volume_uuid_;
  std::string package_name_;
  std::string apk_path_;
  std::string empty_dm_file_;
  std::string app_apk_dir_;
  std::string app_private_dir_ce_;
  std::string app_private_dir_de_;
  std::string se_info_;
  std::string app_oat_dir_;

  int64_t ce_data_inode_;

  std::string secondary_dex_ce_;
  std::string secondary_dex_ce_link_;
  std::string secondary_dex_de_;

  virtual void SetUp() {
    setenv("ANDROID_LOG_TAGS", "*:v", 1);
    android::base::InitLogging(nullptr);
    // Initialize the globals holding the file system main paths (/data/, /system/ etc..).
    // This is needed in order to compute the application and profile paths.
    ASSERT_TRUE(::art::tools::init_globals_from_data_and_root());

    volume_uuid_ = std::nullopt;
    package_name_ = "com.android.art.tools.tests.dex";
    se_info_ = "default";
    app_apk_dir_ = ::art::tools::android_app_dir + package_name_;

    ASSERT_TRUE(CreateMockApp());
  }

  virtual void TearDown() {
    if (!kDebug) {
      run_cmd("rm -rf " + app_apk_dir_);
      run_cmd("rm -rf " + app_private_dir_ce_);
      run_cmd("rm -rf " + app_private_dir_de_);
    }
  }

  ::testing::AssertionResult CreateMockApp() {
    // For debug mode, the directory might already exist. Avoid erroring out.
    if (mkdir(app_apk_dir_, kSystemUid, kSystemGid, 0755) != 0 && !kDebug) {
      return ::testing::AssertionFailure()
             << "Could not create app dir " << app_apk_dir_ << " : " << strerror(errno);
    }

    // Initialize the oat dir path.
    app_oat_dir_ = app_apk_dir_ + "/oat";
    if (mkdir(app_oat_dir_, kSystemUid, kSystemGid, 0755) != 0 && !kDebug) {
      return ::testing::AssertionFailure()
             << "Could not create app oat dir " << app_apk_dir_ << " : " << strerror(errno);
    }

    if (mkdir(app_oat_dir_ + '/' + kRuntimeIsa, kSystemUid, kSystemGid, 0755) != 0 && !kDebug) {
      return ::testing::AssertionFailure()
             << "Could not create app ISA dir " << app_apk_dir_ << " : " << strerror(errno);
    }

    // Copy the primary apk.
    apk_path_ = app_apk_dir_ + "/base.jar";
    std::string error_msg;
    if (!WriteBase64ToFile(kDexFile, apk_path_, kSystemUid, kSystemGid, 0644, &error_msg)) {
      return ::testing::AssertionFailure()
             << "Could not write base64 file to " << apk_path_ << " : " << error_msg;
    }

    return ::testing::AssertionSuccess();
  }

  void InitCompilationArtifacts() {
    LOG(INFO) << "Initializing compilation artifacts.";

    // Create dex, odex, vdex, and art files with random data.

    std::string cached_dex_path = GetDalvikCacheDexArtifactPath(apk_path_, "dex");
    run_cmd(std::string("dd if=/dev/urandom of=") + cached_dex_path + " count=1K");

    std::string cached_vdex_path = GetDalvikCacheDexArtifactPath(apk_path_, "vdex");
    run_cmd(std::string("dd if=/dev/urandom of=") + cached_vdex_path + " count=1K");

    std::string cached_art_path = GetDalvikCacheDexArtifactPath(apk_path_, "art");
    run_cmd(std::string("dd if=/dev/urandom of=") + cached_art_path + " count=1K");

    std::string odex_path = GetPrimaryDexArtifactPath(app_oat_dir_, apk_path_, "odex");
    run_cmd(std::string("dd if=/dev/urandom of=") + odex_path + " count=1K");

    std::string vdex_path = GetPrimaryDexArtifactPath(app_oat_dir_, apk_path_, "vdex");
    run_cmd(std::string("dd if=/dev/urandom of=") + vdex_path + " count=1K");

    std::string art_path = GetPrimaryDexArtifactPath(app_oat_dir_, apk_path_, "art");
    run_cmd(std::string("dd if=/dev/urandom of=") + art_path + " count=1K");
  }

  std::string GetDalvikCacheDexArtifactPath(const std::string& dex_path, const std::string& type) {
    std::string dalvik_cache_key = dex_path;
    std::replace(dalvik_cache_key.begin() + 1, dalvik_cache_key.end(), '/', '@');

    // The / between the runtime ISA and cache key is provided by the leading / in the dex
    // path.
    return ::art::tools::android_data_dir + DALVIK_CACHE + '/' + kRuntimeIsa + dalvik_cache_key +
           "@classes." + type;
  }

  std::string GetPrimaryDexArtifactPath(const std::string& oat_dir,
                                        const std::string& dex_path,
                                        const std::string& type) {
    std::string::size_type name_end = dex_path.rfind('.');
    std::string::size_type name_start = dex_path.rfind('/');
    return std::string(oat_dir) + '/' + kRuntimeIsa + '/' +
           dex_path.substr(name_start + 1, name_end - name_start) + type;
  }

  std::string GetDexArtifactPath(const std::optional<std::string>& oat_dir,
                                 const std::string& dex_path,
                                 const std::string& type) {
    if (oat_dir == std::nullopt) {
      return GetDalvikCacheDexArtifactPath(dex_path, type);
    } else {
      return GetPrimaryDexArtifactPath(oat_dir.value(), dex_path, type);
    }
  }

  int64_t GetSize(const std::string& path) {
    struct stat file_stat;
    if (stat(path.c_str(), &file_stat) == 0) {
      return static_cast<int64_t>(file_stat.st_size);
    }
    PLOG(ERROR) << "Cannot stat path: " << path;
    return -1;
  }

  void TestDeleteOdex(bool in_dalvik_cache) {
    InitCompilationArtifacts();

    const auto& [oat_dir, dex_suffix] =
        in_dalvik_cache ? std::tuple<std::optional<std::string>, std::string>(std::nullopt, "dex") :
                          std::tuple<std::optional<std::string>, std::string>(
                              std::optional(app_oat_dir_), "odex");

    int64_t odex_size = GetSize(GetDexArtifactPath(oat_dir, apk_path_, dex_suffix));
    int64_t vdex_size = GetSize(GetDexArtifactPath(oat_dir, apk_path_, "vdex"));
    int64_t art_size = GetSize(GetDexArtifactPath(oat_dir, apk_path_, "art"));

    LOG(ERROR) << "test odex size: " << odex_size;
    LOG(ERROR) << "test vdex size: " << vdex_size;
    LOG(ERROR) << "test art size: " << art_size;

    ASSERT_GE(odex_size, 0);
    ASSERT_GE(vdex_size, 0);
    ASSERT_GE(art_size, 0);

    int64_t expected_bytes_freed = odex_size + vdex_size + art_size;
    int64_t bytes_freed = ::art::tools::dex::delete_odex(apk_path_, kRuntimeIsa, oat_dir);

    ASSERT_GT(bytes_freed, 0);
    ASSERT_EQ(expected_bytes_freed, bytes_freed);
  }
};

TEST_F(DexTest, DeleteDexoptArtifactsData) {
  LOG(INFO) << "DeleteDexoptArtifactsData";
  TestDeleteOdex(/*in_dalvik_cache=*/false);
}

TEST_F(DexTest, DeleteDexoptArtifactsDalvikCache) {
  LOG(INFO) << "DeleteDexoptArtifactsDalvikCache";
  TestDeleteOdex(/*in_dalvik_cache=*/true);
}

}  // namespace tests
}  // namespace tools
}  // namespace art
}  // namespace android
