/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include "runtime.h"

#include <signal.h>

#include <cinttypes>
#include <iostream>
#include <string>

#include "android-base/stringprintf.h"

#include "base/dumpable.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/mutex.h"
#include "native_stack_dump.h"
#include "runtime_common.h"
#include "thread-inl.h"
#include "thread_list.h"

namespace art {

using android::base::StringPrintf;

static constexpr bool kUseSigRTTimeout = true;
static constexpr bool kDumpNativeStackOnTimeout = true;

// Return the signal number we recognize as timeout. -1 means not active/supported.
static int GetTimeoutSignal() {
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

void HandleUnexpectedSignal(int signal_number, siginfo_t* info, void* raw_context) {
  static bool handlingUnexpectedSignal = false;
  if (handlingUnexpectedSignal) {
    LogHelper::LogLineLowStack(__FILE__,
                               __LINE__,
                               ::android::base::FATAL_WITHOUT_ABORT,
                               "HandleUnexpectedSignal reentered\n");
    if (IsTimeoutSignal(signal_number)) {
      // Ignore a recursive timeout.
      return;
    }
    _exit(1);
  }
  handlingUnexpectedSignal = true;

  gAborting++;  // set before taking any locks
  MutexLock mu(Thread::Current(), *Locks::unexpected_signal_lock_);

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

  // Note: We are using cerr directly instead of LOG macros to ensure even just partial output
  //       makes it out. That means we lose the "dalvikvm..." prefix, but that is acceptable
  //       considering this is an abort situation.

  std::cerr << "*** *** *** *** *** *** *** *** *** *** *** *** *** *** *** ***\n"
            << StringPrintf("Fatal signal %d (%s), code %d (%s)",
                            signal_number, GetSignalName(signal_number),
                            info->si_code,
                            GetSignalCodeName(signal_number, info->si_code))
            << (has_address ? StringPrintf(" fault addr %p", info->si_addr) : "") << std::endl
            << "OS: " << Dumpable<OsInfo>(os_info) << std::endl
            << "Cmdline: " << cmd_line << std::endl
            << "Thread: " << tid << " \"" << thread_name << "\"" << std::endl
            << "Registers:\n" << Dumpable<UContext>(thread_context) << std::endl
            << "Backtrace:\n" << Dumpable<Backtrace>(thread_backtrace) << std::endl;
  if (kIsDebugBuild && signal_number == SIGSEGV) {
    PrintFileToLog("/proc/self/maps", LogSeverity::FATAL_WITHOUT_ABORT);
  }
  Runtime* runtime = Runtime::Current();
  if (runtime != nullptr) {
    if (IsTimeoutSignal(signal_number)) {
      // Special timeout signal. Try to dump all threads.
      // Note: Do not use DumpForSigQuit, as that might disable native unwind, but the native parts
      //       are of value here.
      runtime->GetThreadList()->Dump(std::cerr, kDumpNativeStackOnTimeout);
      std::cerr << std::endl;
    }
    std::cerr << "Fault message: " << runtime->GetFaultMessage() << std::endl;
  }
  if (getenv("debug_db_uid") != nullptr || getenv("art_wait_for_gdb_on_crash") != nullptr) {
    std::cerr << "********************************************************\n"
              << "* Process " << getpid() << " thread " << tid << " \"" << thread_name
              << "\""
              << " has been suspended while crashing.\n"
              << "* Attach gdb:\n"
              << "*     gdb -p " << tid << "\n"
              << "********************************************************"
              << std::endl;
    // Wait for debugger to attach.
    while (true) {
    }
  }
#ifdef __linux__
  // Remove our signal handler for this signal...
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  sigemptyset(&action.sa_mask);
  action.sa_handler = SIG_DFL;
  sigaction(signal_number, &action, nullptr);
  // ...and re-raise so we die with the appropriate status.
  kill(getpid(), signal_number);
#else
  exit(EXIT_FAILURE);
#endif
}

void Runtime::InitPlatformSignalHandlers() {
  // On the host, we don't have debuggerd to dump a stack for us when something unexpected happens.
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  sigemptyset(&action.sa_mask);
  action.sa_sigaction = HandleUnexpectedSignal;
  // Use the three-argument sa_sigaction handler.
  action.sa_flags |= SA_SIGINFO;
  // Use the alternate signal stack so we can catch stack overflows.
  action.sa_flags |= SA_ONSTACK;

  int rc = 0;
  rc += sigaction(SIGABRT, &action, nullptr);
  rc += sigaction(SIGBUS, &action, nullptr);
  rc += sigaction(SIGFPE, &action, nullptr);
  rc += sigaction(SIGILL, &action, nullptr);
  rc += sigaction(SIGPIPE, &action, nullptr);
  rc += sigaction(SIGSEGV, &action, nullptr);
#if defined(SIGSTKFLT)
  rc += sigaction(SIGSTKFLT, &action, nullptr);
#endif
  rc += sigaction(SIGTRAP, &action, nullptr);
  // Special dump-all timeout.
  if (GetTimeoutSignal() != -1) {
    rc += sigaction(GetTimeoutSignal(), &action, nullptr);
  }
  CHECK_EQ(rc, 0);
}

}  // namespace art
