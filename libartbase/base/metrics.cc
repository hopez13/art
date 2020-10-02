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

#include "android-base/logging.h"
#include "base/macros.h"

#pragma clang diagnostic push
#pragma clang diagnostic error "-Wconversion"

namespace art {
namespace metrics {

std::string DatumName(DatumId datum) {
  switch (datum) {
#define ART_COUNTER(name) \
  case DatumId::k##name:  \
    return #name;
    ART_COUNTERS(ART_COUNTER)
#undef ART_COUNTER

#define ART_HISTOGRAM(name, num_buckets, low_value, high_value) \
  case DatumId::k##name:                                        \
    return #name;
    ART_HISTOGRAMS(ART_HISTOGRAM)
#undef ART_HISTOGRAM

    default:
      LOG(FATAL) << "Unknown datum id: " << static_cast<unsigned>(datum);
      UNREACHABLE();
  }
}

ArtMetrics::ArtMetrics()
    :
#define ART_COUNTER(name) name##_{},
      ART_COUNTERS(ART_COUNTER)
#undef ART_COUNTER
#define ART_HISTOGRAM(name, num_buckets, low_value, high_value) name##_{},
          ART_HISTOGRAMS(ART_HISTOGRAM)
#undef ART_HISTOGRAM
              unused_{} {
}

void ArtMetrics::ReportAllMetrics(MetricsBackend* backend) const {
// Dump counters
#define ART_COUNTER(name) name()->Report(backend);
  ART_COUNTERS(ART_COUNTER)
#undef ART_COUNTERS

// Dump histograms
#define ART_HISTOGRAM(name, num_buckets, low_value, high_value) name()->Report(backend);
  ART_HISTOGRAMS(ART_HISTOGRAM)
#undef ART_HISTOGRAM
}

StreamBackend::StreamBackend(std::ostream& os) : os_{os} {}

void StreamBackend::BeginSession(const SessionData& session_data) {
  os_ << "Beginning ART Metrics session for package " << session_data.package_name << "\n";
}

void StreamBackend::EndSession() { os_ << "ART Metrics session ended.\n"; }

void StreamBackend::ReportCounter(DatumId counter_type, uint64_t value) {
  os_ << "Counter: " << DatumName(counter_type) << ", value = " << value << "\n";
}

void StreamBackend::ReportHistogram(DatumId histogram_type,
                                    int64_t /*low_value_*/,
                                    int64_t /*high_value*/,
                                    const std::vector<uint32_t>& buckets) {
  os_ << "Histogram: " << DatumName(histogram_type) << "\n";
  for (size_t i{0}; i < buckets.size(); ++i) {
    os_ << "  Bucket " << i << ": " << buckets[i] << "\n";
  }
}

}  // namespace metrics
}  // namespace art

#pragma clang diagnostic pop  // -Wconversion
