/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef ART_RUNTIME_INTERPRETER_SHADOW_FRAME_INL_H_
#define ART_RUNTIME_INTERPRETER_SHADOW_FRAME_INL_H_

#include "shadow_frame.h"

#include "obj_ptr-inl.h"

namespace art {

template<VerifyObjectFlags kVerifyFlags /*= kDefaultVerifyFlags*/>
inline void ShadowFrame::SetVRegReference(size_t i, ObjPtr<mirror::Object> obj)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  DCHECK_LT(i, NumberOfVRegs());
  if (kVerifyFlags & kVerifyWrites) {
    VerifyObject(obj);
  }
  ReadBarrier::MaybeAssertToSpaceInvariant(obj.Ptr());
  uint32_t value = StackReference<mirror::Object>::FromMirrorPtr(obj.Ptr()).AsVRegValue();
  vregs_[i].value = value;
  vregs_[i].object = value;
}

}  // namespace art

#endif  // ART_RUNTIME_INTERPRETER_SHADOW_FRAME_INL_H_
