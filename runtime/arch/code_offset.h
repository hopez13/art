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

#ifndef ART_RUNTIME_ARCH_CODE_OFFSET_H_
#define ART_RUNTIME_ARCH_CODE_OFFSET_H_

#include <iosfwd>

#include "base/bit_utils.h"
#include "base/logging.h"
#include "instruction_set.h"

namespace art {

class CodeOffset {
 public:
  ALWAYS_INLINE static CodeOffset FromOffset(uint32_t offset, InstructionSet isa = kRuntimeISA) {
    switch (isa) {
      case kThumb2:
        FALLTHROUGH_INTENDED;
      case kArm:
        return CodeOffset(Compress<kArmInstructionAlignment>(offset));
      case kArm64:
        return CodeOffset(Compress<kArm64InstructionAlignment>(offset));
      case kX86:
        return CodeOffset(Compress<kX86InstructionAlignment>(offset));
      case kX86_64:
        return CodeOffset(Compress<kX86_64InstructionAlignment>(offset));
      case kMips:
        return CodeOffset(Compress<kMipsInstructionAlignment>(offset));
      case kMips64:
        return CodeOffset(Compress<kMips64InstructionAlignment>(offset));
      case kNone:
        LOG(FATAL) << "Invalid ISA";
        break;
    }
    UNREACHABLE();
  }

  ALWAYS_INLINE static CodeOffset FromCompressedOffset(uint32_t offset) {
    return CodeOffset(offset);
  }

  ALWAYS_INLINE uint32_t Uint32Value(InstructionSet isa = kRuntimeISA) const {
    switch (isa) {
      case kThumb2:
        FALLTHROUGH_INTENDED;
      case kArm:
        return kArmInstructionAlignment * value_;
      case kArm64:
        return kArm64InstructionAlignment * value_;
      case kX86:
        return kX86InstructionAlignment * value_;
      case kX86_64:
        return kX86_64InstructionAlignment * value_;
      case kMips:
        return kMipsInstructionAlignment * value_;
      case kMips64:
        return kMips64InstructionAlignment * value_;
      case kNone:
        LOG(FATAL) << "Invalid ISA";
        break;
    }
    UNREACHABLE();
  }

  // Return compressed internal value.
  ALWAYS_INLINE uint32_t CompressedValue() const {
    return value_;
  }

  ALWAYS_INLINE CodeOffset() = default;
  ALWAYS_INLINE CodeOffset(const CodeOffset&) = default;
  ALWAYS_INLINE CodeOffset& operator=(const CodeOffset&) = default;
  ALWAYS_INLINE CodeOffset& operator=(CodeOffset&&) = default;

 private:
  template <uint32_t kAlignment>
  ALWAYS_INLINE static uint32_t Compress(uint32_t offset) {
    DCHECK_ALIGNED(offset, kAlignment);
    return offset / kAlignment;
  }

  ALWAYS_INLINE explicit CodeOffset(uint32_t value) : value_(value) {}

  uint32_t value_ = 0u;
};

inline bool operator==(const CodeOffset& a, const CodeOffset& b) {
  return a.CompressedValue() == b.CompressedValue();
}

inline bool operator!=(const CodeOffset& a, const CodeOffset& b) {
  return !(a == b);
}

inline bool operator<(const CodeOffset& a, const CodeOffset& b) {
  return a.CompressedValue() < b.CompressedValue();
}

inline bool operator<=(const CodeOffset& a, const CodeOffset& b) {
  return a.CompressedValue() <= b.CompressedValue();
}

inline bool operator>(const CodeOffset& a, const CodeOffset& b) {
  return a.CompressedValue() > b.CompressedValue();
}

inline bool operator>=(const CodeOffset& a, const CodeOffset& b) {
  return a.CompressedValue() >= b.CompressedValue();
}

inline std::ostream& operator<<(std::ostream& os, const CodeOffset& offset) {
  return os << offset.Uint32Value();
}

}  // namespace art

#endif  // ART_RUNTIME_ARCH_CODE_OFFSET_H_
