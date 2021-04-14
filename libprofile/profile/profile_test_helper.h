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

#ifndef ART_LIBPROFILE_PROFILE_PROFILE_TEST_HELPER_H_
#define ART_LIBPROFILE_PROFILE_PROFILE_TEST_HELPER_H_

#include <memory>
#include <vector>

#include "dex/test_dex_file_builder.h"
#include "profile/profile_compilation_info.h"

namespace art {

class ProfileTestHelper {
 public:
  ProfileTestHelper() = default;

 protected:
  using Hotness = ProfileCompilationInfo::MethodHotness;
  using ProfileInlineCache = ProfileMethodInfo::ProfileInlineCache;
  using ProfileSampleAnnotation = ProfileCompilationInfo::ProfileSampleAnnotation;

  static bool AddMethod(
      ProfileCompilationInfo* info,
      const DexFile* dex,
      uint16_t method_idx,
      const ProfileSampleAnnotation& annotation = ProfileSampleAnnotation::kNone) {
    return AddMethod(info, dex, method_idx, Hotness::kFlagHot, annotation);
  }

  static bool AddMethod(
      ProfileCompilationInfo* info,
      const DexFile* dex,
      uint16_t method_idx,
      Hotness::Flag flags,
      const ProfileSampleAnnotation& annotation = ProfileSampleAnnotation::kNone) {
    return info->AddMethod(
        ProfileMethodInfo(MethodReference(dex, method_idx)), flags, annotation);
  }

  static bool AddMethod(
      ProfileCompilationInfo* info,
      const DexFile* dex,
      uint16_t method_idx,
      const std::vector<ProfileInlineCache>& inline_caches,
      const ProfileSampleAnnotation& annotation = ProfileSampleAnnotation::kNone) {
    return AddMethod(info, dex, method_idx, inline_caches, Hotness::kFlagHot, annotation);
  }

  static bool AddMethod(
      ProfileCompilationInfo* info,
      const DexFile* dex,
      uint16_t method_idx,
      const std::vector<ProfileInlineCache>& inline_caches,
      Hotness::Flag flags,
      const ProfileSampleAnnotation& annotation = ProfileSampleAnnotation::kNone) {
    return info->AddMethod(
        ProfileMethodInfo(MethodReference(dex, method_idx), inline_caches), flags, annotation);
  }

  static bool AddClass(ProfileCompilationInfo* info,
                       const DexFile* dex,
                       dex::TypeIndex type_index,
                       const ProfileSampleAnnotation& annotation = ProfileSampleAnnotation::kNone) {
    std::vector<dex::TypeIndex> classes = {type_index};
    return info->AddClassesForDex(dex, classes.begin(), classes.end(), annotation);
  }

  const DexFile* BuildDex(const std::string& location,
                          uint32_t location_checksum,
                          const std::string& class_descriptor,
                          size_t num_method_ids) {
    TestDexFileBuilder builder;
    constexpr size_t kNumSharedTypes = 10u;
    for (size_t shared_type_index = 0; shared_type_index != kNumSharedTypes; ++shared_type_index) {
      builder.AddType("LSharedType" + std::to_string(shared_type_index) + ";");
    }
    builder.AddType(class_descriptor);
    for (size_t method_index = 0; method_index != num_method_ids; ++method_index) {
      // Keep the number of protos and names low even.
      size_t return_type_index = method_index % kNumSharedTypes;
      size_t method_name_index = method_index / kNumSharedTypes;
      std::string signature = "()LSharedType" + std::to_string(return_type_index) + ";";
      builder.AddMethod(class_descriptor, signature, "m" + std::to_string(method_name_index));
    }
    storage.push_back(builder.Build(location, location_checksum));
    return storage.back().get();
  }

  std::vector<std::unique_ptr<const DexFile>> storage;
};

}  // namespace art

#endif  // ART_LIBPROFILE_PROFILE_PROFILE_TEST_HELPER_H_
