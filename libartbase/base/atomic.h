/*
 * Copyright (C) 2008 The Android Open Source Project
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

#ifndef ART_LIBARTBASE_BASE_ATOMIC_H_
#define ART_LIBARTBASE_BASE_ATOMIC_H_

#include <stdint.h>
#include <atomic>
#include <limits>
#include <vector>

#include <android-base/logging.h>

#include "base/macros.h"

namespace art {

template<typename T>
class PACKED(sizeof(T)) Atomic : public std::atomic<T> {
 public:
  Atomic<T>() : std::atomic<T>(T()) { }

  explicit Atomic<T>(T value) : std::atomic<T>(value) { }

  // Word tearing allowed, but may race.
  // TODO: Optimize?
  // There has been some discussion of eventually disallowing word
  // tearing for Java data loads.
  T LoadJavaData() const {
    return this->load(std::memory_order_relaxed);
  }

  // Word tearing allowed, but may race.
  void StoreJavaData(T desired_value) {
    this->store(desired_value, std::memory_order_relaxed);
  }

  volatile T* Address() {
    return reinterpret_cast<T*>(this);
  }

  static T MaxValue() {
    return std::numeric_limits<T>::max();
  }
};

typedef Atomic<int32_t> AtomicInteger;

static_assert(sizeof(AtomicInteger) == sizeof(int32_t), "Weird AtomicInteger size");
static_assert(alignof(AtomicInteger) == alignof(int32_t),
              "AtomicInteger alignment differs from that of underlyingtype");
static_assert(sizeof(Atomic<int64_t>) == sizeof(int64_t), "Weird Atomic<int64> size");

// Assert the alignment of 64-bit integers is 64-bit. This isn't true on certain 32-bit
// architectures (e.g. x86-32) but we know that 64-bit integers here are arranged to be 8-byte
// aligned.
#if defined(__LP64__)
  static_assert(alignof(Atomic<int64_t>) == alignof(int64_t),
                "Atomic<int64> alignment differs from that of underlying type");
#endif

}  // namespace art

#endif  // ART_LIBARTBASE_BASE_ATOMIC_H_
