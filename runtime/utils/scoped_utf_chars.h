/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef ART_RUNTIME_UTILS_SCOPED_UTF_CHARS_H_
#define ART_RUNTIME_UTILS_SCOPED_UTF_CHARS_H_

#include <cstddef>
#include <cstring>

#include <jni.h>

#include "android-base/macros.h"

namespace art {

class ScopedUtfChars {
 public:
  ScopedUtfChars(JNIEnv* env, jstring str) : env_(env), string_(str) {
    if (str == nullptr) {
      utf_chars_ = nullptr;
      ScopedUtfChars::ThrowNullPointerException();
    } else {
      utf_chars_ = env->GetStringUTFChars(str, nullptr);
    }
  }

  ScopedUtfChars(ScopedUtfChars&& rhs)
      : env_(rhs.env_),
        string_(rhs.string_),
        utf_chars_(rhs.utf_chars_) {
    rhs.env_ = nullptr;
    rhs.string_ = nullptr;
    rhs.utf_chars_ = nullptr;
  }

  ~ScopedUtfChars() {
    if (utf_chars_ != nullptr) {
      env_->ReleaseStringUTFChars(string_, utf_chars_);
    }
  }

  const char* c_str() const {
    return utf_chars_;
  }

 private:
  // Avoids having a header include for common_throws and ScopedObjectAccess.
  static void ThrowNullPointerException();

  JNIEnv* env_;
  jstring string_;
  const char* utf_chars_;

  DISALLOW_COPY_AND_ASSIGN(ScopedUtfChars);
};


}  // namespace art

#endif  // ART_RUNTIME_UTILS_SCOPED_UTF_CHARS_H_
