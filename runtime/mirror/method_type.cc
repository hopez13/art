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

#include "method_type.h"

#include "class-inl.h"
#include "gc_root-inl.h"

namespace art {
namespace mirror {

GcRoot<mirror::Class> MethodType::static_class_;

mirror::MethodType* MethodType::Create(Thread* const self,
                                       Handle<Class> return_type,
                                       Handle<ObjectArray<Class>> param_types) {
  StackHandleScope<1> hs(self);
  Handle<mirror::MethodType> mt(
      hs.NewHandle(static_cast<MethodType*>(StaticClass()->AllocObject(self))));

  // TODO: Do we ever create a MethodType during a transaction ? There doesn't
  // seem like a good reason to do a polymorphic invoke that results in the
  // resolution of a method type in an unstarted runtime.
  mt->SetFieldObject<false>(FormOffset(), nullptr);
  mt->SetFieldObject<false>(MethodDescriptorOffset(), nullptr);
  mt->SetFieldObject<false>(RTypeOffset(), return_type.Get());
  mt->SetFieldObject<false>(PTypesOffset(), param_types.Get());
  mt->SetFieldObject<false>(WrapAltOffset(), nullptr);

  return mt.Get();
}



void MethodType::SetClass(Class* klass) {
  CHECK(static_class_.IsNull()) << static_class_.Read() << " " << klass;
  CHECK(klass != nullptr);
  static_class_ = GcRoot<Class>(klass);
}

void MethodType::ResetClass() {
  CHECK(!static_class_.IsNull());
  static_class_ = GcRoot<Class>(nullptr);
}

void MethodType::VisitRoots(RootVisitor* visitor) {
  static_class_.VisitRootIfNonNull(visitor, RootInfo(kRootStickyClass));
}

}  // namespace mirror
}  // namespace art
