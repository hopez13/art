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
#include <sys/utsname.h>

#include <cinttypes>
#include <string>

#include "runtime_common.h"

namespace art {

struct sigaction old_action;

void HandleUnexpectedSignalAndroid(int signal_number, siginfo_t* info, void* raw_context) {
  HandleUnexpectedSignalCommon(signal_number, info, raw_context, /* running_on_linux */ false);

  // Run the old signal handler.
  old_action.sa_sigaction(signal_number, info, raw_context);
}

void Runtime::InitPlatformSignalHandlers() {
  // Enable the signal handler dumping crash information to the logcat
  // when the Android root is not "/system".
  const char* android_root = getenv("ANDROID_ROOT");
  if (android_root != nullptr && strcmp(android_root, "/system") != 0) {
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    sigemptyset(&action.sa_mask);
    action.sa_sigaction = HandleUnexpectedSignalAndroid;
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
