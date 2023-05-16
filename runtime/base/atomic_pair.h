/*
 * Copyright (C) 2022 The Android Open Source Project
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

#ifndef ART_RUNTIME_BASE_ATOMIC_PAIR_H_
#define ART_RUNTIME_BASE_ATOMIC_PAIR_H_

#include <android-base/logging.h>

#include <type_traits>

#include "base/macros.h"

namespace art {

// Implement 16-byte atomic pair using the seq-lock synchronization algorithm.
//
// This uses top 2-bytes of the key as version counter / lock bit,
// which means the stored pair key can not use those bytes.
//
// The advantage of this is that the readers don't need exclusive cache line access,
// and can use lither barriers.
//
// This does not affect 8-byte atomic pair implementation.

#define ATOMIC_PAIR_USE_SEQLOCK 1

static constexpr uint64_t kSeqMask = (0xFFFFull << 48);
static constexpr uint64_t kSeqLock = (0x0001ull << 48);
static constexpr uint64_t kSeqIncr = (0x0002ull << 48);

// std::pair<> is not trivially copyable and as such it is unsuitable for atomic operations.
template <typename IntType>
struct PACKED(2 * sizeof(IntType)) AtomicPair {
  static_assert(std::is_integral_v<IntType>);

  constexpr AtomicPair() : first(0), second(0) { }
  AtomicPair(IntType f, IntType s) : first(f), second(s) { }
  AtomicPair(const AtomicPair&) = default;
  AtomicPair& operator=(const AtomicPair&) = default;

  IntType first;
  IntType second;
};

template <typename IntType>
ALWAYS_INLINE static inline AtomicPair<IntType> AtomicPairLoadAcquire(AtomicPair<IntType>* pair) {
  auto* target = reinterpret_cast<std::atomic<AtomicPair<IntType>>*>(pair);
  return target->load(std::memory_order_acquire);
}

template <typename IntType>
ALWAYS_INLINE static inline void AtomicPairStoreRelease(AtomicPair<IntType>* pair,
                                                        AtomicPair<IntType> value) {
  auto* target = reinterpret_cast<std::atomic<AtomicPair<IntType>>*>(pair);
  target->store(value, std::memory_order_release);
}

#if ATOMIC_PAIR_USE_SEQLOCK

ALWAYS_INLINE static inline AtomicPair<uint64_t> AtomicPairLoadAcquire(AtomicPair<uint64_t>* pair) {
  auto* first = reinterpret_cast<std::atomic_uint64_t*>(&pair->first);
  auto* second = reinterpret_cast<std::atomic_uint64_t*>(&pair->second);
  while (true) {
    uint64_t key0 = first->load(std::memory_order_acquire);
    uint64_t val = second->load(std::memory_order_acquire);
    uint64_t key1 = first->load(std::memory_order_acquire);
    if (LIKELY(key0 == key1 && (key0 & kSeqLock) == 0)) {
      return {key0 & ~kSeqMask, val};
    }
  }
}

ALWAYS_INLINE static inline void AtomicPairStoreRelease(AtomicPair<uint64_t>* pair,
                                                        AtomicPair<uint64_t> value) {
  auto* first = reinterpret_cast<std::atomic_uint64_t*>(&pair->first);
  auto* second = reinterpret_cast<std::atomic_uint64_t*>(&pair->second);
  DCHECK_EQ(value.first & kSeqMask, 0ull);
  uint64_t key;
  do {
    key = first->load(std::memory_order_relaxed);
  } while ((key & kSeqLock) != 0 || !first->compare_exchange_weak(key, key | kSeqLock));
  key = (value.first & ~kSeqMask) | ((key & kSeqMask) + kSeqIncr);
  second->store(value.second, std::memory_order_release);
  first->store(key, std::memory_order_release);
}

// LLVM uses generic lock-based implementation for x86_64, we can do better with CMPXCHG16B.
#elif defined(__x86_64__)
ALWAYS_INLINE static inline AtomicPair<uint64_t> AtomicPairLoadAcquire(AtomicPair<IntType>* pair) {
  auto* target = reinterpret_cast<std::atomic<AtomicPair<IntType>>*>(pair);
  uint64_t first, second;
  __asm__ __volatile__(
      "lock cmpxchg16b (%2)"
      : "=&a"(first), "=&d"(second)
      : "r"(target), "a"(0), "d"(0), "b"(0), "c"(0)
      : "cc");
  return {first, second};
}

ALWAYS_INLINE static inline void AtomicPairStoreRelease(AtomicPair<IntType>* pair,
                                                        AtomicPair<IntType> value) {
  auto* target = reinterpret_cast<std::atomic<AtomicPair<IntType>>*>(pair);
  uint64_t first, second;
  __asm__ __volatile__ (
      "movq (%2), %%rax\n\t"
      "movq 8(%2), %%rdx\n\t"
      "1:\n\t"
      "lock cmpxchg16b (%2)\n\t"
      "jnz 1b"
      : "=&a"(first), "=&d"(second)
      : "r"(target), "b"(value.first), "c"(value.second)
      : "cc");
}
#endif  // defined(__x86_64__)

}  // namespace art

#endif  // ART_RUNTIME_BASE_ATOMIC_PAIR_H_
