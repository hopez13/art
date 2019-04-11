/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_RUNTIME_VERIFIER_SCOPED_LOG_H_
#define ART_RUNTIME_VERIFIER_SCOPED_LOG_H_

#include <ostream>

namespace art {
namespace verifier {

// RAII to inject a newline after a message.
struct ScopedLog {
  explicit ScopedLog(std::ostream& os) : stream(os), active(true) {}

  ScopedLog(ScopedLog&& other) : stream(other.stream), active(other.active) {
    other.active = false;
  }

  ScopedLog(ScopedLog&) = delete;
  ScopedLog& operator=(ScopedLog&) = delete;

  ~ScopedLog() {
    if (active) {
      stream << std::endl;
    }
  }

  template <class T>
  ScopedLog& operator<<(const T& t) {
    stream << t;
    return *this;
  }

  ScopedLog& operator<<(std::ostream& (*f)(std::ostream&)) {
    stream << f;
    return *this;
  }

  std::ostream& stream;
  bool active = true;
};

}  // namespace verifier
}  // namespace art

#endif  // ART_RUNTIME_VERIFIER_SCOPED_LOG_H_
