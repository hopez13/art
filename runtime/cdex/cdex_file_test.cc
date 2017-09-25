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

#include "cdex/cdex_file.h"
#include "common_runtime_test.h"

namespace art {

class CDexFileTest : public CommonRuntimeTest {};

TEST_F(CDexFileTest, MagicAndVersion) {
  // Test permutations of valid/invalid headers.
  for (size_t i = 0; i < 2; ++i) {
    for (size_t j = 0; j < 2; ++j) {
      static const size_t len = CDexFile::kDexVersionLen + CDexFile::kDexMagicSize;
      uint8_t header[len] = {};
      std::fill_n(header, len, 0x99);
      const bool valid_magic = (i & 1) == 0;
      const bool valid_version = (j & 1) == 0;
      if (valid_magic) {
        std::copy_n(CDexFile::kDexMagic, CDexFile::kDexMagicSize, header);
      }
      if (valid_version) {
        std::copy_n(CDexFile::kDexMagicVersion,
                    CDexFile::kDexVersionLen,
                    header + CDexFile::kDexMagicSize);
      }
      EXPECT_EQ(valid_magic, CDexFile::IsMagicValid(header));
      EXPECT_EQ(valid_version, CDexFile::IsVersionValid(header));
    }
  }
}

}  // namespace art
