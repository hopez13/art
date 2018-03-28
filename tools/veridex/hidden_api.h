/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef ART_TOOLS_VERIDEX_HIDDEN_API_H_
#define ART_TOOLS_VERIDEX_HIDDEN_API_H_

#include <ostream>
#include <set>
#include <string>

namespace art {

class DexFile;

enum ApiList {
  kBlacklist = 0,
  kDarkGreylist = 1,
  kLightGreylist = 2,
  kWhitelist = 3,
};

std::ostream& operator<<(std::ostream& os, ApiList api_list);

/**
 * Helper class for logging if a method/field is in a hidden API list.
 */
class HiddenApi {
 public:
  HiddenApi(const char* blacklist, const char* dark_greylist, const char* light_greylist) {
    FillList(light_greylist, light_greylist_);
    FillList(dark_greylist, dark_greylist_);
    FillList(blacklist, blacklist_);
  }

  bool LogIfInList(const std::string& name, const char* access_kind) const {
    return LogIfIn(name, blacklist_, "Blacklist", access_kind) ||
        LogIfIn(name, dark_greylist_, "Dark greylist", access_kind) ||
        LogIfIn(name, light_greylist_, "Light greylist", access_kind);
  }

  ApiList GetApiList(const std::string& name) const {
    if (IsInList(name, blacklist_)) {
      return ApiList::kBlacklist;
    } else if (IsInList(name, dark_greylist_)) {
      return ApiList::kDarkGreylist;
    } else if (IsInList(name, light_greylist_)) {
      return ApiList::kLightGreylist;
    } else {
      return ApiList::kWhitelist;
    }
  }

  bool IsInRestrictionList(const std::string& name) const {
    return GetApiList(name) != ApiList::kWhitelist;
  }

  static std::string GetApiMethodName(const DexFile& dex_file, uint32_t method_index);

  static std::string GetApiFieldName(const DexFile& dex_file, uint32_t field_index);

 private:
  static bool LogIfIn(const std::string& name,
                      const std::set<std::string>& list,
                      const std::string& log,
                      const std::string& access_kind);

  static bool IsInList(const std::string& name, const std::set<std::string>& list) {
    return list.find(name) != list.end();
  }

  static void FillList(const char* filename, std::set<std::string>& entries);

  std::set<std::string> blacklist_;
  std::set<std::string> light_greylist_;
  std::set<std::string> dark_greylist_;
};

}  // namespace art

#endif  // ART_TOOLS_VERIDEX_HIDDEN_API_H_
