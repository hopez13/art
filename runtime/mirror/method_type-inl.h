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

template <typename PTypesType>
inline MethodType::PTypesAccessor<PTypesType>::PTypesAccessor(PTypesType p_types)
    : p_types_(p_types) {}

template <typename PTypesType>
inline int32_t MethodType::PTypesAccessor<PTypesType>::GetLength() const {
  return p_types_->GetLength();
}

template <typename PTypesType>
inline ObjPtr<mirror::Class> MethodType::PTypesAccessor<PTypesType>::Get(int32_t i) const {
  DCHECK_LT(i, GetLength());
  return p_types_->GetWithoutChecks(i);
}

inline MethodType::RawPTypesAccessor::RawPTypesAccessor(
    VariableSizedHandleScope* method_type)
    : method_type_(method_type) {
  DCHECK_NE(method_type->Size(), 0u);
}

inline int32_t MethodType::RawPTypesAccessor::GetLength() const {
  return method_type_->Size() - 1u;
}

inline ObjPtr<mirror::Class> MethodType::RawPTypesAccessor::Get(int32_t i) const {
  DCHECK_LT(i, GetLength());
  return method_type_->GetHandle<mirror::Class>(i + 1).Get();
}

template <typename HandleScopeType>
inline MethodType::HandlePTypesAccessor MethodType::NewHandlePTypes(
    Handle<MethodType> method_type, HandleScopeType* hs) {
  Handle<ObjectArray<mirror::Class>> p_types = hs->NewHandle(method_type->GetPTypes());
  return HandlePTypesAccessor(p_types);
}

template <typename HandleScopeType>
inline MethodType::RawPTypesAccessor MethodType::NewHandlePTypes(
    VariableSizedHandleScope* method_type, [[maybe_unused]] HandleScopeType* other_hs) {
  return RawPTypesAccessor(method_type);
}

inline MethodType::ObjPtrPTypesAccessor MethodType::GetPTypes(ObjPtr<MethodType> method_type) {
  return ObjPtrPTypesAccessor(method_type->GetPTypes());
}

inline MethodType::ObjPtrPTypesAccessor MethodType::GetPTypes(Handle<MethodType> method_type) {
  return GetPTypes(method_type.Get());
}

inline MethodType::RawPTypesAccessor MethodType::GetPTypes(VariableSizedHandleScope* method_type) {
  return RawPTypesAccessor(method_type);
}

inline ObjPtr<mirror::Class> MethodType::GetRType(ObjPtr<MethodType> method_type) {
  return method_type->GetRType();
}

inline ObjPtr<mirror::Class> MethodType::GetRType(Handle<MethodType> method_type) {
  return GetRType(method_type.Get());
}

inline ObjPtr<mirror::Class> MethodType::GetRType(VariableSizedHandleScope* method_type) {
  DCHECK_NE(method_type->Size(), 0u);
  return method_type->GetHandle<mirror::Class>(0).Get();
}

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_METHOD_TYPE_INL_H_
