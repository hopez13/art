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

#ifndef ART_RUNTIME_GC_SPACE_IMAGE_SPACE_FIXUP_H_
#define ART_RUNTIME_GC_SPACE_IMAGE_SPACE_FIXUP_H_

#include <iosfwd>

#include "base/mutex.h"
#include "gc_root.h"
#include "mirror/object_reference.h"

namespace art {

namespace mirror {
class Object;
}

namespace gc {
namespace space {

class ObjectFixup {
 private:
  using FixupOffset = uint32_t;

 public:
  ObjectFixup(GcRoot<mirror::Object> object,
              uint32_t num_heap_references,
              uint32_t num_compressed_references)
      : object_(object),
        num_heap_references_(num_heap_references),
        num_compressed_references_(num_compressed_references) {}

  static size_t ComputeSize(size_t num_fixups) {
    const size_t header_size = sizeof(ObjectFixup);
    const size_t data_size = sizeof(FixupOffset) * num_fixups;
    return header_size + data_size;
  }

  size_t SizeOf() const {
    return ComputeSize(num_heap_references_ + num_compressed_references_);
  }

  FixupOffset* HeapReferenceFixupOffset(size_t index) {
    DCHECK_LE(index, NumHeapReferenceFixups());
    return GetFixupOffset(index);
  }

  FixupOffset* CompressedReferenceFixupOffset(size_t index) {
    DCHECK_LE(index, NumCompressedReferenceFixups());
    return GetFixupOffset(num_heap_references_ + index);
  }

  const FixupOffset* HeapReferenceFixupOffset(size_t index) const {
    DCHECK_LE(index, NumHeapReferenceFixups());
    return GetFixupOffset(index);
  }

  const FixupOffset* CompressedReferenceFixupOffset(size_t index) const {
    DCHECK_LE(index, NumCompressedReferenceFixups());
    return GetFixupOffset(num_heap_references_ + index);
  }

  mirror::HeapReference<mirror::Object>* GetHeapReference(uint8_t* image_base, size_t index)
      const {
    return reinterpret_cast<mirror::HeapReference<mirror::Object>*>(
        image_base + *HeapReferenceFixupOffset(index));
  }

  mirror::HeapReference<mirror::Object>* GetCompressedReference(uint8_t* image_base, size_t index)
      const {
    return reinterpret_cast<mirror::HeapReference<mirror::Object>*>(
        image_base + *CompressedReferenceFixupOffset(index));
  }

  void FixupAllReferences(uint8_t* image_base,
                          ObjPtr<mirror::Object> new_obj,
                          ObjPtr<mirror::Object> expected_obj) const
      REQUIRES_SHARED(Locks::mutator_lock_);

  size_t NumHeapReferenceFixups() const {
    return num_heap_references_;
  }

  size_t NumCompressedReferenceFixups() const {
    return num_compressed_references_;
  }

  GcRoot<mirror::Object>& Object() const {
    return object_;
  }

  void Dump(std::ostream& os) const;

 private:
  const FixupOffset* GetFixupOffset(size_t index) const {
    return &(reinterpret_cast<const FixupOffset*>(this + 1)[index]);
  }

  FixupOffset* GetFixupOffset(size_t index) {
    return &(reinterpret_cast<FixupOffset*>(this + 1)[index]);
  }

  mutable GcRoot<mirror::Object> object_;
  uint32_t num_heap_references_;
  uint32_t num_compressed_references_;
  /*
   * The actual fixups are a hidden field in the following format.
   * FixupOffset fixups_[];
   */
};

}  // namespace space
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_SPACE_IMAGE_SPACE_FIXUP_H_
