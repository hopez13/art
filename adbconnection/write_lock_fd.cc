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

#include "write_lock_fd.h"

#include <android-base/macros.h>
#include <stdint.h>
#include <sys/eventfd.h>


namespace adbconnection {

WriteLockFd::WriteLockFd() : fd_(eventfd(1, EFD_CLOEXEC)) { }

WriteLockFd::~WriteLockFd() {
  close(fd_);
}

void Lock() {
  uint64_t data;
  TEMP_FAILURE_RETRY(read(fd_, &data, sizeof(data)));
}

void Unlock() {
  uint64_t data = 1;
  TEMP_FAILURE_RETRY(write(fd_, &data, sizeof(data)));
}

}  // namespace adbconnection
