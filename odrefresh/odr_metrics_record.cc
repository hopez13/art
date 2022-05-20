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

#include "android-base/logging.h"
#include "odr_metrics_record.h"
#include "tinyxml2.h"

#include <iosfwd>
#include <string>

namespace art {
namespace odrefresh {

bool OdrMetricsRecord::ReadFromFile(const std::string& filename) {
  tinyxml2::XMLDocument xml_document;
  tinyxml2::XMLError result = xml_document.LoadFile(filename.data());
  if (result != tinyxml2::XML_SUCCESS) {
    LOG(ERROR) << "Loading XML file " << filename
               << "failed, error: " << xml_document.ErrorStr();
    return false;
  }

  tinyxml2::XMLElement* metrics = xml_document.FirstChildElement("odrefresh_metrics");
  if (metrics == nullptr) {
    LOG(ERROR) << "odrefresh_metrics element not found in " << filename;
    return false;
  }

  return
    ReadInt32(metrics, "odrefresh_metrics_version", &odrefresh_metrics_version) &&
    ReadInt64(metrics, "art_apex_version", &art_apex_version) &&
    ReadInt32(metrics, "trigger", &trigger) &&
    ReadInt32(metrics, "stage_reached", &stage_reached) &&
    ReadInt32(metrics, "status", &status) &&
    ReadInt32(metrics, "primary_bcp_compilation_seconds", &primary_bcp_compilation_seconds) &&
    ReadInt32(metrics, "secondary_bcp_compilation_seconds", &secondary_bcp_compilation_seconds) &&
    ReadInt32(metrics, "system_server_compilation_seconds", &system_server_compilation_seconds) &&
    ReadInt32(metrics, "cache_space_free_start_mib", &cache_space_free_start_mib) &&
    ReadInt32(metrics, "cache_space_free_end_mib", &cache_space_free_end_mib);
}

bool OdrMetricsRecord::ReadInt64(tinyxml2::XMLElement* parent,
                                 const char* name,
                                 /*out*/ int64_t* metric) {
  tinyxml2::XMLElement* element = parent->FirstChildElement(name);
  if (element == nullptr) {
    LOG(ERROR) << "Expected Odfresh metric " << name << " not found";
    return false;
  }

  tinyxml2::XMLError result = element->QueryInt64Text(metric);
  return result == tinyxml2::XML_SUCCESS;
}

bool OdrMetricsRecord::ReadInt32(tinyxml2::XMLElement* parent,
                                 const char* name,
                                 /*out8*/ int32_t* metric) {
  tinyxml2::XMLElement* element = parent->FirstChildElement(name);
  if (element == nullptr) {
    LOG(ERROR) << "Expected Odrefresh metric " << name << " not found";
    return false;
  }

  tinyxml2::XMLError result = element->QueryIntText(metric);
  return result == tinyxml2::XML_SUCCESS;
}

bool OdrMetricsRecord::WriteToFile(const std::string& filename) const {
  tinyxml2::XMLDocument xml_document;
  tinyxml2::XMLElement* metrics = xml_document.NewElement("odrefresh_metrics");
  xml_document.InsertEndChild(metrics);

  // The order here matches the field order of MetricsRecord.
  AddMetric(metrics, "odrefresh_metrics_version", odrefresh_metrics_version);
  AddMetric(metrics, "art_apex_version", art_apex_version);
  AddMetric(metrics, "trigger", trigger);
  AddMetric(metrics, "stage_reached", stage_reached);
  AddMetric(metrics, "status", status);
  AddMetric(metrics, "primary_bcp_compilation_seconds", primary_bcp_compilation_seconds);
  AddMetric(metrics, "secondary_bcp_compilation_seconds", secondary_bcp_compilation_seconds);
  AddMetric(metrics, "system_server_compilation_seconds", system_server_compilation_seconds);
  AddMetric(metrics, "cache_space_free_start_mib", cache_space_free_start_mib);
  AddMetric(metrics, "cache_space_free_end_mib", cache_space_free_end_mib);

  tinyxml2::XMLError result = xml_document.SaveFile(filename.data(), /*compact=*/true);
  return result == tinyxml2::XML_SUCCESS;
}

template<typename T>
void OdrMetricsRecord::AddMetric(tinyxml2::XMLElement* parent,
                                 const char* name,
                                 T& value) {
  parent->InsertNewChildElement(name)->SetText(value);
}

}  // namespace odrefresh
}  // namespace art
