/*
 * Copyright 2017 The Android Open Source Project
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

#if 0
#include "jdwp_provider.h"

namespace art {

bool operator==(const JdwpProvider& lhs, const JdwpProvider& rhs) {
  return lhs.Equals(rhs);
}

JdwpProvider::Type JdwpProvider::StrToType(const std::string& val) {
  if (val == "internal" || val == "default") {
    return JdwpProvider::Type::kInternal;
  } else if (val == "AdbConnection") {
    return JdwpProvider::Type::kAdbConnection;
  } else {
    return JdwpProvider::Type::kUnknown;
  }
}

}  // namespace art
#endif
