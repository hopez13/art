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

#ifndef ART_RUNTIME_JDWP_PROVIDER_H_
#define ART_RUNTIME_JDWP_PROVIDER_H_

#include <string>

#include "base/macros.h"
#include "base/logging.h"

namespace art {

class JdwpProvider {
 public:
  // TODO Remove the kInternal type when old jdwp implementation is removed and make it default to
  // libjdwp.so
  enum class Type {
    kInternal,
    kAgent,
  };

  JdwpProvider() : type_(Type::kInternal), agent_("") {}
  explicit JdwpProvider(const std::string& agent) : type_(Type::kAgent), agent_(agent) {}

  bool IsInternal() {
    return type_ == Type::kInternal;
  }

  const std::string& GetAgent() {
    DCHECK(!IsInternal());
    return agent_;
  }

  bool Equals(const JdwpProvider& rhs) const {
    return type_ == rhs.type_ && agent_ == rhs.agent_;
  }

 private:
  Type type_;
  std::string agent_;
};

bool operator==(const JdwpProvider& lhs, const JdwpProvider& rhs);

}  // namespace art
#endif  // ART_RUNTIME_JDWP_PROVIDER_H_
