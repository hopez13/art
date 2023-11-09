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

#include "handle_scope-inl.h"
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

template <typename PTypesType, typename MethodTypeType>
inline MethodType::MethodTypeAccessor<PTypesType, MethodTypeType>::MethodTypeAccessor(
    PTypesType p_types, MethodTypeType method_type)
    : p_types_(p_types), method_type_(method_type) {}

template <typename PTypesType, typename MethodTypeType>
inline int32_t MethodType::MethodTypeAccessor<PTypesType, MethodTypeType>::GetPTypesLength() const {
  return p_types_->GetLength();
}

template <typename PTypesType, typename MethodTypeType>
inline ObjPtr<mirror::Class> MethodType::MethodTypeAccessor<PTypesType, MethodTypeType>::GetPType(
    int32_t i) const {
  DCHECK_LT(i, GetPTypesLength());
  return p_types_->GetWithoutChecks(i);
}

template <typename PTypesType, typename MethodTypeType>
inline
ObjPtr<mirror::Class> MethodType::MethodTypeAccessor<PTypesType, MethodTypeType>::GetRType() const {
  return method_type_->GetRType();
}

inline MethodType::RawMethodTypeAccessor::RawMethodTypeAccessor(
    VariableSizedHandleScope* method_type)
    : method_type_(method_type) {
  DCHECK_NE(method_type->Size(), 0u);
}

inline int32_t MethodType::RawMethodTypeAccessor::GetPTypesLength() const {
  return method_type_->Size() - 1u;
}

inline ObjPtr<mirror::Class> MethodType::RawMethodTypeAccessor::GetPType(int32_t i) const {
  DCHECK_LT(i, GetPTypesLength());
  return method_type_->GetHandle<mirror::Class>(i + 1).Get();
}

inline ObjPtr<mirror::Class> MethodType::RawMethodTypeAccessor::GetRType() const {
  DCHECK_NE(method_type_->Size(), 0u);
  return method_type_->GetHandle<mirror::Class>(0).Get();
}

template <typename HandleScopeType>
inline MethodType::HandleMethodTypeAccessor MethodType::NewHandleAccessor(
    Handle<MethodType> method_type, HandleScopeType* hs) {
  Handle<ObjectArray<mirror::Class>> p_types = hs->NewHandle(method_type->GetPTypes());
  return HandleMethodTypeAccessor(p_types, method_type);
}

template <typename HandleScopeType>
inline MethodType::RawMethodTypeAccessor MethodType::NewHandleAccessor(
    VariableSizedHandleScope* method_type, [[maybe_unused]] HandleScopeType* hs) {
  return RawMethodTypeAccessor(method_type);
}

inline MethodType::ObjPtrMethodTypeAccessor MethodType::GetAccessor(
    ObjPtr<MethodType> method_type) {
  return ObjPtrMethodTypeAccessor(method_type->GetPTypes(), method_type);
}

inline MethodType::ObjPtrMethodTypeAccessor MethodType::GetAccessor(
    Handle<MethodType> method_type) {
  return GetAccessor(method_type.Get());
}

inline MethodType::RawMethodTypeAccessor MethodType::GetAccessor(VariableSizedHandleScope* method_type) {
  return RawMethodTypeAccessor(method_type);
}

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_METHOD_TYPE_INL_H_
