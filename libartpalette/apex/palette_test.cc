/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include "palette/palette.h"

#include <unistd.h>
#include <sys/syscall.h>

#include "gtest/gtest.h"

#ifndef __BIONIC__

namespace {

pid_t gettid() {
  return syscall(__NR_gettid);
}

}  // namespace

#endif  // __BIONIC__

class PaletteClientTest : public testing::Test {};

TEST_F(PaletteClientTest, GetVersion) {
  int32_t version = -1;
  PaletteStatus status = PaletteGetVersion(&version);
  ASSERT_EQ(PaletteStatus::kOkay, status);
  ASSERT_GE(version, 1);
}

TEST_F(PaletteClientTest, MetricsHappyPath) {
  PaletteMetricsRecordTaggedData tagged_data[3];
  tagged_data[0].tag = PaletteEventTag::kHiddenApiAccessMethod;
  tagged_data[0].kind = PaletteEventTaggedDataKind::kInt32;
  tagged_data[0].value.i32 = PaletteEventCategoryHiddenApiAccess::kMethodViaJNI;

  tagged_data[1].tag = PaletteEventTag::kHiddenApiAccessDenied;
  tagged_data[1].kind = PaletteEventTaggedDataKind::kInt32;
  tagged_data[1].value.i32 = 1;

  tagged_data[2].tag = PaletteEventTag::kHiddenApiSignature;
  tagged_data[2].kind = PaletteEventTaggedDataKind::kString;
  tagged_data[2].value.c_str = "Ltoolbox/Wrench;.tighten:(i)i";

  int32_t tagged_data_count = sizeof(tagged_data) / sizeof(tagged_data[0]);
  EXPECT_EQ(PaletteStatus::kOkay,
            PaletteMetricsLogEvent(PaletteEventCategory::kHiddenApiAccess,
                                   "PaletteClientTest",
                                   tagged_data,
                                   tagged_data_count));
}


TEST_F(PaletteClientTest, MetricsUnhappyPaths) {
// Only test on Android as fake paths on other platforms always return
// PaletteStatus::kOkay.
#ifdef ART_TARGET_ANDROID
  PaletteMetricsRecordTaggedData tagged_data[3];
  tagged_data[0].tag = PaletteEventTag::kHiddenApiAccessMethod;
  tagged_data[0].kind = PaletteEventTaggedDataKind::kInt32;
  tagged_data[0].value.i32 = PaletteEventCategoryHiddenApiAccess::kMethodViaJNI;

  tagged_data[1].tag = PaletteEventTag::kHiddenApiAccessDenied;
  tagged_data[1].kind = PaletteEventTaggedDataKind::kInt32;
  tagged_data[1].value.i32 = 1;

  tagged_data[2].tag = PaletteEventTag::kHiddenApiSignature;
  tagged_data[2].kind = PaletteEventTaggedDataKind::kString;
  tagged_data[2].value.c_str = "Ltoolbox/Wrench;.tighten:(i)i";

  int32_t tagged_data_count = sizeof(tagged_data) / sizeof(tagged_data[0]);
  tagged_data[0].kind = static_cast<PaletteEventTaggedDataKind>(-1);
  EXPECT_EQ(PaletteStatus::kInvalidArgument,
            PaletteMetricsLogEvent(PaletteEventCategory::kHiddenApiAccess,
                                   "PaletteClientTest",
                                   tagged_data,
                                   tagged_data_count));
  tagged_data[0].kind = PaletteEventTaggedDataKind::kInt32;
#endif  // ART_TARGET_ANDROID
}

TEST_F(PaletteClientTest, SchedPriority) {
  int32_t tid = gettid();
  int32_t saved_priority;
  EXPECT_EQ(PaletteStatus::kOkay, PaletteSchedGetPriority(tid, &saved_priority));

  EXPECT_EQ(PaletteStatus::kInvalidArgument, PaletteSchedSetPriority(tid, 0));
  EXPECT_EQ(PaletteStatus::kInvalidArgument, PaletteSchedSetPriority(tid, -1));
  EXPECT_EQ(PaletteStatus::kInvalidArgument, PaletteSchedSetPriority(tid, 11));

  EXPECT_EQ(PaletteStatus::kOkay, PaletteSchedSetPriority(tid, 1));
  EXPECT_EQ(PaletteStatus::kOkay, PaletteSchedSetPriority(tid, saved_priority));
}

TEST_F(PaletteClientTest, Trace) {
  int32_t enabled;
  EXPECT_EQ(PaletteStatus::kOkay, PaletteTraceEnabled(&enabled));
  EXPECT_EQ(PaletteStatus::kOkay, PaletteTraceBegin("Hello world!"));
  EXPECT_EQ(PaletteStatus::kOkay, PaletteTraceEnd());
  EXPECT_EQ(PaletteStatus::kOkay, PaletteTraceIntegerValue("Beans", 3));
}
