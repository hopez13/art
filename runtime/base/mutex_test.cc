/*
 * Copyright (C) 2012 The Android Open Source Project
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

#include "mutex-inl.h"

#include "common_runtime_test.h"

namespace art {

class MutexTest : public CommonRuntimeTest {};

struct MutexTester {
  static void AssertDepth(Mutex& mu, uint32_t expected_depth) {
    ASSERT_EQ(expected_depth, mu.GetDepth());

    // This test is single-threaded, so we also know _who_ should hold the lock.
    if (expected_depth == 0) {
      mu.AssertNotHeld(BaseMutex::CurrentThread());
    } else {
      mu.AssertHeld(BaseMutex::CurrentThread());
    }
  }
};

TEST_F(MutexTest, LockUnlock) {
  Mutex mu("test mutex");
  MutexTester::AssertDepth(mu, 0U);
  mu.Lock(BaseMutex::CurrentThread());
  MutexTester::AssertDepth(mu, 1U);
  mu.Unlock(BaseMutex::CurrentThread());
  MutexTester::AssertDepth(mu, 0U);
}

// GCC has trouble with our mutex tests, so we have to turn off thread safety analysis.
static void TryLockUnlockTest() NO_THREAD_SAFETY_ANALYSIS {
  Mutex mu("test mutex");
  MutexTester::AssertDepth(mu, 0U);
  ASSERT_TRUE(mu.TryLock(BaseMutex::CurrentThread()));
  MutexTester::AssertDepth(mu, 1U);
  mu.Unlock(BaseMutex::CurrentThread());
  MutexTester::AssertDepth(mu, 0U);
}

TEST_F(MutexTest, TryLockUnlock) {
  TryLockUnlockTest();
}

// GCC has trouble with our mutex tests, so we have to turn off thread safety analysis.
static void RecursiveLockUnlockTest() NO_THREAD_SAFETY_ANALYSIS {
  Mutex mu("test mutex", kDefaultMutexLevel, true);
  MutexTester::AssertDepth(mu, 0U);
  mu.Lock(BaseMutex::CurrentThread());
  MutexTester::AssertDepth(mu, 1U);
  mu.Lock(BaseMutex::CurrentThread());
  MutexTester::AssertDepth(mu, 2U);
  mu.Unlock(BaseMutex::CurrentThread());
  MutexTester::AssertDepth(mu, 1U);
  mu.Unlock(BaseMutex::CurrentThread());
  MutexTester::AssertDepth(mu, 0U);
}

TEST_F(MutexTest, RecursiveLockUnlock) {
  RecursiveLockUnlockTest();
}

// GCC has trouble with our mutex tests, so we have to turn off thread safety analysis.
static void RecursiveTryLockUnlockTest() NO_THREAD_SAFETY_ANALYSIS {
  Mutex mu("test mutex", kDefaultMutexLevel, true);
  MutexTester::AssertDepth(mu, 0U);
  ASSERT_TRUE(mu.TryLock(BaseMutex::CurrentThread()));
  MutexTester::AssertDepth(mu, 1U);
  ASSERT_TRUE(mu.TryLock(BaseMutex::CurrentThread()));
  MutexTester::AssertDepth(mu, 2U);
  mu.Unlock(BaseMutex::CurrentThread());
  MutexTester::AssertDepth(mu, 1U);
  mu.Unlock(BaseMutex::CurrentThread());
  MutexTester::AssertDepth(mu, 0U);
}

TEST_F(MutexTest, RecursiveTryLockUnlock) {
  RecursiveTryLockUnlockTest();
}


struct RecursiveLockWait {
  RecursiveLockWait()
      : mu("test mutex", kDefaultMutexLevel, true), cv("test condition variable", mu) {
  }

  Mutex mu;
  ConditionVariable cv;
};

static void* RecursiveLockWaitCallback(void* arg) {
  RecursiveLockWait* state = reinterpret_cast<RecursiveLockWait*>(arg);
  state->mu.Lock(BaseMutex::CurrentThread());
  state->cv.Signal(BaseMutex::CurrentThread());
  state->mu.Unlock(BaseMutex::CurrentThread());
  return nullptr;
}

// GCC has trouble with our mutex tests, so we have to turn off thread safety analysis.
static void RecursiveLockWaitTest() NO_THREAD_SAFETY_ANALYSIS {
  RecursiveLockWait state;
  state.mu.Lock(BaseMutex::CurrentThread());
  state.mu.Lock(BaseMutex::CurrentThread());

  pthread_t pthread;
  int pthread_create_result = pthread_create(&pthread, nullptr, RecursiveLockWaitCallback, &state);
  ASSERT_EQ(0, pthread_create_result);

  state.cv.Wait(BaseMutex::CurrentThread());

  state.mu.Unlock(BaseMutex::CurrentThread());
  state.mu.Unlock(BaseMutex::CurrentThread());
  EXPECT_EQ(pthread_join(pthread, nullptr), 0);
}

// This ensures we don't hang when waiting on a recursively locked mutex,
// which is not supported with bare pthread_mutex_t.
TEST_F(MutexTest, RecursiveLockWait) {
  RecursiveLockWaitTest();
}

TEST_F(MutexTest, SharedLockUnlock) {
  ReaderWriterMutex mu("test rwmutex");
  mu.AssertNotHeld(BaseMutex::CurrentThread());
  mu.AssertNotExclusiveHeld(BaseMutex::CurrentThread());
  mu.SharedLock(BaseMutex::CurrentThread());
  mu.AssertSharedHeld(BaseMutex::CurrentThread());
  mu.AssertNotExclusiveHeld(BaseMutex::CurrentThread());
  mu.SharedUnlock(BaseMutex::CurrentThread());
  mu.AssertNotHeld(BaseMutex::CurrentThread());
}

TEST_F(MutexTest, ExclusiveLockUnlock) {
  ReaderWriterMutex mu("test rwmutex");
  mu.AssertNotHeld(BaseMutex::CurrentThread());
  mu.ExclusiveLock(BaseMutex::CurrentThread());
  mu.AssertSharedHeld(BaseMutex::CurrentThread());
  mu.AssertExclusiveHeld(BaseMutex::CurrentThread());
  mu.ExclusiveUnlock(BaseMutex::CurrentThread());
  mu.AssertNotHeld(BaseMutex::CurrentThread());
}

// GCC has trouble with our mutex tests, so we have to turn off thread safety analysis.
static void SharedTryLockUnlockTest() NO_THREAD_SAFETY_ANALYSIS {
  ReaderWriterMutex mu("test rwmutex");
  mu.AssertNotHeld(BaseMutex::CurrentThread());
  ASSERT_TRUE(mu.SharedTryLock(BaseMutex::CurrentThread()));
  mu.AssertSharedHeld(BaseMutex::CurrentThread());
  mu.SharedUnlock(BaseMutex::CurrentThread());
  mu.AssertNotHeld(BaseMutex::CurrentThread());
}

TEST_F(MutexTest, SharedTryLockUnlock) {
  SharedTryLockUnlockTest();
}

}  // namespace art
