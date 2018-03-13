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
#include <stdint.h>
#include <sys/time.h>

#include <set>

#include "android-base/stringprintf.h"

#include "arch/instruction_set.h"
#include "base/atomic.h"
#include "base/logging.h"
#include "base/safe_map.h"
#include "base/systrace.h"
#include "base/time_utils.h"
#include "base/value_object.h"
#include "mutex-inl.h"

namespace art {

using android::base::StringPrintf;

struct AllMutexData {
  // A guard for all_mutexes_ that's not a mutex (Mutexes must CAS to acquire and busy wait).
  Atomic<const BaseMutex*> all_mutexes_guard;
  // All created mutexes guarded by all_mutexes_guard_.
  std::set<BaseMutex*>* all_mutexes;
  AllMutexData() : all_mutexes(nullptr) {}
};
static struct AllMutexData gAllMutexData[kAllMutexDataSize];

#if ART_USE_FUTEXES
static bool ComputeRelativeTimeSpec(timespec* result_ts, const timespec& lhs, const timespec& rhs) {
  const int32_t one_sec = 1000 * 1000 * 1000;  // one second in nanoseconds.
  result_ts->tv_sec = lhs.tv_sec - rhs.tv_sec;
  result_ts->tv_nsec = lhs.tv_nsec - rhs.tv_nsec;
  if (result_ts->tv_nsec < 0) {
    result_ts->tv_sec--;
    result_ts->tv_nsec += one_sec;
  } else if (result_ts->tv_nsec > one_sec) {
    result_ts->tv_sec++;
    result_ts->tv_nsec -= one_sec;
  }
  return result_ts->tv_sec < 0;
}
#endif

void BaseMutex::BackOff(uint32_t i) {
  static constexpr uint32_t kSpinMax = 10;
  static constexpr uint32_t kYieldMax = 20;
  if (i <= kSpinMax) {
    // TODO: Esp. in very latency-sensitive cases, consider replacing this with an explicit
    // test-and-test-and-set loop in the caller.  Possibly skip entirely on a uniprocessor.
    volatile uint32_t x = 0;
    const uint32_t spin_count = 10 * i;
    for (uint32_t spin = 0; spin < spin_count; ++spin) {
      ++x;  // Volatile; hence should not be optimized away.
    }
    // TODO: Consider adding x86 PAUSE and/or ARM YIELD here.
  } else if (i <= kYieldMax) {
    sched_yield();
  } else {
    NanoSleep(1000ull * (i - kYieldMax));
  }
}

Thread* BaseMutex::CurrentThread() {
  return MutexContract::CurrentThread();
}

class ScopedAllMutexesLock FINAL {
 public:
  explicit ScopedAllMutexesLock(const BaseMutex* mutex) : mutex_(mutex) {
    for (uint32_t i = 0;
         !gAllMutexData->all_mutexes_guard.CompareAndSetWeakAcquire(0, mutex);
         ++i) {
      BaseMutex::BackOff(i);
    }
  }

  ~ScopedAllMutexesLock() {
    DCHECK_EQ(gAllMutexData->all_mutexes_guard.LoadRelaxed(), mutex_);
    gAllMutexData->all_mutexes_guard.StoreRelease(0);
  }

 private:
  const BaseMutex* const mutex_;
};

// Scoped class that generates events at the beginning and end of lock contention.
class ScopedContentionRecorder FINAL : public ValueObject {
 public:
  ScopedContentionRecorder(BaseMutex* mutex, uint64_t blocked_tid, uint64_t owner_tid)
      : mutex_(kLogLockContentions ? mutex : nullptr),
        blocked_tid_(kLogLockContentions ? blocked_tid : 0),
        owner_tid_(kLogLockContentions ? owner_tid : 0),
        start_nano_time_(kLogLockContentions ? NanoTime() : 0) {
    if (ATRACE_ENABLED()) {
      std::string msg = StringPrintf("Lock contention on %s (owner tid: %" PRIu64 ")",
                                     mutex->GetName(), owner_tid);
      ATRACE_BEGIN(msg.c_str());
    }
  }

  ~ScopedContentionRecorder() {
    ATRACE_END();
    if (kLogLockContentions) {
      uint64_t end_nano_time = NanoTime();
      mutex_->RecordContention(blocked_tid_, owner_tid_, end_nano_time - start_nano_time_);
    }
  }

 private:
  BaseMutex* const mutex_;
  const uint64_t blocked_tid_;
  const uint64_t owner_tid_;
  const uint64_t start_nano_time_;
};

BaseMutex::BaseMutex(const char* name, LockLevel level)
    : level_(level),
      name_(name),
      should_respond_to_empty_checkpoint_request_(false) {
  if (kLogLockContentions) {
    ScopedAllMutexesLock mu(this);
    std::set<BaseMutex*>** all_mutexes_ptr = &gAllMutexData->all_mutexes;
    if (*all_mutexes_ptr == nullptr) {
      // We leak the global set of all mutexes to avoid ordering issues in global variable
      // construction/destruction.
      *all_mutexes_ptr = new std::set<BaseMutex*>();
    }
    (*all_mutexes_ptr)->insert(this);
  }
}

BaseMutex::~BaseMutex() {
  if (kLogLockContentions) {
    ScopedAllMutexesLock mu(this);
    gAllMutexData->all_mutexes->erase(this);
  }
}

void BaseMutex::DumpAll(std::ostream& os) {
  if (kLogLockContentions) {
    os << "Mutex logging:\n";
    ScopedAllMutexesLock mu(reinterpret_cast<const BaseMutex*>(-1));
    std::set<BaseMutex*>* all_mutexes = gAllMutexData->all_mutexes;
    if (all_mutexes == nullptr) {
      // No mutexes have been created yet during at startup.
      return;
    }
    typedef std::set<BaseMutex*>::const_iterator It;
    os << "(Contended)\n";
    for (It it = all_mutexes->begin(); it != all_mutexes->end(); ++it) {
      BaseMutex* mutex = *it;
      if (mutex->HasEverContended()) {
        mutex->Dump(os);
        os << "\n";
      }
    }
    os << "(Never contented)\n";
    for (It it = all_mutexes->begin(); it != all_mutexes->end(); ++it) {
      BaseMutex* mutex = *it;
      if (!mutex->HasEverContended()) {
        mutex->Dump(os);
        os << "\n";
      }
    }
  }
}

void BaseMutex::CheckSafeToWait(Thread* self) {
  MutexContract::CheckSafeToWait(self, level_, this);
}

void BaseMutex::ContentionLogData::AddToWaitTime(uint64_t value) {
  if (kLogLockContentions) {
    // Atomically add value to wait_time.
    wait_time.FetchAndAddSequentiallyConsistent(value);
  }
}

void BaseMutex::RecordContention(uint64_t blocked_tid,
                                 uint64_t owner_tid,
                                 uint64_t nano_time_blocked) {
  if (kLogLockContentions) {
    ContentionLogData* data = contention_log_data_;
    ++(data->contention_count);
    data->AddToWaitTime(nano_time_blocked);
    ContentionLogEntry* log = data->contention_log;
    // This code is intentionally racy as it is only used for diagnostics.
    uint32_t slot = data->cur_content_log_entry.LoadRelaxed();
    if (log[slot].blocked_tid == blocked_tid &&
        log[slot].owner_tid == blocked_tid) {
      ++log[slot].count;
    } else {
      uint32_t new_slot;
      do {
        slot = data->cur_content_log_entry.LoadRelaxed();
        new_slot = (slot + 1) % kContentionLogSize;
      } while (!data->cur_content_log_entry.CompareAndSetWeakRelaxed(slot, new_slot));
      log[new_slot].blocked_tid = blocked_tid;
      log[new_slot].owner_tid = owner_tid;
      log[new_slot].count.StoreRelaxed(1);
    }
  }
}

void BaseMutex::DumpContention(std::ostream& os) const {
  if (kLogLockContentions) {
    const ContentionLogData* data = contention_log_data_;
    const ContentionLogEntry* log = data->contention_log;
    uint64_t wait_time = data->wait_time.LoadRelaxed();
    uint32_t contention_count = data->contention_count.LoadRelaxed();
    if (contention_count == 0) {
      os << "never contended";
    } else {
      os << "contended " << contention_count
         << " total wait of contender " << PrettyDuration(wait_time)
         << " average " << PrettyDuration(wait_time / contention_count);
      SafeMap<uint64_t, size_t> most_common_blocker;
      SafeMap<uint64_t, size_t> most_common_blocked;
      for (size_t i = 0; i < kContentionLogSize; ++i) {
        uint64_t blocked_tid = log[i].blocked_tid;
        uint64_t owner_tid = log[i].owner_tid;
        uint32_t count = log[i].count.LoadRelaxed();
        if (count > 0) {
          auto it = most_common_blocked.find(blocked_tid);
          if (it != most_common_blocked.end()) {
            most_common_blocked.Overwrite(blocked_tid, it->second + count);
          } else {
            most_common_blocked.Put(blocked_tid, count);
          }
          it = most_common_blocker.find(owner_tid);
          if (it != most_common_blocker.end()) {
            most_common_blocker.Overwrite(owner_tid, it->second + count);
          } else {
            most_common_blocker.Put(owner_tid, count);
          }
        }
      }
      uint64_t max_tid = 0;
      size_t max_tid_count = 0;
      for (const auto& pair : most_common_blocked) {
        if (pair.second > max_tid_count) {
          max_tid = pair.first;
          max_tid_count = pair.second;
        }
      }
      if (max_tid != 0) {
        os << " sample shows most blocked tid=" << max_tid;
      }
      max_tid = 0;
      max_tid_count = 0;
      for (const auto& pair : most_common_blocker) {
        if (pair.second > max_tid_count) {
          max_tid = pair.first;
          max_tid_count = pair.second;
        }
      }
      if (max_tid != 0) {
        os << " sample shows tid=" << max_tid << " owning during this time";
      }
    }
  }
}


Mutex::Mutex(const char* name, LockLevel level, bool recursive)
    : BaseMutex(name, level), exclusive_owner_(0), recursive_(recursive), recursion_count_(0) {
#if ART_USE_FUTEXES
  DCHECK_EQ(0, state_.LoadRelaxed());
  DCHECK_EQ(0, num_contenders_.LoadRelaxed());
#else
  CHECK_MUTEX_CALL(pthread_mutex_init, (&mutex_, nullptr));
#endif
}

Mutex::~Mutex() {
  bool safe_to_call_abort = MutexContract::IsSafeToCallAbortRacy();
#if ART_USE_FUTEXES
  if (state_.LoadRelaxed() != 0) {
    LOG(safe_to_call_abort ? FATAL : WARNING)
        << "destroying mutex with owner: " << GetExclusiveOwnerTid();
  } else {
    if (GetExclusiveOwnerTid() != 0) {
      LOG(safe_to_call_abort ? FATAL : WARNING)
          << "unexpectedly found an owner on unlocked mutex " << name_;
    }
    if (num_contenders_.LoadSequentiallyConsistent() != 0) {
      LOG(safe_to_call_abort ? FATAL : WARNING)
          << "unexpectedly found a contender on mutex " << name_;
    }
  }
#else
  // We can't use CHECK_MUTEX_CALL here because on shutdown a suspended daemon thread
  // may still be using locks.
  int rc = pthread_mutex_destroy(&mutex_);
  if (rc != 0) {
    errno = rc;
    PLOG(safe_to_call_abort ? FATAL : WARNING)
        << "pthread_mutex_destroy failed for " << name_;
  }
#endif
}

void Mutex::ExclusiveLock(Thread* self) {
  DCHECK(MutexContract::IsNullOrCurrentThread(self));
  if (kDebugLocking && !recursive_) {
    AssertNotHeld(self);
  }
  if (!recursive_ || !IsExclusiveHeld(self)) {
#if ART_USE_FUTEXES
    bool done = false;
    do {
      int32_t cur_state = state_.LoadRelaxed();
      if (LIKELY(cur_state == 0)) {
        // Change state from 0 to 1 and impose load/store ordering appropriate for lock acquisition.
        done = state_.CompareAndSetWeakAcquire(0 /* cur_state */, 1 /* new state */);
      } else {
        // Failed to acquire, hang up.
        ScopedContentionRecorder scr(this, SafeGetTid(self), GetExclusiveOwnerTid());
        num_contenders_++;
        if (UNLIKELY(should_respond_to_empty_checkpoint_request_)) {
          MutexContract::CheckEmptyCheckpoint(self);
        }
        if (futex(state_.Address(), FUTEX_WAIT, 1, nullptr, nullptr, 0) != 0) {
          // EAGAIN and EINTR both indicate a spurious failure, try again from the beginning.
          // We don't use TEMP_FAILURE_RETRY so we can intentionally retry to acquire the lock.
          if ((errno != EAGAIN) && (errno != EINTR)) {
            PLOG(FATAL) << "futex wait failed for " << name_;
          }
        }
        num_contenders_--;
      }
    } while (!done);
    DCHECK_EQ(state_.LoadRelaxed(), 1);
#else
    CHECK_MUTEX_CALL(pthread_mutex_lock, (&mutex_));
#endif
    DCHECK_EQ(GetExclusiveOwnerTid(), 0);
    exclusive_owner_.StoreRelaxed(SafeGetTid(self));
    RegisterAsLocked(self);
  }
  recursion_count_++;
  if (kDebugLocking) {
    CHECK(recursion_count_ == 1 || recursive_) << "Unexpected recursion count on mutex: "
        << name_ << " " << recursion_count_;
    AssertHeld(self);
  }
}

bool Mutex::ExclusiveTryLock(Thread* self) {
  DCHECK(MutexContract::IsNullOrCurrentThread(self));
  if (kDebugLocking && !recursive_) {
    AssertNotHeld(self);
  }
  if (!recursive_ || !IsExclusiveHeld(self)) {
#if ART_USE_FUTEXES
    bool done = false;
    do {
      int32_t cur_state = state_.LoadRelaxed();
      if (cur_state == 0) {
        // Change state from 0 to 1 and impose load/store ordering appropriate for lock acquisition.
        done = state_.CompareAndSetWeakAcquire(0 /* cur_state */, 1 /* new state */);
      } else {
        return false;
      }
    } while (!done);
    DCHECK_EQ(state_.LoadRelaxed(), 1);
#else
    int result = pthread_mutex_trylock(&mutex_);
    if (result == EBUSY) {
      return false;
    }
    if (result != 0) {
      errno = result;
      PLOG(FATAL) << "pthread_mutex_trylock failed for " << name_;
    }
#endif
    DCHECK_EQ(GetExclusiveOwnerTid(), 0);
    exclusive_owner_.StoreRelaxed(SafeGetTid(self));
    RegisterAsLocked(self);
  }
  recursion_count_++;
  if (kDebugLocking) {
    CHECK(recursion_count_ == 1 || recursive_) << "Unexpected recursion count on mutex: "
        << name_ << " " << recursion_count_;
    AssertHeld(self);
  }
  return true;
}

void Mutex::ExclusiveUnlock(Thread* self) {
  if (kIsDebugBuild) {
    MutexContract::CheckAndLogInvalidThreadNames(self, level_, this);
  }
  AssertHeld(self);
  DCHECK_NE(GetExclusiveOwnerTid(), 0);
  recursion_count_--;
  if (!recursive_ || recursion_count_ == 0) {
    if (kDebugLocking) {
      CHECK(recursion_count_ == 0 || recursive_) << "Unexpected recursion count on mutex: "
          << name_ << " " << recursion_count_;
    }
    RegisterAsUnlocked(self);
#if ART_USE_FUTEXES
    bool done = false;
    do {
      int32_t cur_state = state_.LoadRelaxed();
      if (LIKELY(cur_state == 1)) {
        // We're no longer the owner.
        exclusive_owner_.StoreRelaxed(0);
        // Change state to 0 and impose load/store ordering appropriate for lock release.
        // Note, the relaxed loads below mustn't reorder before the CompareAndSet.
        // TODO: the ordering here is non-trivial as state is split across 3 fields, fix by placing
        // a status bit into the state on contention.
        done = state_.CompareAndSetWeakSequentiallyConsistent(cur_state, 0 /* new state */);
        if (LIKELY(done)) {  // Spurious fail?
          // Wake a contender.
          if (UNLIKELY(num_contenders_.LoadRelaxed() > 0)) {
            futex(state_.Address(), FUTEX_WAKE, 1, nullptr, nullptr, 0);
          }
        }
      } else {
        MutexContract::LogUnlockFailed(cur_state, this);
      }
    } while (!done);
#else
    exclusive_owner_.StoreRelaxed(0);
    CHECK_MUTEX_CALL(pthread_mutex_unlock, (&mutex_));
#endif
  }
}

void Mutex::Dump(std::ostream& os) const {
  os << (recursive_ ? "recursive " : "non-recursive ")
      << name_
      << " level=" << static_cast<int>(level_)
      << " rec=" << recursion_count_
      << " owner=" << GetExclusiveOwnerTid() << " ";
  DumpContention(os);
}

std::ostream& operator<<(std::ostream& os, const Mutex& mu) {
  mu.Dump(os);
  return os;
}

void Mutex::WakeupToRespondToEmptyCheckpoint() {
#if ART_USE_FUTEXES
  // Wake up all the waiters so they will respond to the emtpy checkpoint.
  DCHECK(should_respond_to_empty_checkpoint_request_);
  if (UNLIKELY(num_contenders_.LoadRelaxed() > 0)) {
    futex(state_.Address(), FUTEX_WAKE, -1, nullptr, nullptr, 0);
  }
#else
  LOG(FATAL) << "Non futex case isn't supported.";
#endif
}

ReaderWriterMutex::ReaderWriterMutex(const char* name, LockLevel level)
    : BaseMutex(name, level)
#if ART_USE_FUTEXES
    , state_(0), num_pending_readers_(0), num_pending_writers_(0)
#endif
{
#if !ART_USE_FUTEXES
  CHECK_MUTEX_CALL(pthread_rwlock_init, (&rwlock_, nullptr));
#endif
  exclusive_owner_.StoreRelaxed(0);
}

ReaderWriterMutex::~ReaderWriterMutex() {
#if ART_USE_FUTEXES
  CHECK_EQ(state_.LoadRelaxed(), 0);
  CHECK_EQ(GetExclusiveOwnerTid(), 0);
  CHECK_EQ(num_pending_readers_.LoadRelaxed(), 0);
  CHECK_EQ(num_pending_writers_.LoadRelaxed(), 0);
#else
  // We can't use CHECK_MUTEX_CALL here because on shutdown a suspended daemon thread
  // may still be using locks.
  int rc = pthread_rwlock_destroy(&rwlock_);
  if (rc != 0) {
    errno = rc;
    bool is_safe_to_call_abort = MutexContract::IsSafeToCallAbortSafe();
    PLOG(is_safe_to_call_abort ? FATAL : WARNING) << "pthread_rwlock_destroy failed for " << name_;
  }
#endif
}

void ReaderWriterMutex::ExclusiveLock(Thread* self) {
  DCHECK(MutexContract::IsNullOrCurrentThread(self));
  AssertNotExclusiveHeld(self);
#if ART_USE_FUTEXES
  bool done = false;
  do {
    int32_t cur_state = state_.LoadRelaxed();
    if (LIKELY(cur_state == 0)) {
      // Change state from 0 to -1 and impose load/store ordering appropriate for lock acquisition.
      done = state_.CompareAndSetWeakAcquire(0 /* cur_state*/, -1 /* new state */);
    } else {
      // Failed to acquire, hang up.
      ScopedContentionRecorder scr(this, SafeGetTid(self), GetExclusiveOwnerTid());
      ++num_pending_writers_;
      if (UNLIKELY(should_respond_to_empty_checkpoint_request_)) {
        MutexContract::CheckEmptyCheckpoint(self);
      }
      if (futex(state_.Address(), FUTEX_WAIT, cur_state, nullptr, nullptr, 0) != 0) {
        // EAGAIN and EINTR both indicate a spurious failure, try again from the beginning.
        // We don't use TEMP_FAILURE_RETRY so we can intentionally retry to acquire the lock.
        if ((errno != EAGAIN) && (errno != EINTR)) {
          PLOG(FATAL) << "futex wait failed for " << name_;
        }
      }
      --num_pending_writers_;
    }
  } while (!done);
  DCHECK_EQ(state_.LoadRelaxed(), -1);
#else
  CHECK_MUTEX_CALL(pthread_rwlock_wrlock, (&rwlock_));
#endif
  DCHECK_EQ(GetExclusiveOwnerTid(), 0);
  exclusive_owner_.StoreRelaxed(SafeGetTid(self));
  RegisterAsLocked(self);
  AssertExclusiveHeld(self);
}

void ReaderWriterMutex::ExclusiveUnlock(Thread* self) {
  DCHECK(MutexContract::IsNullOrCurrentThread(self));
  AssertExclusiveHeld(self);
  RegisterAsUnlocked(self);
  DCHECK_NE(GetExclusiveOwnerTid(), 0);
#if ART_USE_FUTEXES
  bool done = false;
  do {
    int32_t cur_state = state_.LoadRelaxed();
    if (LIKELY(cur_state == -1)) {
      // We're no longer the owner.
      exclusive_owner_.StoreRelaxed(0);
      // Change state from -1 to 0 and impose load/store ordering appropriate for lock release.
      // Note, the relaxed loads below musn't reorder before the CompareAndSet.
      // TODO: the ordering here is non-trivial as state is split across 3 fields, fix by placing
      // a status bit into the state on contention.
      done = state_.CompareAndSetWeakSequentiallyConsistent(-1 /* cur_state*/, 0 /* new state */);
      if (LIKELY(done)) {  // Weak CAS may fail spuriously.
        // Wake any waiters.
        if (UNLIKELY(num_pending_readers_.LoadRelaxed() > 0 ||
                     num_pending_writers_.LoadRelaxed() > 0)) {
          futex(state_.Address(), FUTEX_WAKE, -1, nullptr, nullptr, 0);
        }
      }
    } else {
      LOG(FATAL) << "Unexpected state_:" << cur_state << " for " << name_;
    }
  } while (!done);
#else
  exclusive_owner_.StoreRelaxed(0);
  CHECK_MUTEX_CALL(pthread_rwlock_unlock, (&rwlock_));
#endif
}

#if HAVE_TIMED_RWLOCK
bool ReaderWriterMutex::ExclusiveLockWithTimeout(Thread* self, int64_t ms, int32_t ns) {
  DCHECK(MutexContract::IsNullOrCurrentThread(self));
#if ART_USE_FUTEXES
  bool done = false;
  timespec end_abs_ts;
  InitTimeSpec(true, CLOCK_MONOTONIC, ms, ns, &end_abs_ts);
  do {
    int32_t cur_state = state_.LoadRelaxed();
    if (cur_state == 0) {
      // Change state from 0 to -1 and impose load/store ordering appropriate for lock acquisition.
      done = state_.CompareAndSetWeakAcquire(0 /* cur_state */, -1 /* new state */);
    } else {
      // Failed to acquire, hang up.
      timespec now_abs_ts;
      InitTimeSpec(true, CLOCK_MONOTONIC, 0, 0, &now_abs_ts);
      timespec rel_ts;
      if (ComputeRelativeTimeSpec(&rel_ts, end_abs_ts, now_abs_ts)) {
        return false;  // Timed out.
      }
      ScopedContentionRecorder scr(this, SafeGetTid(self), GetExclusiveOwnerTid());
      ++num_pending_writers_;
      if (UNLIKELY(should_respond_to_empty_checkpoint_request_)) {
        MutexContract::CheckEmptyCheckpoint(self);
      }
      if (futex(state_.Address(), FUTEX_WAIT, cur_state, &rel_ts, nullptr, 0) != 0) {
        if (errno == ETIMEDOUT) {
          --num_pending_writers_;
          return false;  // Timed out.
        } else if ((errno != EAGAIN) && (errno != EINTR)) {
          // EAGAIN and EINTR both indicate a spurious failure,
          // recompute the relative time out from now and try again.
          // We don't use TEMP_FAILURE_RETRY so we can recompute rel_ts;
          PLOG(FATAL) << "timed futex wait failed for " << name_;
        }
      }
      --num_pending_writers_;
    }
  } while (!done);
#else
  timespec ts;
  InitTimeSpec(true, CLOCK_REALTIME, ms, ns, &ts);
  int result = pthread_rwlock_timedwrlock(&rwlock_, &ts);
  if (result == ETIMEDOUT) {
    return false;
  }
  if (result != 0) {
    errno = result;
    PLOG(FATAL) << "pthread_rwlock_timedwrlock failed for " << name_;
  }
#endif
  exclusive_owner_.StoreRelaxed(SafeGetTid(self));
  RegisterAsLocked(self);
  AssertSharedHeld(self);
  return true;
}
#endif

#if ART_USE_FUTEXES
void ReaderWriterMutex::HandleSharedLockContention(Thread* self, int32_t cur_state) {
  // Owner holds it exclusively, hang up.
  ScopedContentionRecorder scr(this, SafeGetTid(self), GetExclusiveOwnerTid());
  ++num_pending_readers_;
  if (UNLIKELY(should_respond_to_empty_checkpoint_request_)) {
    MutexContract::CheckEmptyCheckpoint(self);
  }
  if (futex(state_.Address(), FUTEX_WAIT, cur_state, nullptr, nullptr, 0) != 0) {
    if (errno != EAGAIN && errno != EINTR) {
      PLOG(FATAL) << "futex wait failed for " << name_;
    }
  }
  --num_pending_readers_;
}
#endif

bool ReaderWriterMutex::SharedTryLock(Thread* self) {
  DCHECK(MutexContract::IsNullOrCurrentThread(self));
#if ART_USE_FUTEXES
  bool done = false;
  do {
    int32_t cur_state = state_.LoadRelaxed();
    if (cur_state >= 0) {
      // Add as an extra reader and impose load/store ordering appropriate for lock acquisition.
      done = state_.CompareAndSetWeakAcquire(cur_state, cur_state + 1);
    } else {
      // Owner holds it exclusively.
      return false;
    }
  } while (!done);
#else
  int result = pthread_rwlock_tryrdlock(&rwlock_);
  if (result == EBUSY) {
    return false;
  }
  if (result != 0) {
    errno = result;
    PLOG(FATAL) << "pthread_mutex_trylock failed for " << name_;
  }
#endif
  RegisterAsLocked(self);
  AssertSharedHeld(self);
  return true;
}

bool ReaderWriterMutex::IsSharedHeld(const Thread* self) const {
  DCHECK(MutexContract::IsNullOrCurrentThread(self));
  bool result;
  if (UNLIKELY(self == nullptr)) {  // Handle unattached threads.
    result = IsExclusiveHeld(self);  // TODO: a better best effort here.
  } else {
    result = MutexContract::IsCurrentMutexAtLevel(self, level_, this);
  }
  return result;
}

void ReaderWriterMutex::Dump(std::ostream& os) const {
  os << name_
      << " level=" << static_cast<int>(level_)
      << " owner=" << GetExclusiveOwnerTid()
#if ART_USE_FUTEXES
      << " state=" << state_.LoadSequentiallyConsistent()
      << " num_pending_writers=" << num_pending_writers_.LoadSequentiallyConsistent()
      << " num_pending_readers=" << num_pending_readers_.LoadSequentiallyConsistent()
#endif
      << " ";
  DumpContention(os);
}

std::ostream& operator<<(std::ostream& os, const ReaderWriterMutex& mu) {
  mu.Dump(os);
  return os;
}

std::ostream& operator<<(std::ostream& os, const MutatorMutex& mu) {
  mu.Dump(os);
  return os;
}

void ReaderWriterMutex::WakeupToRespondToEmptyCheckpoint() {
#if ART_USE_FUTEXES
  // Wake up all the waiters so they will respond to the emtpy checkpoint.
  DCHECK(should_respond_to_empty_checkpoint_request_);
  if (UNLIKELY(num_pending_readers_.LoadRelaxed() > 0 ||
               num_pending_writers_.LoadRelaxed() > 0)) {
    futex(state_.Address(), FUTEX_WAKE, -1, nullptr, nullptr, 0);
  }
#else
  LOG(FATAL) << "Non futex case isn't supported.";
#endif
}

ConditionVariable::ConditionVariable(const char* name, Mutex& guard)
    : name_(name), guard_(guard) {
#if ART_USE_FUTEXES
  DCHECK_EQ(0, sequence_.LoadRelaxed());
  num_waiters_ = 0;
#else
  pthread_condattr_t cond_attrs;
  CHECK_MUTEX_CALL(pthread_condattr_init, (&cond_attrs));
#if !defined(__APPLE__)
  // Apple doesn't have CLOCK_MONOTONIC or pthread_condattr_setclock.
  CHECK_MUTEX_CALL(pthread_condattr_setclock, (&cond_attrs, CLOCK_MONOTONIC));
#endif
  CHECK_MUTEX_CALL(pthread_cond_init, (&cond_, &cond_attrs));
#endif
}

ConditionVariable::~ConditionVariable() {
#if ART_USE_FUTEXES
  if (num_waiters_!= 0) {
    bool is_safe_to_call_abort = MutexContract::IsSafeToCallAbortSafe();
    LOG(is_safe_to_call_abort ? FATAL : WARNING)
        << "ConditionVariable::~ConditionVariable for " << name_
        << " called with " << num_waiters_ << " waiters.";
  }
#else
  // We can't use CHECK_MUTEX_CALL here because on shutdown a suspended daemon thread
  // may still be using condition variables.
  int rc = pthread_cond_destroy(&cond_);
  if (rc != 0) {
    errno = rc;
    bool is_safe_to_call_abort = MutexContract::IsSafeToCallAbortSafe();
    PLOG(is_safe_to_call_abort ? FATAL : WARNING) << "pthread_cond_destroy failed for " << name_;
  }
#endif
}

void ConditionVariable::Broadcast(Thread* self) {
  DCHECK(MutexContract::IsNullOrCurrentThread(self));
  // TODO: enable below, there's a race in thread creation that causes false failures currently.
  // guard_.AssertExclusiveHeld(self);
  DCHECK_EQ(guard_.GetExclusiveOwnerTid(), SafeGetTid(self));
#if ART_USE_FUTEXES
  if (num_waiters_ > 0) {
    sequence_++;  // Indicate the broadcast occurred.
    bool done = false;
    do {
      int32_t cur_sequence = sequence_.LoadRelaxed();
      // Requeue waiters onto mutex. The waiter holds the contender count on the mutex high ensuring
      // mutex unlocks will awaken the requeued waiter thread.
      done = futex(sequence_.Address(), FUTEX_CMP_REQUEUE, 0,
                   reinterpret_cast<const timespec*>(std::numeric_limits<int32_t>::max()),
                   guard_.state_.Address(), cur_sequence) != -1;
      if (!done) {
        if (errno != EAGAIN && errno != EINTR) {
          PLOG(FATAL) << "futex cmp requeue failed for " << name_;
        }
      }
    } while (!done);
  }
#else
  CHECK_MUTEX_CALL(pthread_cond_broadcast, (&cond_));
#endif
}

void ConditionVariable::Signal(Thread* self) {
  DCHECK(MutexContract::IsNullOrCurrentThread(self));
  guard_.AssertExclusiveHeld(self);
#if ART_USE_FUTEXES
  if (num_waiters_ > 0) {
    sequence_++;  // Indicate a signal occurred.
    // Futex wake 1 waiter who will then come and in contend on mutex. It'd be nice to requeue them
    // to avoid this, however, requeueing can only move all waiters.
    int num_woken = futex(sequence_.Address(), FUTEX_WAKE, 1, nullptr, nullptr, 0);
    // Check something was woken or else we changed sequence_ before they had chance to wait.
    CHECK((num_woken == 0) || (num_woken == 1));
  }
#else
  CHECK_MUTEX_CALL(pthread_cond_signal, (&cond_));
#endif
}

void ConditionVariable::Wait(Thread* self) {
  guard_.CheckSafeToWait(self);
  WaitHoldingLocks(self);
}

void ConditionVariable::WaitHoldingLocks(Thread* self) {
  DCHECK(MutexContract::IsNullOrCurrentThread(self));
  guard_.AssertExclusiveHeld(self);
  unsigned int old_recursion_count = guard_.recursion_count_;
#if ART_USE_FUTEXES
  num_waiters_++;
  // Ensure the Mutex is contended so that requeued threads are awoken.
  guard_.num_contenders_++;
  guard_.recursion_count_ = 1;
  int32_t cur_sequence = sequence_.LoadRelaxed();
  guard_.ExclusiveUnlock(self);
  if (futex(sequence_.Address(), FUTEX_WAIT, cur_sequence, nullptr, nullptr, 0) != 0) {
    // Futex failed, check it is an expected error.
    // EAGAIN == EWOULDBLK, so we let the caller try again.
    // EINTR implies a signal was sent to this thread.
    if ((errno != EINTR) && (errno != EAGAIN)) {
      PLOG(FATAL) << "futex wait failed for " << name_;
    }
  }
  MutexContract::SleepForeverIfRuntimeDeleted(self);
  guard_.ExclusiveLock(self);
  CHECK_GE(num_waiters_, 0);
  num_waiters_--;
  // We awoke and so no longer require awakes from the guard_'s unlock.
  CHECK_GE(guard_.num_contenders_.LoadRelaxed(), 0);
  guard_.num_contenders_--;
#else
  pid_t old_owner = guard_.GetExclusiveOwnerTid();
  guard_.exclusive_owner_.StoreRelaxed(0);
  guard_.recursion_count_ = 0;
  CHECK_MUTEX_CALL(pthread_cond_wait, (&cond_, &guard_.mutex_));
  guard_.exclusive_owner_.StoreRelaxed(old_owner);
#endif
  guard_.recursion_count_ = old_recursion_count;
}

bool ConditionVariable::TimedWait(Thread* self, int64_t ms, int32_t ns) {
  DCHECK(MutexContract::IsNullOrCurrentThread(self));
  bool timed_out = false;
  guard_.AssertExclusiveHeld(self);
  guard_.CheckSafeToWait(self);
  unsigned int old_recursion_count = guard_.recursion_count_;
#if ART_USE_FUTEXES
  timespec rel_ts;
  InitTimeSpec(false, CLOCK_REALTIME, ms, ns, &rel_ts);
  num_waiters_++;
  // Ensure the Mutex is contended so that requeued threads are awoken.
  guard_.num_contenders_++;
  guard_.recursion_count_ = 1;
  int32_t cur_sequence = sequence_.LoadRelaxed();
  guard_.ExclusiveUnlock(self);
  if (futex(sequence_.Address(), FUTEX_WAIT, cur_sequence, &rel_ts, nullptr, 0) != 0) {
    if (errno == ETIMEDOUT) {
      // Timed out we're done.
      timed_out = true;
    } else if ((errno == EAGAIN) || (errno == EINTR)) {
      // A signal or ConditionVariable::Signal/Broadcast has come in.
    } else {
      PLOG(FATAL) << "timed futex wait failed for " << name_;
    }
  }
  guard_.ExclusiveLock(self);
  CHECK_GE(num_waiters_, 0);
  num_waiters_--;
  // We awoke and so no longer require awakes from the guard_'s unlock.
  CHECK_GE(guard_.num_contenders_.LoadRelaxed(), 0);
  guard_.num_contenders_--;
#else
#if !defined(__APPLE__)
  int clock = CLOCK_MONOTONIC;
#else
  int clock = CLOCK_REALTIME;
#endif
  pid_t old_owner = guard_.GetExclusiveOwnerTid();
  guard_.exclusive_owner_.StoreRelaxed(0);
  guard_.recursion_count_ = 0;
  timespec ts;
  InitTimeSpec(true, clock, ms, ns, &ts);
  int rc = TEMP_FAILURE_RETRY(pthread_cond_timedwait(&cond_, &guard_.mutex_, &ts));
  if (rc == ETIMEDOUT) {
    timed_out = true;
  } else if (rc != 0) {
    errno = rc;
    PLOG(FATAL) << "TimedWait failed for " << name_;
  }
  guard_.exclusive_owner_.StoreRelaxed(old_owner);
#endif
  guard_.recursion_count_ = old_recursion_count;
  return timed_out;
}

}  // namespace art
