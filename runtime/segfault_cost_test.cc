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

#include <chrono>
#include <iostream>
#include <cstring>
#include <cassert>
#include <array>
#include <atomic>
#include <random>

#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */

#include "cutils/ashmem.h"
#include "gtest/gtest.h"

namespace art {
namespace runtime {
volatile size_t *unprotected_to = nullptr, *to = nullptr, *from = nullptr;
int shared_fd = 0;
int page_sz = 0, from_wc = 0, from_pg_count = 0;
// This will support maximum 4K source pages.
std::array<size_t, 512*4096/64> bset = { 0 };

constexpr uint8_t bits_per_value = sizeof(size_t) * 8;
constexpr uint8_t value_log_bits = 6;

constexpr size_t compute_bset_index(const size_t word) {
  return word >> value_log_bits;
}

constexpr size_t compute_bit_number(const size_t word) {
  return word & (bits_per_value - 1);
}

constexpr size_t construct_bitmap_word(const size_t bit) {
  return size_t(1) << ((bits_per_value - 1) - bit);
}

void set_bit(const size_t idx) {
  bset[compute_bset_index(idx)] |= construct_bitmap_word(compute_bit_number(idx));
}

constexpr size_t construct_left_mask(const size_t bit) {
  return size_t(-1) >> bit;
}

size_t find_next_set_bit(const size_t word) {
  size_t bit = compute_bit_number(word);
  size_t idx = compute_bset_index(word);
  size_t end_idx = compute_bset_index(from_wc);
  if (idx >= end_idx) {
    return idx << value_log_bits;
  }
  size_t B = bset[idx] & construct_left_mask(bit);
  while (B == 0) {
    idx++;
    if (idx == end_idx) {
      return idx << value_log_bits;
    }
    B = bset[idx];
  }
  return (idx << value_log_bits) + __builtin_clzl(B);
}

void cache_flush(size_t *p, size_t size) {
  assert(p);
  const size_t wc = size >> 3;
#ifdef __aarch64__
  uint64_t ctr_el0;
  asm volatile ("mrs %0, ctr_el0" : "=r"(ctr_el0));
  const size_t dcache_line_size = (ctr_el0 >> 16) & 15;
#else
  const size_t dcache_line_size = 8;
#endif
  for (size_t i = 0; i < wc; i += dcache_line_size) {
#ifdef __aarch64__
    asm volatile ("dc civac, %0" :: "r"(p + i));
#elif __x86_64__ || __i386__
    asm volatile ("clflush (%0)" :: "r"(p + i));
#endif
  }
}

void cache_flush() {
  int i = 0;
#ifdef __aarch64__
  uint64_t ctr_el0;
  asm volatile ("mrs %0, ctr_el0" : "=r"(ctr_el0));
  const size_t dcache_line_size = (ctr_el0 >> 16) & 15;
#else
  const size_t dcache_line_size = 8;
#endif
  i = find_next_set_bit(i);
  while (i < from_wc) {
#ifdef __aarch64__
    asm volatile ("dc civac, %0" :: "r"(from + i));
#elif __x86_64__ || __i386__
    asm volatile ("clflush (%0)" :: "r"(from + i));
#endif
    if ((i / dcache_line_size) < ((i + 8) / dcache_line_size)) {
#ifdef __aarch64__
      asm volatile ("dc civac, %0" :: "r"(from + i + 8));
#elif __x86_64__ || __i386__
      asm volatile ("clflush (%0)" :: "r"(from + i + 8));
#endif
    }
    i = find_next_set_bit(i + 8);
  }
}

void sanity_check() {
  // 3 bits for bytes to words and 3 for 8-word object size.
  const size_t nr_objs = page_sz >> 6;
  for (volatile size_t i = 0; i < nr_objs; i++) {
    for (volatile int j = i * 8, k = 0; k < 8; k++, j++) {
      assert(to[j] == i);
    }
  }
}

void populate_from_space() {
  /* For simplicity, each object is 8-word long. The following code places every
   * object randomly in a N-object long window, where N is the number of source
   * pages.
   */
  std::random_device rd("/dev/urandom");
  std::mt19937 gen(rd());
  const int remaining_words = (from_pg_count - 1) * 8;
  const int nr_objs = page_sz >> 6;
  std::uniform_int_distribution<> dis(0, remaining_words);
  for (int idx = 0, obj = 0; obj < nr_objs; obj++) {
    int seek = dis(gen);
    idx += seek;
    set_bit(idx);
    for (int j = 0; j < 8; j++) {
      from[idx++] = obj;
    }
    idx += remaining_words - seek;
  }
}

void segfault_hdl(int sig, siginfo_t *siginfo, void *context) {
  assert(sig == SIGSEGV && siginfo && context);
  int k = 0, i = 0;
  do {
    i = find_next_set_bit(i);
    if (i == from_wc) {
      break;
    }
    for (int j = 0; j < 8; j++) {
      unprotected_to[k++] = from[i++];
    }
  } while (true);
  assert(k == page_sz / 8);
  mprotect(const_cast<size_t*>(to), page_sz, PROT_READ | PROT_WRITE);
}

int runTest(const int src_pg_count, const size_t nr_iter) {
  std::chrono::steady_clock::time_point startBench, endBench;
  struct sigaction act;

  page_sz = getpagesize();
  from_pg_count = src_pg_count;
  from_wc = page_sz * from_pg_count / 8;

  shared_fd = ashmem_create_region("segfault-cost", page_sz);
  if (shared_fd == -1) {
    std::cerr << "Couldn't create shared memory object\n";
    return -1;
  }

  // Install segfault handler
  std::memset(&act, '\0', sizeof(act));
  act.sa_flags = SA_SIGINFO | SA_RESTART;
  act.sa_sigaction = &segfault_hdl;
  if (sigaction(SIGSEGV, &act, NULL) < 0) {
    std::cerr << "Coudln't install segfault handler\n";
    return -1;
  }

  from = reinterpret_cast<volatile size_t*>(mmap(NULL, from_pg_count * page_sz,
                                                 PROT_READ | PROT_WRITE,
                                                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
  populate_from_space();
  to = reinterpret_cast<volatile size_t*>(mmap(NULL, page_sz, PROT_NONE,
                                               MAP_SHARED, shared_fd, 0));
  unprotected_to = reinterpret_cast<volatile size_t*>(mmap(NULL, page_sz,
                                                           PROT_READ | PROT_WRITE,
                                                           MAP_SHARED, shared_fd, 0));
  size_t total_time = 0;
  for (size_t i = 0; i < nr_iter; i++) {
    // Ensure TLBs and caches are flushed
    cache_flush(bset.data(), from_wc >> 3);
    cache_flush();
    mprotect(const_cast<size_t*>(from), from_pg_count * page_sz, PROT_NONE);
    mprotect(const_cast<size_t*>(from), from_pg_count * page_sz, PROT_READ);

    // Perform read which should trigger page fault
    startBench = std::chrono::steady_clock::now();
    std::atomic_thread_fence(std::memory_order_seq_cst);
    size_t read = to[64];
    std::atomic_thread_fence(std::memory_order_seq_cst);
    endBench = std::chrono::steady_clock::now();
    assert(read != 0);
    // Gather timing info
    auto timeDiff = std::chrono::duration_cast<std::chrono::microseconds>(endBench - startBench);
    total_time += timeDiff.count();

    // Sanity check
    sanity_check();

    // Ensure dest side page caches are flushed
    cache_flush(const_cast<size_t*>(to), page_sz);
    // Remove physical page
    madvise(const_cast<size_t*>(to), page_sz, MADV_DONTNEED);
    // Put page protection for next iteration
    mprotect(const_cast<size_t*>(to), page_sz, PROT_NONE);
  }

  std::cout << "Avg finish time: " << total_time / nr_iter << "us.\n";
  munmap(const_cast<size_t*>(to), page_sz);
  munmap(const_cast<size_t*>(unprotected_to), page_sz);
  munmap(const_cast<size_t*>(from), from_pg_count * page_sz);
  return 0;
}

TEST(SegfaultCostTest, SegfaultCost) {
  runTest(128, 100000);
}

}  // namespace runtime
}  // namespace art
