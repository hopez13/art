/*
 * Copyright (C) 2008 The Android Open Source Project
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

#include "base/memory_tool.h"
#include <inttypes.h>
#include <stdlib.h>

#include <memory>
#include <sstream>

#include "android-base/stringprintf.h"

#include "base/unix_file/fd_file.h"
#include "os.h"
#include "thread-inl.h"
#include "utils.h"

#include <cutils/ashmem.h>

#ifndef ANDROID_OS
#include <sys/resource.h>
#endif

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

namespace art {

using android::base::StringPrintf;

MemMap::Maps* MemMap::maps_ = nullptr;

// Return true if the address range is contained in a single memory map by either reading
// the maps_ variable or the /proc/self/map entry.
bool MemMap::ContainedWithinExistingMap(uint8_t* ptr, size_t size, std::string* error_msg) {
  UNUSED(ptr);
  UNUSED(size);
  UNUSED(error_msg);
  return false;
}

// CheckMapRequest to validate a non-MAP_FAILED mmap result based on
// the expected value, calling munmap if validation fails, giving the
// reason in error_msg.
//
// If the expected_ptr is null, nothing is checked beyond the fact
// that the actual_ptr is not MAP_FAILED. However, if expected_ptr is
// non-null, we check that pointer is the actual_ptr == expected_ptr,
// and if not, report in error_msg what the conflict mapping was if
// found, or a generic error in other cases.
static bool CheckMapRequest(uint8_t* expected_ptr, void* actual_ptr, size_t byte_count,
                            std::string* error_msg) {
  // Handled first by caller for more specific error messages.
  CHECK(actual_ptr != MAP_FAILED);

  if (expected_ptr == nullptr) {
    return true;
  }

  uintptr_t actual = reinterpret_cast<uintptr_t>(actual_ptr);
  uintptr_t expected = reinterpret_cast<uintptr_t>(expected_ptr);

  if (expected_ptr == actual_ptr) {
    return true;
  }

  // We asked for an address but didn't get what we wanted, all paths below here should fail.
  int result = munmap(actual_ptr, byte_count);
  if (result == -1) {
    PLOG(WARNING) << StringPrintf("munmap(%p, %zd) failed", actual_ptr, byte_count);
  }

  if (error_msg != nullptr) {
    // We call this here so that we can try and generate a full error
    // message with the overlapping mapping. There's no guarantee that
    // that there will be an overlap though, since
    // - The kernel is not *required* to honor expected_ptr unless MAP_FIXED is
    //   true, even if there is no overlap
    // - There might have been an overlap at the point of mmap, but the
    //   overlapping region has since been unmapped.
    std::string error_detail;
    std::ostringstream os;
    os <<  StringPrintf("Failed to mmap at expected address, mapped at "
                        "0x%08" PRIxPTR " instead of 0x%08" PRIxPTR,
                        actual, expected);
    if (!error_detail.empty()) {
      os << " : " << error_detail;
    }
    *error_msg = os.str();
  }
  return false;
}

MemMap* MemMap::MapAnonymous(const char* name,
                             uint8_t* expected_ptr,
                             size_t byte_count,
                             int prot,
                             bool low_4gb,
                             bool reuse,
                             std::string* error_msg,
                             bool use_ashmem) {
  CHECK_EQ(low_4gb, false);
  CHECK_EQ(reuse, false);

  use_ashmem = use_ashmem && !kIsTargetLinux;
  if (byte_count == 0) {
    return new MemMap(name, nullptr, 0, nullptr, 0, prot, false);
  }
  size_t page_aligned_byte_count = RoundUp(byte_count, kPageSize);

  int flags = MAP_PRIVATE | MAP_ANONYMOUS;

  File fd;

  if (use_ashmem) {
    if (!kIsTargetBuild) {
      // When not on Android (either host or assuming a linux target) ashmem is faked using
      // files in /tmp. Ensure that such files won't fail due to ulimit restrictions. If they
      // will then use a regular mmap.
      struct rlimit rlimit_fsize;
      CHECK_EQ(getrlimit(RLIMIT_FSIZE, &rlimit_fsize), 0);
      use_ashmem = (rlimit_fsize.rlim_cur == RLIM_INFINITY) ||
        (page_aligned_byte_count < rlimit_fsize.rlim_cur);
    }
  }

  if (use_ashmem) {
    // android_os_Debug.cpp read_mapinfo assumes all ashmem regions associated with the VM are
    // prefixed "dalvik-".
    std::string debug_friendly_name("dalvik-");
    debug_friendly_name += name;
    fd.Reset(ashmem_create_region(debug_friendly_name.c_str(), page_aligned_byte_count),
             /* check_usage */ false);

    if (fd.Fd() == -1) {
      // We failed to create the ashmem region. Print a warning, but continue
      // anyway by creating a true anonymous mmap with an fd of -1. It is
      // better to use an unlabelled anonymous map than to fail to create a
      // map at all.
      PLOG(WARNING) << "ashmem_create_region failed for '" << name << "'";
    } else {
      // We succeeded in creating the ashmem region. Use the created ashmem
      // region as backing for the mmap.
      flags &= ~MAP_ANONYMOUS;
    }
  }

  // We need to store and potentially set an error number for pretty printing of errors
  int saved_errno = 0;

  void* actual = MapInternal(expected_ptr,
                             page_aligned_byte_count,
                             prot,
                             flags,
                             fd.Fd(),
                             0,
                             /*low_4gb*/false);
  saved_errno = errno;

  if (actual == MAP_FAILED) {
    if (error_msg != nullptr) {
      *error_msg = StringPrintf("Failed anonymous mmap(%p, %zd, 0x%x, 0x%x, %d, 0): %s. ",
                                expected_ptr,
                                page_aligned_byte_count,
                                prot,
                                flags,
                                fd.Fd(),
                                strerror(saved_errno));
    }
    return nullptr;
  }
  if (!CheckMapRequest(expected_ptr, actual, page_aligned_byte_count, error_msg)) {
    return nullptr;
  }
  return new MemMap(name, reinterpret_cast<uint8_t*>(actual), byte_count, actual,
                    page_aligned_byte_count, prot, /*reuse*/false);
}

MemMap* MemMap::MapDummy(const char* name, uint8_t* addr, size_t byte_count) {
  if (byte_count == 0) {
    return new MemMap(name, nullptr, 0, nullptr, 0, 0, false);
  }
  const size_t page_aligned_byte_count = RoundUp(byte_count, kPageSize);
  return new MemMap(name, addr, byte_count, addr, page_aligned_byte_count, 0, /*reuse*/false);
}

MemMap* MemMap::MapFile(size_t byte_count,
                        int prot,
                        int flags,
                        int fd,
                        off_t start,
                        bool low_4gb,
                        const char* filename,
                        std::string* error_msg) {
  CHECK_EQ(low_4gb, false);
  return MapFileAtAddress(nullptr,
                          byte_count,
                          prot,
                          flags,
                          fd,
                          start,
                          /*low_4gb*/false,
                          /*reuse*/false,
                          filename,
                          error_msg);
}

MemMap* MemMap::MapFileAtAddress(uint8_t* expected_ptr,
                                 size_t byte_count,
                                 int prot,
                                 int flags,
                                 int fd,
                                 off_t start,
                                 bool low_4gb,
                                 bool reuse,
                                 const char* filename,
                                 std::string* error_msg) {
  CHECK_NE(0, prot);
  CHECK_NE(0, flags & (MAP_SHARED | MAP_PRIVATE));
  CHECK_EQ(low_4gb, false);
  CHECK_EQ(reuse, false);

  CHECK_EQ(0, flags & MAP_FIXED);

  if (byte_count == 0) {
    return new MemMap(filename, nullptr, 0, nullptr, 0, prot, false);
  }
  // Adjust 'offset' to be page-aligned as required by mmap.
  int page_offset = start % kPageSize;
  off_t page_aligned_offset = start - page_offset;
  // Adjust 'byte_count' to be page-aligned as we will map this anyway.
  size_t page_aligned_byte_count = RoundUp(byte_count + page_offset, kPageSize);
  // The 'expected_ptr' is modified (if specified, ie non-null) to be page aligned to the file but
  // not necessarily to virtual memory. mmap will page align 'expected' for us.
  uint8_t* page_aligned_expected =
      (expected_ptr == nullptr) ? nullptr : (expected_ptr - page_offset);

  size_t redzone_size = 0;
  if (RUNNING_ON_MEMORY_TOOL && kMemoryToolAddsRedzones && expected_ptr == nullptr) {
    redzone_size = kPageSize;
    page_aligned_byte_count += redzone_size;
  }

  uint8_t* actual = reinterpret_cast<uint8_t*>(MapInternal(page_aligned_expected,
                                                           page_aligned_byte_count,
                                                           prot,
                                                           flags,
                                                           fd,
                                                           page_aligned_offset,
                                                           /*low_4gb*/ false));
  if (actual == MAP_FAILED) {
    if (error_msg != nullptr) {
      auto saved_errno = errno;

      *error_msg = StringPrintf("mmap(%p, %zd, 0x%x, 0x%x, %d, %" PRId64
                                ") of file '%s' failed: %s.",
                                page_aligned_expected, page_aligned_byte_count, prot, flags, fd,
                                static_cast<int64_t>(page_aligned_offset), filename,
                                strerror(saved_errno));
    }
    return nullptr;
  }
  if (!CheckMapRequest(expected_ptr, actual, page_aligned_byte_count, error_msg)) {
    return nullptr;
  }
  if (redzone_size != 0) {
    const uint8_t *real_start = actual + page_offset;
    const uint8_t *real_end = actual + page_offset + byte_count;
    const uint8_t *mapping_end = actual + page_aligned_byte_count;

    MEMORY_TOOL_MAKE_NOACCESS(actual, real_start - actual);
    MEMORY_TOOL_MAKE_NOACCESS(real_end, mapping_end - real_end);
    page_aligned_byte_count -= redzone_size;
  }

  return new MemMap(filename, actual + page_offset, byte_count, actual, page_aligned_byte_count,
                    prot, /*reuse*/false, redzone_size);
}

MemMap::~MemMap() {
  if (base_begin_ == nullptr && base_size_ == 0) {
    return;
  }

  // Unlike Valgrind, AddressSanitizer requires that all manually poisoned memory is unpoisoned
  // before it is returned to the system.
  if (redzone_size_ != 0) {
    MEMORY_TOOL_MAKE_UNDEFINED(
        reinterpret_cast<char*>(base_begin_) + base_size_ - redzone_size_,
        redzone_size_);
  }

  if (!reuse_) {
    MEMORY_TOOL_MAKE_UNDEFINED(base_begin_, base_size_);
    int result = munmap(base_begin_, base_size_);
    if (result == -1) {
      PLOG(FATAL) << "munmap failed";
    }
  }
}

MemMap::MemMap(const std::string& name, uint8_t* begin, size_t size, void* base_begin,
               size_t base_size, int prot, bool reuse, size_t redzone_size)
    : name_(name), begin_(begin), size_(size), base_begin_(base_begin), base_size_(base_size),
      prot_(prot), reuse_(reuse), redzone_size_(redzone_size) {
  CHECK_EQ(reuse_, false);
  if (size_ == 0) {
    CHECK(begin_ == nullptr);
    CHECK(base_begin_ == nullptr);
    CHECK_EQ(base_size_, 0U);
  } else {
    CHECK(begin_ != nullptr);
    CHECK(base_begin_ != nullptr);
    CHECK_NE(base_size_, 0U);
  }
}

MemMap* MemMap::RemapAtEnd(uint8_t* new_end, const char* tail_name, int tail_prot,
                           std::string* error_msg, bool use_ashmem) {
  use_ashmem = use_ashmem && !kIsTargetLinux;
  DCHECK_GE(new_end, Begin());
  DCHECK_LE(new_end, End());
  DCHECK_LE(begin_ + size_, reinterpret_cast<uint8_t*>(base_begin_) + base_size_);
  DCHECK_ALIGNED(begin_, kPageSize);
  DCHECK_ALIGNED(base_begin_, kPageSize);
  DCHECK_ALIGNED(reinterpret_cast<uint8_t*>(base_begin_) + base_size_, kPageSize);
  DCHECK_ALIGNED(new_end, kPageSize);
  uint8_t* old_end = begin_ + size_;
  uint8_t* old_base_end = reinterpret_cast<uint8_t*>(base_begin_) + base_size_;
  uint8_t* new_base_end = new_end;
  DCHECK_LE(new_base_end, old_base_end);
  if (new_base_end == old_base_end) {
    return new MemMap(tail_name, nullptr, 0, nullptr, 0, tail_prot, false);
  }
  size_ = new_end - reinterpret_cast<uint8_t*>(begin_);
  base_size_ = new_base_end - reinterpret_cast<uint8_t*>(base_begin_);
  DCHECK_LE(begin_ + size_, reinterpret_cast<uint8_t*>(base_begin_) + base_size_);
  size_t tail_size = old_end - new_end;
  uint8_t* tail_base_begin = new_base_end;
  size_t tail_base_size = old_base_end - new_base_end;
  DCHECK_EQ(tail_base_begin + tail_base_size, old_base_end);
  DCHECK_ALIGNED(tail_base_size, kPageSize);

  int int_fd = -1;
  int flags = MAP_PRIVATE | MAP_ANONYMOUS;
  if (use_ashmem) {
    // android_os_Debug.cpp read_mapinfo assumes all ashmem regions associated with the VM are
    // prefixed "dalvik-".
    std::string debug_friendly_name("dalvik-");
    debug_friendly_name += tail_name;
    int_fd = ashmem_create_region(debug_friendly_name.c_str(), tail_base_size);
    flags = MAP_PRIVATE | MAP_FIXED;
    if (int_fd == -1) {
      *error_msg = StringPrintf("ashmem_create_region failed for '%s': %s",
                                tail_name, strerror(errno));
      return nullptr;
    }
  }
  File fd(int_fd, /* check_usage */ false);

  MEMORY_TOOL_MAKE_UNDEFINED(tail_base_begin, tail_base_size);
  // Unmap/map the tail region.
  int result = munmap(tail_base_begin, tail_base_size);
  if (result == -1) {
    PrintFileToLog("/proc/self/maps", LogSeverity::WARNING);
    *error_msg = StringPrintf("munmap(%p, %zd) failed for '%s'. See process maps in the log.",
                              tail_base_begin, tail_base_size, name_.c_str());
    return nullptr;
  }
  // Don't cause memory allocation between the munmap and the mmap
  // calls. Otherwise, libc (or something else) might take this memory
  // region. Note this isn't perfect as there's no way to prevent
  // other threads to try to take this memory region here.
  uint8_t* actual = reinterpret_cast<uint8_t*>(mmap(tail_base_begin, tail_base_size, tail_prot,
                                              flags, fd.Fd(), 0));
  if (actual == MAP_FAILED) {
    PrintFileToLog("/proc/self/maps", LogSeverity::WARNING);
    *error_msg = StringPrintf("anonymous mmap(%p, %zd, 0x%x, 0x%x, %d, 0) failed. See process "
                              "maps in the log.", tail_base_begin, tail_base_size, tail_prot, flags,
                              fd.Fd());
    return nullptr;
  }
  return new MemMap(tail_name, actual, tail_size, actual, tail_base_size, tail_prot, false);
}

void MemMap::MadviseDontNeedAndZero() {
  if (base_begin_ != nullptr || base_size_ != 0) {
    if (!kMadviseZeroes) {
      memset(base_begin_, 0, base_size_);
    }
    int result = madvise(base_begin_, base_size_, MADV_DONTNEED);
    if (result == -1) {
      PLOG(WARNING) << "madvise failed";
    }
  }
}

bool MemMap::Sync() {
  bool result;
  if (redzone_size_ != 0) {
    // To avoid valgrind errors, temporarily lift the lower-end noaccess protection before passing
    // it to msync() as it only accepts page-aligned base address, and exclude the higher-end
    // noaccess protection from the msync range. b/27552451.
    uint8_t* base_begin = reinterpret_cast<uint8_t*>(base_begin_);
    MEMORY_TOOL_MAKE_DEFINED(base_begin, begin_ - base_begin);
    result = msync(BaseBegin(), End() - base_begin, MS_SYNC) == 0;
    MEMORY_TOOL_MAKE_NOACCESS(base_begin, begin_ - base_begin);
  } else {
    result = msync(BaseBegin(), BaseSize(), MS_SYNC) == 0;
  }
  return result;
}

bool MemMap::Protect(int prot) {
  if (base_begin_ == nullptr && base_size_ == 0) {
    prot_ = prot;
    return true;
  }

  if (mprotect(base_begin_, base_size_, prot) == 0) {
    prot_ = prot;
    return true;
  }

  PLOG(ERROR) << "mprotect(" << reinterpret_cast<void*>(base_begin_) << ", " << base_size_ << ", "
              << prot << ") failed";
  return false;
}

bool MemMap::CheckNoGaps(MemMap* begin_map, MemMap* end_map) {
  CHECK(begin_map != nullptr);
  CHECK(end_map != nullptr);
  return true;
}

void MemMap::DumpMaps(std::ostream& os, bool terse) {
  UNUSED(os);
  UNUSED(terse);
}

void MemMap::DumpMapsLocked(std::ostream& os, bool terse) {
  UNUSED(os);
  UNUSED(terse);
}

bool MemMap::HasMemMap(MemMap* map) {
  UNUSED(map);
  return false;
}

MemMap* MemMap::GetLargestMemMapAt(void* address) {
  UNUSED(address);
  LOG(ERROR) << "GetLargestMemMapAt unsupported";
  return nullptr;
}

void MemMap::Init() {
}

void MemMap::Shutdown() {
}

void MemMap::SetSize(size_t new_size) {
  if (new_size == base_size_) {
    return;
  }
  CHECK_ALIGNED(new_size, kPageSize);
  CHECK_EQ(base_size_, size_) << "Unsupported";
  CHECK_LE(new_size, base_size_);
  MEMORY_TOOL_MAKE_UNDEFINED(
      reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(BaseBegin()) +
                              new_size),
      base_size_ - new_size);
  CHECK_EQ(munmap(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(BaseBegin()) + new_size),
                  base_size_ - new_size), 0) << new_size << " " << base_size_;
  base_size_ = new_size;
  size_ = new_size;
}

void* MemMap::MapInternal(void* addr,
                          size_t length,
                          int prot,
                          int flags,
                          int fd,
                          off_t offset,
                          bool low_4gb) {
  CHECK_EQ(low_4gb, false);
  DCHECK_ALIGNED(length, kPageSize);
  return mmap(addr, length, prot, flags, fd, offset);
}

std::ostream& operator<<(std::ostream& os, const MemMap& mem_map) {
  os << StringPrintf("[MemMap: %p-%p prot=0x%x %s]",
                     mem_map.BaseBegin(), mem_map.BaseEnd(), mem_map.GetProtect(),
                     mem_map.GetName().c_str());
  return os;
}

void MemMap::TryReadable() {
  if (base_begin_ == nullptr && base_size_ == 0) {
    return;
  }
  CHECK_NE(prot_ & PROT_READ, 0);
  volatile uint8_t* begin = reinterpret_cast<volatile uint8_t*>(base_begin_);
  volatile uint8_t* end = begin + base_size_;
  DCHECK(IsAligned<kPageSize>(begin));
  DCHECK(IsAligned<kPageSize>(end));
  // Read the first byte of each page. Use volatile to prevent the compiler from optimizing away the
  // reads.
  for (volatile uint8_t* ptr = begin; ptr < end; ptr += kPageSize) {
    // This read could fault if protection wasn't set correctly.
    uint8_t value = *ptr;
    UNUSED(value);
  }
}

void ZeroAndReleasePages(void* address, size_t length) {
  uint8_t* const mem_begin = reinterpret_cast<uint8_t*>(address);
  uint8_t* const mem_end = mem_begin + length;
  uint8_t* const page_begin = AlignUp(mem_begin, kPageSize);
  uint8_t* const page_end = AlignDown(mem_end, kPageSize);
  if (!kMadviseZeroes || page_begin >= page_end) {
    // No possible area to madvise.
    std::fill(mem_begin, mem_end, 0);
  } else {
    // Spans one or more pages.
    DCHECK_LE(mem_begin, page_begin);
    DCHECK_LE(page_begin, page_end);
    DCHECK_LE(page_end, mem_end);
    std::fill(mem_begin, page_begin, 0);
    CHECK_NE(madvise(page_begin, page_end - page_begin, MADV_DONTNEED), -1) << "madvise failed";
    std::fill(page_end, mem_end, 0);
  }
}

}  // namespace art
