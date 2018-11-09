/*
 * Copyright 2018 The Android Open Source Project
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

#ifndef ART_RUNTIME_JIT_JIT_INL_H_
#define ART_RUNTIME_JIT_JIT_INL_H_

#include "jit/jit.h"

#include "art_method.h"
#include "base/bit_utils.h"
#include "thread.h"

namespace art {
namespace jit {

// We add samples in batches to amortize the cost of checking the thresholds.
//
// Given a sample count, this method returns [start,end) range of the enclosing batch.
//
// The batches are small initially, so we check often, but they grow exponentially.
// For example, the sequence of batches for 'n=1' would be:
// [0,1) [1,2) [2,4) [4,8) [8,16) [16,32) [32,64) [64,128) ...
//
inline ALWAYS_INLINE std::pair<size_t, size_t> GetBatchRange(size_t count) {
  constexpr uint32_t kNumBits = 5;
  uint32_t mask = MaxInt<uint32_t>(MinimumBitsToStore(count)) >> kNumBits;
  uint32_t begin = count & ~mask;
  uint32_t end = (count | mask) + 1;
  DCHECK_GE(count, begin);
  DCHECK_LT(count, end);
  return std::make_pair(begin, end);
}

// Returns how many samples we should accumulate before calling AddSamples.
//
// Ideally, this would calculate the exact count remaining to the next compiler threshold.
// However, that would be expensive, so we use simple exponential back-off policy instead.
//
// For low counts this returns 1 (i.e. do the full check every sample), but the period
// increases as the count increases.  We still do the full check reasonably often.
// Specifically, when there is a change in the 'n' highest-set bits of the counter.
//
// This has the same effect as rounding the thresholds to floats with n-bit mantissa.
//
inline size_t Jit::GetHotnessCountdown(size_t count) {
  return GetBatchRange(count).second /*end*/ - count;
}

inline void Jit::AddSamples(Thread* self,
                            ArtMethod* method,
                            uint16_t samples,
                            bool with_backedges) {
  uint32_t old_count = method->GetCounter();
  uint32_t new_count = old_count + samples;
  std::pair<size_t, size_t> batch = GetBatchRange(old_count);
  if (new_count < batch.second /*end*/) {
    DCHECK(GetBatchRange(new_count) == batch);
    method->SetCounter(new_count);
    return;
  }
  AddSamplesImpl(self, method, batch.first, new_count - batch.first, with_backedges);
}

}  // namespace jit
}  // namespace art

#endif  // ART_RUNTIME_JIT_JIT_INL_H_
