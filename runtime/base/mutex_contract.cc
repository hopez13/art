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

#include "mutex.h"

#include <errno.h>
#include <sys/time.h>

#include <set>

#include "android-base/stringprintf.h"

#include "base/atomic.h"
#include "base/logging.h"
#include "base/systrace.h"
#include "base/time_utils.h"
#include "base/value_object.h"
#include "mutex-inl.h"
#include "thread-inl.h"

namespace art {

using android::base::StringPrintf;

static inline void CheckUnattachedThread(LockLevel level) NO_THREAD_SAFETY_ANALYSIS {
  // The check below enumerates the cases where we expect not to be able to sanity check locks
  // on a thread. Lock checking is disabled to avoid deadlock when checking shutdown lock.
  // TODO: tighten this check.
  if (kDebugLocking) {
    CHECK(!Locks::IsSafeToCallAbortRacy() ||
          // Used during thread creation to avoid races with runtime shutdown. Thread::Current not
          // yet established.
          level == kRuntimeShutdownLock ||
          // Thread Ids are allocated/released before threads are established.
          level == kAllocatedThreadIdsLock ||
          // Thread LDT's are initialized without Thread::Current established.
          level == kModifyLdtLock ||
          // Threads are unregistered while holding the thread list lock, during this process they
          // no longer exist and so we expect an unlock with no self.
          level == kThreadListLock ||
          // Ignore logging which may or may not have set up thread data structures.
          level == kLoggingLock ||
          // When transitioning from suspended to runnable, a daemon thread might be in
          // a situation where the runtime is shutting down. To not crash our debug locking
          // mechanism we just pass null Thread* to the MutexLock during that transition
          // (see Thread::TransitionFromSuspendedToRunnable).
          level == kThreadSuspendCountLock ||
          // Avoid recursive death.
          level == kAbortLock ||
          // Locks at the absolute top of the stack can be locked at any time.
          level == kTopLockLevel) << level;
  }
}

pid_t MutexContract::GetTid(const Thread* self) {
  return self->GetTid();
}

Thread* MutexContract::CurrentThread() {
  return Thread::Current();
}

// Helper to allow checking shutdown while locking for thread safety.
bool MutexContract::IsSafeToCallAbortSafe() {
  MutexLock mu(CurrentThread(), *Locks::runtime_shutdown_lock_);
  return IsSafeToCallAbortRacy();
}

bool MutexContract::IsSafeToCallAbortRacy() {
  return Locks::IsSafeToCallAbortRacy();
}

bool MutexContract::IsNullOrCurrentThread(const Thread* self) {
  return self == nullptr || self == CurrentThread();
}

bool MutexContract::IsCurrentMutexAtLevel(const Thread* self,
                                          LockLevel level,
                                          const BaseMutex* mutex) {
  return (self->GetHeldMutex(level) == mutex);
}

void MutexContract::SetCurrentMutexAtLevel(Thread* self,
                                           LockLevel level,
                                           BaseMutex* mutex) {
  self->SetHeldMutex(level, mutex);
}

void MutexContract::CheckSafeToWait(const Thread* self,
                                    LockLevel level,
                                    const BaseMutex* mutex) {
  if (self == nullptr) {
    CheckUnattachedThread(level);
    return;
  }
  if (kDebugLocking) {
    CHECK(self->GetHeldMutex(level) == mutex || level == kMonitorLock)
        << "Waiting on unacquired mutex: " << mutex->GetName();
    bool bad_mutexes_held = false;
    for (int i = kLockLevelCount - 1; i >= 0; --i) {
      if (i != level) {
        BaseMutex* held_mutex = self->GetHeldMutex(static_cast<LockLevel>(i));
        // We allow the thread to wait even if the user_code_suspension_lock_ is held so long as we
        // are some thread's resume_cond_ (level == kThreadSuspendCountLock). This just means that
        // gc or some other internal process is suspending the thread while it is trying to suspend
        // some other thread. So long as the current thread is not being suspended by a
        // SuspendReason::kForUserCode (which needs the user_code_suspension_lock_ to clear) this is
        // fine.
        if (held_mutex == Locks::user_code_suspension_lock_ && level == kThreadSuspendCountLock) {
          // No thread safety analysis is fine since we have both the user_code_suspension_lock_
          // from the line above and the ThreadSuspendCountLock since it is our level. We use this
          // lambda to avoid having to annotate the whole function as NO_THREAD_SAFETY_ANALYSIS.
          auto is_suspending_for_user_code = [self]() NO_THREAD_SAFETY_ANALYSIS {
            return self->GetUserCodeSuspendCount() != 0;
          };
          if (is_suspending_for_user_code()) {
            LOG(ERROR) << "Holding \"" << held_mutex->GetName() << "\" "
                      << "(level " << LockLevel(i) << ") while performing wait on "
                      << "\"" << mutex->GetName() << "\" (level " << level << ") "
                      << "with SuspendReason::kForUserCode pending suspensions";
            bad_mutexes_held = true;
          }
        } else if (held_mutex != nullptr) {
          LOG(ERROR) << "Holding \"" << held_mutex->GetName() << "\" "
                     << "(level " << LockLevel(i) << ") while performing wait on "
                     << "\"" << mutex->GetName() << "\" (level " << level << ")";
          bad_mutexes_held = true;
        }
      }
    }
    if (gAborting == 0) {  // Avoid recursive aborts.
      CHECK(!bad_mutexes_held) << mutex;
    }
  }
}

void MutexContract::CheckEmptyCheckpoint(Thread* self) {
  self->CheckEmptyCheckpointFromMutex();
}

void MutexContract::CheckAndLogInvalidThreadNames(const Thread* self,
                                                  LockLevel level,
                                                  const BaseMutex* mutex) {
  if (!IsNullOrCurrentThread(self)) {
    std::string name1 = "<null>";
    std::string name2 = "<null>";
    if (self != nullptr) {
      self->GetThreadName(name1);
    }
    if (CurrentThread() != nullptr) {
      CurrentThread()->GetThreadName(name2);
    }
    LOG(FATAL) << mutex->GetName() << " level=" << level << " self=" << name1
               << " CurrentThread()=" << name2;
  }
}

void MutexContract::RegisterAsLocked(Thread* self,
                                     LockLevel level,
                                     BaseMutex* mutex) {
  if (UNLIKELY(self == nullptr)) {
    CheckUnattachedThread(level);
    return;
  }
  if (kDebugLocking) {
    // Check if a bad Mutex of this level or lower is held.
    bool bad_mutexes_held = false;
    // Specifically allow a kTopLockLevel lock to be gained when the current thread holds the
    // mutator_lock_ exclusive. This is because we suspending when holding locks at this level is
    // not allowed and if we hold the mutator_lock_ exclusive we must unsuspend stuff eventually
    // so there are no deadlocks.
    if (level == kTopLockLevel &&
        Locks::mutator_lock_->IsSharedHeld(self) &&
        !Locks::mutator_lock_->IsExclusiveHeld(self)) {
      LOG(ERROR) << "Lock level violation: holding \"" << Locks::mutator_lock_->GetName() << "\" "
                  << "(level " << kMutatorLock << " - " << static_cast<int>(kMutatorLock)
                  << ") non-exclusive while locking \"" << mutex->GetName() << "\" "
                  << "(level " << level << " - " << static_cast<int>(level) << ") a top level"
                  << "mutex. This is not allowed.";
      bad_mutexes_held = true;
    } else if (mutex == Locks::mutator_lock_ && self->GetHeldMutex(kTopLockLevel) != nullptr) {
      LOG(ERROR) << "Lock level violation. Locking mutator_lock_ while already having a "
                 << "kTopLevelLock (" << self->GetHeldMutex(kTopLockLevel)->GetName() << "held is "
                 << "not allowed.";
      bad_mutexes_held = true;
    }
    for (int i = level; i >= 0; --i) {
      LockLevel lock_level_i = static_cast<LockLevel>(i);
      BaseMutex* held_mutex = self->GetHeldMutex(lock_level_i);
      if (level == kTopLockLevel &&
          lock_level_i == kMutatorLock &&
          Locks::mutator_lock_->IsExclusiveHeld(self)) {
        // This is checked above.
        continue;
      } else if (UNLIKELY(held_mutex != nullptr) && lock_level_i != kAbortLock) {
        LOG(ERROR) << "Lock level violation: holding \"" << held_mutex->GetName() << "\" "
                   << "(level " << lock_level_i << " - " << i
                   << ") while locking \"" << mutex->GetName() << "\" "
                   << "(level " << level << " - " << static_cast<int>(level) << ")";
        if (lock_level_i > kAbortLock) {
          // Only abort in the check below if this is more than abort level lock.
          bad_mutexes_held = true;
        }
      }
    }
    if (gAborting == 0) {  // Avoid recursive aborts.
      CHECK(!bad_mutexes_held);
    }
  }
  // Don't record monitors as they are outside the scope of analysis. They may be inspected off of
  // the monitor list.
  if (level != kMonitorLock) {
    SetCurrentMutexAtLevel(self, level, mutex);
  }
}

void MutexContract::RegisterAsUnlocked(Thread* self,
                                       LockLevel level,
                                       const BaseMutex* mutex) {
  if (UNLIKELY(self == nullptr)) {
    CheckUnattachedThread(level);
    return;
  }
  if (level != kMonitorLock) {
    if (kDebugLocking && gAborting == 0) {  // Avoid recursive aborts.
      CHECK(IsCurrentMutexAtLevel(self, level, mutex))
          << "Unlocking on unacquired mutex: " << mutex->GetName();
    }
    SetCurrentMutexAtLevel(self, level, nullptr);
  }
}

void MutexContract::LogUnlockFailed(int32_t cur_state, BaseMutex* mutex) {
  // Logging acquires the logging lock, avoid infinite recursion in that case.
  if (mutex != Locks::logging_lock_) {
    LOG(FATAL) << "Unexpected state_ in unlock " << cur_state << " for " << mutex->GetName();
  } else {
    LogHelper::LogLineLowStack(__FILE__,
                               __LINE__,
                               ::android::base::FATAL_WITHOUT_ABORT,
                               StringPrintf("Unexpected state_ %d in unlock for %s",
                                            cur_state, mutex->GetName()).c_str());
    _exit(1);
  }
}

void MutexContract::SleepForeverIfRuntimeDeleted(const Thread* self) {
  if (self != nullptr) {
    JNIEnvExt* const env = self->GetJniEnv();
    if (UNLIKELY(env != nullptr && env->IsRuntimeDeleted())) {
      CHECK(self->IsDaemon());
      // If the runtime has been deleted, then we cannot proceed. Just sleep forever. This may
      // occur for user daemon threads that get a spurious wakeup. This occurs for test 132 with
      // --host and --gdb.
      // After we wake up, the runtime may have been shutdown, which means that this condition may
      // have been deleted. It is not safe to retry the wait.
      SleepForever();
    }
  }
}

}  // namespace art
