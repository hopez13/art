/*
 * Copyright (C) 2021 The Android Open Source Project
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

#ifndef ART_ODREFRESH_ODR_METRICS_RECORD_H_
#define ART_ODREFRESH_ODR_METRICS_RECORD_H_

#include <cstdint>
#include <iosfwd>  // For forward-declaration of std::string.
#include <tinyxml2.h>

namespace art {
namespace odrefresh {

// Default location for storing metrics from odrefresh.
constexpr const char* kOdrefreshMetricsFile = "/data/misc/odrefresh/odrefresh-metrics.xml";

// MetricsRecord is a simpler container for Odrefresh metric values reported to statsd. The order
// and types of fields here mirror definition of `OdrefreshReported` in
// frameworks/proto_logging/stats/atoms.proto.
struct OdrMetricsRecord {
  int64_t art_apex_version;
  int32_t trigger;
  int32_t stage_reached;
  int32_t status;
  int32_t primary_bcp_compilation_seconds;
  int32_t secondary_bcp_compilation_seconds;
  int32_t system_server_compilation_seconds;
  int32_t cache_space_free_start_mib;
  int32_t cache_space_free_end_mib;

  // Read a `MetricsRecord` from an XML file.
  // Returns true if the XML document was found
  // and parsed correctly, false otherwise.
  bool ReadFromFile(const std::string& filename);

  // Write a `MetricsRecord` to an XML file.
  // Returns true is the XML document was
  // saved correctly, false otherwise.
  bool WriteToFile(const std::string& filename) const;

 private:
  static void AddMetric(tinyxml2::XMLElement* parent, const char* name, int64_t value);
  static void AddMetric(tinyxml2::XMLElement* parent, const char* name, int32_t value);

  static int64_t ReadInt64(tinyxml2::XMLElement* element, const char* name);
  static int32_t ReadInt32(tinyxml2::XMLElement* element, const char* name);
};

}  // namespace odrefresh
}  // namespace art

#endif  // ART_ODREFRESH_ODR_METRICS_RECORD_H_
