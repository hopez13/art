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

#include "odr_metrics_record.h"

#include <string.h>

#include <fstream>

#include "base/common_art_test.h"

namespace art {
namespace odrefresh {

class OdrMetricsRecordTest : public CommonArtTest {};

TEST_F(OdrMetricsRecordTest, HappyPath) {
  const OdrMetricsRecord expected {
    .art_apex_version = 0x01233456'789abcde,
    .trigger = 0x01020304,
    .stage_reached = 0x11121314,
    .status = 0x21222324,
    .primary_bcp_compilation_seconds = 0x31323334,
    .secondary_bcp_compilation_seconds = 0x41424344,
    .system_server_compilation_seconds = 0x51525354,
    .cache_space_free_start_mib = 0x61626364,
    .cache_space_free_end_mib = 0x71727374
  };

  ScratchDir dir(/*keep_files=*/false);
  std::string file_path = dir.GetPath() + "/metrics-record.xml";
  ASSERT_TRUE(expected.WriteToFile(file_path));

  OdrMetricsRecord actual {};
  ASSERT_TRUE(actual.ReadFromFile(file_path));

  ASSERT_EQ(expected.art_apex_version, actual.art_apex_version);
  ASSERT_EQ(expected.trigger, actual.trigger);
  ASSERT_EQ(expected.stage_reached, actual.stage_reached);
  ASSERT_EQ(expected.status, actual.status);
  ASSERT_EQ(expected.primary_bcp_compilation_seconds, actual.primary_bcp_compilation_seconds);
  ASSERT_EQ(expected.secondary_bcp_compilation_seconds, actual.secondary_bcp_compilation_seconds);
  ASSERT_EQ(expected.system_server_compilation_seconds, actual.system_server_compilation_seconds);
  ASSERT_EQ(expected.cache_space_free_start_mib, actual.cache_space_free_start_mib);
  ASSERT_EQ(expected.cache_space_free_end_mib, actual.cache_space_free_end_mib);
  ASSERT_EQ(0, memcmp(&expected, &actual, sizeof(expected)));
}

TEST_F(OdrMetricsRecordTest, EmptyInput) {
  ScratchDir dir(/*keep_files=*/false);
  std::string file_path = dir.GetPath() + "/metrics-record.xml";

  OdrMetricsRecord record{};
  ASSERT_FALSE(record.ReadFromFile(file_path));
}

TEST_F(OdrMetricsRecordTest, UnexpectedInput) {
  ScratchDir dir(/*keep_files=*/false);
  std::string file_path = dir.GetPath() + "/metrics-record.xml";

  std::ofstream ofs(file_path);
  ofs << "<not_odrefresh_metrics></not_odrefresh_metrics>";
  ofs.close();

  OdrMetricsRecord record{};
  ASSERT_FALSE(record.ReadFromFile(file_path));
}

}  // namespace odrefresh
}  // namespace art
