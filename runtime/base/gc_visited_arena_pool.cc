/*
 * Copyright 2022 The Android Open Source Project
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

#include "base/gc_visited_arena_pool.h"

#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

namespace art {

#if defined(__LP64__)
// Use a size in multiples of 1GB as that can utilize the optimized mremap
// page-table move.
static constexpr size_t kLinearAllocSize = 1 * GB;
#else
static constexpr size_t kLinearAllocSize = 16 * MB;
#endif

TrackedArena::TrackedArena(uint8_t* start, size_t size) : Arena(), first_obj_array_(nullptr) {
  DCHECK_ALIGNED(size, kPageSize);
  DCHECK_ALIGNED(start, kPageSize);
  memory_ = start;
  size_ = size;
  AllocateFirstObjArray();
}

void TrackedArena::FreeFirstObjArray() {
  if (first_obj_array_ != nullptr) {
    delete[] first_obj_array_;
    first_obj_array_ = nullptr;
  }
}

void TrackedArena::Release() {
  if (bytes_allocated_ > 0) {
    ZeroAndReleasePages(Begin(), Size());
    bytes_allocated_ = 0;
  }
}

TrackedArena::~TrackedArena() {
  FreeFirstObjArray();
}

void TrackedArena::AllocateFirstObjArray() {
  CHECK_EQ(first_obj_array_, nullptr);
  size_t arr_size = size_ / kPageSize;
  first_obj_array_ = new uint8_t*[arr_size];
  std::fill_n(first_obj_array_, arr_size, 0);
}

void TrackedArena::SetFirstObject(uint8_t* addr, uint8_t* end) {
  void* start = addr;
  addr += std::min(end - addr, AlignUp(addr, kPageSize) - addr);
  for (size_t idx = (addr - Begin()) / kPageSize; addr < end; addr += kPageSize, idx++) {
    first_obj_array[idx] = start;
  }
}

GcVisitedArenaPool::GcVisitedArenaPool(bool low_4gb, const char* name) : bytes_allocated_(0) {
  fd_ = memfd_create(name, MFD_CLOEXEC);
  if (fd_ == -1) {
    LOG(FATAL) << "Unable to create an anonymous file for " << name
               << " reason: " << strerror(errno);
  }
  int ret = ftruncate(fd_, kLinearAllocSize);
  if (ret == -1) {
    LOG(FATAL) << "Unable to truncate file for " << name
               << " with size: " << PrettySize(kLinearAllocSize)
               << " reason: " << strerror(errno);
  }
  std::string err_msg;
  memory_ = MemMap::MapFile(kLinearAllocSize,
                            PROT_READ | PROT_WRITE,
                            MAP_SHARED,
                            fd_,
                            0,
                            low_4gb,
                            name,
                            &err_msg);
  if (!memory_.IsValid()) {
    LOG(FATAL) << "Failed to allocate " << name
               << ": " << err_msg;
  }
  Chunk* chunk = new Chunk(memory_.Begin(), memory_.Size());
  best_fit_allocs_.insert(chunk);
  free_chunks_.insert(chunk);
}

size_t GcVisitedArenaPool::GetBytesAllocated() const {
  size_t total = 0;
  std::lock_guard<std::mutex> lock(lock_);
  return bytes_allocated_;
}

Arena* GcVisitedArenaPool::AllocArena(size_t size) {
  Arena* ret = nullptr;
  // Return only page aligned sizes so that madvise can be leveraged.
  size = RoundUp(size, kPageSize);
  Chunk temp_chunk(nullptr, size);
  std::lock_guard<std::mutex> lock(lock_);
  auto it = best_fit_allocs_.lower_bound(&temp_chunk);
  // TODO: consider implementing a mechanism where we can allocated a new memory
  // range in the extremely rare case when it happens.
  CHECK_NE(it, best_fit_allocs_.end()) << "Out of memory. Increase arena-pool's size";
  auto free_chunks_it = free_chunks_.find(*it);
  DCHECK_NE(free_chunks_it, free_chunks_.end());
  Chunk* chunk = *it;
  DCHECK_EQ(chunk, *free_chunks_it);
  // if the best-fit chunk < 2x the requested size, then give the whole chunk.
  if (chunk->size_ < 2 * size) {
    DCHECK_GE(chunk->size_, size);
    ret = new TrackedArena(chunk->addr_, chunk->size_);
    free_chunks_.erase(free_chunks_it);
    best_fit_allocs_.erase(it);
    delete chunk;
  } else {
    ret = new TrackedArena((chunk->addr_, size);
    auto next_it = it;
    next_it++;
    auto next_free_chunks_it = free_chunks_it;
    next_free_chunks_it++;
    auto nh = best_fit_allocs_.extract(it);
    auto free_chunks_nh = free_chunks_.extract(free_chunks_it);
    nh.value()->addr_ += size;
    nh.value()->size_ -= size;
    best_fit_allocs_.insert(next_it, nh);
    free_chunks_.insert(next_free_chunks_it, free_chunks_nh);
  }
  allocated_arenas_.insert(ret);
  return ret;
}

void GcVisitedArenaPool::FreeRangeLocked(uint8_t* range_begin, size_t range_size) {
  Chunk temp_chunk(range_begin, range_size);
  auto it = free_chunks_.lower_bound(&temp_chunk);
  auto prev_it = it;
  prev_it--;
  auto sized_set_it = best_fit_allocs_.end();
  // Try to merge with the next chunk.
  if (it != free_chunks_.end() && range_begin + range_size == (*it)->addr_) {
    sized_set_it = best_fit_allocs_.find(*it);
    DCHECK_NE(sized_set_it, best_fit_allocs_.end());
    // Fetch the next iterators for fast insertion
    auto next_it = it;
    next_it++;
    auto next_sized_set_it = sized_set_it;
    next_sized_set_it++;

    auto nh = free_chunks_.extract(it);
    auto sized_set_nh = best_fit_allocs_.extract(sized_set_it);
    DCHECK(!nh.empty());
    DCHECK(!sized_set_nh.empty());

    nh.value()->addr_ = range_begin;
    nb.value()->size_ += range_size;
    it = free_chunks_.insert(next_it, nh);
    // Store the iterator of the expanded chunk for best-fit set for the next
    // part.
    sized_set_it = best_fit_allocs_.insert(next_sized_set_it, sized_set_nh);
    DCHECK_NE(sized_set_it, best_fit_allocs_.end());
  }
  // Try to merge with the previous chunk.
  if (prev_it != free_chunks_.begin()) {
    Chunk* chunk = *prev_it;
    DCHECK_LT(chunk->addr_, range_begin);
    if (chunk->addr_ + chunk->size_ == range_begin) {
      bool expanding_prev = false;
      if (it != free_chunks_.end()) {
        DCHECK_NE(sized_set_it, best_fit_allocs_, end());
        best_fit_allocs_.erase(*prev_it);
        free_chunks_.erase(prev_it);
      } else {
        expanding_prev = true;
        sized_set_it = best_fit_allocs_.find(chunk);
        DCHECK_NE(sized_set_it, best_fit_allocs_.end());
        it = prev_it;
      }
      // For fast insertion
      auto next_sized_set_it = sized_set_it;
      next_sized_set_it++;
      auto next_it = it;
      next_it++;

      auto nh = free_chunks_.extract(it);
      auto sized_set_nh = best_fit_allocs_.extract(sized_set_it);

      if (expanding_prev) {
        nh.value()->size_ += range_size;
      } else {
        nh.value()->addr_ = chunk->addr_;
        nh.value()->size_ += chunk->size_;
        // Both node handles should be pointing to the same chunk instance.
        DCHECK_EQ(sized_set_nh.value()->addr_, chunk->addr_);
        delete chunk;
      }
      it = free_chunks_.insert(next_it, nh);
      sized_set_it = best_fit_allocs_.insert(next_sized_set_it, sized_set_nh);
    }
  }
  // Couldn't merge with any pre-existing chunk so create a new one and insert.
  if (it == free_chunks_.end()
      || (*it)->addr_ > range_begin
      || (*it)->addr_ + (*it)->size_ <= range_begin) {
    Chunk* chunk = new Chunk(range_begin, range_size);
    free_chunks_.insert(it, chunk);
    best_fit_allocs_.insert(sized_set_it, chunk);
  }
}

void GcVisitedArenaPool::FreeArenaChain(Arena* first) {
  if (kRunningOnMemoryTool) {
    for (Arena* arena = first; arena != nullptr; arena = arena->next_) {
      MEMORY_TOOL_MAKE_UNDEFINED(arena->memory_, arena->bytes_allocated_);
    }
  }

  // madvise the arenas before acquiring lock for scalability
  for (Arena* temp = first; temp != nullptr; temp = temp->next_) {
    temp->Release();
  }

  std::lock_guard<std::mutex> lock(lock_);
  while (first != nullptr) {
    FreeRangeLocked(first->Begin(), first->Size());
    Arena* temp = first;
    bytes_allocated_ += temp->GetBytesAllocated();
    first = first->next_;
    allocated_arenas_.erase(temp);
    delete temp;
  }
}

}  // namespace art

