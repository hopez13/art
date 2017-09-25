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
  const size_t len = CDexFile::kDexVersionLen + CDexFile::kDexMagicSize;
  uint8_t invalid[len] = {};
  std::fill_n(invalid, len, 0x99);
  EXPECT_FALSE(CDexFile::IsMagicValid(invalid));
  EXPECT_FALSE(CDexFile::IsVersionValid(invalid));

  uint8_t valid[len] = {};
  std::copy_n(CDexFile::kDexMagic, CDexFile::kDexMagicSize, valid);
  std::copy_n(CDexFile::kDexMagicVersion,
              CDexFile::kDexVersionLen,
              valid + CDexFile::kDexMagicSize);
  EXPECT_TRUE(CDexFile::IsMagicValid(valid));
  EXPECT_TRUE(CDexFile::IsVersionValid(valid));
}

}  // namespace art
