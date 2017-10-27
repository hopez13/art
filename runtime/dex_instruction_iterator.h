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

class DexInstructionPCPair {
 public:
  ALWAYS_INLINE const Instruction& Inst() const {
    return *Instruction::At(instructions_ + DexPC());
  }

  ALWAYS_INLINE const Instruction* operator->() const {
    return &Inst();
  }

  ALWAYS_INLINE uint32_t DexPC() const {
    return dex_pc_;
  }

  ALWAYS_INLINE const uint16_t* Instructions() const {
    return instructions_;
  }

 protected:
  DexInstructionPCPair() = default;
  DexInstructionPCPair(const DexInstructionPCPair&) = default;
  DexInstructionPCPair(DexInstructionPCPair&&) = default;
  DexInstructionPCPair& operator=(const DexInstructionPCPair&) = default;
  DexInstructionPCPair& operator=(DexInstructionPCPair&&) = default;

  explicit DexInstructionPCPair(const uint16_t* instructions, uint32_t dex_pc)
      : instructions_(instructions), dex_pc_(dex_pc) {}

  const uint16_t* instructions_ = nullptr;
  uint32_t dex_pc_ = 0;

  friend class DexInstructionIterator;
};

class DexInstructionIterator : public
    std::iterator<std::forward_iterator_tag, DexInstructionPCPair> {
 public:
  using value_type = std::iterator<std::forward_iterator_tag, DexInstructionPCPair>::value_type;
  using difference_type = std::iterator<std::forward_iterator_tag, value_type>::difference_type;

  DexInstructionIterator() = default;
  DexInstructionIterator(const DexInstructionIterator&) = default;
  DexInstructionIterator(DexInstructionIterator&&) = default;
  DexInstructionIterator& operator=(const DexInstructionIterator&) = default;
  DexInstructionIterator& operator=(DexInstructionIterator&&) = default;

  explicit DexInstructionIterator(const uint16_t* instructions, uint32_t dex_pc)
      : data_(instructions, dex_pc) {}

  // Value after modification.
  DexInstructionIterator& operator++() {
    data_.dex_pc_ += Inst().SizeInCodeUnits();
    return *this;
  }

  // Value before modification.
  DexInstructionIterator operator++(int) {
    DexInstructionIterator temp = *this;
    ++*this;
    return temp;
  }

  const value_type& operator*() const {
    return data_;
  }

  const Instruction* operator->() const {
    return &data_.Inst();
  }

  // Return the dex pc for the iterator.
  ALWAYS_INLINE uint32_t DexPC() const {
    return data_.DexPC();
  }

  // Return the current instruction of the iterator.
  ALWAYS_INLINE const Instruction& Inst() const {
    return data_.Inst();
  }

  const uint16_t* Instructions() const {
    return data_.Instructions();
  }

 private:
  value_type data_;

  friend class DexInstructionPCPair;
};

static ALWAYS_INLINE inline bool operator==(const DexInstructionIterator& lhs,
                                            const DexInstructionIterator& rhs) {
  DCHECK_EQ(lhs.Instructions(), rhs.Instructions())
      << "Comparing iterators from different code items";
  return lhs.DexPC() == rhs.DexPC();
}

static inline bool operator!=(const DexInstructionIterator& lhs,
                              const DexInstructionIterator& rhs) {
  return !(lhs == rhs);
}

static inline bool operator<(const DexInstructionIterator& lhs,
                             const DexInstructionIterator& rhs) {
  DCHECK_EQ(lhs.Instructions(), rhs.Instructions())
      << "Comparing iterators from different code items";
  return lhs.DexPC() < rhs.DexPC();
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
