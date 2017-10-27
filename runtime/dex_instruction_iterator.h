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

#ifndef ART_RUNTIME_DEX_INSTRUCTION_ITERATOR_H_
#define ART_RUNTIME_DEX_INSTRUCTION_ITERATOR_H_

#include <iterator>

#include "dex_instruction.h"
#include "base/logging.h"

namespace art {

class DexInstructionIterator : public std::iterator<std::forward_iterator_tag, Instruction> {
 public:
  using value_type = std::iterator<std::forward_iterator_tag, Instruction>::value_type;
  using difference_type = std::iterator<std::forward_iterator_tag, value_type>::difference_type;

  DexInstructionIterator() = default;
  DexInstructionIterator(const DexInstructionIterator&) = default;
  DexInstructionIterator(DexInstructionIterator&&) = default;
  DexInstructionIterator& operator=(const DexInstructionIterator&) = default;
  DexInstructionIterator& operator=(DexInstructionIterator&&) = default;

  explicit DexInstructionIterator(const uint16_t* instructions,
                                  uint32_t max_dex_pc,
                                  uint32_t dex_pc = 0)
      : instructions_(instructions),
        max_dex_pc_(max_dex_pc),
        dex_pc_(dex_pc) {
    if (dex_pc_ >= max_dex_pc_) {
      // Went are at the end, mark as the special end iterator.
      MarkAsEndIterator();
    }
  }
  explicit DexInstructionIterator(const value_type* instructions,
                                  uint32_t max_dex_pc,
                                  uint32_t dex_pc = 0)
      : DexInstructionIterator(reinterpret_cast<const uint16_t*>(instructions),
                               max_dex_pc,
                               dex_pc) {}

  static DexInstructionIterator MakeEndIterator(const uint16_t* instructions) {
    return DexInstructionIterator(instructions, kEndIteratorDexPC, kEndIteratorDexPC);
  }

  // Value after modification.
  DexInstructionIterator& operator++() {
    DCHECK_NE(dex_pc_, kEndIteratorDexPC);
    dex_pc_ += Inst()->SizeInCodeUnits();
    if (dex_pc_ >= max_dex_pc_) {
      // Went are at the end, mark as the special end iterator.
      MarkAsEndIterator();
    }
    return *this;
  }

  // Value before modification.
  DexInstructionIterator operator++(int) {
    DexInstructionIterator temp = *this;
    ++*this;
    return temp;
  }

  const value_type& operator*() const {
    return *Inst();
  }

  const value_type* operator->() const {
    return &**this;
  }

  // Return the dex pc for the iterator.
  ALWAYS_INLINE uint32_t GetDexPC() const {
    return dex_pc_;
  }

  ALWAYS_INLINE const value_type* Inst() const {
    return Instruction::At(instructions_ + GetDexPC());
  }

  const uint16_t* Instructions() const {
    return instructions_;
  }

 private:
  static constexpr uint32_t kEndIteratorDexPC = std::numeric_limits<uint32_t>::max();

  const uint16_t* instructions_ = nullptr;
  uint32_t max_dex_pc_ = kEndIteratorDexPC;
  uint32_t dex_pc_ = kEndIteratorDexPC;

  void MarkAsEndIterator() {
    dex_pc_ = kEndIteratorDexPC;
  }
};

static ALWAYS_INLINE inline bool operator==(const DexInstructionIterator& lhs,
                                            const DexInstructionIterator& rhs) {
  DCHECK_EQ(lhs.Instructions(), rhs.Instructions())
      << "Comparing iterators from different code items";
  return lhs.GetDexPC() == rhs.GetDexPC();
}

static inline bool operator!=(const DexInstructionIterator& lhs,
                              const DexInstructionIterator& rhs) {
  return !(lhs == rhs);
}

static inline bool operator<(const DexInstructionIterator& lhs,
                             const DexInstructionIterator& rhs) {
  DCHECK_EQ(lhs.Instructions(), rhs.Instructions())
      << "Comparing iterators from different code items";
  return lhs.GetDexPC() < rhs.GetDexPC();
}

static inline bool operator>(const DexInstructionIterator& lhs,
                             const DexInstructionIterator& rhs) {
  return rhs < lhs;
}

static inline bool operator<=(const DexInstructionIterator& lhs,
                              const DexInstructionIterator& rhs) {
  return !(rhs < lhs);
}

static inline bool operator>=(const DexInstructionIterator& lhs,
                              const DexInstructionIterator& rhs) {
  return !(lhs < rhs);
}

}  // namespace art

#endif  // ART_RUNTIME_DEX_INSTRUCTION_ITERATOR_H_
