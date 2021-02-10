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

#ifndef ART_ODREFRESH_ODR_ARTIFACTS_H_
#define ART_ODREFRESH_ODR_ARTIFACTS_H_

#include <iosfwd>
#include <string>

#include <base/file_utils.h>

namespace art {
namespace odrefresh {

class OdrArtifacts {
 public:
  static OdrArtifacts ForBootImageExtension(const std::string& image_location) {
    return OdrArtifacts(image_location, "oat");
  }

  static OdrArtifacts ForSystemServer(const std::string& image_location) {
    return OdrArtifacts(image_location, "odex");
  }

  const std::string& ImageLocation() const { return image_location_; }
  const std::string& OatLocation() const { return oat_location_; }
  const std::string& VdexLocation() const { return vdex_location_; }

 private:
  OdrArtifacts(const std::string& image_location, const char* aot_extension)
      : image_location_{image_location},
        oat_location_{ReplaceFileExtension(image_location, aot_extension)},
        vdex_location_{ReplaceFileExtension(image_location, "vdex")} {}

  OdrArtifacts() = delete;
  OdrArtifacts(const OdrArtifacts&) = delete;
  OdrArtifacts& operator=(const OdrArtifacts&) = delete;

  const std::string image_location_;
  const std::string oat_location_;
  const std::string vdex_location_;
};

}  // namespace odrefresh
}  // namespace art

#endif  // ART_ODREFRESH_ODR_ARTIFACTS_H_
