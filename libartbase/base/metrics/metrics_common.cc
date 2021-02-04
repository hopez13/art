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

#include <sstream>

#include "android-base/file.h"
#include "android-base/logging.h"
#include "base/macros.h"
#include "base/scoped_flock.h"
#include "metrics.h"

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

SessionData SessionData::CreateDefault() {
  return SessionData{
    .compilation_reason = CompilationReason::kUnknown,
    .compiler_filter = std::nullopt,
    .session_id = kInvalidSessionId,
    .uid = static_cast<int32_t>(getuid()),
  };
}

ArtMetrics::ArtMetrics() : unused_ {}
#define ART_COUNTER(name) \
  , name##_ {}
ART_COUNTERS(ART_COUNTER)
#undef ART_COUNTER
#define ART_HISTOGRAM(name, num_buckets, low_value, high_value) \
  , name##_ {}
ART_HISTOGRAMS(ART_HISTOGRAM)
#undef ART_HISTOGRAM
{
}

void ArtMetrics::ReportAllMetrics(MetricsBackend* backend) const {
  backend->BeginReport(MilliTime());

// Dump counters
#define ART_COUNTER(name) name()->Report(backend);
  ART_COUNTERS(ART_COUNTER)
#undef ART_COUNTERS

// Dump histograms
#define ART_HISTOGRAM(name, num_buckets, low_value, high_value) name()->Report(backend);
  ART_HISTOGRAMS(ART_HISTOGRAM)
#undef ART_HISTOGRAM

  backend->EndReport();
}

void ArtMetrics::DumpForSigQuit(std::ostream& os) const {
  StringBackend backend;
  ReportAllMetrics(&backend);
  os << backend.GetAndResetBuffer();
}

StringBackend::StringBackend() {}

std::string StringBackend::GetAndResetBuffer() {
  std::string result = os_.str();
  os_.clear();
  os_.str("");
  return result;
}

void StringBackend::BeginSession([[maybe_unused]] const SessionData& session_data) {
  // Not needed for now.
}

void StringBackend::BeginReport(uint64_t timestamp_millis) {
  os_ << "\n*** ART internal metrics ***\n\n";
  os_ << "timestamp: " << timestamp_millis << "\n";
}

void StringBackend::EndReport() { os_ << "\n*** Done dumping ART internal metrics ***\n"; }

void StringBackend::ReportCounter(DatumId counter_type, uint64_t value) {
  os_ << DatumName(counter_type) << ": count = " << value << "\n";
}

void StringBackend::ReportHistogram(DatumId histogram_type,
                                    int64_t minimum_value_,
                                    int64_t maximum_value_,
                                    const std::vector<uint32_t>& buckets) {
  os_ << DatumName(histogram_type) << ": range = " << minimum_value_ << "..." << maximum_value_;
  if (buckets.size() > 0) {
    os_ << ", buckets: ";
    bool first = true;
    for (const auto& count : buckets) {
      if (!first) {
        os_ << ",";
      }
      first = false;
      os_ << count;
    }
    os_ << "\n";
  } else {
    os_ << ", no buckets\n";
  }
}

LogBackend::LogBackend(android::base::LogSeverity level) : level_{level} {}

void LogBackend::BeginReport(uint64_t timestamp_millis) {
  GetAndResetBuffer();
  StringBackend::BeginReport(timestamp_millis);
}

void LogBackend::EndReport() {
  StringBackend::EndReport();
  LOG_STREAM(level_) << GetAndResetBuffer();
}

FileBackend::FileBackend(std::string filename) : filename_{filename} {}

void FileBackend::BeginReport(uint64_t timestamp_millis) {
  GetAndResetBuffer();
  StringBackend::BeginReport(timestamp_millis);
}

void FileBackend::EndReport() {
  StringBackend::EndReport();
  std::string error_message;
  auto file{
      LockedFile::Open(filename_.c_str(), O_CREAT | O_WRONLY | O_APPEND, true, &error_message)};
  if (file.get() == nullptr) {
    LOG(WARNING) << "Could open metrics file '" << filename_ << "': " << error_message;
  } else {
    if (!android::base::WriteStringToFd(GetAndResetBuffer(), file.get()->Fd())) {
      PLOG(WARNING) << "Error writing metrics to file";
    }
  }
}

}  // namespace metrics
}  // namespace art

#pragma clang diagnostic pop  // -Wconversion
