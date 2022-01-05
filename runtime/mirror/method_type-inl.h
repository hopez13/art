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

#ifndef ART_RUNTIME_MIRROR_METHOD_TYPE_INL_H_
#define ART_RUNTIME_MIRROR_METHOD_TYPE_INL_H_

#include "method_type.h"

#include "mirror/object-inl.h"

namespace art {
namespace mirror {

inline ObjPtr<ObjectArray<Class>> MethodType::GetPTypes() {
  return GetFieldObject<ObjectArray<Class>>(OFFSET_OF_OBJECT_MEMBER(MethodType, p_types_));
}

inline int MethodType::GetNumberOfPTypes() {
  return GetPTypes()->GetLength();
}

inline ObjPtr<Class> MethodType::GetRType() {
  return GetFieldObject<Class>(OFFSET_OF_OBJECT_MEMBER(MethodType, r_type_));
}

inline bool MethodType::IsParameterMatch(ObjPtr<MethodType> target) {
    const ObjPtr<ObjectArray<Class>> p_types = GetPTypes();
  const int32_t params_length = p_types->GetLength();

  const ObjPtr<ObjectArray<Class>> target_p_types = target->GetPTypes();
  if (params_length != target_p_types->GetLength()) {
    return false;
  }
  for (int32_t i = 0; i < params_length; ++i) {
    if (p_types->GetWithoutChecks(i) != target_p_types->GetWithoutChecks(i)) {
      return false;
    }
  }
  return true;
}

inline bool MethodType::IsRTypeVoid() {
  return GetRType()->IsPrimitiveVoid();
}

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_METHOD_TYPE_INL_H_
