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

#include <array>
#include <stdint.h>
#include <ostream>
#include <string_view>

#include "base/time_utils.h"

#pragma clang diagnostic push
#pragma clang diagnostic error "-Wconversion"

// COUNTER(counter_name)
#define ART_COUNTERS(COUNTER) \

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
};

struct SessionData {
  const std::string_view package_name;
  // TODO: compiler filter / dexopt state
};

class MetricsBackend {
 public:
  virtual ~MetricsBackend() {}

  virtual void BeginSession(const SessionData& session_data) = 0;
  virtual void EndSession() = 0;

  virtual void ReportCounter(DatumId counter_type, uint64_t value) = 0;
};

class MetricsCounter {
 public:
  explicit constexpr MetricsCounter(uint64_t value = 0) : value_{value} {}

  void AddOne() { value_++; }
  void Add(uint64_t value) { value_ += value; }

  uint64_t Value() const { return value_; }

 private:
  uint64_t value_;
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

    default:
      return "<unknown datum>";
  }
}

}  // namespace metrics
}  // namespace art

#pragma clang diagnostic pop  // -Wconversion

#endif  // ART_LIBARTBASE_BASE_METRICS_H_
