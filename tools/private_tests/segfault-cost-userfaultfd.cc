#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
#include <random>
#include <vector>

#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
#include <errno.h>
#include <linux/userfaultfd.h>
#include <asm/unistd.h>

#define __ARM_NR_COMPAT_BASE	0x0f0000
#define __ARM_NR_compat_cacheflush	(__ARM_NR_COMPAT_BASE+2)
#define __NR_compat_syscalls	399
#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); \
                        } while (0)
#ifndef UFFDIO_REMAP
#define UFFDIO_REMAP    _IOWR(UFFDIO, 0x05, struct uffdio_copy)
#endif


volatile uint64_t *to_unprotected = nullptr, *to = nullptr, *from = nullptr;
volatile uint64_t read_;
int shared_fd = 0, choice = 0;
size_t page_sz = 0, from_pg_count = 0, to_pg_count = 0, from_wc = 0, page_wc = 0, nr_objs = 0;
std::vector<size_t> bset;

// It's assumed that cacheline size on arm64 is 4 words, whereas it's 8 words on intel.
#if __aarch64__
#define OBJ_SIZE 4
#else
#define OBJ_SIZE 8
#endif

constexpr uint8_t bits_per_word = sizeof(size_t) * 8;
constexpr uint8_t word_log_bits = 6;

constexpr size_t bset_index(const size_t word) {
  return word >> word_log_bits;
}

constexpr size_t bit_number(const size_t word) {
  return word & (bits_per_word - 1);
}

constexpr size_t construct_bmap_word(const size_t bit) {
  return size_t(1) << ((bits_per_word - 1) - bit);
}

void set_bit(const size_t idx) {
  bset[bset_index(idx)] |= construct_bmap_word(bit_number(idx));
}

constexpr size_t construct_leftside_mask(const size_t bit) {
  return size_t(-1) >> bit;
}

size_t find_next_set_bit(const size_t word) {
  size_t bit = bit_number(word);
  size_t index = bset_index(word);
  size_t end_index = bset_index(to_pg_count * from_wc);
  if (index >= end_index) {
    return index << word_log_bits;
  }
  size_t bset_word = bset[index] & construct_leftside_mask(bit);
  while (bset_word == 0) {
    index++;
    if (index == end_index) {
      return index << word_log_bits;
    }
    bset_word = bset[index];
  }
  return (index << word_log_bits) + __builtin_clzl(bset_word);
}

void cacheflush(uint8_t *p, const size_t size) {
/*
#if __aarch64__
  if (syscall(__ARM_NR_compat_cacheflush, p, p + size, 0)) {
    errExit("cacheflush failed");
  }
#else
*/
#if __aarch64__
  for (size_t i = 0; i < size; i += OBJ_SIZE * sizeof(size_t)) {
    asm volatile ("dc civac, %0" :: "r"(p + i));
#else
  for (size_t i = 0; i < size; i += OBJ_SIZE * sizeof(size_t)) {
    asm volatile ("clflush (%0)" :: "r"(p + i));
#endif
  }
}

void cacheflush() {
/*
#if __aarch64__
  uint8_t* start = reinterpret_cast<uint8_t*>(const_cast<uint64_t*>(from));
  if (syscall(__ARM_NR_compat_cacheflush, start, start + (page_sz * from_pg_count), 0)) {
    errExit("cacheflush failed");
  }
#else
*/
  size_t i = 0;
  i = find_next_set_bit(i);
  while (i < from_wc) {
#if __aarch64__
    asm volatile ("dc civac, %0" :: "r"(from + i));
    if ((i >> 2) < ((i + OBJ_SIZE) >> 2)) {
      asm volatile ("dc civac, %0" :: "r"(from + i + OBJ_SIZE));
    }
    i = find_next_set_bit(i + OBJ_SIZE);
#else
    asm volatile ("clflush (%0)" :: "r"(from + i));
    if ((i >> 3) < ((i + OBJ_SIZE) >> 3)) {
      asm volatile ("clflush (%0)" :: "r"(from + i + OBJ_SIZE));
    }
    i = find_next_set_bit(i + OBJ_SIZE);
#endif
  }
}

void sanity_check(volatile uint64_t *page) {
  for (volatile size_t i = 0; i < (size_t)nr_objs; i++) {
    for (volatile int j = i * OBJ_SIZE, k = 0; k < OBJ_SIZE; k++, j++) {
      if (page[j] != i) {
        exit(EXIT_FAILURE);
      }
    }
  }
}

void compress_page(volatile uint64_t *page, size_t idx) {
  size_t k = 0, i = idx;
  do {
    i = find_next_set_bit(i);
    if (i - idx >= from_wc) {
      break;
    }
    for (int j = 0; j < OBJ_SIZE; j++) {
      page[k++] = from[i++];
    }
  } while(true);
  //std::cout << k << " " << i << "\n";
  assert(k == page_sz / sizeof(size_t));
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wframe-larger-than="
void populate_from_space() {
  /*
   * Every object is OBJ_SIZE long and spread uniformly among all the src pages.
   * This means that we have nr_objs which must fit perfectly in the to page.
   */
  std::random_device rd("/dev/urandom");
  std::mt19937 gen(rd());
  int remaining_words = (from_pg_count - 1) * OBJ_SIZE;
  std::uniform_int_distribution<> dis(0, remaining_words);
  for (size_t idx = 0, i = 0; i < to_pg_count; i++, idx = i * from_wc) {
      for (size_t obj = 0; obj < nr_objs; obj++) {
        int seek = dis(gen);
        idx += seek;
        set_bit(idx);
        for (int j = 0; j < OBJ_SIZE; j++) {
          from[idx++] = obj;
        }
        idx += remaining_words - seek;
      }
  }
}

#pragma GCC diagnostic ignored "-Wunused-parameter"
void sigsegv_hdl(int sig, siginfo_t *info, void *ctxt) {
  uint64_t* addr = (uint64_t*)((unsigned long) info->si_addr & ~(page_sz - 1));
  ptrdiff_t diff = addr - to;
  compress_page(to_unprotected + diff, diff / page_wc * from_wc);
  mprotect((void*)addr, page_sz, PROT_READ | PROT_WRITE);
}

void sigbus_hdl(int sig, siginfo_t *info, void *ctxt) {
  struct uffdio_copy uffdio_copy;
  uint64_t* addr = (uint64_t*)((unsigned long) info->si_addr & ~(page_sz - 1));
  ptrdiff_t diff = addr - to;
  int cmd = choice == 1 ? UFFDIO_COPY : UFFDIO_REMAP;
  if (from_pg_count > 1) {
    compress_page(from + diff, diff / page_wc * from_wc);
  }
  uffdio_copy.src = (unsigned long) (from + diff);
  uffdio_copy.dst = (unsigned long)addr;
  uffdio_copy.len = page_sz;
  uffdio_copy.mode = UFFDIO_COPY_MODE_DONTWAKE;
  uffdio_copy.copy = 0;
  if (ioctl(shared_fd, cmd, &uffdio_copy) == -1) {
    errExit("ioctl-UFFDIO_COPY");
  }
}
#pragma GCC diagnostic pop

int main(int argc, char** argv) {
  std::chrono::steady_clock::time_point startBench, endBench;
  struct sigaction act;

  if (argc < 4) {
    std::cerr << "Usage: " << argv[0]
              << " <#from-pages per to-page> <#to-pages> <#iters> [0 (default) for mprotect/1 for UFFD_COPY/2 for UFFD_REMAP]\n";
    exit(EXIT_FAILURE);
  }

  from_pg_count = std::stod(argv[1]);
  to_pg_count = std::stod(argv[2]);
  const size_t iters = std::stoul(argv[3]);
  choice = 0;
  if (argc > 4) {
    choice = std::stod(argv[4]);
  }
  page_sz = getpagesize();
  nr_objs = page_sz / (OBJ_SIZE * sizeof(size_t));
  from_wc = page_sz * from_pg_count / sizeof(size_t);
  page_wc = page_sz / sizeof(size_t);
  bset.resize(bset_index(to_pg_count * from_wc));

  std::memset(&act, '\0', sizeof(act));
  act.sa_flags = SA_SIGINFO | SA_RESTART;

  from = (volatile uint64_t*)mmap(NULL, to_pg_count * from_pg_count * page_sz, PROT_READ | PROT_WRITE,
                                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  madvise((void*)from, to_pg_count * from_pg_count * page_sz, MADV_NOHUGEPAGE);
  if (choice == 0) {
    shared_fd = memfd_create("segfault-cost", MFD_CLOEXEC);
    if (shared_fd == -1) {
      errExit("memfd_create");
    }
    if (ftruncate(shared_fd, to_pg_count * page_sz)) {
      errExit("ftruncate");
    }
    //Install segfault handler
    act.sa_sigaction = &sigsegv_hdl;
    if (sigaction(SIGSEGV, &act, NULL)) {
      errExit("sigaction-sigsegv");
    }
    to = (volatile uint64_t*)mmap(NULL, to_pg_count * page_sz, PROT_NONE, MAP_SHARED, shared_fd, 0);
    to_unprotected = (volatile uint64_t*)mmap(NULL, to_pg_count * page_sz,
                                              PROT_READ | PROT_WRITE, MAP_SHARED, shared_fd, 0);
  } else {
    struct uffdio_api uffdio_api;
    /* Create and enable userfaultfd object */
    shared_fd = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK | UFFD_USER_MODE_ONLY);
    if (shared_fd == -1)
      errExit("userfaultfd");

    uffdio_api.api = UFFD_API;
    // uffdio_api.features = UFFD_FEATURE_SIGBUS;
    uffdio_api.features = 1 << 7;
    if (ioctl(shared_fd, UFFDIO_API, &uffdio_api) == -1)
      errExit("ioctl-UFFDIO_API");

    act.sa_sigaction = &sigbus_hdl;
    if (sigaction(SIGBUS, &act, NULL)) {
      errExit("sigaction-SIGBUS");
    }
    to = (volatile uint64_t*)mmap(NULL, to_pg_count * page_sz, PROT_READ | PROT_WRITE,
                                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    struct uffdio_register uffdio_register;
    uffdio_register.range.start = (unsigned long) to;
    uffdio_register.range.len = page_sz * to_pg_count;
    uffdio_register.mode = UFFDIO_REGISTER_MODE_MISSING;
    if (ioctl(shared_fd, UFFDIO_REGISTER, &uffdio_register) == -1) {
      errExit("ioctl-UFFDIO_REGISTER");
    }
  }

  size_t total_time = 0, total_gc_time = 0;
  for (size_t i = 0; i < iters; i++) {
    populate_from_space();

    // Emulate GC work for userfaultfd cases.
    if (choice != 0) {
      startBench = std::chrono::steady_clock::now();
      std::atomic_thread_fence(std::memory_order_seq_cst);
      if (from_pg_count > 1) {
        for (size_t j = 0; j < page_wc * to_pg_count; j += page_wc) {
          compress_page(from + j, j / page_wc * from_wc);
        }
      }
      struct uffdio_copy uffdio_copy;
      int cmd = choice == 1 ? UFFDIO_COPY : UFFDIO_REMAP;
      uffdio_copy.src = (unsigned long)from;
      uffdio_copy.dst = (unsigned long)to;
      uffdio_copy.len = page_sz * to_pg_count;
      uffdio_copy.mode = UFFDIO_COPY_MODE_DONTWAKE;
      uffdio_copy.copy = 0;
      if (ioctl(shared_fd, cmd, &uffdio_copy) == -1) {
        errExit("ioctl-UFFDIO_COPY");
      }
      madvise((void*)from, to_pg_count * from_pg_count * page_sz, MADV_DONTNEED);
      std::atomic_thread_fence(std::memory_order_seq_cst);
      endBench = std::chrono::steady_clock::now();
      total_gc_time += std::chrono::duration_cast<std::chrono::milliseconds>(endBench - startBench).count();
      // Clear things for application side (below).
      std::fill(bset.begin(), bset.end(), 0);
      madvise((void*)to, to_pg_count * page_sz, MADV_DONTNEED);
      populate_from_space();
    }
#if 0
    // Ensure TLBs and caches are flushed
    cacheflush();
    mprotect((void*)from, from_pg_count * page_sz * to_pg_count, PROT_NONE);
    cacheflush(reinterpret_cast<uint8_t*>(bset.data()), to_pg_count * from_wc / sizeof(size_t));
    mprotect((void*)from, from_pg_count * page_sz * to_pg_count, PROT_READ | PROT_WRITE);
#endif

    // Emulate application thread: perform reads which trigger page faults
    startBench = std::chrono::steady_clock::now();
    std::atomic_thread_fence(std::memory_order_seq_cst);
    for (size_t j = 0; j < page_wc * to_pg_count; j += page_wc) {
      read_ = to[j + 64];
      if (read_ != 64 / OBJ_SIZE) {
        std::cerr << "to[" << j + 64 << "]:" << read_ << "\n";
        exit(EXIT_FAILURE);
      }
    }
    std::atomic_thread_fence(std::memory_order_seq_cst);
    endBench = std::chrono::steady_clock::now();
    total_time += std::chrono::duration_cast<std::chrono::milliseconds>(endBench - startBench).count();
#if 0
    // Sanity check
    for (size_t j = 0; j < page_wc * to_pg_count; j += page_wc) {
      sanity_check(to + j);
    }
#endif
    // Prepare for next iteration.
    madvise((void*)from, to_pg_count * from_pg_count * page_sz, MADV_DONTNEED);
    std::fill(bset.begin(), bset.end(), 0);
    if (choice == 0) {
      // Remove physical page
      madvise((void*)to, to_pg_count * page_sz, MADV_REMOVE);
      // Put page protection for next iteration
      mprotect((void*)to, to_pg_count * page_sz, PROT_NONE);
    } else {
      // Remove physical page
      madvise((void*)to, to_pg_count * page_sz, MADV_DONTNEED);
    }
  }

  std::cout << "Avg finish time: " << total_time / iters
            << "ms.\tAvg GC time: " << total_gc_time / iters << "ms.\n";
  return 0;
}
