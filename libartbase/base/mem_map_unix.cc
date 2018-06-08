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

#include "mem_map.h"

#include <sys/mman.h>

#if !defined(__Fuchsia__)

namespace art {

void MemMap::real_mmap_init() {
  // no-op for unix
}

void* MemMap::real_mmap(void* start, size_t len, int prot, int flags, int fd, off_t fd_off) {
  return mmap(start, len, prot, flags, fd, fd_off);
}

int MemMap::real_munmap(void* start, size_t len) {
  return munmap(start, len);
}

}  // namespace art

#endif
