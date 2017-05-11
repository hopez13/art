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

#include "scoped_flock.h"

#include <sys/file.h>
#include <sys/stat.h>

#include "android-base/stringprintf.h"

#include "base/logging.h"
#include "base/unix_file/fd_file.h"

namespace art {

using android::base::StringPrintf;

bool ScopedFlock::Init(const char* filename,
                       int flags,
                       bool block,
                       bool flush_on_close,
                       std::string* error_msg) {
  flush_on_close_ = flush_on_close;
  while (true) {
    if (file_.get() != nullptr) {
      UNUSED(file_->FlushCloseOrErase());  // Ignore result.
    }

    bool check_usage = flush_on_close;  // Check usage only if we need to flush on close.
    file_.reset(OS::OpenFileWithFlags(filename, flags, check_usage));
    if (file_.get() == nullptr) {
      *error_msg = StringPrintf("Failed to open file '%s': %s", filename, strerror(errno));
      return false;
    }
    int operation = block ? LOCK_EX : (LOCK_EX | LOCK_NB);
    int flock_result = TEMP_FAILURE_RETRY(flock(file_->Fd(), operation));
    if (flock_result == EWOULDBLOCK) {
      // File is locked by someone else and we are required not to block;
      return false;
    }
    if (flock_result != 0) {
      *error_msg = StringPrintf("Failed to lock file '%s': %s", filename, strerror(errno));
      return false;
    }
    struct stat fstat_stat;
    int fstat_result = TEMP_FAILURE_RETRY(fstat(file_->Fd(), &fstat_stat));
    if (fstat_result != 0) {
      *error_msg = StringPrintf("Failed to fstat file '%s': %s", filename, strerror(errno));
      return false;
    }
    struct stat stat_stat;
    int stat_result = TEMP_FAILURE_RETRY(stat(filename, &stat_stat));
    if (stat_result != 0) {
      PLOG(WARNING) << "Failed to stat, will retry: " << filename;
      // ENOENT can happen if someone racing with us unlinks the file we created so just retry.
      if (block) {
        continue;
      } else {
        // Note that in theory we could race with someone here for a long time and end up retrying
        // over and over again. This potential behavior does not fit well in the non-blocking
        // semantics. Thus, if we are not require to block return failure when racing.
        return false;
      }
    }
    if (fstat_stat.st_dev != stat_stat.st_dev || fstat_stat.st_ino != stat_stat.st_ino) {
      LOG(WARNING) << "File changed while locking, will retry: " << filename;
      if (block) {
        continue;
      } else {
        // See comment above.
        return false;
      }
    }
    return true;
  }
}

bool ScopedFlock::Lock(const File& file, bool block, std::string* error_msg) {
  locked_fd_.reset();

  while (true) {
    const std::string& file_path = file.GetPath();
    android::base::unique_fd dup_fd(dup(file.Fd()));

    if (dup_fd.get() == -1) {
      *error_msg = StringPrintf("Failed to duplicate open file '%s': %s",
                                file_path.c_str(), strerror(errno));
      return false;
    }

    const int operation = block ? LOCK_EX : (LOCK_EX | LOCK_NB);
    const int flock_result = TEMP_FAILURE_RETRY(flock(dup_fd.get(), operation));
    if (flock_result == EWOULDBLOCK) {
      // File is locked by someone else and we are required not to block;
      return false;
    }
    if (flock_result != 0) {
      *error_msg = StringPrintf("Failed to lock file '%s': %s", file_path.c_str(),
                                strerror(errno));
      return false;
    }

    if (file_path.empty()) {
      return false;
    }

    struct stat fstat_stat;
    int fstat_result = TEMP_FAILURE_RETRY(fstat(file.Fd(), &fstat_stat));
    if (fstat_result != 0) {
      *error_msg = StringPrintf("Failed to fstat file '%s': %s", file_path.c_str(),
                                strerror(errno));
      return false;
    }
    struct stat stat_stat;
    int stat_result = TEMP_FAILURE_RETRY(stat(file->GetPath(), &stat_stat));
    if (stat_result != 0) {
      PLOG(WARNING) << "Failed to stat, will retry: " << filename;
      // ENOENT can happen if someone racing with us unlinks the file we created so just retry.
      if (block) {
        continue;
      } else {
        // Note that in theory we could race with someone here for a long time and end up retrying
        // over and over again. This potential behavior does not fit well in the non-blocking
        // semantics. Thus, if we are not require to block return failure when racing.
        return false;
      }
    }
    if (fstat_stat.st_dev != stat_stat.st_dev || fstat_stat.st_ino != stat_stat.st_ino) {
      LOG(WARNING) << "File changed while locking, will retry: " << filename;
      if (block) {
        continue;
      } else {
        // See comment above.
        return false;
      }
    }
    return true;
  }
}

ScopedFlock::ScopedFlock() { }

ScopedFlock::~ScopedFlock() {
  if (file_.get() != nullptr) {
    int flock_result = TEMP_FAILURE_RETRY(flock(file_->Fd(), LOCK_UN));
    if (flock_result != 0) {
      // Only printing a warning is okay since this is only used with either:
      // 1) a non-blocking Init call, or
      // 2) as a part of a seperate binary (eg dex2oat) which has it's own timeout logic to prevent
      //    deadlocks.
      // This means we can be sure that the warning won't cause a deadlock.
      PLOG(WARNING) << "Unable to unlock file " << file_->GetPath();
    }
  }
}

}  // namespace art
