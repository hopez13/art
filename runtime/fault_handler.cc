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

#include "fault_handler.h"

#include <atomic>
#include <string.h>
#include <sys/mman.h>
#include <sys/ucontext.h>

#include "art_method-inl.h"
#include "base/logging.h"  // For VLOG
#include "base/safe_copy.h"
#include "base/stl_util.h"
#include "dex/dex_file_types.h"
#include "jit/jit.h"
#include "jit/jit_code_cache.h"
#include "mirror/class.h"
#include "mirror/object_reference.h"
#include "oat_file.h"
#include "oat_quick_method_header.h"
#include "sigchain.h"
#include "thread-current-inl.h"
#include "verify_object-inl.h"

namespace art {
// Static fault manger object accessed by signal handler.
FaultManager fault_manager;

// This needs to be NO_INLINE since some debuggers do not read the inline-info to set a breakpoint
// if it isn't.
extern "C" NO_INLINE __attribute__((visibility("default"))) void art_sigsegv_fault() {
  // Set a breakpoint here to be informed when a SIGSEGV is unhandled by ART.
  VLOG(signals)<< "Caught unknown SIGSEGV in ART fault handler - chaining to next handler.";
}

// Signal handler called on SIGSEGV.
static bool art_fault_handler(int sig, siginfo_t* info, void* context) {
  return fault_manager.HandleFault(sig, info, context);
}

struct FaultManager::GeneratedCodeRange {
  std::atomic<GeneratedCodeRange*> next;
  const void* start;
  size_t size;
};

FaultManager::FaultManager()
    : generated_code_ranges_lock_("FaultHandler generated code ranges lock",
                                  LockLevel::kGenericBottomLock),
      initialized_(false) {
  sigaction(SIGSEGV, nullptr, &oldaction_);
}

FaultManager::~FaultManager() {
}

void FaultManager::Init() {
  CHECK(!initialized_);
  sigset_t mask;
  sigfillset(&mask);
  sigdelset(&mask, SIGABRT);
  sigdelset(&mask, SIGBUS);
  sigdelset(&mask, SIGFPE);
  sigdelset(&mask, SIGILL);
  sigdelset(&mask, SIGSEGV);

  SigchainAction sa = {
    .sc_sigaction = art_fault_handler,
    .sc_mask = mask,
    .sc_flags = 0UL,
  };

  AddSpecialSignalHandlerFn(SIGSEGV, &sa);
  initialized_ = true;
}

void FaultManager::Release() {
  if (initialized_) {
    RemoveSpecialSignalHandlerFn(SIGSEGV, art_fault_handler);
    initialized_ = false;
  }
}

void FaultManager::Shutdown() {
  if (initialized_) {
    Release();

    // Free all handlers.
    STLDeleteElements(&generated_code_handlers_);
    STLDeleteElements(&other_handlers_);

    // Delete remaining code ranges if any (such as nterp code or oat code from
    // oat files that have not been unloaded, including boot image oat files).
    GeneratedCodeRange* range;
    {
      MutexLock lock(Thread::Current(), generated_code_ranges_lock_);
      range = generated_code_ranges_.load(std::memory_order_acquire);
      generated_code_ranges_.store(nullptr, std::memory_order_release);
    }
    while (range != nullptr) {
      GeneratedCodeRange* next_range = range->next.load(std::memory_order_relaxed);
      delete range;
      range = next_range;
    }
  }
}

bool FaultManager::HandleFaultByOtherHandlers(int sig, siginfo_t* info, void* context) {
  if (other_handlers_.empty()) {
    return false;
  }

  Thread* self = Thread::Current();

  DCHECK(self != nullptr);
  DCHECK(Runtime::Current() != nullptr);
  DCHECK(Runtime::Current()->IsStarted());
  for (const auto& handler : other_handlers_) {
    if (handler->Action(sig, info, context)) {
      return true;
    }
  }
  return false;
}

static const char* SignalCodeName(int sig, int code) {
  if (sig != SIGSEGV) {
    return "UNKNOWN";
  } else {
    switch (code) {
      case SEGV_MAPERR: return "SEGV_MAPERR";
      case SEGV_ACCERR: return "SEGV_ACCERR";
      case 8:           return "SEGV_MTEAERR";
      case 9:           return "SEGV_MTESERR";
      default:          return "UNKNOWN";
    }
  }
}
static std::ostream& PrintSignalInfo(std::ostream& os, siginfo_t* info) {
  os << "  si_signo: " << info->si_signo << " (" << strsignal(info->si_signo) << ")\n"
     << "  si_code: " << info->si_code
     << " (" << SignalCodeName(info->si_signo, info->si_code) << ")";
  if (info->si_signo == SIGSEGV) {
    os << "\n" << "  si_addr: " << info->si_addr;
  }
  return os;
}

bool FaultManager::HandleFault(int sig, siginfo_t* info, void* context) {
  if (VLOG_IS_ON(signals)) {
    PrintSignalInfo(VLOG_STREAM(signals) << "Handling fault:" << "\n", info);
  }

#ifdef TEST_NESTED_SIGNAL
  // Simulate a crash in a handler.
  raise(SIGSEGV);
#endif

  if (IsInGeneratedCode(info, context, true)) {
    VLOG(signals) << "in generated code, looking for handler";
    for (const auto& handler : generated_code_handlers_) {
      VLOG(signals) << "invoking Action on handler " << handler;
      if (handler->Action(sig, info, context)) {
        // We have handled a signal so it's time to return from the
        // signal handler to the appropriate place.
        return true;
      }
    }
  }

  // We hit a signal we didn't handle.  This might be something for which
  // we can give more information about so call all registered handlers to
  // see if it is.
  if (HandleFaultByOtherHandlers(sig, info, context)) {
    return true;
  }

  // Set a breakpoint in this function to catch unhandled signals.
  art_sigsegv_fault();
  return false;
}

void FaultManager::AddHandler(FaultHandler* handler, bool generated_code) {
  DCHECK(initialized_);
  if (generated_code) {
    generated_code_handlers_.push_back(handler);
  } else {
    other_handlers_.push_back(handler);
  }
}

void FaultManager::RemoveHandler(FaultHandler* handler) {
  auto it = std::find(generated_code_handlers_.begin(), generated_code_handlers_.end(), handler);
  if (it != generated_code_handlers_.end()) {
    generated_code_handlers_.erase(it);
    return;
  }
  auto it2 = std::find(other_handlers_.begin(), other_handlers_.end(), handler);
  if (it2 != other_handlers_.end()) {
    other_handlers_.erase(it2);
    return;
  }
  LOG(FATAL) << "Attempted to remove non existent handler " << handler;
}

void FaultManager::AddGeneratedCodeRange(const void* start, size_t size) {
  GeneratedCodeRange* new_range = new GeneratedCodeRange{nullptr, start, size};
  bool success;
  {
    MutexLock lock(Thread::Current(), generated_code_ranges_lock_);
    GeneratedCodeRange* old_head = generated_code_ranges_.load(std::memory_order_relaxed);
    new_range->next.store(old_head, std::memory_order_relaxed);
    success = generated_code_ranges_.compare_exchange_strong(
        old_head, new_range, std::memory_order_release, std::memory_order_relaxed);
  }
  CHECK(success);

  // The above release operation on `generated_code_ranges_` with an acquire operation
  // on the same atomic object in `IsInGeneratedCode()` ensures the correct memory
  // visibility for the contents of `*new_range` for any thread that loads the value
  // written above (or a value written by a release sequence headed by that write).
  //
  // However, we also need to ensure that any thread that encounters a segmentation
  // fault in the provided range shall actually see the written value. For JIT code
  // cache and nterp, the registration happens while the process is single-threaded
  // but the synchronization is more complicated for code in oat files.
  //
  // Threads that load classes register dex files under the `Locks::dex_lock_` and
  // the first one to register a dex file with a given oat file shall add the oat
  // code range; the memory visibility for these threads is guaranteed by the lock.
  // However a thread that did not try to load a class with oat code can execute the
  // code if a direct or indirect reference to such class escapes from one of the
  // threads that loaded it. Given that Java reference stores and loads are relaxed
  // atomic operations, we can use the `std::atomic_thread_fence()` to ensure proper
  // memory visibility for an aribitrary chain reference stores and loads that led
  // the execution of the oat code by using a release fence here and an acquire
  // fence in `IsInGeneratedCode()`.
  std::atomic_thread_fence(std::memory_order_release);
}

void FaultManager::RemoveGeneratedCodeRange(const void* start, size_t size) {
  Thread* self = Thread::Current();
  GeneratedCodeRange* range = nullptr;
  bool success = false;
  {
    MutexLock lock(self, generated_code_ranges_lock_);
    std::atomic<GeneratedCodeRange*>* before = &generated_code_ranges_;
    range = before->load(std::memory_order_acquire);
    while (range != nullptr && range->start != start) {
      before = &range->next;
      range = before->load(std::memory_order_relaxed);
    }
    if (range != nullptr) {
      GeneratedCodeRange* old_value = range;
      GeneratedCodeRange* new_value = range->next.load(std::memory_order_relaxed);
      success = before->compare_exchange_strong(
          old_value, new_value, std::memory_order_release, std::memory_order_relaxed);
    }
  }
  CHECK(range != nullptr);
  DCHECK_EQ(range->start, start);
  CHECK_EQ(range->size, size);
  CHECK(success);

  Runtime* runtime = Runtime::Current();
  CHECK(runtime != nullptr);
  if (runtime->IsStarted() && runtime->GetThreadList() != nullptr) {
    // Run a checkpoint before deleting the range to ensure that no thread
    // is walking the list in `IsInGeneratedCode()`.
    runtime->GetThreadList()->RunEmptyCheckpoint();
  }
  delete range;
}

// This function is called within the signal handler.  It checks that
// the mutator_lock is held (shared).  No annotalysis is done.
bool FaultManager::IsInGeneratedCode(siginfo_t* siginfo, void* context, bool check_dex_pc) {
  // We can only be running Java code in the current thread if it
  // is in Runnable state.
  VLOG(signals) << "Checking for generated code";
  Thread* thread = Thread::Current();
  if (thread == nullptr) {
    VLOG(signals) << "no current thread";
    return false;
  }

  ThreadState state = thread->GetState();
  if (state != ThreadState::kRunnable) {
    VLOG(signals) << "not runnable";
    return false;
  }

  // Current thread is runnable.
  // Make sure it has the mutator lock.
  if (!Locks::mutator_lock_->IsSharedHeld(thread)) {
    VLOG(signals) << "no lock";
    return false;
  }

  ArtMethod* method_obj = nullptr;
  uintptr_t return_pc = 0;
  uintptr_t sp = 0;
  bool is_stack_overflow = false;

  // Get the architecture specific method address and return address.  These
  // are in architecture specific files in arch/<arch>/fault_handler_<arch>.
  GetMethodAndReturnPcAndSp(siginfo, context, &method_obj, &return_pc, &sp, &is_stack_overflow);

  // Ensure proper memory visibility of registered code ranges.
  // See `AddGeneratedCodeRange()` for details.
  std::atomic_thread_fence(std::memory_order_acquire);

  // Walk over the list of registered code ranges.
  GeneratedCodeRange* range = generated_code_ranges_.load(std::memory_order_acquire);
  while (range != nullptr &&
         // Note: For all implicit checks we expect the return PC to be within
         // the code range. Implicit check cannot be the last instruction.
         return_pc - reinterpret_cast<uintptr_t>(range->start) >= range->size) {
    range = range->next.load(std::memory_order_relaxed);
  }
  if (range == nullptr) {
    return false;
  }

  // If we don't have a potential method, we're outta here.
  VLOG(signals) << "potential method: " << method_obj;
  // TODO: Check linear alloc and image.
  DCHECK_ALIGNED(ArtMethod::Size(kRuntimePointerSize), sizeof(void*))
      << "ArtMethod is not pointer aligned";
  if (method_obj == nullptr || !IsAligned<sizeof(void*)>(method_obj)) {
    VLOG(signals) << "no method";
    return false;
  }

  // Check if we have a GC map at the return PC address.
  const OatQuickMethodHeader* method_header = nullptr;
  if (kIsDebugBuild || (check_dex_pc && !is_stack_overflow)) {
    method_header = method_obj->GetOatQuickMethodHeader(return_pc);
    CHECK(method_header != nullptr);
    VLOG(signals) << "looking for dex pc for return pc 0x" << std::hex << return_pc
                  << " pc offset: 0x" << std::hex
                  << (return_pc - reinterpret_cast<uintptr_t>(method_header->GetEntryPoint()));
  }
  uint32_t dexpc = dex::kDexNoIndex;
  if (is_stack_overflow) {
    // If it's an implicit stack overflow check, the frame is not setup, so we
    // just infer the dex PC as zero.
    dexpc = 0;
    VLOG(signals) << "dexpc = 0 for stack overflow";
  } else if (check_dex_pc || kIsDebugBuild) {
    CHECK_EQ(*reinterpret_cast<ArtMethod**>(sp), method_obj);
    dexpc = method_header->ToDexPc(reinterpret_cast<ArtMethod**>(sp), return_pc, false);
    VLOG(signals) << "dexpc: " << dexpc;
  }
  return !check_dex_pc || dexpc != dex::kDexNoIndex;
}

FaultHandler::FaultHandler(FaultManager* manager) : manager_(manager) {
}

//
// Null pointer fault handler
//
NullPointerHandler::NullPointerHandler(FaultManager* manager) : FaultHandler(manager) {
  manager_->AddHandler(this, true);
}

//
// Suspension fault handler
//
SuspensionHandler::SuspensionHandler(FaultManager* manager) : FaultHandler(manager) {
  manager_->AddHandler(this, true);
}

//
// Stack overflow fault handler
//
StackOverflowHandler::StackOverflowHandler(FaultManager* manager) : FaultHandler(manager) {
  manager_->AddHandler(this, true);
}

//
// Stack trace handler, used to help get a stack trace from SIGSEGV inside of compiled code.
//
JavaStackTraceHandler::JavaStackTraceHandler(FaultManager* manager) : FaultHandler(manager) {
  manager_->AddHandler(this, false);
}

bool JavaStackTraceHandler::Action(int sig ATTRIBUTE_UNUSED, siginfo_t* siginfo, void* context) {
  // Make sure that we are in the generated code, but we may not have a dex pc.
  bool in_generated_code = manager_->IsInGeneratedCode(siginfo, context, false);
  if (in_generated_code) {
    LOG(ERROR) << "Dumping java stack trace for crash in generated code";
    ArtMethod* method = nullptr;
    uintptr_t return_pc = 0;
    uintptr_t sp = 0;
    bool is_stack_overflow = false;
    Thread* self = Thread::Current();

    manager_->GetMethodAndReturnPcAndSp(
        siginfo, context, &method, &return_pc, &sp, &is_stack_overflow);
    // Inside of generated code, sp[0] is the method, so sp is the frame.
    self->SetTopOfStack(reinterpret_cast<ArtMethod**>(sp));
    self->DumpJavaStack(LOG_STREAM(ERROR));
  }

  return false;  // Return false since we want to propagate the fault to the main signal handler.
}

}   // namespace art
