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

#include "runtime_common.h"

#include <signal.h>

#include <cinttypes>
#include <iostream>
#include <sstream>
#include <string>

#include "android-base/stringprintf.h"

#include "art_method.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/mutex.h"
#include "class_linker.h"
#include "fault_handler.h"
#include "jit/jit.h"
#include "jit/jit_code_cache.h"
#include "native_stack_dump.h"
#include "oat_quick_method_header.h"
#include "runtime.h"
#include "stack.h"
#include "thread-current-inl.h"
#include "thread_list.h"

namespace art {

using android::base::StringPrintf;

static constexpr bool kUseSigRTTimeout = true;
static constexpr bool kDumpNativeStackOnTimeout = true;

const char* GetSignalName(int signal_number) {
  switch (signal_number) {
    case SIGABRT: return "SIGABRT";
    case SIGBUS: return "SIGBUS";
    case SIGFPE: return "SIGFPE";
    case SIGILL: return "SIGILL";
    case SIGPIPE: return "SIGPIPE";
    case SIGSEGV: return "SIGSEGV";
#if defined(SIGSTKFLT)
    case SIGSTKFLT: return "SIGSTKFLT";
#endif
    case SIGTRAP: return "SIGTRAP";
  }
  return "??";
}

const char* GetSignalCodeName(int signal_number, int signal_code) {
  // Try the signal-specific codes...
  switch (signal_number) {
    case SIGILL:
      switch (signal_code) {
        case ILL_ILLOPC: return "ILL_ILLOPC";
        case ILL_ILLOPN: return "ILL_ILLOPN";
        case ILL_ILLADR: return "ILL_ILLADR";
        case ILL_ILLTRP: return "ILL_ILLTRP";
        case ILL_PRVOPC: return "ILL_PRVOPC";
        case ILL_PRVREG: return "ILL_PRVREG";
        case ILL_COPROC: return "ILL_COPROC";
        case ILL_BADSTK: return "ILL_BADSTK";
      }
      break;
    case SIGBUS:
      switch (signal_code) {
        case BUS_ADRALN: return "BUS_ADRALN";
        case BUS_ADRERR: return "BUS_ADRERR";
        case BUS_OBJERR: return "BUS_OBJERR";
      }
      break;
    case SIGFPE:
      switch (signal_code) {
        case FPE_INTDIV: return "FPE_INTDIV";
        case FPE_INTOVF: return "FPE_INTOVF";
        case FPE_FLTDIV: return "FPE_FLTDIV";
        case FPE_FLTOVF: return "FPE_FLTOVF";
        case FPE_FLTUND: return "FPE_FLTUND";
        case FPE_FLTRES: return "FPE_FLTRES";
        case FPE_FLTINV: return "FPE_FLTINV";
        case FPE_FLTSUB: return "FPE_FLTSUB";
      }
      break;
    case SIGSEGV:
      switch (signal_code) {
        case SEGV_MAPERR: return "SEGV_MAPERR";
        case SEGV_ACCERR: return "SEGV_ACCERR";
#if defined(SEGV_BNDERR)
        case SEGV_BNDERR: return "SEGV_BNDERR";
#endif
      }
      break;
    case SIGTRAP:
      switch (signal_code) {
        case TRAP_BRKPT: return "TRAP_BRKPT";
        case TRAP_TRACE: return "TRAP_TRACE";
      }
      break;
  }
  // Then the other codes...
  switch (signal_code) {
    case SI_USER:     return "SI_USER";
#if defined(SI_KERNEL)
    case SI_KERNEL:   return "SI_KERNEL";
#endif
    case SI_QUEUE:    return "SI_QUEUE";
    case SI_TIMER:    return "SI_TIMER";
    case SI_MESGQ:    return "SI_MESGQ";
    case SI_ASYNCIO:  return "SI_ASYNCIO";
#if defined(SI_SIGIO)
    case SI_SIGIO:    return "SI_SIGIO";
#endif
#if defined(SI_TKILL)
    case SI_TKILL:    return "SI_TKILL";
#endif
  }
  // Then give up...
  return "?";
}

struct UContext {
  explicit UContext(void* raw_context)
      : context(reinterpret_cast<ucontext_t*>(raw_context)->uc_mcontext) {}

  void Dump(std::ostream& os) const;

  void DumpRegister32(std::ostream& os, const char* name, uint32_t value) const;
  void DumpRegister64(std::ostream& os, const char* name, uint64_t value) const;

  void DumpX86Flags(std::ostream& os, uint32_t flags) const;
  // Print some of the information from the status register (CPSR on ARMv7, PSTATE on ARMv8).
  template <typename RegisterType>
  void DumpArmStatusRegister(std::ostream& os, RegisterType status_register) const;

  mcontext_t& context;
};

void UContext::Dump(std::ostream& os) const {
#if defined(__APPLE__) && defined(__i386__)
  DumpRegister32(os, "eax", context->__ss.__eax);
  DumpRegister32(os, "ebx", context->__ss.__ebx);
  DumpRegister32(os, "ecx", context->__ss.__ecx);
  DumpRegister32(os, "edx", context->__ss.__edx);
  os << '\n';

  DumpRegister32(os, "edi", context->__ss.__edi);
  DumpRegister32(os, "esi", context->__ss.__esi);
  DumpRegister32(os, "ebp", context->__ss.__ebp);
  DumpRegister32(os, "esp", context->__ss.__esp);
  os << '\n';

  DumpRegister32(os, "eip", context->__ss.__eip);
  os << "                   ";
  DumpRegister32(os, "eflags", context->__ss.__eflags);
  DumpX86Flags(os, context->__ss.__eflags);
  os << '\n';

  DumpRegister32(os, "cs",  context->__ss.__cs);
  DumpRegister32(os, "ds",  context->__ss.__ds);
  DumpRegister32(os, "es",  context->__ss.__es);
  DumpRegister32(os, "fs",  context->__ss.__fs);
  os << '\n';
  DumpRegister32(os, "gs",  context->__ss.__gs);
  DumpRegister32(os, "ss",  context->__ss.__ss);
#elif defined(__linux__) && defined(__i386__)
  DumpRegister32(os, "eax", context.gregs[REG_EAX]);
  DumpRegister32(os, "ebx", context.gregs[REG_EBX]);
  DumpRegister32(os, "ecx", context.gregs[REG_ECX]);
  DumpRegister32(os, "edx", context.gregs[REG_EDX]);
  os << '\n';

  DumpRegister32(os, "edi", context.gregs[REG_EDI]);
  DumpRegister32(os, "esi", context.gregs[REG_ESI]);
  DumpRegister32(os, "ebp", context.gregs[REG_EBP]);
  DumpRegister32(os, "esp", context.gregs[REG_ESP]);
  os << '\n';

  DumpRegister32(os, "eip", context.gregs[REG_EIP]);
  os << "                   ";
  DumpRegister32(os, "eflags", context.gregs[REG_EFL]);
  DumpX86Flags(os, context.gregs[REG_EFL]);
  os << '\n';

  DumpRegister32(os, "cs",  context.gregs[REG_CS]);
  DumpRegister32(os, "ds",  context.gregs[REG_DS]);
  DumpRegister32(os, "es",  context.gregs[REG_ES]);
  DumpRegister32(os, "fs",  context.gregs[REG_FS]);
  os << '\n';
  DumpRegister32(os, "gs",  context.gregs[REG_GS]);
  DumpRegister32(os, "ss",  context.gregs[REG_SS]);
#elif defined(__linux__) && defined(__x86_64__)
  DumpRegister64(os, "rax", context.gregs[REG_RAX]);
  DumpRegister64(os, "rbx", context.gregs[REG_RBX]);
  DumpRegister64(os, "rcx", context.gregs[REG_RCX]);
  DumpRegister64(os, "rdx", context.gregs[REG_RDX]);
  os << '\n';

  DumpRegister64(os, "rdi", context.gregs[REG_RDI]);
  DumpRegister64(os, "rsi", context.gregs[REG_RSI]);
  DumpRegister64(os, "rbp", context.gregs[REG_RBP]);
  DumpRegister64(os, "rsp", context.gregs[REG_RSP]);
  os << '\n';

  DumpRegister64(os, "r8 ", context.gregs[REG_R8]);
  DumpRegister64(os, "r9 ", context.gregs[REG_R9]);
  DumpRegister64(os, "r10", context.gregs[REG_R10]);
  DumpRegister64(os, "r11", context.gregs[REG_R11]);
  os << '\n';

  DumpRegister64(os, "r12", context.gregs[REG_R12]);
  DumpRegister64(os, "r13", context.gregs[REG_R13]);
  DumpRegister64(os, "r14", context.gregs[REG_R14]);
  DumpRegister64(os, "r15", context.gregs[REG_R15]);
  os << '\n';

  DumpRegister64(os, "rip", context.gregs[REG_RIP]);
  os << "   ";
  DumpRegister32(os, "eflags", context.gregs[REG_EFL]);
  DumpX86Flags(os, context.gregs[REG_EFL]);
  os << '\n';

  DumpRegister32(os, "cs",  (context.gregs[REG_CSGSFS]) & 0x0FFFF);
  DumpRegister32(os, "gs",  (context.gregs[REG_CSGSFS] >> 16) & 0x0FFFF);
  DumpRegister32(os, "fs",  (context.gregs[REG_CSGSFS] >> 32) & 0x0FFFF);
  os << '\n';
#elif defined(__linux__) && defined(__arm__)
  DumpRegister32(os, "r0", context.arm_r0);
  DumpRegister32(os, "r1", context.arm_r1);
  DumpRegister32(os, "r2", context.arm_r2);
  DumpRegister32(os, "r3", context.arm_r3);
  os << '\n';

  DumpRegister32(os, "r4", context.arm_r4);
  DumpRegister32(os, "r5", context.arm_r5);
  DumpRegister32(os, "r6", context.arm_r6);
  DumpRegister32(os, "r7", context.arm_r7);
  os << '\n';

  DumpRegister32(os, "r8", context.arm_r8);
  DumpRegister32(os, "r9", context.arm_r9);
  DumpRegister32(os, "r10", context.arm_r10);
  DumpRegister32(os, "fp", context.arm_fp);
  os << '\n';

  DumpRegister32(os, "ip", context.arm_ip);
  DumpRegister32(os, "sp", context.arm_sp);
  DumpRegister32(os, "lr", context.arm_lr);
  DumpRegister32(os, "pc", context.arm_pc);
  os << '\n';

  DumpRegister32(os, "cpsr", context.arm_cpsr);
  DumpArmStatusRegister(os, context.arm_cpsr);
  os << '\n';
#elif defined(__linux__) && defined(__aarch64__)
  for (size_t i = 0; i <= 30; ++i) {
    std::string reg_name = "x" + std::to_string(i);
    DumpRegister64(os, reg_name.c_str(), context.regs[i]);
    if (i % 4 == 3) {
      os << '\n';
    }
  }
  os << '\n';

  DumpRegister64(os, "sp", context.sp);
  DumpRegister64(os, "pc", context.pc);
  os << '\n';

  DumpRegister64(os, "pstate", context.pstate);
  DumpArmStatusRegister(os, context.pstate);
  os << '\n';
#else
  // TODO: Add support for MIPS32 and MIPS64.
  os << "Unknown architecture/word size/OS in ucontext dump";
#endif
}

void UContext::DumpRegister32(std::ostream& os, const char* name, uint32_t value) const {
  os << StringPrintf(" %6s: 0x%08x", name, value);
}

void UContext::DumpRegister64(std::ostream& os, const char* name, uint64_t value) const {
  os << StringPrintf(" %6s: 0x%016" PRIx64, name, value);
}

void UContext::DumpX86Flags(std::ostream& os, uint32_t flags) const {
  os << " [";
  if ((flags & (1 << 0)) != 0) {
    os << " CF";
  }
  if ((flags & (1 << 2)) != 0) {
    os << " PF";
  }
  if ((flags & (1 << 4)) != 0) {
    os << " AF";
  }
  if ((flags & (1 << 6)) != 0) {
    os << " ZF";
  }
  if ((flags & (1 << 7)) != 0) {
    os << " SF";
  }
  if ((flags & (1 << 8)) != 0) {
    os << " TF";
  }
  if ((flags & (1 << 9)) != 0) {
    os << " IF";
  }
  if ((flags & (1 << 10)) != 0) {
    os << " DF";
  }
  if ((flags & (1 << 11)) != 0) {
    os << " OF";
  }
  os << " ]";
}

template <typename RegisterType>
void UContext::DumpArmStatusRegister(std::ostream& os, RegisterType status_register) const {
  // Condition flags.
  constexpr RegisterType kFlagV = 1U << 28;
  constexpr RegisterType kFlagC = 1U << 29;
  constexpr RegisterType kFlagZ = 1U << 30;
  constexpr RegisterType kFlagN = 1U << 31;

  os << " [";
  if ((status_register & kFlagN) != 0) {
    os << " N";
  }
  if ((status_register & kFlagZ) != 0) {
    os << " Z";
  }
  if ((status_register & kFlagC) != 0) {
    os << " C";
  }
  if ((status_register & kFlagV) != 0) {
    os << " V";
  }
  os << " ]";
}

int GetTimeoutSignal() {
#if defined(__APPLE__)
  // Mac does not support realtime signals.
  UNUSED(kUseSigRTTimeout);
  return -1;
#else
  return kUseSigRTTimeout ? (SIGRTMIN + 2) : -1;
#endif
}

static bool IsTimeoutSignal(int signal_number) {
  return signal_number == GetTimeoutSignal();
}

#if defined(__APPLE__)
// On macOS, clang complains about art::HandleUnexpectedSignalCommon's
// stack frame size being too large; disable that warning locally.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wframe-larger-than="
#endif

// Returns whether the specified pc points to code from JIT code cache.
static bool IsInJitCode(uintptr_t pc) {
  Runtime* runtime = Runtime::Current();
  const void* code_ptr = reinterpret_cast<const void*>(pc);
  if (runtime->GetJITOptions()->UseJitCompilation() &&
      runtime->GetJit()->GetCodeCache()->ContainsPc(code_ptr)) {
    return true;
  }
  return false;
}

// Method checks that a valid stack frame is set up.
static bool CheckValidStackFrame(ArtMethod* expected_method,
                                 OatQuickMethodHeader* expected_method_hdr,
                                 uintptr_t sp)
    NO_THREAD_SAFETY_ANALYSIS {
  ArtMethod** frame = reinterpret_cast<ArtMethod**>(sp);
  ArtMethod* top_method = *frame;
  // Check that the pointer on the top of the stack
  // is equals to the expected_method.
  if (top_method != expected_method) {
    return false;
  }
  uintptr_t ret = *reinterpret_cast<uintptr_t*>(
      sp + expected_method_hdr->GetFrameInfo().GetReturnPcOffset());
  ArtMethod** prev_frame = reinterpret_cast<ArtMethod**>(
      sp + expected_method_hdr->GetFrameSizeInBytes());
  ArtMethod* caller = *prev_frame;
  // Check a pointer from the top of the previous frame points to
  // a valid art method.
  if (caller == nullptr || !FaultManager::CheckArtMethod(caller)) {
    return false;
  }
  // Check the previous art method contains the return address
  // from the current frame.
  const OatQuickMethodHeader* caller_hdr = caller->GetOatQuickMethodHeader(ret);
  return caller_hdr != nullptr && caller_hdr->Contains(ret);
}

namespace {
// The class prints runtime information about java methods in the stack.
// The pc for the first frame is obtained from signal context and is passed to the ctor.
// Other pc are obtained from quick frames.
class StackDumpVisitor: public StackVisitor {
 public:
  StackDumpVisitor(uintptr_t pc, Thread* self, std::ostream& stream)
      : StackVisitor(self, nullptr, StackVisitor::StackWalkKind::kSkipInlinedFrames, false),
        frame_num_(0),
        pc_(pc),
        stream_(stream) {
  }

  bool VisitFrame() REQUIRES_SHARED(Locks::mutator_lock_) {
    ArtMethod* method = GetMethod();
    const OatQuickMethodHeader* hdr = nullptr;
    stream_ << "\t#" << frame_num_ << ": " << method->PrettyMethod();
    if (IsShadowFrame()) {
      pc_ = 0;
      // Print dex_pc for shadow frames.
      stream_ << '+' << GetDexPc(false);
    } else {
      if (pc_ == 0) {
        pc_ = GetCurrentQuickFramePc();
      }
      hdr = method->GetOatQuickMethodHeader(pc_);
      if (hdr != nullptr) {
        // Print the offset from the start of a method.
        stream_ << '+' << pc_ - reinterpret_cast<uintptr_t>(hdr->GetCode());
      } else {
        // Cannot find method header. Print pc.
        stream_ << pc_;
      }
    }
    if (!IsShadowFrame() && hdr != nullptr) {
      // Print additional information about compiled code.
      stream_ << " (code: " << static_cast<const void*>(hdr->GetCode())
          << ", code size: " << hdr->GetCodeSize()
          << ", frame size: " << hdr->GetFrameSizeInBytes()
          << ")";
    }
    stream_ << "\n";
    ++frame_num_;
    return true;
  }

 private:
  size_t frame_num_;
  uintptr_t pc_;
  std::ostream& stream_;
};
}  // namespace

static void DumpJavaBacktrace(uintptr_t pc, uintptr_t frame, std::ostream& stream)
    NO_THREAD_SAFETY_ANALYSIS {
  stream << "\nJava backtrace:\n";
  Thread* self = Thread::Current();
  // A signal is received at unexpcted moment and runtime may not set the stack frame
  // in the top ManagedStack. Set the frame and call WalkStack.
  self->SetTopOfStack(reinterpret_cast<ArtMethod**>(frame));
  StackDumpVisitor dumper(pc, self, stream);
  dumper.WalkStack();
}

// The method prints information about jit compiled ArtMethods from the stack.
static void DumpArtMethodsFromStack(uintptr_t sp, std::ostream& stream)
    NO_THREAD_SAFETY_ANALYSIS {
  // Maximum number of methods to print.
  static constexpr size_t MAX_METHODS = 20;
  Thread* self = Thread::Current();
  // Find the bound of Java stack. Runtime creates instances of ManagedStack
  // on the stack, so the last one ManageStack should be the bound of
  // Java stack. There are no ArtMethods should be located behind
  // the bound.
  const ManagedStack* stack = self->GetManagedStack();
  while (stack->GetLink() != nullptr) {
    stack = stack->GetLink();
  }
  uintptr_t bound = reinterpret_cast<uintptr_t>(stack);
  Runtime* runtime = Runtime::Current();
  jit::JitCodeCache* code_cache = runtime->GetJit()->GetCodeCache();
  size_t num_printed_methods = 0;

  stream << "\nJIT compiled methods on the stack:\n";
  // Iterate throught the stack as array of pointers.
  while (sp < bound && num_printed_methods < MAX_METHODS) {
    const void* ptr = *reinterpret_cast<const void**>(sp);
    // Check whether this pointer points to code cache and find
    // corresponding ArtMethod.
    auto code_and_method =
        code_cache->LookupMethod(reinterpret_cast<uintptr_t>(ptr));
    ArtMethod* method = code_and_method.second;
    if (method != nullptr) {
      const void* code_ptr = code_and_method.first;
      OatQuickMethodHeader* hdr =
          OatQuickMethodHeader::FromCodePointer(code_ptr);
      stream << "\t" << reinterpret_cast<void*>(sp) << ": " << ptr
             << " " << method->PrettyMethod()
             << " (" << reinterpret_cast<void*>(method) << ")"
             << " code: " << code_ptr << ", code size: " << hdr->GetCodeSize()
             << " frame size: " << hdr->GetFrameSizeInBytes() << '\n';
      ++num_printed_methods;
    }
    sp += static_cast<uintptr_t>(kRuntimePointerSize);
  }
}

static void DumpJitInfo(uintptr_t pc, void* raw_context, std::ostream& stream)
    NO_THREAD_SAFETY_ANALYSIS {
  Runtime* runtime = Runtime::Current();
  // Find a method which contains the pc.
  auto code_and_method = runtime->GetJit()->GetCodeCache()->LookupMethod(pc);
  ArtMethod* fault_method = code_and_method.second;
  if (fault_method == nullptr) {
    return;
  }

  const void* code_ptr = code_and_method.first;
  OatQuickMethodHeader* hdr = OatQuickMethodHeader::FromEntryPoint(code_ptr);
  stream << "*** *** *** *** *** *** *** *** *** *** *** *** *** *** *** ***\n"
         << "Fault method is from JIT code cache\n"
         << "Fault method: " << fault_method->PrettyMethod()
         << " (address: " << static_cast<void*>(fault_method)
         << ", code: " << code_ptr
         << ", code size: " << hdr->GetCodeSize()
         << ", frame size: " << hdr->GetFrameInfo().FrameSizeInBytes()
         << ")\n\n";
  stream << "JIT code cache info:\n";
  // Print JIT code cache statistics.
  runtime->GetJit()->GetCodeCache()->Dump(stream);
  uintptr_t sp = FaultManager::GetSp(raw_context);
  if (CheckValidStackFrame(fault_method, hdr, sp)) {
    // Stack frame is valid. Print java backtrace.
    DumpJavaBacktrace(pc, sp, stream);
  } else {
    // In case we cannot recover the stack frame
    // try to recognize art methods in the stack and print
    // information about them.
    DumpArtMethodsFromStack(sp, stream);
  }
  stream << std::flush;
}

void HandleUnexpectedSignalCommon(int signal_number,
                                  siginfo_t* info,
                                  void* raw_context,
                                  bool handle_timeout_signal,
                                  bool dump_on_stderr,
                                  bool running_on_linux) {
  static bool handling_unexpected_signal = false;
  if (handling_unexpected_signal) {
    LogHelper::LogLineLowStack(__FILE__,
                               __LINE__,
                               ::android::base::FATAL_WITHOUT_ABORT,
                               "HandleUnexpectedSignal reentered\n");
    if (handle_timeout_signal) {
      if (IsTimeoutSignal(signal_number)) {
        // Ignore a recursive timeout.
        return;
      }
    }
    _exit(1);
  }
  handling_unexpected_signal = true;

  std::ostringstream jit_info;
  uintptr_t pc = FaultManager::GetPc(raw_context);
  if (IsInJitCode(pc)) {
    // Something unexpected is happend in the jit code.
    // Dump information about jit code cache and the stack
    if (!running_on_linux) {
      if (dump_on_stderr) {
        DumpJitInfo(pc, raw_context, std::cerr);
      } else {
        DumpJitInfo(pc, raw_context, LOG_STREAM(FATAL_WITHOUT_ABORT));
      }
      return;
    } else {
      DumpJitInfo(pc, raw_context, jit_info);
    }
  }

  gAborting++;  // set before taking any locks
  MutexLock mu(Thread::Current(), *Locks::unexpected_signal_lock_);

  auto logger = [&](auto& stream) {
    bool has_address = (signal_number == SIGILL || signal_number == SIGBUS ||
                        signal_number == SIGFPE || signal_number == SIGSEGV);
    OsInfo os_info;
    const char* cmd_line = GetCmdLine();
    if (cmd_line == nullptr) {
      cmd_line = "<unset>";  // Because no-one called InitLogging.
    }
    pid_t tid = GetTid();
    std::string thread_name(GetThreadName(tid));
    UContext thread_context(raw_context);
    Backtrace thread_backtrace(raw_context);

    stream << "*** *** *** *** *** *** *** *** *** *** *** *** *** *** *** ***" << std::endl
           << StringPrintf("Fatal signal %d (%s), code %d (%s)",
                             signal_number,
                             GetSignalName(signal_number),
                             info->si_code,
                             GetSignalCodeName(signal_number, info->si_code))
           << (has_address ? StringPrintf(" fault addr %p", info->si_addr) : "") << std::endl
           << "OS: " << Dumpable<OsInfo>(os_info) << std::endl
           << "Cmdline: " << cmd_line << std::endl
           << "Thread: " << tid << " \"" << thread_name << "\"" << std::endl
           << "Registers:\n" << Dumpable<UContext>(thread_context) << std::endl
           << "Backtrace:\n" << Dumpable<Backtrace>(thread_backtrace) << std::endl
           << jit_info.str();
    stream << std::flush;
  };

  if (dump_on_stderr) {
    // Note: We are using cerr directly instead of LOG macros to ensure even just partial output
    //       makes it out. That means we lose the "dalvikvm..." prefix, but that is acceptable
    //       considering this is an abort situation.
    logger(std::cerr);
  } else {
    logger(LOG_STREAM(FATAL_WITHOUT_ABORT));
  }
  if (kIsDebugBuild && signal_number == SIGSEGV) {
    PrintFileToLog("/proc/self/maps", LogSeverity::FATAL_WITHOUT_ABORT);
  }

  Runtime* runtime = Runtime::Current();
  if (runtime != nullptr) {
    if (handle_timeout_signal && IsTimeoutSignal(signal_number)) {
      // Special timeout signal. Try to dump all threads.
      // Note: Do not use DumpForSigQuit, as that might disable native unwind, but the native parts
      //       are of value here.
      runtime->GetThreadList()->Dump(std::cerr, kDumpNativeStackOnTimeout);
      std::cerr << std::endl;
    }

    if (dump_on_stderr) {
      std::cerr << "Fault message: " << runtime->GetFaultMessage() << std::endl;
    } else {
      LOG(FATAL_WITHOUT_ABORT) << "Fault message: " << runtime->GetFaultMessage();
    }
  }
}

#if defined(__APPLE__)
#pragma GCC diagnostic pop
#endif

void InitPlatformSignalHandlersCommon(void (*newact)(int, siginfo_t*, void*),
                                      struct sigaction* oldact,
                                      bool handle_timeout_signal) {
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  sigemptyset(&action.sa_mask);
  action.sa_sigaction = newact;
  // Use the three-argument sa_sigaction handler.
  action.sa_flags |= SA_SIGINFO;
  // Use the alternate signal stack so we can catch stack overflows.
  action.sa_flags |= SA_ONSTACK;

  int rc = 0;
  rc += sigaction(SIGABRT, &action, oldact);
  rc += sigaction(SIGBUS, &action, oldact);
  rc += sigaction(SIGFPE, &action, oldact);
  rc += sigaction(SIGILL, &action, oldact);
  rc += sigaction(SIGPIPE, &action, oldact);
  rc += sigaction(SIGSEGV, &action, oldact);
#if defined(SIGSTKFLT)
  rc += sigaction(SIGSTKFLT, &action, oldact);
#endif
  rc += sigaction(SIGTRAP, &action, oldact);
  // Special dump-all timeout.
  if (handle_timeout_signal && GetTimeoutSignal() != -1) {
    rc += sigaction(GetTimeoutSignal(), &action, oldact);
  }
  CHECK_EQ(rc, 0);
}

}  // namespace art
