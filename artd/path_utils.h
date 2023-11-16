/*
 * Copyright (C) 2022 The Android Open Source Project
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

#ifndef ART_ARTD_PATH_UTILS_H_
#define ART_ARTD_PATH_UTILS_H_

#include <string>
#include <vector>

#include "aidl/com/android/server/art/BnArtd.h"
#include "android-base/result.h"
#include "base/file_utils.h"

namespace art {
namespace artd {

struct RawArtifactsPath {
  std::string oat_path;
  std::string vdex_path;
  std::string art_path;
};

android::base::Result<std::string> GetAndroidDataOrError();

android::base::Result<std::string> GetAndroidExpandOrError();

android::base::Result<std::string> GetArtRootOrError();

// Returns all existing files that are managed by artd.
std::vector<std::string> ListManagedFiles(const std::string& android_data,
                                          const std::string& android_expand);

std::vector<std::string> ListRuntimeArtifactsFiles(
    const std::string& android_data,
    const std::string& android_expand,
    const aidl::com::android::server::art::RuntimeArtifactsPath& runtime_artifacts_path);

android::base::Result<void> ValidateRuntimeArtifactsPath(
    const aidl::com::android::server::art::RuntimeArtifactsPath& runtime_artifacts_path);

android::base::Result<std::string> BuildArtBinPath(const std::string& binary_name);

// Returns the absolute paths to files built from the `ArtifactsPath`.
android::base::Result<RawArtifactsPath> BuildArtifactsPath(
    const aidl::com::android::server::art::ArtifactsPath& artifacts_path, bool is_pre_reboot);

android::base::Result<std::string> BuildPrimaryRefProfilePath(
    const aidl::com::android::server::art::ProfilePath::PrimaryRefProfilePath&
        primary_ref_profile_path);

android::base::Result<std::string> BuildPrebuiltProfilePath(
    const aidl::com::android::server::art::ProfilePath::PrebuiltProfilePath& prebuilt_profile_path);

android::base::Result<std::string> BuildPrimaryCurProfilePath(
    const aidl::com::android::server::art::ProfilePath::PrimaryCurProfilePath&
        primary_cur_profile_path);

android::base::Result<std::string> BuildSecondaryRefProfilePath(
    const aidl::com::android::server::art::ProfilePath::SecondaryRefProfilePath&
        secondary_ref_profile_path);

android::base::Result<std::string> BuildSecondaryCurProfilePath(
    const aidl::com::android::server::art::ProfilePath::SecondaryCurProfilePath&
        secondary_cur_profile_path);

android::base::Result<std::string> BuildWritableProfilePath(
    const aidl::com::android::server::art::ProfilePath::WritableProfilePath& profile_path,
    bool is_pre_reboot);

android::base::Result<std::string> BuildFinalProfilePath(
    const aidl::com::android::server::art::ProfilePath::TmpProfilePath& tmp_profile_path,
    bool is_pre_reboot);

android::base::Result<std::string> BuildTmpProfilePath(
    const aidl::com::android::server::art::ProfilePath::TmpProfilePath& tmp_profile_path);

android::base::Result<std::string> BuildDexMetadataPath(
    const aidl::com::android::server::art::DexMetadataPath& dex_metadata_path);

android::base::Result<std::string> BuildProfileOrDmPath(
    const aidl::com::android::server::art::ProfilePath& profile_path);

android::base::Result<std::string> BuildVdexPath(
    const aidl::com::android::server::art::VdexPath& vdex_path);

// Sets the root dir for `ListManagedFiles` and `ListRuntimeImageFiles`.
// The passed string must be alive until the test ends.
// For testing use only.
void TestOnlySetListRootDir(std::string_view root_dir);

}  // namespace artd
}  // namespace art

#endif  // ART_ARTD_PATH_UTILS_H_
