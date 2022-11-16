/*
 * Copyright (C) 2018 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <dlfcn.h>
#include <pthread.h>
#include <signal.h>
#include <sys/syscall.h>

#include <functional>

#include <gtest/gtest.h>

#include "sigchain.h"

#include "base/common_art_test.h"

#if defined(__clang__) && __has_feature(hwaddress_sanitizer)
#define DISABLE_HWASAN __attribute__((no_sanitize("hwaddress")))
#else
#define DISABLE_HWASAN
#endif

class SigchainTest : public ::testing::Test {
  void SetUp() final {
    art::AddSpecialSignalHandlerFn(SIGSEGV, &action);
  }

  void TearDown() final {
    art::RemoveSpecialSignalHandlerFn(SIGSEGV, action.sc_sigaction);
  }

  art::SigchainAction action = {
      .sc_sigaction = [](int, siginfo_t* info, void*) -> bool {
        return info->si_value.sival_ptr;
      },
      .sc_mask = {},
      .sc_flags = 0,
  };

 protected:
  void RaiseHandled() {
      sigval value;
      value.sival_ptr = &value;
      // pthread_sigqueue would guarantee the signal is delivered to this
      // thread, but it is a nonstandard extension and does not exist in
      // musl.  Gtest is single threaded, and these tests don't create any
      // threads, so sigqueue can be used and will deliver to this thread.
      sigqueue(getpid(), SIGSEGV, value);
  }

  void RaiseUnhandled() {
      sigval value;
      value.sival_ptr = nullptr;
      sigqueue(getpid(), SIGSEGV, value);
  }
};

TEST_F(SigchainTest, sigprocmask_setmask) {
#if 0
  TestSignalBlocking([]() {
    sigset_t mask;
    sigfillset(&mask);
    ASSERT_EQ(0, sigprocmask(SIG_SETMASK, &mask, nullptr));
  });
#endif
}
