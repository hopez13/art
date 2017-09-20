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

#ifndef ART_RUNTIME_UTILS_SCOPED_LOCAL_REF_H_
#define ART_RUNTIME_UTILS_SCOPED_LOCAL_REF_H_

#include <cstddef>

#include <jni.h>

#include "android-base/macros.h"

namespace art {

template <typename T>
class ScopedLocalRef {
 public:
  ScopedLocalRef(JNIEnv* env, T local_ref) : env_(env), local_ref_(local_ref) {
  }

  ScopedLocalRef(ScopedLocalRef&& s) : env_(s.env_), local_ref_(s.release()) {
  }

  ~ScopedLocalRef() {
    reset();
  }

  void reset(T ptr = nullptr) {
    if (ptr != local_ref_) {
      if (local_ref_ != nullptr) {
        env_->DeleteLocalRef(local_ref_);
      }
      local_ref_ = ptr;
    }
  }

  T release() {
    T localRef = local_ref_;
    local_ref_ = nullptr;
    return localRef;
  }

  T get() const {
    return local_ref_;
  }

  ScopedLocalRef& operator=(ScopedLocalRef&& s) {
    reset(s.release());
    env_ = s.env_;
  }

  bool operator==(std::nullptr_t) const {
    return local_ref_ == nullptr;
  }

  bool operator!=(std::nullptr_t) const {
    return local_ref_ != nullptr;
  }

 private:
  JNIEnv* const env_;
  T local_ref_;

  DISALLOW_COPY_AND_ASSIGN(ScopedLocalRef);
};

}  // namespace art

#endif  // ART_RUNTIME_UTILS_SCOPED_LOCAL_REF_H_
