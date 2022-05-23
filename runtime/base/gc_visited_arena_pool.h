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

#ifndef ART_RUNTIME_BASE_GC_VISITED_ARENA_POOL_H_
#define ART_RUNTIME_BASE_GC_VISITED_ARENA_POOL_H_

#include "base/arena_allocator.h"
#include "base/locks.h"
#include "base/mem_map.h"

#include <set>

namespace art {

// GcVisitedArenaPool can be used for tracking allocations so that they can
// visited during GC to update the GC-roots inside them.

// An Arena which tracks allocations within the arena.
class TrackedArena final : public Arena {
 public:
  TrackedArena(uint8_t* start, size_t size);
  virtual ~TrackedArena();

  template <typename PageVisitor>
  void VisitRoots(PageVisitor& visitor) REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK_ALIGNED(Size(), kPageSize);
    int nr_pages = Size() / kPageSize;
    uint8_t* page_begin = Begin();
    for (int i = 0; i < nr_pages && first_obj_array_[i] != nullptr; i++, page_begin += kPageSize) {
      visitor(page_begin, first_obj_array_[i]);
    }
  }

  void SetFirstObject(uint8_t* addr, uint8_t* end);

  void Release() override;

 private:
  void AllocateFirstObjArray();
  void FreeFirstObjArray();
  // first_obj_array_[i] is the object that overlaps with the ith page's
  // beginning.
  uint8_t** first_obj_array_;
};

class GcVisitedArenaPool final : public ArenaPool {
 public:
  explicit GcVisitedArenaPool(bool low_4gb = false, const char* name = "LinearAlloc");
  virtual ~GcVisitedArenaPool();
  Arena* AllocArena(size_t size) override;
  void FreeArenaChain(Arena* first) override;
  size_t GetBytesAllocated() const override;
  void ReclaimMemory() override {}
  void LockReclaimMemory() override {}
  void TrimMaps() override {}

  template <typename PageVisitor>
  void VisitRoots(PageVisitor& visitor) REQUIRES_SHARED(Locks::mutator_lock_) {
    for (auto arena : allocated_arenas_) {
      TrackedArena* tarena = static_cast<TrackedArena*>(arena);
      tarena->VisitRoots(visitor);
    }
  }

 private:
  void FreeRangeLocked(uint8_t* range_begin, size_t range_size);

  class Chunk {
   public:
    Chunk(uint8_t* addr, size_t size) : addr_(addr), size_(size) {}
    uint8_t* addr_;
    size_t size_;
  };

  class SortByChunkAddr {
   public:
    bool operator()(const Chunk* a, const Chunk* b) const {
      return a->addr_ < b->addr_;
    }
  };

  class SortByChunkSize {
   public:
    // Since two chunks could have the same size, use addr when that happens.
    bool operator()(const Chunk* a, const Chunk* b) const {
      return (a->size_ < b->size_) || (a->size_ == b->size_ && a->addr_ < b->addr_);
    }
  };

  class SortByArenaAddr {
   public:
    bool operator()(const Arena* a, const Arena* b) const {
      return a->Begin() < b->Begin();
    }
  };

  MemMap memory_;
  // Use a std::mutex here as Arenas are second-from-the-bottom when using MemMaps, and MemMap
  // itself uses std::mutex scoped to within an allocate/free only.
  mutable std::mutex lock_;
  std::set<Chunk*, SortByChunkSize> best_fit_allocs_;
  std::set<Chunk*, SortByChunkAddr> free_chunks_;
  // Set of allocated arenas. It's required to be able to find the arena
  // corresponding to a given address.
  // TODO: We can manage without this set if we decide to have a large
  // 'first-object' array for the entire space, instead of per arena. Analyse
  // which approach is better.
  std::set<Arena*, SortByArenaAddr> allocated_arenas_;
  size_t bytes_allocated_;

  DISALLOW_COPY_AND_ASSIGN(GcVisitedArenaPool);
};

}  // namespace art

#endif  // ART_RUNTIME_BASE_GC_VISITED_ARENA_POOL_H_
