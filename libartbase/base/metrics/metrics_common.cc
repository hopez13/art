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
#define ART_METRIC(name, Kind, ...) \
  case DatumId::k##name:  \
    return #name;
    ART_METRICS(ART_METRIC)
#undef ART_METRIC

    default:
      LOG(FATAL) << "Unknown datum id: " << static_cast<unsigned>(datum);
      UNREACHABLE();
  }
}

SessionData SessionData::CreateDefault() {
#ifdef _WIN32
  int32_t uid = kInvalidUserId;  // Windows does not support getuid();
#else
  int32_t uid = static_cast<int32_t>(getuid());
#endif

  return SessionData{
    .compilation_reason = CompilationReason::kUnknown,
    .compiler_filter = CompilerFilterReporting::kUnknown,
    .session_id = kInvalidSessionId,
    .uid = uid,
  };
}

ArtMetrics::ArtMetrics() : beginning_timestamp_ {MilliTime()}
#define ART_METRIC(name, Kind, ...) \
  , name##_ {}
ART_METRICS(ART_METRIC)
#undef ART_METRIC
{
}

void ArtMetrics::ReportAllMetrics(MetricsBackend* backend) const {
  backend->BeginReport(MilliTime() - beginning_timestamp_);

#define ART_METRIC(name, Kind, ...) name()->Report(backend);
  ART_METRICS(ART_METRIC)
#undef ART_METRIC

  backend->EndReport();
}

void ArtMetrics::DumpForSigQuit(std::ostream& os) const {
  StringBackend backend(new TextFormatter());
  ReportAllMetrics(&backend);
  os << backend.GetAndResetBuffer();
}

void ArtMetrics::Reset() {
  beginning_timestamp_ = MilliTime();
#define ART_METRIC(name, kind, ...) name##_.Reset();
  ART_METRICS(ART_METRIC);
#undef ART_METRIC
}

StringBackend::StringBackend(MetricsFormatter* formatter)
  : formatter_(formatter)
{}

StringBackend::~StringBackend() {
  delete formatter_;
}

std::string StringBackend::GetAndResetBuffer() {
  return formatter_->GetAndResetBuffer();
}

void StringBackend::BeginOrUpdateSession(const SessionData& session_data) {
  session_data_ = session_data;
}

void StringBackend::BeginReport(uint64_t timestamp_since_start_ms) {
  formatter_->FormatBeginReport(timestamp_since_start_ms, session_data_);
}

void StringBackend::EndReport() {
  formatter_->FormatEndReport();
}

void StringBackend::ReportCounter(DatumId counter_type, uint64_t value) {
  formatter_->FormatReportCounter(counter_type, value);
}

void StringBackend::ReportHistogram(DatumId histogram_type,
                                    int64_t minimum_value_,
                                    int64_t maximum_value_,
                                    const std::vector<uint32_t>& buckets) {
  formatter_->FormatReportHistogram(histogram_type, minimum_value_, maximum_value_, buckets);
}

TextFormatter::TextFormatter() = default;

void TextFormatter::FormatBeginReport(uint64_t timestamp_since_start_ms,
                                      std::optional<SessionData> session_data) {
  os_ << "\n*** ART internal metrics ***\n";
  os_ << "  Metadata:\n";
  os_ << "    timestamp_since_start_ms: " << timestamp_since_start_ms << "\n";
  if (session_data.has_value()) {
    os_ << "    session_id: " << session_data->session_id << "\n";
    os_ << "    uid: " << session_data->uid << "\n";
    os_ << "    compilation_reason: " << CompilationReasonName(session_data->compilation_reason)
                   << "\n";
    os_ << "    compiler_filter: " << CompilerFilterReportingName(session_data->compiler_filter)
        << "\n";
  }
  os_ << "  Metrics:\n";
}

void TextFormatter::FormatReportCounter(DatumId counter_type, uint64_t value) {
  os_ << "    " << DatumName(counter_type) << ": count = " << value << "\n";
}

void TextFormatter::FormatReportHistogram(DatumId histogram_type,
                                          int64_t minimum_value_,
                                          int64_t maximum_value_,
                                          const std::vector<uint32_t>& buckets) {
  os_ << "    " << DatumName(histogram_type) << ": range = " << minimum_value_ << "..." << maximum_value_;
  if (!buckets.empty()) {
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

void TextFormatter::FormatEndReport() {
  os_ << "*** Done dumping ART internal metrics ***\n";
}

std::string TextFormatter::GetAndResetBuffer() {
  std::string result = os_.str();
  os_.clear();
  os_.str("");
  return result;
}

JsonFormatter::JsonFormatter() = default;

void JsonFormatter::FormatBeginReport(uint64_t timestamp_millis,
                                      std::optional<SessionData> session_data) {
  Json::Value& metadata = json_root_["metadata"];
  metadata["timestamp_since_start_ms"] = timestamp_millis;
  if (session_data.has_value()) {
    metadata["session_id"] = session_data->session_id;
    metadata["uid"] = session_data->uid;
    metadata["compilation_reason"] = CompilationReasonName(session_data->compilation_reason);
    metadata["compiler_filter"] = CompilerFilterReportingName(session_data->compiler_filter);
  }
}

void JsonFormatter::FormatReportCounter(DatumId counter_type, uint64_t value) {
  Json::Value& counter = json_root_["metrics"][DatumName(counter_type)];
  counter["counter_type"] = "count";
  counter["value"] = value;
}

void JsonFormatter::FormatReportHistogram(DatumId histogram_type,
                                          int64_t low_value,
                                          int64_t high_value,
                                          const std::vector<uint32_t>& buckets) {
  Json::Value& histogram = json_root_["metrics"][DatumName(histogram_type)];
  histogram["counter_type"] = "range";
  histogram["minimum_value"] = low_value;
  histogram["maximum_value"] = high_value;

  Json::Value bucketsList(Json::arrayValue);
  if (!buckets.empty()) {
    for (const auto& count : buckets) {
      bucketsList.append(count);
    }
  }
  histogram["buckets"] = bucketsList;
}

void JsonFormatter::FormatEndReport() {}

std::string JsonFormatter::GetAndResetBuffer() {
  Json::StreamWriterBuilder builder;
  builder["indentation"] = "";
  std::string document = Json::writeString(builder, json_root_);
  json_root_.clear();
  return document;
}

LogBackend::LogBackend(MetricsFormatter* formatter, android::base::LogSeverity level)
  : StringBackend(formatter), level_{level}
{}

void LogBackend::BeginReport(uint64_t timestamp_since_start_ms) {
  StringBackend::GetAndResetBuffer();
  StringBackend::BeginReport(timestamp_since_start_ms);
}

void LogBackend::EndReport() {
  StringBackend::EndReport();
  LOG_STREAM(level_) << StringBackend::GetAndResetBuffer();
}

FileBackend::FileBackend(MetricsFormatter* formatter, const std::string& filename)
  : StringBackend(formatter), filename_{filename}
{}

void FileBackend::BeginReport(uint64_t timestamp_since_start_ms) {
  StringBackend::GetAndResetBuffer();
  StringBackend::BeginReport(timestamp_since_start_ms);
}

void FileBackend::EndReport() {
  StringBackend::EndReport();
  std::string error_message;
  auto file{
      LockedFile::Open(filename_.c_str(), O_CREAT | O_WRONLY | O_APPEND, true, &error_message)};
  if (file.get() == nullptr) {
    LOG(WARNING) << "Could open metrics file '" << filename_ << "': " << error_message;
  } else {
    if (!android::base::WriteStringToFd(StringBackend::GetAndResetBuffer(), file.get()->Fd())) {
      PLOG(WARNING) << "Error writing metrics to file";
    }
  }
}

// Make sure CompilationReasonName and CompilationReasonForName are inverses.
static_assert(CompilationReasonFromName(CompilationReasonName(CompilationReason::kError)) ==
              CompilationReason::kError);
static_assert(CompilationReasonFromName(CompilationReasonName(CompilationReason::kUnknown)) ==
              CompilationReason::kUnknown);
static_assert(CompilationReasonFromName(CompilationReasonName(CompilationReason::kFirstBoot)) ==
              CompilationReason::kFirstBoot);
static_assert(CompilationReasonFromName(CompilationReasonName(CompilationReason::kBootAfterOTA)) ==
              CompilationReason::kBootAfterOTA);
static_assert(CompilationReasonFromName(CompilationReasonName(CompilationReason::kPostBoot)) ==
              CompilationReason::kPostBoot);
static_assert(CompilationReasonFromName(CompilationReasonName(CompilationReason::kInstall)) ==
              CompilationReason::kInstall);
static_assert(CompilationReasonFromName(CompilationReasonName(CompilationReason::kInstallFast)) ==
              CompilationReason::kInstallFast);
static_assert(CompilationReasonFromName(CompilationReasonName(CompilationReason::kInstallBulk)) ==
              CompilationReason::kInstallBulk);
static_assert(
    CompilationReasonFromName(CompilationReasonName(CompilationReason::kInstallBulkSecondary)) ==
    CompilationReason::kInstallBulkSecondary);
static_assert(
    CompilationReasonFromName(CompilationReasonName(CompilationReason::kInstallBulkDowngraded)) ==
    CompilationReason::kInstallBulkDowngraded);
static_assert(CompilationReasonFromName(
                  CompilationReasonName(CompilationReason::kInstallBulkSecondaryDowngraded)) ==
              CompilationReason::kInstallBulkSecondaryDowngraded);
static_assert(CompilationReasonFromName(CompilationReasonName(CompilationReason::kBgDexopt)) ==
              CompilationReason::kBgDexopt);
static_assert(CompilationReasonFromName(CompilationReasonName(CompilationReason::kABOTA)) ==
              CompilationReason::kABOTA);
static_assert(CompilationReasonFromName(CompilationReasonName(CompilationReason::kInactive)) ==
              CompilationReason::kInactive);
static_assert(CompilationReasonFromName(CompilationReasonName(CompilationReason::kShared)) ==
              CompilationReason::kShared);
static_assert(
    CompilationReasonFromName(CompilationReasonName(CompilationReason::kInstallWithDexMetadata)) ==
    CompilationReason::kInstallWithDexMetadata);
static_assert(CompilationReasonFromName(CompilationReasonName(CompilationReason::kPrebuilt)) ==
              CompilationReason::kPrebuilt);
static_assert(CompilationReasonFromName(CompilationReasonName(CompilationReason::kCmdLine)) ==
              CompilationReason::kCmdLine);

}  // namespace metrics
}  // namespace art

#pragma clang diagnostic pop  // -Wconversion
