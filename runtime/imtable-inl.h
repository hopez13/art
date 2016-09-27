/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef ART_RUNTIME_IMTABLE_INL_H_
#define ART_RUNTIME_IMTABLE_INL_H_

#include "imtable.h"

#include "art_method-inl.h"
#include "utf.h"

namespace art {

static constexpr bool kImTableHashUseName = true;
static constexpr bool kImTableHashUseCoefficients = true;

// Magic configuration that minimizes some common runtime calls.
static constexpr uint32_t kImTableHashCoefficient = 28388;
static constexpr uint32_t kImTableHashSummand = 1;

inline uint32_t ImTable::GetBaseImtHash(ArtMethod* method) REQUIRES_SHARED(Locks::mutator_lock_) {
  if (kImTableHashUseName) {
    return ComputeModifiedUtf8Hash(PrettyMethod(method, true).c_str());
  } else {
    return method->GetDexMethodIndex();
  }
}

inline uint32_t ImTable::GetImtIndex(ArtMethod* method) {
  if (!kImTableHashUseCoefficients) {
    return GetBaseImtHash(method) % ImTable::kSize;
  } else {
    return
        (kImTableHashCoefficient * GetBaseImtHash(method) + kImTableHashSummand) % ImTable::kSize;
  }
}

}  // namespace art

#endif  // ART_RUNTIME_IMTABLE_INL_H_

