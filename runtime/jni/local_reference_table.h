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

#ifndef ART_RUNTIME_JNI_LOCAL_REFERENCE_TABLE_H_
#define ART_RUNTIME_JNI_LOCAL_REFERENCE_TABLE_H_

#include <stdint.h>

#include <iosfwd>
#include <limits>
#include <string>

#include <android-base/logging.h>

#include "base/bit_field.h"
#include "base/bit_utils.h"
#include "base/casts.h"
#include "base/dchecked_vector.h"
#include "base/locks.h"
#include "base/macros.h"
#include "base/mem_map.h"
#include "base/mutex.h"
#include "gc_root.h"
#include "indirect_reference_table.h"
#include "mirror/object_reference.h"
#include "obj_ptr.h"
#include "offsets.h"

namespace art {

class RootInfo;

namespace mirror {
class Object;
}  // namespace mirror

namespace jni {

// Maintain a table of local JNI references.
//
// The table contains object references that are part of the GC root set. When an object is
// added we return an `IndirectRef` that is not a valid pointer but can be used to find the
// original value in O(1) time. Conversions to and from local JNI references are performed
// on upcalls and downcalls as well as in JNI functions, so they need to be very fast.
//
// To be efficient for JNI local variable storage, we need to provide operations that allow us to
// operate on segments of the table, where segments are pushed and popped as if on a stack. For
// example, deletion of an entry should only succeed if it appears in the current segment, and we
// want to be able to strip off the current segment quickly when a method returns. Additions to the
// table must be made in the current segment even if space is available in an earlier area.
//
// A new segment is created when we call into native code from interpreted code, or when we handle
// the JNI PushLocalFrame function.
//
// The GC must be able to scan the entire table quickly.
//
// In summary, these must be very fast:
//  - adding or removing a segment
//  - adding references (always adding to the current segment)
//  - converting an local reference back to an Object
// These can be a little slower, but must still be pretty quick:
//  - removing individual references
//  - scanning the entire table straight through
//
// If there's more than one segment, we don't guarantee that the table will fill completely before
// we fail due to lack of space. We do ensure that the current segment will pack tightly, which
// should satisfy JNI requirements (e.g. EnsureLocalCapacity).

// FIXME
// If we delete entries from the middle of the list, we will be left with "holes".  We track the
// number of holes so that, when adding new elements, we can quickly decide to do a trivial append
// or go slot-hunting.
//
// When the top-most entry is removed, any holes immediately below it are also removed. Thus,
// deletion of an entry may reduce "top_index" by more than one.
//
// To get the desired behavior for JNI locals, we need to know the bottom and top of the current
// "segment". The top is managed internally, and the bottom is passed in as a function argument.
// When we call a native method or push a local frame, the current top index gets pushed on, and
// serves as the new bottom. When we pop a frame off, the value from the stack becomes the new top
// index, and the value stored in the previous frame becomes the new bottom.
//
// Holes are being locally cached for the segment. Otherwise we'd have to pass bottom index and
// number of holes, which restricts us to 16 bits for the top index. The value is cached within the
// table. To avoid code in generated JNI transitions, which implicitly form segments, the code for
// adding and removing references needs to detect the change of a segment. Helper fields are used
// for this detection.
//
// Common alternative implementation: make IndirectRef a pointer to the actual reference slot.
// Instead of getting a table and doing a lookup, the lookup can be done instantly. Operations like
// determining the type and deleting the reference are more expensive because the table must be
// hunted for (i.e. you have to do a pointer comparison to see which table it's in), you can't move
// the table when expanding it (so realloc() is out), and tricks like serial number checking to
// detect stale references aren't possible (though we may be able to get similar benefits with other
// approaches).

// The state of the current segment. We only store the index. Splitting it for index and hole
// count restricts the range too much.
struct LRTSegmentState {
  uint32_t top_index;
};

// Use as initial value for "cookie", and when table has only one segment.
static constexpr LRTSegmentState kLRTFirstSegment = { 0 };

// The entry in the `LocalReferenceTable` can contain a null or reference, or
// it can be marked as free and hold the index of the next free entry. For better
// diagnostics of invalid uses, free entries can also be tagged as deleted.
class LrtEntry {
 public:
  void SetReference(ObjPtr<mirror::Object> ref) REQUIRES_SHARED(Locks::mutator_lock_);

  ObjPtr<mirror::Object> GetReference() REQUIRES_SHARED(Locks::mutator_lock_);

  bool IsNull() const {
    return root_.IsNull();
  }

  void SetFree(uint32_t next_free);

  void SetDeleted(uint32_t next_free);

  bool IsFree() {
    return (AsVRegValue() & (1u << kFlagFree)) != 0u;
  }

  bool IsDeleted() {
    return (AsVRegValue() & (1u << kFlagDeleted)) != 0u;
  }

  uint32_t GetNextFree() {
    DCHECK(IsFree());
    return NextFreeField::Decode(AsVRegValue());
  }

  GcRoot<mirror::Object>* GetRootAddress() {
    return &root_;
  }

  static constexpr uint32_t FreeListEnd() {
    return MaxInt<uint32_t>(kFieldNextFreeBits);
  }

 private:
  static constexpr size_t kFlagFree = 0u;
  static constexpr size_t kFlagDeleted = 1u;
  static constexpr size_t kFieldNextFree = 2u;
  static constexpr size_t kFieldNextFreeBits = BitSizeOf<uint32_t>() - kFieldNextFree;

  using NextFreeField = BitField<uint32_t, kFieldNextFree, kFieldNextFreeBits>;

  static_assert(kObjectAlignment > (1u << kFlagFree));
  static_assert(kObjectAlignment > (1u << kFlagDeleted));

  uint32_t AsVRegValue() {
    return root_.AddressWithoutBarrier()->AsVRegValue();
  }

  // We record the contents as a `CompressedReference` but we 
  GcRoot<mirror::Object> root_;
};
static_assert(sizeof(LrtEntry) == sizeof(mirror::CompressedReference<mirror::Object>));

// We initially allocate local reference tables with a small number of entries, packing
// multiple tables into a single page. If we need to expand, we double the capacity,
// first allocating another chunk with the same number of entries as the first chunk
// and then allocating twice as big chunk on each subsequent expansion.
static constexpr size_t kInitialLrtBytes = 512;  // Number of bytes in an initial local table.
static constexpr size_t kSmallLrtEntries = kInitialLrtBytes / sizeof(LrtEntry);
static_assert(IsPowerOfTwo(kInitialLrtBytes));
static_assert(kPageSize % kInitialLrtBytes == 0);
static_assert(kInitialLrtBytes % sizeof(LrtEntry) == 0);

// A minimal stopgap allocator for initial small local LRT tables.
class SmallLrtAllocator {
 public:
  SmallLrtAllocator();

  // Allocate an LRT table for `kSmallLrtEntries`.
  LrtEntry* Allocate(size_t size, std::string* error_msg) REQUIRES(!lock_);

  void Deallocate(LrtEntry* unneeded, size_t size) REQUIRES(!lock_);

 private:
  static constexpr size_t kNumSlots = WhichPowerOf2(kPageSize / kInitialLrtBytes);

  static size_t GetIndex(size_t size);

  // A free list of kInitialLrtBytes chunks linked through the first word.
  dchecked_vector<void*> free_lists_;

  // Repository of MemMaps used for small LRT tables.
  dchecked_vector<MemMap> shared_lrt_maps_;

  Mutex lock_;  // Level kGenericBottomLock; acquired before mem_map_lock_, which is a C++ mutex.
};

class LocalReferenceTable {
 public:
  LocalReferenceTable();
  ~LocalReferenceTable();

  // Initialize the `LocalReferenceTable`.
  // Must be called before the `LocalReferenceTable` can escape to other threads.
  //
  // Max_count is the requested minimum initial capacity (resizable). The actual initial
  // capacity can be higher to utilize all allocated memory.
  //
  // Returns true on success.
  // On failure, returns false and reports error in `*error_msg`.
  bool Initialize(size_t max_count, std::string* error_msg);

  // Add a new entry. The `obj` must be a valid non-null object reference. This function
  // will return null if an error happened (with an appropriate error message set).
  IndirectRef Add(LRTSegmentState previous_state,
                  ObjPtr<mirror::Object> obj,
                  std::string* error_msg)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Given an `IndirectRef` in the table, return the Object it refers to.
  //
  // This function may abort under error conditions in debug build.
  // In release builds, error conditions are unchecked and the function can
  // return old or invalid references from popped segments and deleted entries.
  ObjPtr<mirror::Object> Get(IndirectRef iref) const
      REQUIRES_SHARED(Locks::mutator_lock_) ALWAYS_INLINE;

  // Updates an existing indirect reference to point to a new object.
  // Used exclusively for updating `String` references after calling a `String` constructor.
  void Update(IndirectRef iref, ObjPtr<mirror::Object> obj) REQUIRES_SHARED(Locks::mutator_lock_);

  // Remove an existing entry.
  //
  // If the entry is not between the current top index and the bottom index
  // specified by the cookie, we don't remove anything.  This is the behavior
  // required by JNI's DeleteLocalRef function.
  //
  // Returns "false" if nothing was removed.
  bool Remove(LRTSegmentState previous_state, IndirectRef iref)
      REQUIRES_SHARED(Locks::mutator_lock_);

  void AssertEmpty() REQUIRES_SHARED(Locks::mutator_lock_);

  void Dump(std::ostream& os) const
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!Locks::alloc_tracker_lock_);

  IndirectRefKind GetKind() const {
    return kLocal;
  }

  // Return the  number of entries in the entire table.  This includes holes,
  // and so may be larger than the actual number of "live" entries.
  size_t Capacity() const {
    return segment_state_.top_index;
  }

  // Ensure that at least free_capacity elements are available, or return false.
  // Caller ensures free_capacity > 0.
  bool EnsureFreeCapacity(size_t free_capacity, std::string* error_msg)
      REQUIRES_SHARED(Locks::mutator_lock_);
  // See implementation of EnsureFreeCapacity. We'll only state here how much is trivially free,
  // without recovering holes. Thus this is a conservative estimate.
  size_t FreeCapacity() const;

  void VisitRoots(RootVisitor* visitor, const RootInfo& root_info)
      REQUIRES_SHARED(Locks::mutator_lock_);

  LRTSegmentState GetSegmentState() const {
    return segment_state_;
  }

  void SetSegmentState(LRTSegmentState new_state);

  static Offset SegmentStateOffset(size_t pointer_size ATTRIBUTE_UNUSED) {
    // Note: Currently segment_state_ is at offset 0. We're testing the expected value in
    //       jni_internal_test to make sure it stays correct. It is not OFFSETOF_MEMBER, as that
    //       is not pointer-size-safe.
    return Offset(0);
  }

  // Release pages past the end of the table that may have previously held references.
  void Trim() REQUIRES_SHARED(Locks::mutator_lock_);

  /* Reference validation for CheckJNI and debug build. */
  bool IsValidReference(IndirectRef, /*out*/std::string* error_msg) const
      REQUIRES_SHARED(Locks::mutator_lock_);

  void SweepJniWeakGlobals(IsMarkedVisitor* visitor)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!Locks::jni_weak_globals_lock_);

 private:
  // The value indicating the end of the free list.
  static constexpr uint32_t kFreeListEnd = LrtEntry::FreeListEnd();

  // The maximum total table size we allow.
  static constexpr size_t kMaxTableSizeInBytes = 128 * MB;
  static_assert(IsPowerOfTwo(kMaxTableSizeInBytes));
  static_assert(IsPowerOfTwo(sizeof(LrtEntry)));
  static constexpr size_t kMaxTableSize = kMaxTableSizeInBytes / sizeof(LrtEntry);

  // Indirect reference encoding. This must be the same as in `IndirectReferenceTable`.
  static constexpr size_t kKindBits =
      MinimumBitsToStore(enum_cast<uint32_t>(IndirectRefKind::kLastKind));
  static constexpr uintptr_t kKindMask = MaxInt<uint32_t>(kKindBits);
  static_assert(kKindMask < alignof(LrtEntry));

  ALWAYS_INLINE static inline IndirectRefKind GetIndirectRefKind(IndirectRef iref) {
    return enum_cast<IndirectRefKind>(reinterpret_cast<uintptr_t>(iref) & kKindMask);
  }

  static IndirectRef ToIndirectRef(LrtEntry* entry) {
    // The `IndirectRef` can be used to directly access the underlying `GcRoot<>`.
    DCHECK_EQ(reinterpret_cast<GcRoot<mirror::Object>*>(entry), entry->GetRootAddress());
    return reinterpret_cast<IndirectRef>(
        reinterpret_cast<uintptr_t>(entry) | static_cast<uintptr_t>(kLocal));
  }

  static LrtEntry* ToLrtEntry(IndirectRef iref) {
    DCHECK_EQ(GetIndirectRefKind(iref), kLocal);
    return reinterpret_cast<LrtEntry*>(
        reinterpret_cast<uintptr_t>(iref) & ~static_cast<uintptr_t>(kKindMask));
  }

  static constexpr size_t GetTableSize(size_t table_index) {
    // First two tables have size `kSmallLrtEntries`, then it doubles for subsequent tables.
    return kSmallLrtEntries << (table_index != 0u ? table_index - 1u : 0u);
  }

  static constexpr size_t NumTablesForSize(size_t size) {
    DCHECK_GE(size, kSmallLrtEntries);
    DCHECK(IsPowerOfTwo(size));
    return 1u + WhichPowerOf2(size / kSmallLrtEntries);
  }

  static constexpr size_t MaxSmallTables() {
    return NumTablesForSize(kPageSize / sizeof(LrtEntry));
  }

  LrtEntry* GetEntry(size_t entry_index) const {
    DCHECK_LT(entry_index, max_entries_);
    if (LIKELY(small_table_ != nullptr)) {
      DCHECK_LT(entry_index, kSmallLrtEntries);
      DCHECK_EQ(max_entries_, kSmallLrtEntries);
      return &small_table_[entry_index];
    }
    size_t table_start_index =
        (entry_index < kSmallLrtEntries) ? 0u : TruncToPowerOfTwo(entry_index);
    size_t table_index =
        (entry_index < kSmallLrtEntries) ? 0u : NumTablesForSize(table_start_index);
    LrtEntry* table = tables_[table_index];
    return &table[entry_index - table_start_index];
  }

  // Get the entry index for an indirect reference. Note that this may be higher than
  // the current segment state. Returns maximum uint32 value if the reference does not
  // point to one of the internal tables.
  uint32_t GetReferenceEntryIndex(IndirectRef iref) const;

  // Debug mode check that the reference is valid.
  void DCheckValidReference(IndirectRef iref) const REQUIRES_SHARED(Locks::mutator_lock_);

  // Resize the backing table to be at least `new_size` elements long. The `new_size`
  // must be larger than the current size. After return max_entries_ >= new_size.
  bool Resize(size_t new_size, std::string* error_msg);

  // Remove popped free entries from the list.
  // Called only if `free_entries_list_` points to a popped entry.
  void PrunePoppedFreeEntries();

  /// semi-public - read/write by jni down calls.
  LRTSegmentState segment_state_;

  // The maximum number of entries (modulo resizing).
  uint32_t max_entries_;

  // The singly-linked list of free nodes.
  // These comprise of deleted nodes and skipped nodes.
  uint32_t free_entries_list_;

  // Individual tables. As long as we have only one small table, we use
  // `small_table_`, otherwise we set it to null and use `tables_`.
  LrtEntry* small_table_;
  dchecked_vector<LrtEntry*> tables_;

  // Mem maps where we store tables allocated directly with `MemMap`
  // rather than the `SmallLrtAllocator`.
  dchecked_vector<MemMap> table_mem_maps_;
};

}  // namespace jni
}  // namespace art

#endif  // ART_RUNTIME_JNI_LOCAL_REFERENCE_TABLE_H_
