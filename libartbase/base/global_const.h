/*
 * Copyright (C) 2023 The Android Open Source Project
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

#ifndef ART_LIBARTBASE_BASE_GLOBAL_CONST_H_
#define ART_LIBARTBASE_BASE_GLOBAL_CONST_H_

namespace art {

// Helper class that acts as a global constant which can be initialized with
// a dynamically computed value while not being subject to static initialization
// order issues via gating access to the value through a function which ensures
// the value is initialized before being accessed.
//
// The Initialize function should return T type. It shouldn't have side effects
// and should always return the same value.
template<typename T, auto Initialize>
struct GlobalConst {
  operator T() const {
    static T data = Initialize();
    return data;
  }
};

}  // namespace art

#endif  // ART_LIBARTBASE_BASE_GLOBAL_CONST_H_
