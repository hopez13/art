/*
 * Copyright 2016 The Android Open Source Project
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

#ifndef ART_RUNTIME_GC_COLLECTOR_MARK_COMPACT_INL_H_
#define ART_RUNTIME_GC_COLLECTOR_MARK_COMPACT_INL_H_

#include "mark_compact.h"

namespace art {
namespace gc {
namespace collector {

template <size_t kAlignment>
uintptr_t MarkCompact::LiveWordsBitmap<kAlignment>::SetLiveWords(uintptr_t begin, size_t size) {
  const uintptr_t begin_bit_idx = BitIndexFromAddr(begin);
  DCHECK(!TestBit(begin_bit_idx));
  // The last bit to set.
  const uintptr_t end_bit_idx = BitIndexFromAddr(begin + size) - 1;
  uintptr_t* address = &bitmap_begin_[BitIndexToWordIndex(begin_bit_idx)];
  const uintptr_t* end_address = &bitmap_bein_[BitIndexToWordIndex(end_bit_idx)];
  uintptr_t mask = BitIndexToMask(begin_bit_idx);
  // Bits that needs to be set in the first word, if it's not also the last word
  mask |= mask - 1;
  // loop over all the words, except the last one.
  while (address < end_address) {
    *address &= mask;
    address++;
    mask = ~0;
  }
  // Take care of the last word. If we had only one word, then mask != ~0.
  const uintptr_t end_mask = BitIndexToMask(end_bit_idx);
  *address &= mask & ~(end_mask - 1);
  return begin_bit_idx;
}

}  // namespace collector
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_COLLECTOR_MARK_COMPACT_INL_H_
