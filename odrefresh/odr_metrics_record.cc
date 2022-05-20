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

#include "odr_metrics_record.h"
#include "tinyxml2.h"

#include <iosfwd>
#include <string>

namespace art {
namespace odrefresh {

bool OdrMetricsRecord::ReadFromFile(const std::string& filename) {
  tinyxml2::XMLDocument xml_document;
  tinyxml2::XMLError result = xml_document.LoadFile(filename.data());
  if (result != tinyxml2::XML_SUCCESS) return false;

  tinyxml2::XMLElement* metrics = xml_document.FirstChildElement("odrefresh_metrics");
  if (metrics == nullptr) return false;

  art_apex_version = ReadInt64(metrics, "art_apex_version");
  trigger = ReadInt32(metrics, "trigger");
  stage_reached = ReadInt32(metrics, "stage_reached");
  status = ReadInt32(metrics, "status");
  primary_bcp_compilation_seconds = ReadInt32(metrics, "primary_bcp_compilation_seconds");
  secondary_bcp_compilation_seconds = ReadInt32(metrics, "secondary_bcp_compilation_seconds");
  system_server_compilation_seconds = ReadInt32(metrics, "system_server_compilation_seconds");
  cache_space_free_start_mib = ReadInt32(metrics, "cache_space_free_start_mib");
  cache_space_free_end_mib = ReadInt32(metrics, "cache_space_free_end_mib");

  return true;
}

int64_t OdrMetricsRecord::ReadInt64(tinyxml2::XMLElement* element,
                                    const char* name) {
  return element->FirstChildElement(name)->Int64Text();
}

int32_t OdrMetricsRecord::ReadInt32(tinyxml2::XMLElement* element,
                                    const char* name) {
  return element->FirstChildElement(name)->IntText();
}

bool OdrMetricsRecord::WriteToFile(const std::string& filename) const {
  tinyxml2::XMLDocument xml_document;
  tinyxml2::XMLElement* metrics = xml_document.NewElement("odrefresh_metrics");
  xml_document.InsertEndChild(metrics);

  // The order here matches the field order of MetricsRecord.
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

void OdrMetricsRecord::AddMetric(tinyxml2::XMLElement* parent,
                                 const char* name,
                                 int64_t value) {
  parent->InsertNewChildElement(name)->SetText(value);
}

void OdrMetricsRecord::AddMetric(tinyxml2::XMLElement* parent,
                                 const char* name,
                                 int32_t value) {
  parent->InsertNewChildElement(name)->SetText(value);
}

}  // namespace odrefresh
}  // namespace art
