/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef ART_RUNTIME_DEOPTIMIZATION_KIND_H_
#define ART_RUNTIME_DEOPTIMIZATION_KIND_H_

namespace art {

enum DeoptimizationKind {
  kAotInlineCache = 0,
  kJitInlineCache,
  kJitSameTarget,
  kBCE,
  kCHA,
  kFullFrame,
  kLast
};

inline const char* GetDeoptimizationKindName(size_t i) {
  switch (i) {
    case kAotInlineCache: return "AOT inline cache";
    case kJitInlineCache: return "JIT inline cache";
    case kJitSameTarget: return "JIT same target";
    case kBCE: return "bounds check elimination";
    case kCHA: return "class hierarchy analysis";
    case kFullFrame: return "full frame";
    default:
      LOG(FATAL) << "UNREACHABLE";
  }
  return "";
}

}  // namespace art

#endif  // ART_RUNTIME_DEOPTIMIZATION_KIND_H_
