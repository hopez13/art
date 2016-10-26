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

#ifndef ART_RUNTIME_METHOD_HANDLES_INL_H_
#define ART_RUNTIME_METHOD_HANDLES_INL_H_

#include "method_handles.h"

#include "common_throws.h"
#include "dex_instruction.h"
#include "interpreter/interpreter_common.h"
#include "jvalue.h"
#include "mirror/class.h"
#include "mirror/method_type.h"
#include "mirror/object.h"
#include "reflection.h"
#include "stack.h"

namespace art {

template <typename G, typename S>
bool PerformConversions(Thread* self,
                        Handle<mirror::MethodType> callsite_type,
                        Handle<mirror::MethodType> callee_type,
                        Handle<mirror::ObjectArray<mirror::Class>> from_types,
                        Handle<mirror::ObjectArray<mirror::Class>> to_types,
                        G* getter,
                        S* setter,
                        int32_t num_conversions) REQUIRES_SHARED(Locks::mutator_lock_) {
  StackHandleScope<2> hs(self);
  MutableHandle<mirror::Class> from(hs.NewHandle<mirror::Class>(nullptr));
  MutableHandle<mirror::Class> to(hs.NewHandle<mirror::Class>(nullptr));

  for (int32_t i = 0; i < num_conversions; ++i) {
    from.Assign(from_types->GetWithoutChecks(i));
    to.Assign(to_types->GetWithoutChecks(i));

    const Primitive::Type from_type = from->GetPrimitiveType();
    const Primitive::Type to_type = to->GetPrimitiveType();

    if (from.Get() == to.Get()) {
      // Easy case - the types are identical. Nothing left to do except to pass
      // the arguments along verbatim.
      if (Primitive::Is64BitType(from_type)) {
        setter->SetLong(getter->GetLong());
      } else if (from_type == Primitive::kPrimNot) {
        setter->SetReference(getter->GetReference());
      } else {
        setter->Set(getter->Get());
      }

      continue;
    } else {
      JValue from_value;
      JValue to_value;

      if (Primitive::Is64BitType(from_type)) {
        from_value.SetJ(getter->GetLong());
      } else if (from_type == Primitive::kPrimNot) {
        from_value.SetL(getter->GetReference());
      } else {
        from_value.SetI(getter->Get());
      }

      if (!ConvertJValue(callee_type, callsite_type, from, to, from_value, &to_value)) {
        DCHECK(self->IsExceptionPending());
        return false;
      }

      if (Primitive::Is64BitType(to_type)) {
        setter->SetLong(to_value.GetJ());
      } else if (to_type == Primitive::kPrimNot) {
        setter->SetReference(to_value.GetL());
      } else {
        setter->Set(to_value.GetI());
      }
    }
  }

  return true;
}

template <bool is_range>
bool ConvertAndCopyArgumentsFromCallerFrame(Thread* self,
                                            Handle<mirror::MethodType> callsite_type,
                                            Handle<mirror::MethodType> callee_type,
                                            const ShadowFrame& caller_frame,
                                            uint32_t first_src_reg,
                                            uint32_t first_dest_reg,
                                            const uint32_t (&arg)[Instruction::kMaxVarArgRegs],
                                            ShadowFrame* callee_frame) REQUIRES_SHARED(Locks::mutator_lock_) {
  StackHandleScope<4> hs(self);
  Handle<mirror::ObjectArray<mirror::Class>> from_types(hs.NewHandle(callsite_type->GetPTypes()));
  Handle<mirror::ObjectArray<mirror::Class>> to_types(hs.NewHandle(callee_type->GetPTypes()));

  const int32_t num_method_params = from_types->GetLength();
  if (to_types->GetLength() != num_method_params) {
    ThrowWrongMethodTypeException(callee_type.Get(), callsite_type.Get());
    return false;
  }

  ShadowFrameGetter<is_range> getter(first_src_reg, arg, caller_frame);
  ShadowFrameSetter setter(callee_frame, first_dest_reg);

  return PerformConversions<ShadowFrameGetter<is_range>, ShadowFrameSetter>(self,
                                                                            callsite_type,
                                                                            callee_type,
                                                                            from_types,
                                                                            to_types,
                                                                            &getter,
                                                                            &setter,
                                                                            num_method_params);
}

}  // namespace art

#endif  // ART_RUNTIME_METHOD_HANDLES_INL_H_
