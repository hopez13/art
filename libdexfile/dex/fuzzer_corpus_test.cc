/*
 * Copyright (C) 2023 The Android Open Source Project
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

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <unordered_set>

#include "android-base/file.h"
#include "base/common_art_test.h"
#include "dex/dex_file_verifier.h"
#include "dex/standard_dex_file.h"
#include "gtest/gtest.h"

namespace art {

class FuzzerCorpusTest : public testing::Test {
 public:
  static void VerifyDexFile(const char* dex_file_location, bool expected_success) {
    // Read file.
    std::ifstream input(dex_file_location, std::ios_base::binary);
    std::vector<char> bytes((std::istreambuf_iterator<char>(input)),
                            (std::istreambuf_iterator<char>()));
    const uint8_t* data = reinterpret_cast<const uint8_t*>(bytes.data());

    // Special case for empty dex file. Set a fake data since the size is 0 anyway.
    if (data == nullptr) {
      ASSERT_EQ(bytes.size(), 0);
      data = reinterpret_cast<const uint8_t*>(dex_file_location);
    }

    // Do not verify the checksum as we only care about the DEX file contents,
    // and know that the checksum would probably be erroneous (i.e. random).
    constexpr bool kVerify = false;
    auto container = std::make_shared<art::MemoryDexFileContainer>(data, bytes.size());
    art::StandardDexFile dex_file(data,
                                  /*location=*/dex_file_location,
                                  /*location_checksum=*/0,
                                  /*oat_dex_file=*/nullptr,
                                  container);

    std::string error_msg;
    bool success = art::dex::Verify(&dex_file, dex_file.GetLocation().c_str(), kVerify, &error_msg);
    ASSERT_EQ(success, expected_success) << " Failed for " << dex_file_location;
  }
};

// Tests that we can verify dex files without crashing.
TEST_F(FuzzerCorpusTest, VerifyCorpusDexFiles) {
  // These dex files are expected to pass verification. The others are regressions tests.
  const std::unordered_set<std::string> valid_dex_files = {"Main.dex", "hello_world.dex"};

  // Consistency checks.
  const std::string folder = android::base::GetExecutableDirectory() + "/art-gtest-jars-DexFuzzerFolder";
  ASSERT_TRUE(std::filesystem::is_directory(folder)) << folder << " is not a folder";
  ASSERT_FALSE(std::filesystem::is_empty(folder)) << " No files found for directory " << folder;

  // Run the verifier.
  for (const auto& file : std::filesystem::directory_iterator(folder)) {
    LOG(ERROR) << file.path();
    const bool expected_success =
        valid_dex_files.find(file.path().filename().string()) != valid_dex_files.end();
    VerifyDexFile(file.path().c_str(), expected_success);
  }
}

}  // namespace art
