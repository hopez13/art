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

#include "reporter.h"

#include "gtest/gtest.h"

#include "common_runtime_test.h"
#include "base/metrics/metrics.h"

#pragma clang diagnostic push
#pragma clang diagnostic error "-Wconversion"

namespace art {
namespace metrics {

class ReportingPeriodSpecTest : public testing::Test {
 public:
  void VerifyFalse(const std::string& spec_str) {
    Verify(spec_str, false, false, false, {});
  }

  void VerifyTrue(
      const std::string& spec_str,
      bool startup_first,
      bool continuous,
      std::vector<uint32_t> periods) {
    Verify(spec_str, true, startup_first, continuous, periods);
  }

  void Verify(
      const std::string& spec_str,
      bool valid,
      bool startup_first,
      bool continuous,
      std::vector<uint32_t> periods) {
    std::string error_msg;
    std::unique_ptr<ReportingPeriodSpec> spec = ReportingPeriodSpec::Parse(spec_str, &error_msg);

    ASSERT_EQ(valid, spec != nullptr) << spec_str;
    if (valid) {
        ASSERT_EQ(spec->spec, spec_str) << spec_str;
        ASSERT_EQ(spec->report_startup_first, startup_first) << spec_str;
        ASSERT_EQ(spec->continuous_reporting, continuous) << spec_str;
        ASSERT_EQ(spec->periods_seconds, periods) << spec_str;
    }
  }
};

TEST_F(ReportingPeriodSpecTest, ParseTestsInvalid) {
  VerifyFalse("");
  VerifyFalse("*");
  VerifyFalse("S *");
  VerifyFalse("foo");
  VerifyFalse("-1");
  VerifyFalse("1 S");
  VerifyFalse("* 1");
  VerifyFalse("1 2 3 -1 3");
  VerifyFalse("1 * 2");
  VerifyFalse("1 S 2");
}

TEST_F(ReportingPeriodSpecTest, ParseTestsValid) {
  VerifyTrue("S", true, false, {});
  VerifyTrue("S 1", true, false, {1});
  VerifyTrue("S 1 2 3 4", true, false, {1, 2, 3, 4});
  VerifyTrue("S 1 *", true, true, {1});
  VerifyTrue("S 1 2 3 4 *", true, true, {1, 2, 3, 4});

  VerifyTrue("1", false, false, {1});
  VerifyTrue("1 2 3 4", false, false, {1, 2, 3, 4});
  VerifyTrue("1 *", false, true, {1});
  VerifyTrue("1 2 3 4 *", false, true, {1, 2, 3, 4});
}

class TestBackend : public MetricsBackend {
 public:
  struct Report {
    uint64_t timestamp_millis;
    SafeMap<DatumId, uint64_t> data;
  };

  void BeginSession(const SessionData& session_data) override {
    session_data_ = session_data;
  }

  void BeginReport(uint64_t timestamp_millis) override {
    current_report_.reset(new Report());
    current_report_->timestamp_millis = timestamp_millis;
  }

  void ReportCounter(DatumId counter_type, uint64_t value) override {
    current_report_->data.Put(counter_type, value);
  }

  void ReportHistogram(DatumId histogram_type ATTRIBUTE_UNUSED,
                       int64_t low_value ATTRIBUTE_UNUSED,
                       int64_t high_value ATTRIBUTE_UNUSED,
                       const std::vector<uint32_t>& buckets ATTRIBUTE_UNUSED) override {
    // TODO: nothing yet. We should implement and test histograms as well.
  }

  void EndReport() override {
    reports_.push_back(*current_report_);
    current_report_ = nullptr;
  }

  SessionData session_data_;
  std::vector<Report> reports_;
  std::unique_ptr<Report> current_report_;
};

class MockMetricsReporter : public MetricsReporter {
 protected:
  MockMetricsReporter(ReportingConfig config, Runtime* runtime) :
      MetricsReporter(std::move(config), runtime),
      art_metrics_(new ArtMetrics()) {}

  const ArtMetrics* GetMetrics() override {
    return art_metrics_.get();
  }

  std::unique_ptr<ArtMetrics> art_metrics_;

  friend class MetricsReporterTest;
};

class MetricsReporterTest : public CommonRuntimeTest {
 protected:
  void SetUp() override {
    // Do the normal setup.
    CommonRuntimeTest::SetUp();

    Thread::Current()->TransitionFromSuspendedToRunnable();
    bool started = runtime_->Start();
    CHECK(started);
  }

  void SetupReporter(const char* period_spec) {
    ReportingConfig config;
    if (period_spec != nullptr) {
      std::string error;
      config.period_spec = ReportingPeriodSpec::Parse(period_spec, &error);
      ASSERT_TRUE(config.period_spec != nullptr);
    }

    reporter_.reset(new MockMetricsReporter(std::move(config), Runtime::Current()));
    backend_ = new TestBackend();
    reporter_->backends_.emplace_back(backend_);

    session_data_ = metrics::SessionData::CreateDefault();
    session_data_.session_id = 1;
  }

  void TearDown() override {
    reporter_ = nullptr;
    backend_ = nullptr;
  }

  bool ShouldReportAtStartup() {
    return reporter_->ShouldReportAtStartup();
  }

  bool ShouldReportAtPeriod() {
    return reporter_->ShouldReportAtPeriod();
  }

  uint32_t GetNextPeriodSeconds() {
      return reporter_->GetNextPeriodSeconds();
  }

  void ReportMetrics() {
    reporter_->ReportMetrics();
  }

  void MaybeStartBackgroundThread(bool add_metrics) {
    // TODO: set session_data.compilation_reason and session_data.compiler_filter
    reporter_->MaybeStartBackgroundThread(session_data_);
    if (add_metrics) {
      reporter_->art_metrics_->JitMethodCompileCount()->Add(1);
      reporter_->art_metrics_->ClassVerificationCount()->Add(2);
    }
  }

  // Right now we either:
  //   1) don't add metrics (with_metrics = false)
  //   2) or always add the same metrics (see MaybeStartBackgroundThread)
  // So we can write a global verify method.
  void VerifyReport(const TestBackend::Report& report, bool with_metrics) {
    ASSERT_EQ(report.data.size(), with_metrics ? 2u : 0u);
    if (with_metrics) {
      ASSERT_EQ(report.data.Get(DatumId::kClassVerificationCount), 2u);
      ASSERT_EQ(report.data.Get(DatumId::kJitMethodCompileCount), 1u);
    }
  }

  void WaitForReport(uint32_t report_count, uint32_t sleep_period_ms) {
    while (true) {
      if (backend_->reports_.size() == report_count) {
        return;
      }
      usleep(sleep_period_ms * 1000);
    }
  }

  std::unique_ptr<MockMetricsReporter> reporter_;
  TestBackend* backend_;
  metrics::SessionData session_data_;
};

TEST_F(MetricsReporterTest, CheckPeriodSpecStartupOnly) {
  SetupReporter("S");

  // Verify startup conditions
  ASSERT_TRUE(ShouldReportAtStartup());
  ASSERT_FALSE(ShouldReportAtPeriod());

  // Start the thread and notify the startup. This will advance the state.
  MaybeStartBackgroundThread(/*add_metrics=*/ true);

  reporter_->NotifyStartupCompleted();
  WaitForReport(/*report_count=*/ 1, /*sleep_period_ms=*/ 50);

  ASSERT_EQ(backend_->reports_.size(), 1u);
  VerifyReport(backend_->reports_[0], /*with_metrics*/ true);

  // We still should not report at period.
  ASSERT_FALSE(ShouldReportAtPeriod());
}

// LARGE TEST: This test takes 1s to run.
TEST_F(MetricsReporterTest, CheckPeriodSpecStartupAndPeriod) {
  SetupReporter("S 1");

  // Verify startup conditions
  ASSERT_TRUE(ShouldReportAtStartup());
  ASSERT_FALSE(ShouldReportAtPeriod());

  // Start the thread and notify the startup. This will advance the state.
  MaybeStartBackgroundThread(/*add_metrics=*/ true);
  reporter_->NotifyStartupCompleted();

  // We're waiting for 2 reports: the startup one, and the 1s one.
  WaitForReport(/*report_count=*/ 2, /*sleep_period_ms=*/ 500);

  ASSERT_EQ(backend_->reports_.size(), 2u);
  // We should not longer report at period.
  ASSERT_FALSE(ShouldReportAtPeriod());
}

// LARGE TEST: This takes take 2s to run.
TEST_F(MetricsReporterTest, CheckPeriodSpecStartupAndPeriodContinuous) {
  SetupReporter("S 1 *");

  // Verify startup conditions
  ASSERT_TRUE(ShouldReportAtStartup());
  ASSERT_FALSE(ShouldReportAtPeriod());

  // Start the thread and notify the startup. This will advance the state.
  MaybeStartBackgroundThread(/*add_metrics=*/ true);
  reporter_->NotifyStartupCompleted();

  // We're waiting for 2 reports: the startup one, and the 1s one.
  WaitForReport(/*report_count=*/ 3, /*sleep_period_ms=*/ 500);

  ASSERT_EQ(backend_->reports_.size(), 3u);

  // We should keep reporting at period.
  ASSERT_TRUE(ShouldReportAtPeriod());
}

}  // namespace metrics
}  // namespace art

#pragma clang diagnostic pop  // -Wconversion
