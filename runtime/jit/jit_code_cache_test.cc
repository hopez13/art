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
#include <signal.h>
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

  inline void ReaderAcquire() {
    uint32_t old_value;
    uint32_t new_value;
    do {
      old_value = lockword.load(std::memory_order_relaxed) & kReaderMask;
      new_value = old_value + 1;
    } while (!lockword.compare_exchange_weak(old_value, new_value, std::memory_order_relaxed));
    // Enabling this instruction synchronization barrier avoids
    // SIGILLs (as one might reasonably expect from the ARMv8 reference manual).
    // __asm __volatile("isb");
  }

  inline void ReaderRelease() {
    lockword.fetch_sub(1, std::memory_order_relaxed);
  }

  inline void WriterAcquire() {
    while (lockword.fetch_or(kWriterMask, std::memory_order_relaxed) != kWriterMask) {
    }
  }

  inline void WriterRelease() {
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

struct JitFunctionInfo {
  volatile JitFunction function;
  size_t instruction_count;
  RWLock lock;

  inline void InvokeFunction() {
    lock.ReaderAcquire();
    function();
    lock.ReaderRelease();
  }
};

static JitFunctionInfo g_jit_function_info[kJitFunctionCount];

// Number of iterations run (JIT code re-generations).
static std::atomic<size_t> g_iteration(0);

static std::vector<pid_t> g_thread_ids;
static std::atomic<size_t> g_thread_idx(0);
static thread_local pid_t g_current_thread_id;
static size_t g_current_function[8];

static void InitializeThreadIds(size_t thread_count) {
  g_thread_ids.resize(thread_count);
}

static void SaveThreadId() {
  size_t index = g_thread_idx.fetch_add(1);
  g_current_thread_id = index;
  g_thread_ids[index] = gettid();
}

void ShuffleAffinity() {
  // Shuffle array of thread ids and then bind threads to different cores.
  std::mt19937 generator(g_rd());
  std::shuffle(g_thread_ids.begin(), g_thread_ids.end(), generator);
  for (size_t i = 0; i < g_thread_ids.size(); ++i) {
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
    CPU_SET(i, &cpu_set);
    int result = sched_setaffinity(g_thread_ids[i], sizeof(cpu_set), &cpu_set);
    if (result != 0) {
      fprintf(stderr, "Thread %d Cpu %zu\n", g_thread_ids[i], i);
      perror("sched_setaffinity");
    }
  }
}

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
  SaveThreadId();

  fprintf(stderr, "Starting thread %d (tid = %08x)\n", (int)g_current_thread_id, gettid());

  std::mt19937 generator(g_rd());
  std::uniform_int_distribution<size_t> rng(0, kJitFunctionCount - 1);
  while (g_iteration.load(std::memory_order_relaxed) < kMaxIterations) {
    size_t index[4];
    index[0] = rng(generator);
    index[1] = rng(generator);
    index[2] = rng(generator);
    index[3] = rng(generator);
    // Try to invoke functions with few instructions in between in
    // case this is factor.
    {
      g_current_function[g_current_thread_id] = index[0];
      JitFunctionInfo* jfi = &g_jit_function_info[index[0]];
      jfi->InvokeFunction();
    }
    {
      g_current_function[g_current_thread_id] = index[1];
      JitFunctionInfo* jfi = &g_jit_function_info[index[1]];
      jfi->InvokeFunction();
    }
    {
      g_current_function[g_current_thread_id] = index[2];
      JitFunctionInfo* jfi = &g_jit_function_info[index[2]];
      jfi->InvokeFunction();
    }
    {
      g_current_function[g_current_thread_id] = index[3];
      JitFunctionInfo* jfi = &g_jit_function_info[index[3]];
      jfi->InvokeFunction();
    }
  }
  return nullptr;
}

void DriverMain() {
  SaveThreadId();

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
      ShuffleAffinity();
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

static struct sigaction g_default_sigill_action;

void UndefinedInstructionHandler(int signo, siginfo_t* info, void* opaque_ucontext) {
  struct ucontext* ucontext = reinterpret_cast<struct ucontext*>(opaque_ucontext);
  mcontext_t* context = &ucontext->uc_mcontext;
  fprintf(stderr, "SIGNAL %d pc %p fault %p\n", signo,
          reinterpret_cast<void*>(context->pc), reinterpret_cast<void*>(context->fault_address));

  fprintf(stderr, "JIT function info\n");
  for (size_t i = 0; i < kJitFunctionCount; ++i) {
    JitFunctionInfo* jfi = &g_jit_function_info[i];
    fprintf(stderr, "  Function %zu %p..%p\n",
            i, jfi->function, reinterpret_cast<uint32_t*>(jfi->function) + jfi->instruction_count);
  }

  // This isn't very smart, should check bounds.
  if (context->fault_address != 0ull) {
    fprintf(stderr, "Around fault address\n");
    uint32_t* start_address = std::max(reinterpret_cast<uint32_t*>(context->fault_address) - 8,
                                       reinterpret_cast<uint32_t*>(g_cache));
    uint32_t* end_address = std::min(reinterpret_cast<uint32_t*>(context->fault_address) + 8,
                                     reinterpret_cast<uint32_t*>(g_cache + kJitCacheSize));
    while (start_address < end_address) {
      fprintf(stderr, "  %p: %08x\n", start_address, *start_address);
      ++start_address;
    }
  }

  if (context->pc != 0ull) {
    fprintf(stderr, "Memory around pc\n");
    uint32_t* addr = reinterpret_cast<uint32_t*>(context->pc) - 8;
    for (int i = 0; i < 16; i += 4) {
      fprintf(stderr, "  %p: %08x %08x %08x %08x\n",
              addr + i, addr[i], addr[i + 1], addr[i + 2], addr[i + 3]);
    }
  }

  fprintf(stderr, "Worker thread calling info (current tid = %08x)\n", gettid());
  for (size_t i = 1; i < sizeof(g_current_function) / sizeof(g_current_function[0]); ++i) {
    fprintf(stderr, "  %zu: was calling %zu\n", i, g_current_function[i]);
  }

  // Invoke the default handler.
  g_default_sigill_action.sa_sigaction(signo, info, opaque_ucontext);
}

void InstallUndefinedInstructionHandler() {
  struct sigaction action;
  memset(&action, 0, sizeof(action));

  action.sa_sigaction = UndefinedInstructionHandler;
  action.sa_flags = SA_SIGINFO;

  if (sigaction(SIGILL, &action, &g_default_sigill_action) < 0) {
    perror("sigaction");
    exit(EXIT_FAILURE);
  }
}

}  // namespace

int main() {
  uint32_t cpu_count = GetCpuCount();
  InitializeThreadIds(cpu_count);
  SetupTest();
  InstallUndefinedInstructionHandler();
  for (uint32_t i = 0; i < cpu_count - 1; ++i) {
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
