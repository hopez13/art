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

#ifndef ART_LIBARTBASE_BASE_METRICS_H_
#define ART_LIBARTBASE_BASE_METRICS_H_

#include <stdint.h>

#include <array>
#include <ostream>
#include <string_view>

#include "android-base/logging.h"
#include "base/time_utils.h"

#pragma clang diagnostic push
#pragma clang diagnostic error "-Wconversion"

// COUNTER(counter_name)
#define ART_COUNTERS(COUNTER) COUNTER(ClassVerificationTotalTime)

// HISTOGRAM(counter_name, num_buckets, low_value, high_value)
#define ART_HISTOGRAMS(HISTOGRAM) HISTOGRAM(JitMethodCompileTime, 15, 0, 1'000'000)

namespace art {
namespace metrics {

/**
 * An enumeration of all ART counters and histograms.
 */
enum class DatumId {
#define ART_COUNTER(name) k##name,
  ART_COUNTERS(ART_COUNTER)
#undef ART_COUNTER

#define ART_HISTOGRAM(name, num_buckets, low_value, high_value) k##name,
      ART_HISTOGRAMS(ART_HISTOGRAM)
#undef ART_HISTOGRAM
};

struct SessionData {
  const uint64_t session_id;
  const std::string_view package_name;
  // TODO: compiler filter / dexopt state
};

// MetricsBackends are used by a metrics reporter to write metrics to some external location. For
// example, a backend might write to logcat, or to a file, or to statsd.
class MetricsBackend {
 public:
  virtual ~MetricsBackend() {}

  // Begins an ART metrics session.
  //
  // This is called by the metrics reporter when the runtime is starting up. The session_data
  // includes a session id which is used to correlate any metric reports with the same instance of
  // the ART runtime. Additionally, session_data includes useful metadata such as the package name
  // for this process.
  virtual void BeginSession(const SessionData& session_data) = 0;

  // Marks the end of a metrics session.
  //
  // The metrics reporter will call this when metrics reported ends (e.g. when the runtime is
  // shutting down). No further metrics will be reported for this session. Note that EndSession is
  // not guaranteed to be called, since clean shutdowns for the runtime are quite rare in practice.
  virtual void EndSession() = 0;

  // Called by the metrics reporter to give the current value of the counter with id counter_type.
  //
  // This will be called multiple times for each counter based on when the metrics reporter chooses
  // to report metrics. For example, the metrics reporter may call this at shutdown or every N
  // minutes. Counters are not reset in between invocations, so the value reported here should
  // overwrite any previously reported value.
  virtual void ReportCounter(DatumId counter_type, uint64_t value) = 0;

  virtual void BeginHistogram(DatumId histogram_type,
                              size_t num_buckets,
                              int64_t low_value_,
                              int64_t high_value) = 0;
  virtual void ReportHistogramBucket(size_t index, uint32_t value) = 0;
  virtual void EndHistogram() = 0;
};

class MetricsCounter {
 public:
  using domain_t = uint64_t;

  explicit constexpr MetricsCounter(domain_t value = 0) : value_{value} {}

  void AddOne() { value_++; }
  void Add(domain_t value) { value_ += value; }

  domain_t Value() const { return value_; }

 private:
  domain_t value_;
};

template <size_t num_buckets_, int64_t low_value_, int64_t high_value_>
class MetricsHistogram {
  static_assert(num_buckets_ >= 1);
  static_assert(low_value_ < high_value_);

 public:
  using domain_t = int64_t;

  constexpr MetricsHistogram() : buckets_{} {}

  void Add(int64_t value) {
    const size_t i = value <= low_value_ ? 0
                     : value >= high_value_
                         ? num_buckets_ - 1
                         : static_cast<size_t>(value - low_value_) * num_buckets_ /
                               static_cast<size_t>(high_value_ - low_value_);
    buckets_[i]++;
  }

  void ReportBuckets(MetricsBackend* backend) const {
    for (size_t i = 0; i < num_buckets_; i++) {
      backend->ReportHistogramBucket(i, buckets_[i]);
    }
  }

 private:
  std::array<uint32_t, num_buckets_> buckets_;
};

template <typename Metric>
class AutoTimer {
 public:
  explicit AutoTimer(Metric* metric, bool autostart = true)
      : running_{false}, start_time_microseconds_{}, metric_{metric} {
    if (autostart) {
      Start();
    }
  }

  ~AutoTimer() {
    if (running_) {
      Stop();
    }
  }

  void Start() {
    DCHECK(!running_);
    running_ = true;
    // TODO(eholk): add support for more time units
    start_time_microseconds_ = MicroTime();
  }

  void Stop() {
    DCHECK(running_);
    uint64_t stop_time_microseconds = MicroTime();
    running_ = false;

    metric_->Add(
        static_cast<typename Metric::domain_t>(stop_time_microseconds - start_time_microseconds_));
  }

 private:
  bool running_;
  uint64_t start_time_microseconds_;
  Metric* metric_;
};

/**
 * This struct contains all of the metrics that ART reports.
 */
class ArtMetrics {
 public:
  ArtMetrics();

  void ReportAllMetrics(MetricsBackend* backend) const;

#define ART_COUNTER(name) MetricsCounter name;
  ART_COUNTERS(ART_COUNTER)
#undef ART_COUNTER

#define ART_HISTOGRAM(name, num_buckets, low_value, high_value) \
  MetricsHistogram<num_buckets, low_value, high_value> name;
  ART_HISTOGRAMS(ART_HISTOGRAM)
#undef ART_HISTOGRAM

 private:
  // This field is only included to allow us expand the ART_COUNTERS and ART_HISTOGRAMS macro in
  // the initializer list in ArtMetrics::ArtMetrics. See metrics.cc for how it's used.
  //
  // It's declared as a zero-length array so it has no runtime space impact.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-private-field"
  int unused_[0];
#pragma clang diagnostic pop  // -Wunused-private-field
};

// Returns a human readable name for the given DatumId.
std::string DatumName(DatumId datum);

}  // namespace metrics
}  // namespace art

#pragma clang diagnostic pop  // -Wconversion

#endif  // ART_LIBARTBASE_BASE_METRICS_H_
