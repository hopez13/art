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

#ifndef ART_RUNTIME_GC_SPACE_IMAGE_SPACE_FIXUP_INL_H_
#define ART_RUNTIME_GC_SPACE_IMAGE_SPACE_FIXUP_INL_H_

#include "image_space_fixup.h"

#include "obj_ptr-inl.h"

namespace art {
namespace gc {
namespace space {

inline void ObjectFixup::FixupAllReferences(uint8_t* image_base,
                                            ObjPtr<mirror::Object> new_obj,
                                            ObjPtr<mirror::Object> expected_obj) const {
  for (size_t i = 0; i < NumHeapReferenceFixups(); ++i) {
    CHECK_EQ(GetHeapReference(image_base, i)->AsMirrorPtr(), expected_obj);
    GetHeapReference(image_base, i)->Assign(new_obj);
  }
  for (size_t i = 0; i < NumCompressedReferenceFixups(); ++i) {
    CHECK_EQ(GetCompressedReference(image_base, i)->AsMirrorPtr(), expected_obj);
    GetCompressedReference(image_base, i)->Assign(new_obj);
  }
}

inline void ObjectFixup::Dump(std::ostream& os) const {
  os << "NumHeapReferenceFixups=" << NumHeapReferenceFixups() << "\n";
  for (size_t i = 0; i < NumHeapReferenceFixups(); ++i) {
    os << "heap reference " << i << " = " << *HeapReferenceFixupOffset(i) << "\n";
  }
  os << "NumHeapReferenceFixups=" << NumCompressedReferenceFixups() << "\n";
  for (size_t i = 0; i < NumCompressedReferenceFixups(); ++i) {
    os << "heap reference " << i << " = " << *CompressedReferenceFixupOffset(i) << "\n";
  }
}

}  // namespace space
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_SPACE_IMAGE_SPACE_FIXUP_INL_H_
