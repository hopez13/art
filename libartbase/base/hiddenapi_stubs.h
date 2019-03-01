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

#ifndef ART_LIBARTBASE_BASE_HIDDENAPI_STUBS_H_
#define ART_LIBARTBASE_BASE_HIDDENAPI_STUBS_H_

#include <set>
#include <string_view>

namespace art {
namespace hiddenapi {

enum class ApiStubsKind {
  kPublicApi,
  kSystemApi,
  kTestApi,
  kCorePlatformApi,
};

class HiddenApiStubs {
 public:
  static constexpr std::string_view kPublicApiStr{"public-api"};
  static constexpr std::string_view kSystemApiStr{"system-api"};
  static constexpr std::string_view kTestApiStr{"test-api"};
  static constexpr std::string_view kCorePlatformApiStr{"core-platform-api"};

  static const std::string_view ApiToString(ApiStubsKind api) {
    switch (api) {
      case ApiStubsKind::kPublicApi:
        return kPublicApiStr;
      case ApiStubsKind::kSystemApi:
        return kSystemApiStr;
      case ApiStubsKind::kTestApi:
        return kTestApiStr;
      case ApiStubsKind::kCorePlatformApi:
        return kCorePlatformApiStr;
    }
  }

  static bool IsIgnoredApiFlag(const std::string_view& api_flag_name) {
    return kIgnoredApiFlags.find(api_flag_name) != kIgnoredApiFlags.end();
  }


 private:
  static const std::set<std::string_view> kIgnoredApiFlags;
};


}  // namespace hiddenapi
}  // namespace art


#endif  // ART_LIBARTBASE_BASE_HIDDENAPI_STUBS_H_
