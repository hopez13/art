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

#include "base/arena_allocator-inl.h"
#include "base/utils.h"

#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

namespace art {

#if defined(__LP64__)
// Use a size in multiples of 1GB as that can utilize the optimized mremap
// page-table move.
static constexpr size_t kLinearAllocPoolSize = 1 * GB;
static constexpr size_t kLow4GBLinearAllocPoolSize = 32 * MB;
#else
static constexpr size_t kLinearAllocPoolSize = 32 * MB;
#endif

TrackedArena::TrackedArena(uint8_t* start, size_t size) : Arena(), first_obj_array_(nullptr) {
  static_assert(ArenaAllocator::kArenaAlignment <= kPageSize,
                "Arena should not need stronger alignment than kPageSize.");
  DCHECK_ALIGNED(size, kPageSize);
  DCHECK_ALIGNED(start, kPageSize);
  memory_ = start;
  size_ = size;
  size_t arr_size = size / kPageSize;
  first_obj_array_.reset(new uint8_t*[arr_size]);
  std::fill_n(first_obj_array_.get(), arr_size, nullptr);
}

void TrackedArena::Release() {
  if (bytes_allocated_ > 0) {
    ZeroAndReleasePages(Begin(), Size());
    std::fill_n(first_obj_array_.get(), Size() / kPageSize, nullptr);
    bytes_allocated_ = 0;
  }
}

void TrackedArena::SetFirstObject(uint8_t* addr, uint8_t* end) {
  DCHECK_LE(static_cast<void*>(Begin()), static_cast<void*>(addr));
  DCHECK_LT(static_cast<void*>(addr), static_cast<void*>(end));
  size_t idx = static_cast<size_t>(addr - Begin()) / kPageSize;
  size_t last_byte_idx = static_cast<size_t>(end - 1 - Begin()) / kPageSize;
  // If the addr is at the beginning of a page, then we set it for that page too.
  if (IsAligned<kPageSize>(addr)) {
    first_obj_array_[idx] = addr;
  }
  while (idx < last_byte_idx) {
    first_obj_array_[++idx] = addr;
  }
}

GcVisitedArenaPool::GcVisitedArenaPool(bool low_4gb, const char* name) : bytes_allocated_(0) {
  size_t size = kLinearAllocPoolSize;
#if defined(__LP64__)
  // This is true only when we are running a 64-bit dex2oat to compile a 32-bit image.
  if (low_4gb) {
    size = kLow4GBLinearAllocPoolSize;
  }
#endif
  std::string err_msg;
  memory_ = MemMap::MapAnonymous(name,
                                 size,
                                 PROT_READ | PROT_WRITE,
                                 low_4gb,
                                 &err_msg);
  if (!memory_.IsValid()) {
    LOG(FATAL) << "Failed to allocate " << name
               << ": " << err_msg;
    UNREACHABLE();
  }
  Chunk* chunk = new Chunk(memory_.Begin(), memory_.Size());
  best_fit_allocs_.insert(chunk);
  free_chunks_.insert(chunk);
}

GcVisitedArenaPool::~GcVisitedArenaPool() {
  for (Chunk* chunk : free_chunks_) {
    delete chunk;
  }
  // Must not delete chunks from best_fit_allocs_ as they are shared with
  // free_chunks_.
}

size_t GcVisitedArenaPool::GetBytesAllocated() const {
  std::lock_guard<std::mutex> lock(lock_);
  return bytes_allocated_;
}

static size_t allocated_sz = 0;
Arena* GcVisitedArenaPool::AllocArena(size_t size) {
  TrackedArena* ret = nullptr;
  // Return only page aligned sizes so that madvise can be leveraged.
  size = RoundUp(size, kPageSize);
  Chunk temp_chunk(nullptr, size);
  std::lock_guard<std::mutex> lock(lock_);
  auto best_fit_iter = best_fit_allocs_.lower_bound(&temp_chunk);
  // TODO: consider implementing a mechanism where we can allocate a new memory
  // range in the extremely rare case when it happens.
  CHECK(best_fit_iter != best_fit_allocs_.end()) << "Out of memory. Increase arena-pool's size"
                                                 << ". Allocated-sz:" << PrettySize(allocated_sz);
  auto free_chunks_iter = free_chunks_.find(*best_fit_iter);
  DCHECK(free_chunks_iter != free_chunks_.end());
  Chunk* chunk = *best_fit_iter;
  DCHECK_EQ(chunk, *free_chunks_iter);
  // if the best-fit chunk < 2x the requested size, then give the whole chunk.
  if (chunk->size_ < 2 * size) {
    DCHECK_GE(chunk->size_, size);
    ret = new TrackedArena(chunk->addr_, chunk->size_);
    free_chunks_.erase(free_chunks_iter);
    best_fit_allocs_.erase(best_fit_iter);
    delete chunk;
  } else {
    ret = new TrackedArena(chunk->addr_, size);
    // Compute next iterators for faster insert later.
    auto next_best_fit_iter = best_fit_iter;
    next_best_fit_iter++;
    auto next_free_chunks_iter = free_chunks_iter;
    next_free_chunks_iter++;
    auto best_fit_nh = best_fit_allocs_.extract(best_fit_iter);
    auto free_chunks_nh = free_chunks_.extract(free_chunks_iter);
    best_fit_nh.value()->addr_ += size;
    best_fit_nh.value()->size_ -= size;
    DCHECK_EQ(free_chunks_nh.value()->addr_, chunk->addr_);
    best_fit_allocs_.insert(next_best_fit_iter, std::move(best_fit_nh));
    free_chunks_.insert(next_free_chunks_iter, std::move(free_chunks_nh));
  }
  allocated_sz += ret->Size();
  allocated_arenas_.emplace(ret);
  return ret;
}

void GcVisitedArenaPool::FreeRangeLocked(uint8_t* range_begin, size_t range_size) {
  Chunk temp_chunk(range_begin, range_size);
  auto free_chunks_iter = free_chunks_.lower_bound(&temp_chunk);
  auto best_fit_iter = best_fit_allocs_.end();
  // Try to merge with the next chunk.
  if (free_chunks_iter != free_chunks_.end()
      && range_begin + range_size == (*free_chunks_iter)->addr_) {
    best_fit_iter = best_fit_allocs_.find(*free_chunks_iter);
    DCHECK(best_fit_iter != best_fit_allocs_.end());
    // Fetch the next iterators for fast insertion
    auto next_free_chunks_iter = free_chunks_iter;
    next_free_chunks_iter++;
    auto next_best_fit_iter = best_fit_iter;
    next_best_fit_iter++;

    auto free_chunks_nh = free_chunks_.extract(free_chunks_iter);
    auto best_fit_nh = best_fit_allocs_.extract(best_fit_iter);
    DCHECK(!free_chunks_nh.empty());
    DCHECK(!best_fit_nh.empty());

    free_chunks_nh.value()->addr_ = range_begin;
    free_chunks_nh.value()->size_ += range_size;
    DCHECK_EQ(best_fit_nh.value()->addr_, range_begin);
    free_chunks_iter = free_chunks_.insert(next_free_chunks_iter, std::move(free_chunks_nh));
    // Store the iterator of the expanded chunk for best-fit set for the next
    // part.
    best_fit_iter = best_fit_allocs_.insert(next_best_fit_iter, std::move(best_fit_nh));
    DCHECK(best_fit_iter != best_fit_allocs_.end());
  }
  // Try to merge with the previous chunk.
  if (free_chunks_iter != free_chunks_.begin()) {
    auto prev_free_chunks_iter = free_chunks_iter;
    prev_free_chunks_iter--;
    Chunk* chunk = *prev_free_chunks_iter;
    DCHECK_LT(chunk->addr_, range_begin);
    // Merging with previous chunk is possible.
    if (chunk->addr_ + chunk->size_ == range_begin) {
      auto prev_best_fit_iter = best_fit_allocs_.find(chunk);
      DCHECK_EQ(*prev_best_fit_iter, chunk);
      // We previousely merged with the next chunk.
      if (free_chunks_iter != free_chunks_.end() && (*free_chunks_iter)->addr_ == range_begin) {
        // erase the next chunk
        Chunk* next_chunk = *free_chunks_iter;
        range_size = next_chunk->size_;
        DCHECK(best_fit_iter != best_fit_allocs_.end());
        DCHECK_EQ(*best_fit_iter, *free_chunks_iter);
        best_fit_allocs_.erase(best_fit_iter);
        free_chunks_.erase(free_chunks_iter);
        delete next_chunk;
      }
      // expand the previous chunk
      free_chunks_iter = prev_free_chunks_iter;
      free_chunks_iter++;
      best_fit_iter = prev_best_fit_iter;
      best_fit_iter++;

      auto free_chunks_nh = free_chunks_.extract(prev_free_chunks_iter);
      auto best_fit_nh = best_fit_allocs_.extract(prev_best_fit_iter);
      free_chunks_nh.value()->size_ += range_size;

      free_chunks_.insert(free_chunks_iter, std::move(free_chunks_nh));
      best_fit_allocs_.insert(best_fit_iter, std::move(best_fit_nh));
      return;
    }
  }
  // Couldn't merge with any pre-existing chunk so create a new one and insert.
  if (free_chunks_iter == free_chunks_.end() || (*free_chunks_iter)->addr_ > range_begin) {
    Chunk* chunk = new Chunk(range_begin, range_size);
    free_chunks_.insert(free_chunks_iter, chunk);
    best_fit_allocs_.insert(best_fit_iter, chunk);
  }
}

void GcVisitedArenaPool::FreeArenaChain(Arena* first) {
  if (kRunningOnMemoryTool) {
    for (Arena* arena = first; arena != nullptr; arena = arena->Next()) {
      MEMORY_TOOL_MAKE_UNDEFINED(arena->Begin(), arena->GetBytesAllocated());
    }
  }

  // TODO: Handle the case when arena_allocator::kArenaAllocatorPreciseTracking
  // is true. See MemMapArenaPool::FreeArenaChain() for example.
  DCHECK(!arena_allocator::kArenaAllocatorPreciseTracking);

  // madvise the arenas before acquiring lock for scalability
  for (Arena* temp = first; temp != nullptr; temp = temp->Next()) {
    temp->Release();
  }

  std::lock_guard<std::mutex> lock(lock_);
  while (first != nullptr) {
    FreeRangeLocked(first->Begin(), first->Size());
    allocated_sz -= first->Size();
    std::unique_ptr<TrackedArena> temp(down_cast<TrackedArena*>(first));
    first = first->Next();
    bytes_allocated_ += temp->GetBytesAllocated();
    allocated_arenas_.erase(temp);
    temp.release();
  }
}

}  // namespace art

