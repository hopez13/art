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
  }
};

class Dex2oatSwapTest : public Dex2oatTest {
};

/*TEST_F(Dex2oatSwapTest, DoNotUseSwapDefaultSingleSmall) {
}*/

}  // namespace art
