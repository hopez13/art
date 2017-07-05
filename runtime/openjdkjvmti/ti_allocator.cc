/* Copyright (C) 2016 The Android Open Source Project
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This file implements interfaces from the file jvmti.h. This implementation
 * is licensed under the same terms as the file jvmti.h.  The
 * copyright and license information for the file jvmti.h follows.
 *
 * Copyright (c) 2003, 2011, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#include "ti_allocator.h"

#include <atomic>
#include <mutex>
#include <unordered_map>

#include "art_jvmti.h"
#include "base/enums.h"

namespace openjdkjvmti {

class JvmtiTrackingAllocator {
 public:
  JvmtiTrackingAllocator() : enabled_(false), allocated_count_(0), freed_count_(0), size_map_(0) { }

  unsigned char* Allocate(jlong size) {
    unsigned char* ret = reinterpret_cast<unsigned char*>(malloc(size));
    if (UNLIKELY(TrackingEnabled())) {
      TrackAllocation(ret, size);
    }
    return ret;
  }

  void Deallocate(unsigned char* p) {
    if (UNLIKELY(TrackingEnabled())) {
      TrackDeallocation(p);
    }
    free(p);
  }

  void StartTracking() {
    enabled_.store(true);
  }

  bool TrackingEnabled() const {
    return enabled_.load();
  }

  void GetStats(jlong* allocated, jlong* freed) {
    DCHECK(TrackingEnabled());
    std::lock_guard<std::mutex> lock(stats_lock_);
    *allocated = allocated_count_;
    *freed = freed_count_;
  }

 private:
  void TrackAllocation(unsigned char* p, size_t size) {
    DCHECK(TrackingEnabled());
    std::lock_guard<std::mutex> lock(stats_lock_);
    allocated_count_ += size;
    auto res = size_map_.insert({p, size});
    CHECK(res.second) << "allocator returned already allocated value!";
  }

  void TrackDeallocation(unsigned char* p) {
    DCHECK(TrackingEnabled());
    std::lock_guard<std::mutex> lock(stats_lock_);
    auto it = size_map_.find(p);
    if (it == size_map_.end()) {
      return;
    }
    freed_count_ += it->second;
    size_map_.erase(it);
  }

  std::atomic<bool> enabled_;

  std::mutex stats_lock_;
  jlong allocated_count_ GUARDED_BY(stats_lock_);
  jlong freed_count_ GUARDED_BY(stats_lock_);

  std::unordered_map<unsigned char*, size_t> size_map_ GUARDED_BY(stats_lock_);
};

JvmtiTrackingAllocator Allocator;

jvmtiError AllocUtil::TrackGlobalJvmtiAllocations(jvmtiEnv* env ATTRIBUTE_UNUSED) {
  Allocator.StartTracking();
  return OK;
}

jvmtiError AllocUtil::GetGlobalJvmtiAllocationStats(jvmtiEnv* env ATTRIBUTE_UNUSED,
                                                    jlong* known_allocated,
                                                    jlong* known_deallocated) {
  if (!Allocator.TrackingEnabled()) {
    return ERR(ABSENT_INFORMATION);
  } else if (known_allocated == nullptr || known_deallocated == nullptr) {
    return ERR(NULL_POINTER);
  }
  Allocator.GetStats(known_allocated, known_deallocated);
  return OK;
}

jvmtiError AllocUtil::Allocate(jvmtiEnv* env ATTRIBUTE_UNUSED,
                               jlong size,
                               unsigned char** mem_ptr) {
  if (size < 0) {
    return ERR(ILLEGAL_ARGUMENT);
  } else if (size == 0) {
    *mem_ptr = nullptr;
    return OK;
  }
  *mem_ptr = Allocator.Allocate(size);
  return (*mem_ptr != nullptr) ? OK : ERR(OUT_OF_MEMORY);
}

jvmtiError AllocUtil::Deallocate(jvmtiEnv* env ATTRIBUTE_UNUSED, unsigned char* mem) {
  if (mem != nullptr) {
    Allocator.Deallocate(mem);
  }
  return OK;
}

}  // namespace openjdkjvmti
