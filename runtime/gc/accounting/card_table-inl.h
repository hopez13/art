/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_RUNTIME_GC_ACCOUNTING_CARD_TABLE_INL_H_
#define ART_RUNTIME_GC_ACCOUNTING_CARD_TABLE_INL_H_

#include "atomic.h"
#include "base/bit_utils.h"
#include "base/logging.h"
#include "card_table.h"
#include "mem_map.h"
#include "space_bitmap.h"

namespace art {
namespace gc {
namespace accounting {

static inline bool byte_cas(uint8_t old_value, uint8_t new_value, uint8_t* address) {
  if (*address == old_value) {
    *address = new_value;
    return true;
  }
  return false;
}

template <bool kClearCard, typename Visitor>
inline size_t CardTable::Scan(ContinuousSpaceBitmap* bitmap, uint8_t* scan_begin, uint8_t* scan_end,
                              const Visitor& visitor, const uint8_t minimum_age) const {
  DCHECK_GE(scan_begin, reinterpret_cast<uint8_t*>(bitmap->HeapBegin()));
  // scan_end is the byte after the last byte we scan.
  DCHECK_LE(scan_end, reinterpret_cast<uint8_t*>(bitmap->HeapLimit()));
  uint8_t* card_cur = CardFromAddr(scan_begin);
  uint8_t* card_end = CardFromAddr(AlignUp(scan_end, kCardSize));
  CheckCardValid(card_cur);
  CheckCardValid(card_end);
  size_t cards_scanned = 0;

  // Handle any unaligned cards at the start.
  while (!IsAligned<sizeof(intptr_t)>(card_cur) && card_cur < card_end) {
    if (*card_cur >= minimum_age) {
      uintptr_t start = reinterpret_cast<uintptr_t>(AddrFromCard(card_cur));
      bitmap->VisitMarkedRange(start, start + kCardSize, visitor);
      ++cards_scanned;
      if (kClearCard) {
        *card_cur = 0;
      }
    }
    ++card_cur;
  }

  uint8_t* aligned_end = card_end -
      (reinterpret_cast<uintptr_t>(card_end) & (sizeof(uintptr_t) - 1));

  uintptr_t* word_end = reinterpret_cast<uintptr_t*>(aligned_end);
  for (uintptr_t* word_cur = reinterpret_cast<uintptr_t*>(card_cur); word_cur < word_end;
      ++word_cur) {
    while (LIKELY(*word_cur == 0)) {
      ++word_cur;
      if (UNLIKELY(word_cur >= word_end)) {
        goto exit_for;
      }
    }

    // Find the first dirty card.
    uintptr_t start_word = *word_cur;
    uintptr_t start = reinterpret_cast<uintptr_t>(AddrFromCard(reinterpret_cast<uint8_t*>(word_cur)));
    // TODO: Investigate if processing continuous runs of dirty cards with a single bitmap visit is
    // more efficient.
    for (size_t i = 0; i < sizeof(uintptr_t); ++i) {
      if (static_cast<uint8_t>(start_word) >= minimum_age) {
        auto* card = reinterpret_cast<uint8_t*>(word_cur) + i;
        DCHECK(*card == static_cast<uint8_t>(start_word) || *card == kCardDirty)
            << "card " << static_cast<size_t>(*card) << " intptr_t " << (start_word & 0xFF);
        bitmap->VisitMarkedRange(start, start + kCardSize, visitor);
        ++cards_scanned;
        if (kClearCard) {
          *card = 0;
        }
      }
      start_word >>= 8;
      start += kCardSize;
    }
  }
  exit_for:

  // Handle any unaligned cards at the end.
  card_cur = reinterpret_cast<uint8_t*>(word_end);
  while (card_cur < card_end) {
    if (*card_cur >= minimum_age) {
      uintptr_t start = reinterpret_cast<uintptr_t>(AddrFromCard(card_cur));
      bitmap->VisitMarkedRange(start, start + kCardSize, visitor);
      ++cards_scanned;
      if (kClearCard) {
        *card_cur = 0;
      }
    }
    ++card_cur;
  }

  return cards_scanned;
}

template <typename Visitor, typename ModifiedVisitor>
inline void CardTable::ModifyCardsNonAtomic(uint8_t* scan_begin, uint8_t* scan_end, const Visitor& visitor,
                                         const ModifiedVisitor& modified) {
  uint8_t* card_cur = CardFromAddr(scan_begin);
  uint8_t* card_end = CardFromAddr(AlignUp(scan_end, kCardSize));
  CheckCardValid(card_cur);
  CheckCardValid(card_end);

  while (card_cur < card_end) {
    uint8_t expected, new_value;
    do {
      expected = *card_cur;
      new_value = visitor(expected);
    } while (expected != new_value && UNLIKELY(!byte_cas(expected, new_value, card_cur)));
    if (expected != new_value) {
      modified(card_cur, expected, new_value);
    }
    ++card_cur;
  }
}

inline void* CardTable::AddrFromCard(const uint8_t *card_addr) const {
  DCHECK(IsValidCard(card_addr))
    << " card_addr: " << reinterpret_cast<const void*>(card_addr)
    << " begin: " << reinterpret_cast<void*>(mem_map_->Begin() + offset_)
    << " end: " << reinterpret_cast<void*>(mem_map_->End());
  uintptr_t offset = card_addr - biased_begin_;
  return reinterpret_cast<void*>(offset << kCardShift);
}

inline uint8_t* CardTable::CardFromAddr(const void *addr) const {
  uint8_t *card_addr = biased_begin_ + (reinterpret_cast<uintptr_t>(addr) >> kCardShift);
  // Sanity check the caller was asking for address covered by the card table
  DCHECK(IsValidCard(card_addr)) << "addr: " << addr
      << " card_addr: " << reinterpret_cast<void*>(card_addr);
  return card_addr;
}

inline bool CardTable::IsValidCard(const uint8_t* card_addr) const {
  uint8_t* begin = mem_map_->Begin() + offset_;
  uint8_t* end = mem_map_->End();
  return card_addr >= begin && card_addr < end;
}

inline void CardTable::CheckCardValid(uint8_t* card) const {
  DCHECK(IsValidCard(card))
      << " card_addr: " << reinterpret_cast<const void*>(card)
      << " begin: " << reinterpret_cast<void*>(mem_map_->Begin() + offset_)
      << " end: " << reinterpret_cast<void*>(mem_map_->End());
}

}  // namespace accounting
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_ACCOUNTING_CARD_TABLE_INL_H_
