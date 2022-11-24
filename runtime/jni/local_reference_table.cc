/*
 * Copyright (C) 2022 The Android Open Source Project
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

#include "base/bit_utils.h"
#include "base/globals.h"
#include "indirect_reference_table-inl.h"

#include "base/casts.h"
#include "base/mutator_locked_dumpable.h"
#include "base/systrace.h"
#include "base/utils.h"
#include "indirect_reference_table.h"
#include "jni/java_vm_ext.h"
#include "jni/jni_internal.h"
#include "mirror/object-inl.h"
#include "nth_caller_visitor.h"
#include "object_callbacks.h"
#include "reference_table.h"
#include "runtime-inl.h"
#include "scoped_thread_state_change-inl.h"
#include "thread.h"

#include <cstdlib>

namespace art {
namespace jni {

static constexpr bool kDumpStackOnNonLocalReference = false;
static constexpr bool kDebugLRT = false;

// Mmap an "indirect ref table region. Table_bytes is a multiple of a page size.
static inline MemMap NewLRTMap(size_t table_bytes, std::string* error_msg) {
  return MemMap::MapAnonymous("local ref table",
                              table_bytes,
                              PROT_READ | PROT_WRITE,
                              /*low_4gb=*/ false,
                              error_msg);
}

SmallLrtAllocator::SmallLrtAllocator()
    : free_lists_(kNumSlots, nullptr),
      shared_lrt_maps_(),
      lock_("Small IRT table lock", LockLevel::kGenericBottomLock) {
}

inline size_t SmallLrtAllocator::GetIndex(size_t size) {
  DCHECK_GE(size, kSmallLrtEntries);
  DCHECK_LT(size, kPageSize / sizeof(LrtEntry));
  DCHECK(IsPowerOfTwo(size));
  size_t index = WhichPowerOf2(size / kSmallLrtEntries);
  DCHECK_LT(index, kNumSlots);
  return index;
}

// Allocate an IRT table for kSmallIrtEntries.
LrtEntry* SmallLrtAllocator::Allocate(size_t size, std::string* error_msg) {
  size_t index = GetIndex(size);
  MutexLock lock(Thread::Current(), lock_);
  size_t fill_from = index;
  while (fill_from != kNumSlots && free_lists_[fill_from] == nullptr) {
    ++fill_from;
  }
  void* result = nullptr;
  if (fill_from != kNumSlots) {
    // We found a slot with enough memory.
    result = free_lists_[fill_from];
    free_lists_[fill_from] = *reinterpret_cast<void**>(result);
  } else {
    // We need to allocate a new page and split it to smaller pieces.
    MemMap map = NewLRTMap(kPageSize, error_msg);
    if (!map.IsValid()) {
      return nullptr;
    }
    result = reinterpret_cast<IrtEntry*>(map.Begin());
    shared_lrt_maps_.emplace_back(std::move(map));
  }
  while (fill_from != index) {
    --fill_from;
    // Store the second half of the current buffer in appropriate free list slot.
    void* mid = reinterpret_cast<uint8_t*>(result) + (kInitialLrtBytes << fill_from);
    DCHECK(free_lists_[fill_from] == nullptr);
    *reinterpret_cast<void**>(mid) = nullptr;
    free_lists_[fill_from] = mid;
  }
  // Clear memory we return to the caller.
  std::memset(result, 0, kInitialLrtBytes << index);
  return reinterpret_cast<LrtEntry*>(result);
}

void SmallLrtAllocator::Deallocate(LrtEntry* unneeded, size_t size) {
  size_t index = GetIndex(size);
  MutexLock lock(Thread::Current(), lock_);
  // TODO: Merge small chunks into bigger chunks. Without this we're permanently keeping up to
  // one page per thread at the peak thread allocation, even if the threads are later destroyed.
  *reinterpret_cast<void**>(unneeded) = free_lists_[index];
  free_lists_[index] = unneeded;
}

LocalReferenceTable::LocalReferenceTable()
    : segment_state_(kLRTFirstSegment),
      max_entries_(0u),
      free_entries_list_(kFreeListEnd),
      small_table_(nullptr),
      tables_(),
      table_mem_maps_() {
}

bool LocalReferenceTable::Initialize(size_t max_count, std::string* error_msg) {
  CHECK(error_msg != nullptr);

  // Overflow and maximum check.
  CHECK_LE(max_count, kMaxTableSizeInBytes / sizeof(LrtEntry));

  SmallLrtAllocator* small_lrt_allocator = Runtime::Current()->GetSmallLrtAllocator();
  LrtEntry* first_table = small_lrt_allocator->Allocate(kSmallLrtEntries, error_msg);
  if (first_table == nullptr) {
    DCHECK(!error_msg->empty());
    return false;
  }
  small_table_ = first_table;
  max_entries_ = kSmallLrtEntries;
  return (max_count <= kSmallLrtEntries) || Resize(max_count, error_msg);
}

LocalReferenceTable::~LocalReferenceTable() {
  SmallLrtAllocator* small_lrt_allocator =
      max_entries_ != 0u ? Runtime::Current()->GetSmallLrtAllocator() : nullptr;
  if (small_table_ != nullptr) {
    small_lrt_allocator->Deallocate(small_table_, kSmallLrtEntries);
    DCHECK(tables_.empty());
  } else {
    size_t num_small_tables = std::min(tables_.size(), MaxSmallTables());
    for (size_t i = 0; i != num_small_tables; ++i) {
      small_lrt_allocator->Deallocate(tables_[i], GetTableSize(i));
    }
  }
}

#if 0
// Holes:
//
// To keep the IRT compact, we want to fill "holes" created by non-stack-discipline Add & Remove
// operation sequences. For simplicity and lower memory overhead, we do not use a free list or
// similar. Instead, we scan for holes, with the expectation that we will find holes fast as they
// are usually near the end of the table (see the header, TODO: verify this assumption). To avoid
// scans when there are no holes, the number of known holes should be tracked.
//
// A previous implementation stored the top index and the number of holes as the segment state.
// This constraints the maximum number of references to 16-bit. We want to relax this, as it
// is easy to require more references (e.g., to list all classes in large applications). Thus,
// the implicitly stack-stored state, the IRTSegmentState, is only the top index.
//
// Thus, hole count is a local property of the current segment, and needs to be recovered when
// (or after) a frame is pushed or popped. To keep JNI transitions simple (and inlineable), we
// cannot do work when the segment changes. Thus, Add and Remove need to ensure the current
// hole count is correct.
//
// To be able to detect segment changes, we require an additional local field that can describe
// the known segment. This is last_known_previous_state_. The requirement will become clear with
// the following (some non-trivial) cases that have to be supported:
//
// 1) Segment with holes (current_num_holes_ > 0), push new segment, add/remove reference
// 2) Segment with holes (current_num_holes_ > 0), pop segment, add/remove reference
// 3) Segment with holes (current_num_holes_ > 0), push new segment, pop segment, add/remove
//    reference
// 4) Empty segment, push new segment, create a hole, pop a segment, add/remove a reference
// 5) Base segment, push new segment, create a hole, pop a segment, push new segment, add/remove
//    reference
//
// Storing the last known *previous* state (bottom index) allows conservatively detecting all the
// segment changes above. The condition is simply that the last known state is greater than or
// equal to the current previous state, and smaller than the current state (top index). The
// condition is conservative as it adds O(1) overhead to operations on an empty segment.

static size_t CountNullEntries(const IrtEntry* table, size_t from, size_t to) {
  size_t count = 0;
  for (size_t index = from; index != to; ++index) {
    if (table[index].GetReference()->IsNull()) {
      count++;
    }
  }
  return count;
}

void LocalReferenceTable::RecoverHoles(LRTSegmentState prev_state) {
  if (free_entries_list_.IsFree() && free_entries_list_.GetNextFree() >= segment_state.top_index) {
    LrtEntry* entry = GetEntry(free_entries_list_.GetNextFree());
    DCHECK(entry->IsFree());
    while (entry->GetNextFree() >= segment_state.top_index) {
      entry = GetEntry(entry->GetNextFree());
      DCHECK(entry->IsFree());
    }
    
    
    free_entries_list_length_ -= recovered;
  }
  if (last_known_previous_state_.top_index >= segment_state_.top_index ||
      last_known_previous_state_.top_index < prev_state.top_index) {
    const size_t top_index = segment_state_.top_index;
    size_t count = CountNullEntries(table_, prev_state.top_index, top_index);

    if (kDebugLRT) {
      LOG(INFO) << "+++ Recovered holes: "
                << " Current prev=" << prev_state.top_index
                << " Current top_index=" << top_index
                << " Old num_holes=" << current_num_holes_
                << " New num_holes=" << count;
    }

    current_num_holes_ = count;
    last_known_previous_state_ = prev_state;
  } else if (kDebugLRT) {
    LOG(INFO) << "No need to recover holes";
  }
}
#endif

bool LocalReferenceTable::Resize(size_t new_size, std::string* error_msg) {
  DCHECK_GE(max_entries_, kSmallLrtEntries);
  DCHECK(IsPowerOfTwo(max_entries_));
  DCHECK_GT(new_size, max_entries_);
  DCHECK_LE(new_size, kMaxTableSizeInBytes / sizeof(LrtEntry));
  size_t required_size = RoundUpToPowerOfTwo(new_size);
  size_t num_required_tables = NumTablesForSize(required_size);
  DCHECK_GE(num_required_tables, 2u);
  // Delay moving the `small_table_` to `tables_` until after the next table allocation succeeds.
  size_t num_tables = (small_table_ != nullptr) ? 1u : tables_.size();
  DCHECK_EQ(num_tables, NumTablesForSize(max_entries_));
  for (; num_tables != num_required_tables; ++num_tables) {
    size_t new_table_size = GetTableSize(num_tables);
    if (num_tables < MaxSmallTables()) {
      SmallLrtAllocator* small_lrt_allocator = Runtime::Current()->GetSmallLrtAllocator();
      LrtEntry* new_table = small_lrt_allocator->Allocate(new_table_size, error_msg);
      if (new_table == nullptr) {
        DCHECK(!error_msg->empty());
        return false;
      }
      tables_.push_back(new_table);
    } else {
      MemMap new_map = NewLRTMap(new_table_size * sizeof(LrtEntry), error_msg);
      if (!new_map.IsValid()) {
        DCHECK(!error_msg->empty());
        return false;
      }
      tables_.push_back(reinterpret_cast<LrtEntry*>(new_map.Begin()));
      table_mem_maps_.push_back(std::move(new_map));
    }
    DCHECK_EQ(num_tables == 1u, small_table_ != nullptr);
    if (num_tables == 1u) {
      tables_.insert(tables_.begin(), small_table_);
      small_table_ = nullptr;
    }
    // Record the new available capacity after each successful allocation.
    DCHECK_EQ(max_entries_, new_table_size);
    max_entries_ = 2u * new_table_size;
  }
  DCHECK_EQ(num_required_tables, tables_.size());
  return true;
}

void LocalReferenceTable::PrunePoppedFreeEntries() {
  uint32_t free_entry_index = free_entries_list_;
  DCHECK_NE(free_entry_index, kFreeListEnd);
  DCHECK_GE(free_entry_index, segment_state_.top_index);
  do {
    free_entry_index = GetEntry(free_entry_index)->GetNextFree();
  } while (free_entry_index != kFreeListEnd && free_entry_index >= segment_state_.top_index);
  free_entries_list_ = free_entry_index;
}

IndirectRef LocalReferenceTable::Add(LRTSegmentState previous_state,
                                     ObjPtr<mirror::Object> obj,
                                     std::string* error_msg) {
  if (kDebugLRT) {
    LOG(INFO) << "+++ Add: previous_state=" << previous_state.top_index
              << " top_index=" << segment_state_.top_index;
  }

  DCHECK(obj != nullptr);
  VerifyObject(obj);

  DCHECK(max_entries_ == kSmallLrtEntries ? small_table_ != nullptr : !tables_.empty());
  DCHECK_LE(previous_state.top_index, segment_state_.top_index);

  if (UNLIKELY(free_entries_list_ != kFreeListEnd)) {
    if (UNLIKELY(free_entries_list_ >= segment_state_.top_index)) {
      PrunePoppedFreeEntries();
    }
    if (free_entries_list_ != kFreeListEnd && free_entries_list_ >= previous_state.top_index) {
      // Reuse the free entry.
      uint32_t free_entry_index = free_entries_list_;
      DCHECK_LT(free_entry_index, segment_state_.top_index);  // Popped entries pruned above.
      LrtEntry* free_entry = GetEntry(free_entry_index);
      free_entries_list_ = free_entry->GetNextFree();
      free_entry->SetReference(obj);
      if (kDebugLRT) {
        LOG(INFO) << "+++ added at index " << free_entry_index
                  << " (reused free entry), top=" << segment_state_.top_index;
      }
      return ToIndirectRef(free_entry);
    }
  }

  if (UNLIKELY(segment_state_.top_index == max_entries_)) {
    // Try to double space.
    if (std::numeric_limits<size_t>::max() / 2 < max_entries_) {
      std::ostringstream oss;
      oss << "JNI ERROR (app bug): " << kLocal << " table overflow "
          << "(max=" << max_entries_ << ")" << std::endl
          << MutatorLockedDumpable<LocalReferenceTable>(*this)
          << " Resizing failed: exceeds size_t";
      *error_msg = oss.str();
      return nullptr;
    }

    std::string inner_error_msg;
    if (!Resize(max_entries_ * 2, &inner_error_msg)) {
      std::ostringstream oss;
      oss << "JNI ERROR (app bug): " << kLocal << " table overflow "
          << "(max=" << max_entries_ << ")" << std::endl
          << MutatorLockedDumpable<LocalReferenceTable>(*this)
          << " Resizing failed: " << inner_error_msg;
      *error_msg = oss.str();
      return nullptr;
    }
  }

  LrtEntry* entry = GetEntry(segment_state_.top_index);
  ++segment_state_.top_index;
  entry->SetReference(obj);
  if (kDebugLRT) {
    LOG(INFO) << "+++ added at end, new top=" << segment_state_.top_index;
  }
  return ToIndirectRef(entry);
}

void LocalReferenceTable::AssertEmpty() {
  // TODO: Should we just assert that `Capacity() == 0`?
  for (size_t i = 0; i < Capacity(); ++i) {
    if (!GetEntry(i)->IsFree()) {
      LOG(FATAL) << "Internal Error: non-empty local reference table\n"
                 << MutatorLockedDumpable<LocalReferenceTable>(*this);
      UNREACHABLE();
    }
  }
}

// Removes an object.
//
// This method is not called when a local frame is popped; this is only used
// for explicit single removals.
//
// If the entry is not at the top, we just add it to the free entry list.
// If the entry is at the top, we pop it from the top and check if there are
// free entries under it to remove in order to reduce the size of the table.
//
// Returns "false" if nothing was removed.
bool LocalReferenceTable::Remove(LRTSegmentState previous_state, IndirectRef iref) {
  if (kDebugLRT) {
    LOG(INFO) << "+++ Remove: previous_state=" << previous_state.top_index
              << " top_index=" << segment_state_.top_index;
  }

  IndirectRefKind kind = GetIndirectRefKind(iref);
  if (UNLIKELY(kind != kLocal)) {
    Thread* self = Thread::Current();
    if (kind == kJniTransition) {
      if (self->IsJniTransitionReference(reinterpret_cast<jobject>(iref))) {
        // Transition references count as local but they cannot be deleted.
        // TODO: They could actually be cleared on the stack, except for the `jclass`
        // reference for static methods that points to the method's declaring class.
        JNIEnvExt* env = self->GetJniEnv();
        DCHECK(env != nullptr);
        if (env->IsCheckJniEnabled()) {
          const char* msg = kDumpStackOnNonLocalReference
              ? "Attempt to remove non-JNI local reference, dumping thread"
              : "Attempt to remove non-JNI local reference";
          LOG(WARNING) << msg;
          if (kDumpStackOnNonLocalReference) {
            self->Dump(LOG_STREAM(WARNING));
          }
        }
        return true;
      }
    }
    if (kDumpStackOnNonLocalReference && self->GetJniEnv()->IsCheckJniEnabled()) {
      ScopedObjectAccess soa(self);
      // Log the error message and stack. Repeat the message as FATAL later.
      LOG(ERROR) << "Attempt to delete " << kind
                 << " reference as local JNI reference, dumping stack";
      self->Dump(LOG_STREAM(ERROR));
    }
    LOG(FATAL) << "Attempt to delete " << kind << " reference as local JNI reference";
    UNREACHABLE();
  }

  DCHECK(max_entries_ == kSmallLrtEntries ? small_table_ != nullptr : !tables_.empty());
  DCHECK_LE(previous_state.top_index, segment_state_.top_index);
  DCheckValidReference(iref);

  LrtEntry* entry = ToLrtEntry(iref);
  uint32_t entry_index = GetReferenceEntryIndex(iref);
  uint32_t top_index = segment_state_.top_index;
  const uint32_t bottom_index = previous_state.top_index;

  if (entry_index < bottom_index) {
    // Wrong segment.
    LOG(WARNING) << "Attempt to remove index outside index area (" << entry_index
                 << " vs " << bottom_index << "-" << top_index << ")";
    return false;
  }
  if (entry_index >= top_index) {
    // Bad --- stale reference?
    LOG(WARNING) << "Attempt to remove invalid index " << entry_index
                 << " (bottom=" << bottom_index << " top=" << top_index << ")";
    return false;
  }

  if (entry_index == top_index - 1) {
    // Top-most entry.  Scan up and consume holes.
    --top_index;
    constexpr uint32_t kDeadLocalValue = 0xdead10c0;
    entry->SetReference(reinterpret_cast32<mirror::Object*>(kDeadLocalValue));
    uint32_t prune_start = top_index;
    while (prune_start > bottom_index && GetEntry(prune_start - 1u)->IsFree()) {
      --prune_start;
    }
    size_t prune_count = top_index - prune_start;
    if (prune_count != 0u) {
      // Remove pruned entries from the free list.
      uint32_t free_index = free_entries_list_;
      while (prune_count != 0u && free_index >= prune_start) {
        DCHECK_NE(free_index, kFreeListEnd);
        LrtEntry* pruned_entry = GetEntry(free_index);
        free_index = pruned_entry->GetNextFree();
        pruned_entry->SetReference(reinterpret_cast32<mirror::Object*>(kDeadLocalValue));
        DCHECK_NE(prune_count, 0u);
        --prune_count;
      }
      free_entries_list_ = free_index;
      while (prune_count != 0u) {
        DCHECK_NE(free_index, kFreeListEnd);
        DCHECK_LT(free_index, prune_start);
        DCHECK_GE(free_index, bottom_index);
        LrtEntry* free_entry = GetEntry(free_index);
        while (free_entry->GetNextFree() < prune_start) {
          free_index = free_entry->GetNextFree();
          DCHECK_GE(free_index, bottom_index);
          free_entry = GetEntry(free_index);
        }
        LrtEntry* pruned_entry = GetEntry(free_entry->GetNextFree());
        free_entry->SetFree(pruned_entry->GetNextFree());
        pruned_entry->SetReference(reinterpret_cast32<mirror::Object*>(kDeadLocalValue));
        --prune_count;
      }
      DCHECK(free_index == kFreeListEnd || free_index < prune_start)
          << "free_index=" << free_index << ", prune_start=" << prune_start;
    }
    segment_state_.top_index = prune_start;
    if (kDebugLRT) {
      LOG(INFO) << "+++ removed last entry, pruned " << (top_index - prune_start)
                << ", new top= " << segment_state_.top_index;
    }
  } else {
    // Not the top-most entry.  This creates a hole.
    entry->SetDeleted(free_entries_list_);
    free_entries_list_ = entry_index;
    if (kDebugLRT) {
      LOG(INFO) << "+++ removed entry and left hole at " << entry_index;
    }
  }

  return true;
}

void LocalReferenceTable::Trim() {
  ScopedTrace trace(__PRETTY_FUNCTION__);
  const size_t num_mem_maps = table_mem_maps_.size();
  if (num_mem_maps == 0u) {
    // Only small tables; nothing to do here. (Do not unnecessarily prune popped free entries.)
    return;
  }
  DCHECK_EQ(tables_.size(), num_mem_maps + MaxSmallTables());
  const size_t capacity = Capacity();
  // Prune popped free entries before potentially losing their memory.
  if (UNLIKELY(free_entries_list_ != kFreeListEnd) &&
      UNLIKELY(free_entries_list_ >= segment_state_.top_index)) {
    PrunePoppedFreeEntries();
  }
  // Small tables can hold as many entries as the next table.
  constexpr size_t kSmallTablesCapacity = GetTableSize(MaxSmallTables());
  size_t mem_map_index = 0u;
  if (capacity > kSmallTablesCapacity) {
    const size_t table_size = TruncToPowerOfTwo(capacity);
    const size_t table_index = NumTablesForSize(table_size);
    const size_t start_index = capacity - table_size;
    LrtEntry* table = tables_[table_index];
    uint8_t* release_start = AlignUp(reinterpret_cast<uint8_t*>(&table[start_index]), kPageSize);
    uint8_t* release_end = AlignUp(reinterpret_cast<uint8_t*>(&table[table_size]), kPageSize);
    DCHECK_GE(reinterpret_cast<uintptr_t>(release_end), reinterpret_cast<uintptr_t>(release_start));
    DCHECK_ALIGNED(release_end, kPageSize);
    DCHECK_ALIGNED(release_end - release_start, kPageSize);
    if (release_start != release_end) {
      madvise(release_start, release_end - release_start, MADV_DONTNEED);
    }
    mem_map_index = table_index - MaxSmallTables();
  }
  for (MemMap& mem_map : ArrayRef<MemMap>(table_mem_maps_).SubArray(mem_map_index)) {
    madvise(mem_map.Begin(), mem_map.Size(), MADV_DONTNEED);
  }
}

void LocalReferenceTable::VisitRoots(RootVisitor* visitor, const RootInfo& root_info) {
  BufferedRootVisitor<kDefaultBufferedRootCount> root_visitor(visitor, root_info);
  auto visit_table = [&](LrtEntry* table, size_t count) REQUIRES_SHARED(Locks::mutator_lock_) {
    for (size_t i = 0; i != count; ++i) {
      if (!table[i].IsFree()) {
        GcRoot<mirror::Object>* ref = table[i].GetRootAddress();
        DCHECK(!ref->IsNull());
        root_visitor.VisitRoot(*ref);
      }
    }
  };
  size_t capacity = Capacity();
  if (small_table_ != nullptr) {
    visit_table(small_table_, capacity);
  } else {
    uint32_t remaining_capacity = capacity;
    size_t table_index = 0u;
    while (remaining_capacity != 0u) {
      size_t count = std::min<size_t>(remaining_capacity, GetTableSize(table_index));
      visit_table(tables_[table_index], count);
      ++table_index;
      remaining_capacity -= count;
    }
  }
}

void LocalReferenceTable::Dump(std::ostream& os) const {
  os << kLocal << " table dump:\n";
  ReferenceTable::Table entries;
  for (size_t i = 0, size = Capacity(); i != size; ++i) {
    LrtEntry* entry = GetEntry(i);
    if (!entry->IsFree()) {
      DCHECK(entry->GetReference() != nullptr);
      entries.push_back(GcRoot<mirror::Object>(entry->GetReference()));
    }
  }
  ReferenceTable::Dump(os, entries);
}

void LocalReferenceTable::SetSegmentState(LRTSegmentState new_state) {
  if (kDebugLRT) {
    LOG(INFO) << "Setting segment state: "
              << segment_state_.top_index
              << " -> "
              << new_state.top_index;
  }
  segment_state_ = new_state;
}

bool LocalReferenceTable::EnsureFreeCapacity(size_t free_capacity, std::string* error_msg) {
  // TODO: Pass `previous_state` so that we can check holes.
  DCHECK_GE(free_capacity, static_cast<size_t>(1));
  size_t top_index = segment_state_.top_index;
  DCHECK_LE(top_index, max_entries_);
  // FIXME: Include holes in the calculation.
  if (free_capacity <= max_entries_ - top_index) {
    return true;
  }

  // We're only gonna do a simple best-effort here, ensuring the asked-for capacity at the end.
  if (free_capacity > kMaxTableSize - top_index) {
    *error_msg = android::base::StringPrintf(
        "Requested size exceeds maximum: %zu > %zu (%zu used)",
        free_capacity,
        kMaxTableSize - top_index,
        top_index);
    return false;
  }

  // Try to increase the table size.
  if (!Resize(top_index + free_capacity, error_msg)) {
    LOG(WARNING) << "JNI ERROR: Unable to reserve space in EnsureFreeCapacity (" << free_capacity
                 << "): " << std::endl
                 << MutatorLockedDumpable<LocalReferenceTable>(*this)
                 << " Resizing failed: " << *error_msg;
    return false;
  }
  return true;
}

size_t LocalReferenceTable::FreeCapacity() const {
  // TODO: Include holes in current segment.
  return max_entries_ - segment_state_.top_index;
}

}  // namespace jni
}  // namespace art
