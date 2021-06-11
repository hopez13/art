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
#include <tools/paths.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <random>
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
#elif defined(__i386__)
static const std::string kRuntimeIsa = "x86";
#elif defined(__x86_64__)
static const std::string kRuntimeIsa = "x86_64";
#else
static const std::string kRuntimeIsa = "none";
#endif

static void RunCmd(const std::string& cmd) {
  if (system(cmd.c_str()) == -1) {
    LOG(ERROR) << "Error executing system command: " << strerror(errno);
  }
}

static int MkDir(const std::string& path, uid_t owner, gid_t group, mode_t mode) {
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

    volume_uuid_ = std::nullopt;
    package_name_ = "com.android.art.tools.tests.dex";
    se_info_ = "default";
    app_apk_dir_ = ::art::tools::paths.AndroidAppDir + package_name_;

    ASSERT_TRUE(CreateMockApp());
  }

  virtual void TearDown() {
    if (!kDebug) {
      RunCmd("rm -rf " + app_apk_dir_);
      RunCmd("rm -rf " + app_private_dir_ce_);
      RunCmd("rm -rf " + app_private_dir_de_);
    }
  }

  ::testing::AssertionResult CreateMockApp() {
    // For debug mode, the directory might already exist. Avoid erroring out.
    if (MkDir(app_apk_dir_, kSystemUid, kSystemGid, 0755) != 0 && !kDebug) {
      return ::testing::AssertionFailure()
             << "Could not create app dir " << app_apk_dir_ << " : " << strerror(errno);
    }

    // Initialize the oat dir path.
    app_oat_dir_ = app_apk_dir_ + "/oat";
    if (MkDir(app_oat_dir_, kSystemUid, kSystemGid, 0755) != 0 && !kDebug) {
      return ::testing::AssertionFailure()
             << "Could not create app oat dir " << app_apk_dir_ << " : " << strerror(errno);
    }

    if (MkDir(app_oat_dir_ + '/' + kRuntimeIsa, kSystemUid, kSystemGid, 0755) != 0 && !kDebug) {
      return ::testing::AssertionFailure()
             << "Could not create app ISA dir " << app_apk_dir_ << " : " << strerror(errno);
    }

    /* For now we initialize the base APK file with random data.  Eventually this will be replaced
     * with a resource in the test package.
     */
    apk_path_ = app_apk_dir_ + "/base.jar";
    WriteRandomData(apk_path_, 19456);

    return ::testing::AssertionSuccess();
  }

  static void WriteRandomData(const std::string& path, size_t num_bytes) {
    std::random_device rd;
    std::uniform_int_distribution<uint8_t> dist(0, UINT8_MAX);
    std::ofstream file(path, std::ios::binary);

    while (num_bytes-- > 0) {
      file << dist(rd);
    }
  }

  void InitCompilationArtifacts() {
    LOG(INFO) << "Initializing compilation artifacts.";

    // Create dex, odex, vdex, and art files with random data.

    std::string cached_dex_path = GetDalvikCacheDexArtifactPath(apk_path_, "dex");
    WriteRandomData(cached_dex_path, 2048);

    std::string cached_vdex_path = GetDalvikCacheDexArtifactPath(apk_path_, "vdex");
    WriteRandomData(cached_vdex_path, 5120);

    std::string cached_art_path = GetDalvikCacheDexArtifactPath(apk_path_, "art");
    WriteRandomData(cached_art_path, 7168);

    std::string odex_path = GetPrimaryDexArtifactPath(app_oat_dir_, apk_path_, "odex");
    WriteRandomData(odex_path, 11264);

    std::string vdex_path = GetPrimaryDexArtifactPath(app_oat_dir_, apk_path_, "vdex");
    WriteRandomData(vdex_path, 13312);

    std::string art_path = GetPrimaryDexArtifactPath(app_oat_dir_, apk_path_, "art");
    WriteRandomData(art_path, 17408);
  }

  std::string GetDalvikCacheDexArtifactPath(const std::string& dex_path, const std::string& type) {
    std::string dalvik_cache_key = dex_path;
    std::replace(dalvik_cache_key.begin() + 1, dalvik_cache_key.end(), '/', '@');

    // The / between the runtime ISA and cache key is provided by the leading / in the dex
    // path.
    return ::art::tools::paths.AndroidDataDir + DALVIK_CACHE + '/' + kRuntimeIsa +
           dalvik_cache_key + "@classes." + type;
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

    LOG(ERROR) << "test odex path: " << GetDexArtifactPath(oat_dir, apk_path_, dex_suffix);

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
