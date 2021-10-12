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
inline uintptr_t MarkCompact::LiveWordsBitmap<kAlignment>::SetLiveWords(uintptr_t begin,
                                                                        size_t size) {
  const uintptr_t begin_bit_idx = BitIndexFromAddr(begin);
  DCHECK(!TestBit(begin_bit_idx));
  // The last bit to set.
  const uintptr_t end_bit_idx = BitIndexFromAddr(begin + size) - 1;
  uintptr_t* address = &bitmap_begin_[BitIndexToWordIndex(begin_bit_idx)];
  const uintptr_t* end_address = &bitmap_begin_[BitIndexToWordIndex(end_bit_idx)];
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

template <size_t kAlignment>
inline
size_t MarkCompact::LiveWordsBitmap<kAlignment>::FindNthLiveWordOffset(size_t offset_vec_idx,
                                                                       size_t n) {
  DCHECK_LT(n, kBitsPerVectorWord);
  const size_t index = offset_vec_idx * kBitmapWordsPerVectorWord;
  for (size_t i = 0; i < kBitmapWordsPerVectorWord; i++) {
    uintptr_t word = bitmap_begin_[index + i];
    if (~word == 0) {
      if (n <= kBitsPerBitmapWord) {
        return i * kBitsPerBitmapWord + n;
      }
      n -= kBitsPerBitmapWord;
      continue;
    }
    size_t j = 0;
    while (word != 0) {
      // count contiguous 0s
      size_t shift = CTZ(word);
      word >>= shift;
      j += shift;
      // count contiguous 1s
      shift = CTZ(~word);
      DCHECK_NE(shift, 0);
      if (shift >= n) {
        return i * kBitsPerBitmapWord + j + n;
      }
      n -= shift;
      word >>= shift;
      j += shift;
    }
  }
  UNREACHABLE();
  return kBitsPerVectorWord;
}

// TODO: a function for the read-barrier to return the from-space address of a
// given pre-compact addr.
inline void MarkCompact::UpdateRef(mirror::Object* obj, MemberOffset offset) {
  mirror::Object* old_ref = obj->GetFieldObject<
      mirror::Object, kVerifyNone, kWithoutReadBarrier, /*kIsVolatile*/false>(offset);
  mirror::Object* new_ref = PostCompactAddress(old_ref);
  if (new_ref != old_ref) {
    obj->SetFieldObjectWithoutWriteBarrier<
        /*kTransactionActive*/false, /*kCheckTransaction*/false, kVerifyNone, /*kIsVolatile*/false>(
            offset,
            new_ref);
  }
}

inline void MarkCompact::UpdateRoot(mirror::CompressedReference<mirror::Object>* root) {
  DCHECK(!root->IsNull());
  mirror::Object* old_ref = root->AsMirrotPtr();
  mirror::Object* new_ref = PostCompactAddress(old_ref);
  if (old_ref != new_ref) {
    root->Assign(new_ref);
  }
}

template <size_t kAlignment>
inline size_t MarkCompact::LiveWordsBitmap<kAlignment>::CountLiveWordsUpto(size_t bit_idx) const {
  const size_t word_offset = BitIndexToWordIndex(bit_idx);
  uintptr_t word;
  size_t ret = 0;
  // This is needed only if we decide to make offset_vector chunks 128-bit but
  // still choose to use 64-bit word for bitmap. Ideally we should use 128-bit
  // SIMD instructions to compute popcount.
  if (kBitmapWordsPerVectorWord > 1) {
    size_t remainder = word_offset % kBitmapWordsPerVectorWord;
    while (remainder > 0) {
      word = bitmap_begin_[word_offset - remainder];
      ret += POPCOUNT(word);
      remainder--;
    }
  }
  word = bitmap_begin_[word_offset];
  const uintptr_t mask = BitIndexToMask(bit_idx);
  DCHECK_NE(word & mask, 0);
  ret += POPCOUNT(word & (mask - 1));
  return ret;
}

inline mirror::Object* MarkCompact::PostCompactAddress(mirror::Object* old_ref) const {
  // TODO: To further speedup the check, maybe we should consider caching heap
  // start/end in this object.
  if (LIKELY(live_words_bitmap_->HasAddress(old_ref))) {
    const uintptr_t begin = live_words_bitmap_->Begin();
    const uintptr_t addr_offset = reinterpret_cast<uintptr_t>(obj) - begin;
    const size_t vec_idx = addr_offset / kOffsetChunkSize;
    const size_t live_bytes_in_bitmap_word =
        live_words_bitmap_->CountLiveWordsUpto(addr_offset / kAlignment) * kAlignment;
    return reinterpret_cast<mirror::Object*>(begin
                                             + offset_vector_[vec_dex]
                                             + count_within_bitmap_word);
  } else {
    return old_ref;
  }
}

}  // namespace collector
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_COLLECTOR_MARK_COMPACT_INL_H_
