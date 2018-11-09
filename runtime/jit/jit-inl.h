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

inline void Jit::AddSamples(Thread* self,
                            ArtMethod* method,
                            uint16_t samples,
                            bool with_backedges) {
  // The full check is fairly expensive so we just add to hotness most of the time,
  // and we do the full check only occasionally (following an exponential back-off).
  // Specifically, only when there is a change in the highest-set bits of the counter.
  // This has the same effect as rounding the thresholds to floats with n-bit mantissa.
  // TODO: Simplify the sample accounting logic so tricks like this are not needed.
  constexpr uint32_t kNumBits = 5;  // The number of most significant bits to check.
  uint32_t old_count = method->GetCounter();
  uint32_t new_count = old_count + samples;
  uint32_t ignore_mask = MaxInt<uint32_t>(MinimumBitsToStore(old_count)) >> kNumBits;
  if (((old_count ^ new_count) & ~ignore_mask) == 0) {
    method->SetCounter(new_count);
    return;
  }
  AddSamplesImpl(self, method, samples, with_backedges);
}

}  // namespace jit
}  // namespace art

#endif  // ART_RUNTIME_JIT_JIT_INL_H_
