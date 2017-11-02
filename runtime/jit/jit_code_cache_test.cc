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
#include <unistd.h>

namespace {

static const uint32_t kNop = 0xd503201fu;
static const uint32_t kRBit = 0x5ac00000u;    // rbit w0, w0
static const uint32_t kEor = 0x52030c21u;     // eor w1, w1, #e0000001
static const uint32_t kReturn = 0xd65f03c0u;
static const uint32_t kUndefined = 0xffffffffu;

static const size_t kReportIterations = 10000;
static const size_t kMaxIterations = 10 * kReportIterations;

static const size_t kJitFunctionCount = 3;
static const size_t kMaxInstructions = 32;

// Function pointer for JIT generated functions.
typedef void(*volatile JitFunction)(void);

// Table of function pointers to JIT generated code.
static JitFunction g_jit_function[kJitFunctionCount];

// A cyclic barrier based on busy waiting (no system calls).
class Barrier {
 public:
  Barrier(uint32_t thread_count) : thread_count(thread_count) {
    std::atomic_store(&waiters_count[0], 0u);
    std::atomic_store(&waiters_count[1], 0u);
  }

  void Initialize() {
    current = 0;
  }

  inline bool WaitIfBlocked() {
    if (!blocked.load()) {
      return false;
    }

    ++waiters_count[current];
    while (waiters_count[current].load(std::memory_order_acquire) != 0u) {}
    current = (current + 1) & 1;
    return true;
  }

  inline void Block() {
    blocked.store(true);
    while (waiters_count[current].load(std::memory_order_acquire) != thread_count) {}
  }

  inline void Unblock() {
    blocked.store(false);
    waiters_count[current].store(0, std::memory_order_release);
    current = (current + 1) & 1;
  }

 private:
  // Number of blocked threads before considered blocked.
  uint32_t thread_count;

  // Index to which waiters_count count to use.
  static thread_local int current;

  // Flag causing the barrier to block threads.
  std::atomic<bool> blocked;

  // Counters of number of threads currently waiting. Use a pair to
  // avoid races when blocking and unblocking.
  std::atomic<uint32_t> waiters_count[2];
};

thread_local int Barrier::current = 0;

// Random number generation state
static std::random_device g_rd;
static std::mt19937 g_gen(g_rd());
static std::uniform_int_distribution<size_t> g_dist(0u, kMaxInstructions);

// Memory allocated for the JIT code cache cache.
static uint8_t *g_cache;

// Number of iterations run (JIT code re-generations).
static std::atomic<size_t> g_iteration(0);

// Writes a JIT generated function body. The selection of instructions
// here is arbitrary, just different sequences amounting to no change.
JitFunction WriteJitFunction(uint32_t* instruction_address, size_t instruction_count) {
  uint32_t* start_address = instruction_address;

  // Deduct 2 for the end instruction sequence.
  instruction_count -= 2;

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

void WriteJitFunctions() {
  uint32_t* current_address =
      reinterpret_cast<uint32_t*>(g_cache) + g_dist(g_gen);
  *current_address++ = kUndefined;
  for (size_t i = 0; i < kJitFunctionCount; ++i) {
    size_t instruction_count = g_dist(g_gen) + 2;
    g_jit_function[i] = WriteJitFunction(current_address, instruction_count);
    current_address += instruction_count;
  }
  __builtin___clear_cache(reinterpret_cast<char*>(g_cache),
                          reinterpret_cast<char*>(current_address));
  __asm __volatile("isb");
}

void SetupTest() {
  // Creating RWX. ART toggles between RX and RW during updates, but this is not material here.
  g_cache = reinterpret_cast<uint8_t*>(
      mmap(nullptr, 4096, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
  WriteJitFunctions();
}

void* WorkerMain(void* cookie) {
  Barrier* barrier = reinterpret_cast<Barrier*>(cookie);
  barrier->Initialize();
  while (g_iteration.load(std::memory_order_relaxed) < kMaxIterations) {
    for (size_t i = 0; i < kJitFunctionCount; ++i) {
      g_jit_function[i]();
      barrier->WaitIfBlocked();
    }
  }
  return nullptr;
}

void Spin(int count) {
  // usleep(0);
  while (count != 0) {
    count -= 1;
  }
}

void DriverMain(Barrier* barrier) {
  barrier->Initialize();
  size_t iterations = 0;
  while (iterations < kMaxIterations) {
    barrier->Block();
    WriteJitFunctions();
    barrier->Unblock();
    Spin(1000);
    if ((iterations % kReportIterations) == 0) {
      fputc('.', stdout);
      fflush(stdout);
    }
    g_iteration.store(++iterations);
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
  uint32_t worker_thread_count = GetCpuCount() - 1;
  fprintf(stderr, "Worker thread count: %u\n", worker_thread_count);
  Barrier barrier(worker_thread_count);
  SetupTest();
  for (uint32_t i = 0; i < worker_thread_count; ++i) {
    int result;
    pthread_t thread;
    pthread_attr_t attr;
    result = pthread_attr_init(&attr);
    if (result != 0) {
      thread_fail(result, "pthread_attr_init");
    }
    result = pthread_create(&thread, &attr, WorkerMain, &barrier);
    if (result != 0) {
      thread_fail(result, "pthread_create");
    }
  }
  DriverMain(&barrier);
  return 0;
}
