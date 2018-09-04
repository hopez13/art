/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "membarrier.h"

#include <errno.h>

#include <sys/syscall.h>
#include <unistd.h>
#include "macros.h"

#if defined(__BIONIC__)
#include <linux/membarrier.h>

static_assert(art::MembarrierCommand::kQuery == MEMBARRIER_CMD_QUERY);
static_assert(art::MembarrierCommand::kGlobal == MEMBARRIER_CMD_SHARED);
static_assert(art::MembarrierCommand::kPrivateExpedited == MEMBARRIER_CMD_PRIVATE_EXPEDITED);
static_assert(art::MembarrierCommand::kRegisterPrivateExpedited ==
              MEMBARRIER_CMD_REGISTER_PRIVATE_EXPEDITED);
static_assert(art::MembarrierCommand::kPrivateExpedited == MEMBARRIER_CMD_PRIVATE_EXPEDITED);
#endif  // __BIONIC

namespace art {

#if defined(__NR_membarrier)

int membarrier(MembarrierCommand command) {
  return syscall(__NR_membarrier, static_cast<int>(command), 0);
}

#else  // __NR_membarrier

int membarrier(MembarrierCommand command ATTRIBUTE_UNUSED) {
  // In principle this could be supported on linux, but Android's prebuilt glibc does not include
  // the system call number defintions (b/111199492).
  errno = ENOSYS;
  return -1;
}

#endif  // __NR_membarrier

}  // namespace art
