/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include "metrics.h"

#include "gtest/gtest.h"

#pragma clang diagnostic push
#pragma clang diagnostic error "-Wconversion"

namespace art {
namespace metrics {

class MetricsTest : public testing::Test {};

// A trivial MetricsBackend that does nothing for all of the members. This can be overridden by
// test cases to test specific behaviors.
class TestBackendBase : public MetricsBackend {
 public:
  virtual void BeginSession([[maybe_unused]] const SessionData& session_data) {}
  virtual void EndSession() {}

  virtual void ReportCounter([[maybe_unused]] DatumId counter_type,
                             [[maybe_unused]] uint64_t value) {}

  virtual void BeginHistogram([[maybe_unused]] DatumId histogram_type,
                              [[maybe_unused]] size_t num_buckets,
                              [[maybe_unused]] int64_t low_value_,
                              [[maybe_unused]] int64_t high_value) {}
  virtual void ReportHistogramBucket([[maybe_unused]] size_t index,
                                     [[maybe_unused]] uint32_t value) {}
  virtual void EndHistogram() {}
};

TEST_F(MetricsTest, SimpleCounter) {
  MetricsCounter test_counter;

  EXPECT_EQ(0u, test_counter.Value());

  test_counter.AddOne();
  EXPECT_EQ(1u, test_counter.Value());

  test_counter.Add(5);
  EXPECT_EQ(6u, test_counter.Value());
}

TEST_F(MetricsTest, DatumName) {
  EXPECT_EQ("ClassVerificationTotalTime", DatumName(DatumId::kClassVerificationTotalTime));
}

TEST_F(MetricsTest, SimpleHistogramTest) {
  MetricsHistogram<5, 0, 100> histogram;

  // bucket 0: 0-19
  histogram.Add(10);

  // bucket 1: 20-39
  histogram.Add(20);
  histogram.Add(25);

  // bucket 2: 40-59
  histogram.Add(56);
  histogram.Add(57);
  histogram.Add(58);
  histogram.Add(59);

  // bucket 3: 60-79
  histogram.Add(70);
  histogram.Add(70);
  histogram.Add(70);

  // bucket 4: 80-99
  // leave this bucket empty

  class TestBackend : public TestBackendBase {
   public:
    virtual void ReportHistogramBucket(size_t index, uint32_t value) {
      switch (index) {
        case 0:
          EXPECT_EQ(1u, value);
          break;

        case 1:
          EXPECT_EQ(2u, value);
          break;

        case 2:
          EXPECT_EQ(4u, value);
          break;

        case 3:
          EXPECT_EQ(3u, value);
          break;

        case 4:
          EXPECT_EQ(0u, value);
          break;

        default:
          FAIL();
      }
    }
  } backend;

  histogram.ReportBuckets(&backend);
}

// Make sure values added outside the range of the histogram go into the first or last bucket.
TEST_F(MetricsTest, HistogramOutOfRangeTest) {
  MetricsHistogram<2, 0, 100> histogram;

  // bucket 0: 0-49
  histogram.Add(-500);

  // bucket 1: 50-99
  histogram.Add(250);
  histogram.Add(1000);

  class TestBackend : public TestBackendBase {
   public:
    virtual void ReportHistogramBucket(size_t index, uint32_t value) {
      switch (index) {
        case 0:
          EXPECT_EQ(1u, value);
          break;

        case 1:
          EXPECT_EQ(2u, value);
          break;

        default:
          FAIL();
      }
    }
  } backend;

  histogram.ReportBuckets(&backend);
}

// Test adding values to ArtMetrics and reporting them through a test backend.
TEST_F(MetricsTest, ArtMetricsReport) {
  ArtMetrics metrics;

  // Collect some data
  static constexpr uint64_t verification_time = 42;
  metrics.ClassVerificationTotalTime()->Add(verification_time);
  metrics.JitMethodCompileTime()->Add(
      -5);  // Add a negative value so we are guaranteed that it lands in the first bucket.

  // Report and check the data
  class TestBackend : public TestBackendBase {
   public:
    ~TestBackend() {
      EXPECT_TRUE(found_counter_);
      EXPECT_TRUE(found_histogram_);
    }

    virtual void ReportCounter(DatumId counter_type, uint64_t value) {
      if (counter_type == DatumId::kClassVerificationTotalTime) {
        EXPECT_EQ(value, verification_time);
        found_counter_ = true;
      } else {
        EXPECT_EQ(value, 0u);
      }
    }

    virtual void BeginHistogram(DatumId histogram_type, size_t, int64_t, int64_t) {
      histogram_type_ = histogram_type;
    }

    virtual void ReportHistogramBucket(size_t index, uint32_t value) {
      if (histogram_type_ == DatumId::kJitMethodCompileTime && index == 0u) {
        EXPECT_EQ(value, 1u);
        found_histogram_ = true;
      } else {
        EXPECT_EQ(value, 0u);
      }
    }

   private:
    DatumId histogram_type_{DatumId::kClassVerificationTotalTime};
    bool found_counter_{false};
    bool found_histogram_{false};
  } backend;

  metrics.ReportAllMetrics(&backend);
}

}  // namespace metrics
}  // namespace art

#pragma clang diagnostic pop  // -Wconversion
