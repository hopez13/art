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

#ifndef ART_RUNTIME_GC_SPACE_IMAGE_SPACE_INL_H_
#define ART_RUNTIME_GC_SPACE_IMAGE_SPACE_INL_H_

#include "image_space.h"

#include "image_space_fixup-inl.h"

namespace art {
namespace gc {
namespace space {

template <typename FixupVisitor>
inline void ImageSpace::VisitFixups(const FixupVisitor& visitor) {
  const ImageSection& section = GetImageHeader().GetImageSection(ImageHeader::kSectionObjectFixups);
  // Copy and write fixups last since they get offsets when the other sections get copied.
  uint8_t* ptr = Begin() + section.Offset();
  const uint8_t* end = Begin() + section.End();
  while (ptr < end) {
    const gc::space::ObjectFixup* fixup = reinterpret_cast<const gc::space::ObjectFixup*>(ptr);
    visitor(*fixup);
    const size_t fixup_size = fixup->SizeOf();
    ptr += fixup_size;
  }
  DCHECK_EQ(ptr, end);
}

}  // namespace space
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_SPACE_IMAGE_SPACE_INL_H_
