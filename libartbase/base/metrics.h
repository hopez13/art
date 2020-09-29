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

#ifndef ART_RUNTIME_METRICS_H_
#define ART_RUNTIME_METRICS_H_

#include <stdint.h>

#include <array>
#include <ostream>
#include <string_view>

#include "android-base/logging.h"
#include "base/time_utils.h"

#pragma clang diagnostic push
#pragma clang diagnostic error "-Wconversion"

// COUNTER(counter_name)
#define ART_COUNTERS(COUNTER) \

// HISTOGRAM(counter_name, num_buckets, low_value, high_value)
#define ART_HISTOGRAMS(HISTOGRAM)         \

namespace art {
namespace metrics {

/**
 * An enumeration of all ART counters and histograms.
 */
enum class DatumId {
  unknown_datum,
#define ART_COUNTER(name) name,
  ART_COUNTERS(ART_COUNTER)
#undef ART_COUNTER

#define ART_HISTOGRAM(name, num_buckets, low_value, high_value) name,
  ART_HISTOGRAMS(ART_HISTOGRAM)
#undef ART_HISTOGRAM
};

struct SessionData {
  const std::string_view package_name;
  // TODO: compiler filter / dexopt state
};

class MetricsBackend {
public:
  virtual ~MetricsBackend() {};

  virtual void BeginSession(const SessionData& session_data) = 0;
  virtual void EndSession() = 0;

  virtual void ReportCounter(DatumId counter_type, uint64_t value) = 0;

  virtual void BeginHistogram(DatumId histogram_type,
                              size_t num_buckets,
                              int64_t low_value_,
                              int64_t high_value) = 0;
  virtual void ReportHistogramBucket(size_t index, uint32_t value) = 0;
  virtual void EndHistogram() = 0;
};

class Counter {
public:
 using domain_t = uint64_t;

 constexpr Counter(domain_t value = 0) : value_{value} {}

 void AddOne() { value_++; }
 void Add(domain_t value) { value_ += value; }

 domain_t Value() const { return value_; }

private:
 domain_t value_;
};

template <size_t num_buckets_, int64_t low_value_, int64_t high_value_>
class Histogram {
  static_assert(num_buckets_ >= 1);
  static_assert(low_value_ < high_value_);

public:
 using domain_t = int64_t;

 constexpr Histogram() : buckets_{} {}

 void Add(domain_t value) {
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
  AutoTimer(Metric* metric, bool autostart = true)
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

#define ART_COUNTER(name) Counter name;
  ART_COUNTERS(ART_COUNTER)
#undef ART_COUNTER

#define ART_HISTOGRAM(name, num_buckets, low_value, high_value) \
  Histogram<num_buckets, low_value, high_value> name;
  ART_HISTOGRAMS(ART_HISTOGRAM)
#undef ART_HISTOGRAM

private:
  // This is field is only included to allow us expand the ART_COUNTERS and ART_HISTOGRAMS macro in
  // the initializer list in ArtMetrics::ArtMetrics. See metrics.cc for how it's used.
  //
  // It's declared as a zero-length array so it has no runtime space impact.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-private-field"
  int unused_[0];
#pragma clang diagnostic pop  // -Wunused-private-field
};

constexpr const char* DatumName(DatumId datum) {
  switch (datum) {
#define ART_COUNTER(name) \
  case DatumId::name:     \
    return #name;
    ART_COUNTERS(ART_COUNTER)
#undef ART_COUNTER

#define ART_HISTOGRAM(name, num_buckets, low_value, high_value) \
  case DatumId::name:                                           \
    return #name;
    ART_HISTOGRAMS(ART_HISTOGRAM)
#undef ART_HISTOGRAM

    default:
      return "<unknown datum>";
  }
}

} // namespace metrics
} // namespace art

#pragma clang diagnostic pop  // -Wconversion

#endif // ART_RUNTIME_METRICS_H_
