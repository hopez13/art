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
#include <sys/mman.h>  // For the PROT_* and MAP_* constants.
#include "logging.h"  // For VLOG_IS_ON.

namespace art {

zx_handle_t MemMap::fuchsia_lowmem_vmar_ = ZX_HANDLE_INVALID;
zx_vaddr_t MemMap::fuchsia_lowmem_base_ = 0;
size_t MemMap::fuchsia_lowmem_size_ = 0;

static const char map_name[] = "mmap-android";
static constexpr uintptr_t FUCHSIA_LOWER_MEM_START = 0x80000000;
static constexpr uintptr_t FUCHSIA_LOWER_MEM_SIZE  = 0x60000000;

void* MemMap::fuchsia_mmap(void* start, size_t len, int prot, int flags, int fd, off_t fd_off) {
  zx_status_t status;
  uint32_t vmarflags = 0;
  bool mmap_lower = false;
  zx_info_vmar_t vmarinfo;
  uintptr_t mem = 0;
  zx_handle_t vmo;

  if (fuchsia_lowmem_vmar_ == ZX_HANDLE_INVALID) {
    status = zx_object_get_info(zx_vmar_root_self(),
                                ZX_INFO_VMAR,
                                &vmarinfo,
                                sizeof(vmarinfo),
                                NULL,
                                NULL);
    if (status != ZX_OK) {
      LOG(FATAL) << "could not find info from root vmar: " << status;
    }
    uintptr_t lower_mem_start = FUCHSIA_LOWER_MEM_START - vmarinfo.base;
    fuchsia_lowmem_size_ = FUCHSIA_LOWER_MEM_SIZE;
    uint32_t allocflags = ZX_VM_FLAG_CAN_MAP_READ |
                          ZX_VM_FLAG_CAN_MAP_WRITE |
                          ZX_VM_FLAG_CAN_MAP_EXECUTE |
                          ZX_VM_FLAG_SPECIFIC;
    status = zx_vmar_allocate(zx_vmar_root_self(),
                              lower_mem_start,
                              fuchsia_lowmem_size_,
                              allocflags,
                              &fuchsia_lowmem_vmar_,
                              &fuchsia_lowmem_base_);
    if (status != ZX_OK) {
      LOG(FATAL) << " could not allocate lowmem vmar: " << status;
    }
  }

  if (flags & MAP_32BIT) {
     mmap_lower = true;
  }

  // for file-based mapping use system library
  if (!(flags & MAP_ANONYMOUS)) {
    if (mmap_lower) {
      LOG(FATAL) << "cannot map files into low memory for Fuchsia";
    }
    return mmap(start, len, prot, flags, fd, fd_off);
  }

  if (prot & PROT_READ) vmarflags |= ZX_VM_FLAG_PERM_READ;
  if (prot & PROT_WRITE) vmarflags |= ZX_VM_FLAG_PERM_WRITE;
  if (prot & PROT_EXEC) vmarflags |= ZX_VM_FLAG_PERM_EXECUTE;

  if (len == 0) {
    errno = EINVAL;
    return MAP_FAILED;
  }

  size_t vmaroffset = 0;
  if (flags & MAP_FIXED) {
    vmarflags |= ZX_VM_FLAG_SPECIFIC;
    status = zx_object_get_info((mmap_lower ? fuchsia_lowmem_vmar_ : zx_vmar_root_self()),
                                ZX_INFO_VMAR,
                                &vmarinfo,
                                sizeof(vmarinfo),
                                NULL,
                                NULL);
     if (status < 0 || (uintptr_t)start < vmarinfo.base) {
          errno = EINVAL;
          return MAP_FAILED;
      }
      vmaroffset = (uintptr_t)start - vmarinfo.base;
  }

  if (zx_vmo_create(len, 0, &vmo) < 0) {
    errno = ENOMEM;
    return MAP_FAILED;
  }
  if (zx_vmo_get_size(vmo, &len)) {
    errno = ENOMEM;
    return MAP_FAILED;
  }
  zx_object_set_property(vmo, ZX_PROP_NAME, map_name, strlen(map_name));

  if (mmap_lower) {
    status = zx_vmar_map(fuchsia_lowmem_vmar_, vmaroffset, vmo, fd_off, len, vmarflags, &mem);
  } else {
    status = zx_vmar_map(zx_vmar_root_self(), vmaroffset, vmo, fd_off, len, vmarflags, &mem);
  }
  zx_handle_close(vmo);
  if (status != ZX_OK) {
    return MAP_FAILED;
  }

  return reinterpret_cast<void *>(mem);
}

int MemMap::fuchsia_munmap(void* start, size_t len) {
  uintptr_t addr = (uintptr_t)start;
  zx_handle_t alloc_vmar = zx_vmar_root_self();
  if (addr >= fuchsia_lowmem_base_ && addr < fuchsia_lowmem_base_ + fuchsia_lowmem_size_) {
    alloc_vmar = fuchsia_lowmem_vmar_;
  }
  zx_status_t status = zx_vmar_unmap(alloc_vmar, addr, len);
  if (status < 0) {
    errno = EINVAL;
    return -1;
  }
  return 0;
}

}  // namespace art

