/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include "logging.h"

#include <type_traits>

#include "android-base/logging.h"
#include "base/bit_utils.h"
#include "base/macros.h"
#include "common_runtime_test.h"

namespace art {

static void SimpleAborter(const char* msg) {
  LOG(FATAL_WITHOUT_ABORT) << msg;
  _exit(1);
}

class LoggingTest : public CommonRuntimeTest {
 protected:
  void PostRuntimeCreate() OVERRIDE {
    // In our abort tests we really don't want the runtime to create a real dump.
    android::base::SetAborter(SimpleAborter);
  }
};

constexpr DebugCheckLevel kLevels[] = {
    DebugCheckLevel::kNone,
    DebugCheckLevel::kAll,
};

TEST_F(LoggingTest, TestConsistency) {
  // Ensure that kLevels contains all DebugCheckLevel cases.
  auto find_in_levels = [](DebugCheckLevel to_find) {
    for (size_t i = 0; i != arraysize(kLevels); ++i) {
      if (kLevels[i] == to_find) {
        return true;
      }
    }
    return false;
  };

  // To check, loop over a fragment of the underlying type, and use a switch. The switch enforces,
  // through missing-case warnings, that this code stays up-to-date.
  uint64_t seen_bitset = 0;
  constexpr size_t kCount = 2;
  static_assert(kCount == arraysize(kLevels), "Unexpected level count");
  static_assert(kCount < sizeof(seen_bitset) * 8, "Not enough bits in uint64_t");

  using T = std::underlying_type<DebugCheckLevel>::type;
  for (T i = -1; i != 100; ++i) {
    DebugCheckLevel level = static_cast<DebugCheckLevel>(i);
    switch (level) {
      case DebugCheckLevel::kNone:
        EXPECT_TRUE(find_in_levels(DebugCheckLevel::kNone));
        seen_bitset |= (1 << 0);
        break;

      case DebugCheckLevel::kAll:
        EXPECT_TRUE(find_in_levels(DebugCheckLevel::kAll));
        seen_bitset |= (1 << 1);
        break;

      static_assert(1 + 1 == kCount, "Unexpected case count");
    }
  }
  size_t actual_count = POPCOUNT(seen_bitset);
  ASSERT_EQ(kCount, actual_count);
}

TEST_F(LoggingTest, IsDebugCheckEnabled) {
  auto check = [](DebugCheckLevel cur, DebugCheckLevel global) {
    gDebugCheckLevel = global;
    return IsDebugCheckEnabled(cur);
  };

  for (size_t i = 0; i != arraysize(kLevels); ++i) {
    for (size_t j = 0; j != arraysize(kLevels); ++j) {
      if (i <= j) {
        EXPECT_TRUE(check(kLevels[i], kLevels[j]));
      } else {
        EXPECT_FALSE(check(kLevels[i], kLevels[j]));
      }
    }
  }

  // Keep some explicit checks around, in case programmatically generating cases above is actually
  // wrong.
  EXPECT_TRUE(check(DebugCheckLevel::kNone, DebugCheckLevel::kNone));
  EXPECT_TRUE(check(DebugCheckLevel::kNone, DebugCheckLevel::kAll));
  EXPECT_FALSE(check(DebugCheckLevel::kAll, DebugCheckLevel::kNone));
  EXPECT_TRUE(check(DebugCheckLevel::kAll, DebugCheckLevel::kAll));
}

TEST_F(LoggingTest, DCHECK_LEVEL_success) {
  // Simple tests: the condition holds.
  for (size_t i = 0; i != arraysize(kLevels); ++i) {
    gDebugCheckLevel = kLevels[i];
    for (size_t j = 0; j != arraysize(kLevels); ++j) {
      DebugCheckLevel check_level = kLevels[j];
      DCHECK_LEVEL(true, check_level) << "Should not fail";
      DCHECK_EQ_LEVEL(1u, 1u, check_level) << "Should not fail";
      DCHECK_LE_LEVEL(1u, 1u, check_level) << "Should not fail";
      DCHECK_LT_LEVEL(1u, 2u, check_level) << "Should not fail";
      DCHECK_GE_LEVEL(1u, 1u, check_level) << "Should not fail";
      DCHECK_GT_LEVEL(2u, 1u, check_level) << "Should not fail";
    }
  }

  // Not so simple tests: the condition doesn't hold, but it shouldn't be checked.
  for (size_t i = 0; i != arraysize(kLevels); ++i) {
    gDebugCheckLevel = kLevels[i];
    for (size_t j = i + 1; j != arraysize(kLevels); ++j) {
      DebugCheckLevel check_level = kLevels[j];
      DCHECK_LEVEL(false, check_level) << "Should not fail";
      DCHECK_EQ_LEVEL(1u, 2u, check_level) << "Should not fail";
      DCHECK_LE_LEVEL(2u, 1u, check_level) << "Should not fail";
      DCHECK_LT_LEVEL(1u, 1u, check_level) << "Should not fail";
      DCHECK_GE_LEVEL(1u, 2u, check_level) << "Should not fail";
      DCHECK_GT_LEVEL(1u, 1u, check_level) << "Should not fail";
    }
  }
}

TEST_F(LoggingTest, DCHECK_LEVEL_fail) {
  // Check that DCHECK itself works.
  ASSERT_DEATH({ DCHECK(false) << "TAG"; }, "TAG");

  // Not so simple tests: the condition doesn't hold, and we're supposed to check.

  for (size_t i = 0; i != arraysize(kLevels); ++i) {
    gDebugCheckLevel = kLevels[i];
    for (size_t j = 0; j < i; ++j) {
      DebugCheckLevel check_level = kLevels[j];
      ASSERT_DEATH({ DCHECK_LEVEL(false, check_level) << "TAG"; }, "TAG");
      ASSERT_DEATH({ DCHECK_EQ_LEVEL(1u, 2u, check_level) << "TAG"; }, "TAG");
      ASSERT_DEATH({ DCHECK_LE_LEVEL(2u, 1u, check_level) << "TAG"; }, "TAG");
      ASSERT_DEATH({ DCHECK_LT_LEVEL(1u, 1u, check_level) << "TAG"; }, "TAG");
      ASSERT_DEATH({ DCHECK_GE_LEVEL(1u, 2u, check_level) << "TAG"; }, "TAG");
      ASSERT_DEATH({ DCHECK_GT_LEVEL(1u, 1u, check_level) << "TAG"; }, "TAG");
    }
  }
}

}  // namespace art
