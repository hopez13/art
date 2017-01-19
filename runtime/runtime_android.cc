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

#include <signal.h>
#include <sys/utsname.h>

#include <cinttypes>
#include <string>

#include "android-base/stringprintf.h"

#include "base/logging.h"
#include "base/mutex.h"
#include "runtime_common.h"
#include "thread-inl.h"
#include "utils.h"

namespace art {

using android::base::StringPrintf;

struct sigaction old_action;

void HandleUnexpectedSignal(int signal_number, siginfo_t* info, void* raw_context) {
  static bool handling_unexpected_signal = false;
  if (handling_unexpected_signal) {
    LogHelper::LogLineLowStack(__FILE__,
                               __LINE__,
                               ::android::base::FATAL_WITHOUT_ABORT,
                               "HandleUnexpectedSignal reentered\n");
    _exit(1);
  }
  handling_unexpected_signal = true;

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

  LOG(FATAL_WITHOUT_ABORT)
      << "*** *** *** *** *** *** *** *** *** *** *** *** *** *** *** ***\n"
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
    // Print this out first in case DumpObject faults.
    LOG(FATAL_WITHOUT_ABORT) << "Fault message: " << runtime->GetFaultMessage();
  }
  // Run the old signal handler.
  old_action.sa_sigaction(signal_number, info, raw_context);
}

void Runtime::InitPlatformSignalHandlers() {
  // Enable the signal handler dumping crash information to the logcat
  // when the Android root is not "/system".
  const char* android_root = getenv("ANDROID_ROOT");
  if (android_root != 0 && strcmp(android_root, "/system") != 0) {
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    sigemptyset(&action.sa_mask);
    action.sa_sigaction = HandleUnexpectedSignal;
    // Use the three-argument sa_sigaction handler.
    action.sa_flags |= SA_SIGINFO;
    // Use the alternate signal stack so we can catch stack overflows.
    action.sa_flags |= SA_ONSTACK;

    int rc = 0;
    rc += sigaction(SIGABRT, &action, &old_action);
    rc += sigaction(SIGBUS, &action, &old_action);
    rc += sigaction(SIGFPE, &action, &old_action);
    rc += sigaction(SIGILL, &action, &old_action);
    rc += sigaction(SIGPIPE, &action, &old_action);
    rc += sigaction(SIGSEGV, &action, &old_action);
#if defined(SIGSTKFLT)
    rc += sigaction(SIGSTKFLT, &action, &old_action);
#endif
    rc += sigaction(SIGTRAP, &action, &old_action);
    CHECK_EQ(rc, 0);
  }
}

}  // namespace art
