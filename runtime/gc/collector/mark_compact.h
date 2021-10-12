/*
 * Copyright 2016 The Android Open Source Project
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

#ifndef ART_RUNTIME_GC_COLLECTOR_MARK_COMPACT_H_
#define ART_RUNTIME_GC_COLLECTOR_MARK_COMPACT_H_

#include <memory>

#include "base/atomic.h"
#include "barrier.h"
#include "base/macros.h"
#include "base/mutex.h"
#include "garbage_collector.h"
#include "gc/accounting/heap_bitmap.h"
#include "gc_root.h"
#include "immune_spaces.h"
#include "offsets.h"

namespace art {
namespace gc {

class Heap;

namespace space {
class BumpPointerSpace;
}  // namespace space

namespace collector {
class MarkCompact : public GarbageCollector {
 public:
  explicit MarkCompact(Heap* heap);

  ~MarkCompact() {}

  void RunPhases() override;

  GcType GetGcType() const override {
    return kGcTypeFull;
  }

  CollectorType GetCollectorType() const override {
    return kCollectorTypeCMC;
  }

  Barrier& GetBarrier() {
    return gc_barrier_;
  }

  mirror::Object* MarkObject(mirror::Object* obj) override
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(Lock::heap_bitmap_lock_);

  void MarkHeapReference(mirror::HeapReference<mirror::Object>* obj,
                         bool do_atomic_update) override
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(Lock::heap_bitmap_lock_);

  void VisitRoots(mirror::Object*** roots,
                  size_t count,
                  const RootInfo& info) override
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(Lock::heap_bitmap_lock_);
  void VisitRoots(mirror::CompressedReference<mirror::Object>** roots,
                  size_t count,
                  const RootInfo& info) override
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(Lock::heap_bitmap_lock_);

  bool IsNullOrMarkedHeapReference(mirror::HeapReference<mirror::Object>* obj,
                                   bool do_atomic_update) override
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(Lock::heap_bitmap_lock_);

  void DelayReferenceReferent(ObjPtr<mirror::Class> klass,
                              ObjPtr<mirror::Reference> reference) override
      REQUIRES_SHARED(Locks::mutator_lock_, Locks::heap_bitmap_lock_);

  mirror::Object* IsMarked(mirror::Object* obj) override
      REQUIRES_SHARED(Locks::mutator_lock_, Locks::heap_bitmap_lock_);

 private:
  static constexpr uint32_t kAlignment = kObjectAlignment;
  // TODO: consider increasing it to 128-bit per chunk.
  static constexpr uint32_t kBitsPerVectorWord = kBitsPerIntPtrT;
  static constexpr uint32_t kOffsetChunkSize = kBitsPerVectorWord * kAlignment;
  static_assert(kOffsetChunkSize < kPageSize);
  // Bitmap with bits corresponding to every live word set. For an object
  // which is 4 words in size will have the corresponding 4 bits set. This is
  // required for efficient computation of new-address (post-compaction) from
  // the given old-address (pre-compaction).
  template <size_t kAlignment>
  class LiveWordsBitmap : private MemoryRangeBitmap<kAlignment> {
   public:
    static_assert(IsPowerOfTwo(kBitsPerVectorWord));
    static_assert(IsPowerOfTwo(kBitsPerBitmapWord));
    static_assert(kBitsPerVectorWord >= kBitsPerBitmapWord);
    static constexpr uint32_t kBitmapWordsPerVectorWord = kBitsPerVectorWord / kBitsPerBitmapWord;
    static LiveWordsBitmap* Create(uintptr_t begin, uintptr_t end) {
      return Create("Concurrent Mark Compact live words bitmap", begin, end);
    }

    // Return offset (within the offset-vector chunk) of the nth live word.
    uint32_t FindNthLiveWordOffset(size_t offset_vec_idx, uint32_t n);
    // Sets all bits in the bitmap corresponding to the given range. Also
    // returns the bit-index of the first word.
    ALWAYS_INLINE uintptr_t SetLiveWords(uintptr_t begin, size_t size);
    // Count number of live words upto the given bit-index. This is to be used
    // to compute the post-compact address of an old reference.
    ALWAYS_INLINE size_t CountLiveWordsUpto(size_t bit_idx) const;
    // Call visitor for every stride of contiguous marked bits in the live-word
    // bitmap. Passes to the visitor index of the first marked bit in the
    // stride, stride-size and whether it's the last stride in the given range
    // or not.
    template <typename Visitor>
    ALWAYS_INLINE void VisitLiveStrides(uintptr_t begin_bit_idx,
                                        const size_t bytes,
                                        Visitor& visitor);
    void ClearBitmap() { Clear(); }
    uintptr_t Begin() const { return CoverBegin(); }
    bool HasAddress(mirror::Object* obj) const {
      return HasAddress(reinterpret_cast<uintptr_t>(obj));
    }
  };

  mirror::Object* GetFromSpaceAddr(mirror::Object* addr) const {
    uintptr_t offset = reinterpret_cast<uintptr_t>(obj) - live_words_bitmap_->Begin();
    return reinterpret_cast<mirror::Object*>(from_space_begin_ + offset);
  }

  void InitializePhase();
  void FinishPhase();
  void MarkingPhase() REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Locks::heap_bitmap_lock_);
  void CompactionPhase() REQUIRES_SHARED(Locks::mutator_lock_);

  // Given the pre-compact address, the function returns the post-compact
  // address of the given object.
  ALWAYS_INLINE mirror::Object* PostCompactAddress(mirror::Object* old_ref) const;
  // Identify immune spaces and reset card-table, mod-union-table, and mark
  // bitmaps.
  void BindAndResetBitmaps() REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(Locks::heap_bitmap_lock_);

  // Perform one last round of marking, identifying roots from dirty cards
  // during a stop-the-world (STW) pause
  void PausePhase() REQUIRES(Locks::mutator_lock_, !Locks::heap_bitmap_lock_);
  // Perform GC-root updation and heap protection so that during the concurrent
  // compaction phase we can receive faults and compact the corresponding pages
  // on the fly. This is performed in a STW pause.
  void PreCompactionPhase() REQUIRES(Locks::mutator_lock_);
  // Compute offset-vector and other data structures required during concurrent
  // compaction
  void PrepareForCompaction() REQUIRES_SHARED(Locks::mutator_lock_);
  // Perform reference-processing and the likes before sweeping the non-movable
  // spaces.
  void ReclaimPhase() REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Locks::heap_bitmap_lock_);

  // Mark GC-roots (except from immune spaces and thread-stacks) during a STW pause.
  void ReMarkRoots(Runtime* runtime) REQUIRES(Locks::mutator_lock_, Locks::heap_bitmap_lock_);
  // Concurrently mark GC-roots, except from immune spaces.
  void MarkRoots(VisitRootFlags flags) REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(Locks::heap_bitmap_lock_);
  // Collect thread stack roots via a checkpoint.
  void MarkRootsCheckpoint(Thread* self, Runtime* runtime) REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(Locks::heap_bitmap_lock_);
  // Second round of concurrent marking. Mark all gray objects that got dirtied
  // since the first round.
  void PreCleanCards() REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(Locks::heap_bitmap_lock_);

  void MarkNonThreadRoots(Runtime* runtime) REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(Locks::heap_bitmap_lock_);
  void MarkConcurrentRoots(VisitRootFlags flags) REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(Locks::heap_bitmap_lock_);

  // Traverse through the reachable objects and mark them.
  void MarkReachableObjects() REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(Lock::heap_bitmap_lock_);
  // Scan (only) immune spaces looking for references into the garbage collected
  // spaces.
  void UpdateAndMarkModUnion() REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(Locks::heap_bitmap_lock_);
  // Scan mod-union and card tables, covering all the spaces, to identify gray objects.
  // These are in 'minimum age' cards, which is 'kCardAged' in case of concurrent (second round)
  // marking and kCardDirty during the STW pause.
  void ScanGrayObjects(bool paused, uint8_t minimum_age) REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(Lock::heap_bitmap_lock_);
  // Recursively mark dirty objects. Invoked both concurrently as well in a STW
  // pause in PausePhase().
  void RecursiveMarkDirtyObjects(bool paused, uint8_t minimum_age)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(Lock::heap_bitmap_lock_);
  // Go through all the objects in the mark-stack until it's empty.
  void ProcessMarkStack() REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(Lock::heap_bitmap_lock_);
  // Expand the mark-stack size.
  void ExpandMarkStack() REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(Lock::heap_bitmap_lock_);

  template <bool kUpdateLiveWords>
  void ScanObject(mirror::Object* obj) REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(Lock::heap_bitmap_lock_);
  // Push objects to the mark-stack right after successfully marking objects.
  void PushOnMarkStack(mirror::Object* obj)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(Lock::heap_bitmap_lock_);

  void MarkObjectNonNull(mirror::Object* obj,
                         mirror::Object* holder = nullptr,
                         MemberOffset offset = MemberOffset(0))
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(Lock::heap_bitmap_lock_);

  void MarkObject(mirror::Object* obj, mirror::Object* holder, MemberOffset offset)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(Lock::heap_bitmap_lock_);

  template <bool kParallel>
  void MarkObjectNonNullNoPush(mirror::Object* obj,
                               mirror::Object* holder = nullptr,
                               MemberOffset offset = MemberOffset(0))
      REQUIRES(Locks::heap_bitmap_lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  void Sweep(bool swap_bitmaps) REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(Locks::heap_bitmap_lock_);
  void SweepLargeObjects(bool swap_bitmaps) REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(Locks::heap_bitmap_lock_);

  // For checkpoints
  Barrier gc_barrier_;
  // Every object inside the immune spaces is assumed to be marked.
  ImmuneSpaces immune_spaces_;
  // Required only when mark-stack is accessed in shared mode, which happens
  // when collecting thread-stack roots using checkpoint.
  Mutex mark_stack_lock_;
  std::unique_ptr<accounting::ObjectStack> mark_stack_;
  // The main space bitmap
  accounting::ContinuousSpaceBitmap* current_space_bitmap_;
  accounting::ContinuousSpaceBitmap* non_moving_space_bitmap_;
  // Special bitmap wherein all the bits corresponding to an object are set.
  // TODO: make LiveWordsBitmap encapsulated in this class rather than a
  // pointer. We tend to access its members in performance-sensitive
  // code-path.
  std::unique_ptr<LiveWordsBitmap<kAlignment>> live_words_bitmap_;
  // Any array of live-bytes in logical chunks of kOffsetChunkSize size
  // in the 'to-be-compacted' space.
  std::unique_ptr<MemMap> offset_vector_map_;
  uint32_t* offset_vector_;

  // For every page in the to-space (post-compact heap) we need to know the
  // first object from which we must compact and/or update references. This is
  // for both non-moving and moving space. Additionally, for the moving-space,
  // we also need the offset within the object from where we need to start
  // copying.
  MemMap from_space_info_map_;
  uint32_t* pre_compact_offset_moving_space_;
  mirror::Object** first_objs_moving_space_;
  mirror::Object** first_objs_non_moving_space_;
  size_t non_moving_first_objs_count_;
  size_t moving_first_objs_count_;

  uint8_t* from_space_begin_;
  uint8_t* black_allocations_begin_;
  size_t vector_length_;
  space::ContinuousSpace* non_moving_space_;
  const space::BumpPointerSpace* bump_pointer_space_;
  Thread* thread_running_gc_;

  class ScanObjectVisitor;
  class CheckpointMarkThreadRoots;
  class CardModifiedVisitor;
  class RefFieldsVisitor;

  DISALLOW_IMPLICIT_CONSTRUCTORS(MarkCompact);
};

}  // namespace collector
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_COLLECTOR_MARK_COMPACT_H_
