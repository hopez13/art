/*
 * Copyright 2017 The Android Open Source Project
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

#include <atomic>
#include <fstream>
#include <random>
#include <string>

#include <assert.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sched.h>
#include <unistd.h>

namespace {

static const uint32_t kNop = 0xd503201fu;
static const uint32_t kRBit = 0x5ac00000u;    // rbit w0, w0
static const uint32_t kEor = 0x52030c21u;     // eor w1, w1, #e0000001
static const uint32_t kReturn = 0xd65f03c0u;
static const uint32_t kUndefined = 0xffffffffu;

static const size_t kReportIterations = 100000;
static const size_t kMaxIterations = 100 * kReportIterations;

static const size_t kJitFunctionCount = 7;
static const size_t kMinInstructions = 2;
static const size_t kMaxInstructions = 16;

static const size_t kPageSize = 4096;
static const size_t kJitCacheSize =
    (kMaxInstructions * sizeof(kNop) * kJitFunctionCount + kPageSize - 1) & ~(kPageSize - 1);

// Non-blocking non-rentrant multiple reader single writer lock
class RWLock {
 public:
  RWLock() : lockword(0) {}

  void ReaderAcquire() {
    uint32_t old_value;
    uint32_t new_value;
    do {
      old_value = lockword.load(std::memory_order_relaxed) & kReaderMask;
      new_value = old_value + 1;
    } while (!lockword.compare_exchange_weak(old_value, new_value, std::memory_order_relaxed));
  }

  void ReaderRelease() {
    lockword.fetch_sub(1, std::memory_order_relaxed);
  }

  void WriterAcquire() {
    while (lockword.fetch_or(kWriterMask, std::memory_order_relaxed) != kWriterMask) {
    }
  }

  void WriterRelease() {
    lockword.fetch_xor(kWriterMask, std::memory_order_relaxed);
  }

 private:
  const uint32_t kReaderMask = 0x7fffffffu;
  const uint32_t kWriterMask = 0x80000000u;

  std::atomic<uint32_t> lockword;
};

// Random number generation state
static std::random_device g_rd;

// Memory allocated for the JIT code cache cache.
static uint8_t *g_cache;

// Function pointer for JIT generated functions.
typedef void(*volatile JitFunction)(void);

typedef struct {
  JitFunction function;
  size_t instruction_count;
  RWLock lock;
} JitFunctionInfo;

static JitFunctionInfo g_jit_function_info[kJitFunctionCount];

// Number of iterations run (JIT code re-generations).
static std::atomic<size_t> g_iteration(0);

// Writes a JIT generated function body. The selection of instructions
// here is arbitrary, just different sequences amounting to no change.
JitFunction WriteJitFunction(uint32_t* instruction_address, size_t instruction_count) {
  uint32_t* start_address = instruction_address;

  if (reinterpret_cast<uint8_t*>(instruction_address) < g_cache ||
      reinterpret_cast<uint8_t*>(instruction_address + instruction_count) > (g_cache + kJitCacheSize)) {
    fprintf(stderr, "Bad function info %p..%p\n",
            instruction_address,
            instruction_address + instruction_count);
    fprintf(stderr, "Cache %p..%p\n", g_cache, g_cache + kJitCacheSize);
    exit(EXIT_FAILURE);
  }

  // Deduct 2 for the end instruction sequence.
  instruction_count -= kMinInstructions;

  if (instruction_count >= 6) {
    *instruction_address++ = 0xd10043ff; // sub sp, sp, #0x10
    *instruction_address++ = 0xb9400fe0; // ldr w0, [sp,#12]
    *instruction_address++ = 0xb9000fe0; // str w0, [sp,#12]
    *instruction_address++ = 0x521e0000; // eor w0, w0, #4
    *instruction_address++ = 0xb9400fe0; // ldr w0, [sp,#12]
    *instruction_address++ = 0x910043ff; // add sp, sp, #0x10
    instruction_count -= 6;
  }

  if (instruction_count >= 6) {
    *instruction_address++ = 0x6b1f001f; // cmp w0, wzr
    *instruction_address++ = 0x540000a1; // b.ne past kUndefined instructions.
    *instruction_address++ = 0x6b1f001f; // cmp w0, wzr
    *instruction_address++ = 0x54000060; // b.eq past kUndefined instructions.
    *instruction_address++ = kUndefined; // Should never be hit.
    *instruction_address++ = kUndefined; // Should never be hit.
    instruction_count -= 6;
  }

  while (instruction_count >= 4) {
    *instruction_address++ = kRBit;
    *instruction_address++ = kEor;
    *instruction_address++ = kRBit;
    *instruction_address++ = kEor;
    instruction_count -= 4;
  }

  while (instruction_count >= 2) {
    *instruction_address++ = kRBit;
    *instruction_address++ = kRBit;
    instruction_count -= 2;
  }

  while (instruction_count > 0) {
    *instruction_address++ = kNop;
    instruction_count -= 1;
  }

  // End with a return and some metadata to generate an undefined
  // instruction trap if pipeline is stale.
  *instruction_address++ = kReturn;
  *instruction_address++ = kUndefined;

  return reinterpret_cast<JitFunction>(start_address);
}

void UpdateJitFunction(size_t index) {
  JitFunctionInfo* const current = &g_jit_function_info[index];

  uint32_t* start_address;
  if (index == 0) {
    start_address = reinterpret_cast<uint32_t*>(g_cache);
  } else {
    JitFunctionInfo* prev = current - 1;
    start_address = reinterpret_cast<uint32_t*>(prev->function) + prev->instruction_count;
  }

  uint32_t* end_address;
  if ((index + 1) == kJitFunctionCount) {
    end_address = reinterpret_cast<uint32_t*>(g_cache + kJitCacheSize);
  } else {
    JitFunctionInfo* next = current + 1;
    end_address = reinterpret_cast<uint32_t*>(next->function);
  }

  size_t max_size = end_address - start_address;
  if (max_size == kMinInstructions) {
    return;
  }

  std::mt19937 generator(g_rd());
  std::uniform_int_distribution<size_t> rng(kMinInstructions, max_size);

  // Get the size for the updated function.
  size_t new_size = rng(generator);

  current->lock.WriterAcquire();

  // Write some invalid metadata ahead of function if that doesn't
  // lead to writes outside the JIT cache.
  if (new_size != max_size) {
    size_t start_offset = rng(generator) % (max_size - new_size);
    uint32_t* cache_end_address = reinterpret_cast<uint32_t*>(g_cache + kJitCacheSize);
    if (start_offset + start_address + new_size < cache_end_address) {
      while (start_offset-- > 0) {
        *start_address++ = kUndefined;
      }
    }
  }
  // Write the function
  WriteJitFunction(start_address, new_size);

  // Update the function information.
  current->function = reinterpret_cast<JitFunction>(start_address);
  current->instruction_count = new_size;

  // Flush the caches and invalidate the instruction pipeline.
  __builtin___clear_cache(reinterpret_cast<char*>(g_cache),
                          reinterpret_cast<char*>(g_cache + kJitCacheSize));
  __asm __volatile("isb");
  current->lock.WriterRelease();
}

void SetupTest() {
  // Creating RWX. ART toggles between RX and RW during updates, but this is not material here.
  g_cache = reinterpret_cast<uint8_t*>(mmap(nullptr,
                                            kJitCacheSize,
                                            PROT_READ | PROT_WRITE | PROT_EXEC,
                                            MAP_PRIVATE | MAP_ANONYMOUS,
                                            -1,
                                            0));
  uint32_t* current_address = reinterpret_cast<uint32_t*>(g_cache);
  for (size_t i = 0; i < kJitFunctionCount; ++i) {
    JitFunctionInfo* jfi = &g_jit_function_info[i];
    jfi->lock.WriterAcquire();
    jfi->function = WriteJitFunction(current_address, kMaxInstructions);
    jfi->instruction_count = kMaxInstructions;
    __builtin___clear_cache(reinterpret_cast<char*>(g_cache),
                            reinterpret_cast<char*>(g_cache + kJitCacheSize));
    __asm __volatile("isb");
    jfi->lock.WriterRelease();
    current_address += kMaxInstructions;
  }
}

void* WorkerMain(void*) {
  std::mt19937 generator(g_rd());
  std::uniform_int_distribution<size_t> rng(0, kJitFunctionCount - 1);
  while (g_iteration.load(std::memory_order_relaxed) < kMaxIterations) {
    size_t index[3];
    index[0] = rng(generator);
    index[1] = rng(generator);
    index[2] = rng(generator);
    // Try to invoke functions with few instructions in between in
    // case this is factor.
    {
      JitFunctionInfo* jfi = &g_jit_function_info[index[0]];
      jfi->lock.ReaderAcquire();
      jfi->function();
      jfi->lock.ReaderRelease();
    }
    {
      JitFunctionInfo* jfi = &g_jit_function_info[index[1]];
      jfi->lock.ReaderAcquire();
      jfi->function();
      jfi->lock.ReaderRelease();
    }
    {
      JitFunctionInfo* jfi = &g_jit_function_info[index[2]];
      jfi->lock.ReaderAcquire();
      jfi->function();
      jfi->lock.ReaderRelease();
    }
  }
  return nullptr;
}

void DriverMain() {
  std::mt19937 generator(g_rd());
  std::uniform_int_distribution<size_t> rng(0, kJitFunctionCount - 1);

  size_t iteration = 0;
  while (iteration < kMaxIterations) {
    size_t index = rng(generator);
    UpdateJitFunction(index);
    g_iteration.store(++iteration);
    if ((iteration % kReportIterations) == 0) {
      fputc('.', stdout);
      fflush(stdout);
    }
  }
}

void thread_fail(int result, const char* msg) {
  errno = result;
  perror(msg);
  exit(EXIT_FAILURE);
}

uint32_t GetCpuCount() {
  std::ifstream is("/sys/devices/system/cpu/present");
  std::string present((std::istreambuf_iterator<char>(is)), (std::istreambuf_iterator<char>()));
  // Assuming form is 0-N so skip the first 2 characters.
  return std::stoul(present.substr(2)) + 1u;
}

}  // namespace

int main() {
  uint32_t cpu_count = GetCpuCount();
  uint32_t worker_thread_count = cpu_count - 1;
  fprintf(stderr, "Worker thread count: %u\n", worker_thread_count);
  SetupTest();
  for (uint32_t i = 0; i < worker_thread_count; ++i) {
    int result;
    pthread_t thread;
    pthread_attr_t attr;
    result = pthread_attr_init(&attr);
    if (result != 0) {
      thread_fail(result, "pthread_attr_init");
    }
    result = pthread_create(&thread, &attr, WorkerMain, nullptr);
    if (result != 0) {
      thread_fail(result, "pthread_create");
    }
  }
  DriverMain();
  return 0;
}
