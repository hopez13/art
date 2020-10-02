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

template <DatumId counter_type>
uint64_t CounterValue(const MetricsCounter<counter_type>& counter) {
  uint64_t counter_value{0};
  struct CounterBackend : public TestBackendBase {
    explicit CounterBackend(uint64_t* counter_value) : counter_value_{counter_value} {}

    void ReportCounter(DatumId, uint64_t value) { *counter_value_ = value; }

    uint64_t* counter_value_;
  } backend{&counter_value};
  counter.Report(&backend);
  return counter_value;
}

TEST_F(MetricsTest, SimpleCounter) {
  MetricsCounter<DatumId::kClassVerificationTotalTime> test_counter;

  EXPECT_EQ(0u, CounterValue(test_counter));

  test_counter.AddOne();
  EXPECT_EQ(1u, CounterValue(test_counter));

  test_counter.Add(5);
  EXPECT_EQ(6u, CounterValue(test_counter));
}

TEST_F(MetricsTest, DatumName) {
  EXPECT_EQ("ClassVerificationTotalTime", DatumName(DatumId::kClassVerificationTotalTime));
}

TEST_F(MetricsTest, SimpleHistogramTest) {
  MetricsHistogram<DatumId::kJitMethodCompileTime, 5, 0, 100> histogram;

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

  histogram.Report(&backend);
}

// Make sure values added outside the range of the histogram go into the first or last bucket.
TEST_F(MetricsTest, HistogramOutOfRangeTest) {
  MetricsHistogram<DatumId::kJitMethodCompileTime, 2, 0, 100> histogram;

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

  histogram.Report(&backend);
}

TEST_F(MetricsTest, StreamBackendReportCounter) {
  std::stringstream os;

  StreamBackend backend(os);
  backend.ReportCounter(DatumId::kClassVerificationTotalTime, 1234);
  EXPECT_EQ("Counter: ClassVerificationTotalTime, value = 1234\n", os.str());
}

}  // namespace metrics
}  // namespace art

#pragma clang diagnostic pop  // -Wconversion
