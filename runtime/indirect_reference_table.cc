/*
 * Copyright (C) 2009 The Android Open Source Project
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

#include "indirect_reference_table-inl.h"

#include "base/dumpable-inl.h"
#include "base/systrace.h"
#include "jni_internal.h"
#include "nth_caller_visitor.h"
#include "reference_table.h"
#include "runtime.h"
#include "scoped_thread_state_change-inl.h"
#include "thread.h"
#include "utils.h"
#include "verify_object-inl.h"

#include <cstdlib>

namespace art {

static constexpr bool kDumpStackOnNonLocalReference = false;
static constexpr bool kDebugIRT = false;

const char* GetIndirectRefKindString(const IndirectRefKind& kind) {
  switch (kind) {
    case kHandleScopeOrInvalid:
      return "HandleScopeOrInvalid";
    case kLocal:
      return "Local";
    case kGlobal:
      return "Global";
    case kWeakGlobal:
      return "WeakGlobal";
  }
  return "IndirectRefKind Error";
}

void IndirectReferenceTable::AbortIfNoCheckJNI(const std::string& msg) {
  // If -Xcheck:jni is on, it'll give a more detailed error before aborting.
  JavaVMExt* vm = Runtime::Current()->GetJavaVM();
  if (!vm->IsCheckJniEnabled()) {
    // Otherwise, we want to abort rather than hand back a bad reference.
    LOG(FATAL) << msg;
  } else {
    LOG(ERROR) << msg;
  }
}

IndirectReferenceTable::IndirectReferenceTable(size_t max_count,
                                               IndirectRefKind desired_kind,
                                               bool resizable,
                                               bool abort_on_error)
    : kind_(desired_kind),
      max_entries_(max_count),
      current_num_holes_(0),
      last_known_prev_top_index_(0),
      hole_at_or_above_(0),
      resizable_(resizable) {
  CHECK_NE(desired_kind, kHandleScopeOrInvalid);

  std::string error_str;
  const size_t table_bytes = max_count * sizeof(IrtEntry);
  table_mem_map_.reset(MemMap::MapAnonymous("indirect ref table", nullptr, table_bytes,
                                            PROT_READ | PROT_WRITE, false, false, &error_str));
  if (abort_on_error) {
    CHECK(table_mem_map_.get() != nullptr) << error_str;
    CHECK_EQ(table_mem_map_->Size(), table_bytes);
    CHECK(table_mem_map_->Begin() != nullptr);
  } else if (table_mem_map_.get() == nullptr ||
             table_mem_map_->Size() != table_bytes ||
             table_mem_map_->Begin() == nullptr) {
    table_mem_map_.reset();
    LOG(ERROR) << error_str;
    return;
  }
  table_ = reinterpret_cast<IrtEntry*>(table_mem_map_->Begin());
  segment_state_ = IRT_FIRST_SEGMENT;
}

IndirectReferenceTable::~IndirectReferenceTable() {
}

bool IndirectReferenceTable::IsValid() const {
  return table_mem_map_.get() != nullptr;
}

void IndirectReferenceTable::RecoverHoles(uint32_t prev_state) {
  if (prev_state != last_known_prev_top_index_ || hole_at_or_above_ >= segment_state_) {
    size_t topIndex = segment_state_;

    size_t count = 0;
    for (size_t index = prev_state; index != topIndex; ++index) {
      if (table_[index].GetReference()->IsNull()) {
        count++;
      }
    }

    if (kDebugIRT) {
      LOG(INFO) << "+++ Recovered holes: "
                << "Last-known prev=" << last_known_prev_top_index_
                << " Current prev=" << prev_state
                << " Current topIndex=" << topIndex
                << " Old num_holes=" << current_num_holes_
                << " New num_holes=" << count;
    }

    current_num_holes_ = count;
    last_known_prev_top_index_ = prev_state;

    if (current_num_holes_ > 0) {
      hole_at_or_above_ = prev_state;
    } else {
      hole_at_or_above_ = 0;
    }
  } else if (kDebugIRT) {
    LOG(INFO) << "No need to recover holes, last-prev-state==prev-state==" << prev_state;
  }
}

ALWAYS_INLINE
static inline void CheckHoleCount(IrtEntry* table,
                                  size_t exp_num_holes,
                                  uint32_t prev_state,
                                  uint32_t topIndex) {
  if (kIsDebugBuild) {
    size_t count = 0;
    for (size_t index = prev_state; index != topIndex; ++index) {
      if (table[index].GetReference()->IsNull()) {
        count++;
      }
    }
    CHECK_EQ(exp_num_holes, count) << "prevState=" << prev_state << " topIndex=" << topIndex;
  }
}

bool IndirectReferenceTable::Resize(size_t new_size, std::string* error_msg) {
  CHECK_GT(new_size, max_entries_);

  const size_t table_bytes = new_size * sizeof(IrtEntry);
  std::unique_ptr<MemMap> new_map(MemMap::MapAnonymous("indirect ref table",
                                                       nullptr,
                                                       table_bytes,
                                                       PROT_READ | PROT_WRITE,
                                                       false,
                                                       false,
                                                       error_msg));
  if (new_map == nullptr) {
    return false;
  }

  memcpy(new_map->Begin(), table_mem_map_->Begin(), table_mem_map_->Size());
  table_mem_map_ = std::move(new_map);
  table_ = reinterpret_cast<IrtEntry*>(table_mem_map_->Begin());
  max_entries_ = new_size;

  return true;
}

IndirectRef IndirectReferenceTable::Add(IRTSegmentState cookie, ObjPtr<mirror::Object> obj) {
  if (kDebugIRT) {
    LOG(INFO) << "+++ Add: cookie=" << cookie << " last_prev=" << last_known_prev_top_index_
              << " topIndex=" << segment_state_
              << " holes=" << current_num_holes_;
  }

  size_t topIndex = segment_state_;

  CHECK(obj != nullptr);
  VerifyObject(obj);
  DCHECK(table_ != nullptr);

  if (topIndex == max_entries_) {
    if (!resizable_) {
      LOG(FATAL) << "JNI ERROR (app bug): " << kind_ << " table overflow "
                 << "(max=" << max_entries_ << ")\n"
                 << MutatorLockedDumpable<IndirectReferenceTable>(*this);
      UNREACHABLE();
    }

    // Try to double space.
    std::string error_msg;
    if (!Resize(max_entries_ * 2, &error_msg)) {
      LOG(FATAL) << "JNI ERROR (app bug): " << kind_ << " table overflow "
                 << "(max=" << max_entries_ << ")" << std::endl
                 << MutatorLockedDumpable<IndirectReferenceTable>(*this)
                 << " Resizing failed: " << error_msg;
      UNREACHABLE();
    }
  }

  RecoverHoles(cookie);
  CheckHoleCount(table_, current_num_holes_, cookie, topIndex);

  // We know there's enough room in the table.  Now we just need to find
  // the right spot.  If there's a hole, find it and fill it; otherwise,
  // add to the end of the list.
  IndirectRef result;
  size_t index;
  if (current_num_holes_ > 0) {
    DCHECK_GT(topIndex, 1U);
    // Find the first hole; likely to be near the end of the list.
    IrtEntry* pScan = &table_[topIndex - 1];
    DCHECK(!pScan->GetReference()->IsNull());
    --pScan;
    while (!pScan->GetReference()->IsNull()) {
      DCHECK_GE(pScan, table_ + cookie);
      --pScan;
    }
    index = pScan - table_;
    current_num_holes_--;
  } else {
    // Add to the end.
    index = topIndex++;
    segment_state_ = topIndex;
  }
  table_[index].Add(obj);
  result = ToIndirectRef(index);
  if (kDebugIRT) {
    LOG(INFO) << "+++ added at " << ExtractIndex(result) << " top=" << segment_state_
              << " holes=" << current_num_holes_;
  }

  DCHECK(result != nullptr);
  return result;
}

void IndirectReferenceTable::AssertEmpty() {
  for (size_t i = 0; i < Capacity(); ++i) {
    if (!table_[i].GetReference()->IsNull()) {
      LOG(FATAL) << "Internal Error: non-empty local reference table\n"
                 << MutatorLockedDumpable<IndirectReferenceTable>(*this);
      UNREACHABLE();
    }
  }
}

// Removes an object. We extract the table offset bits from "iref"
// and zap the corresponding entry, leaving a hole if it's not at the top.
// If the entry is not between the current top index and the bottom index
// specified by the cookie, we don't remove anything. This is the behavior
// required by JNI's DeleteLocalRef function.
// This method is not called when a local frame is popped; this is only used
// for explicit single removals.
// Returns "false" if nothing was removed.
bool IndirectReferenceTable::Remove(IRTSegmentState cookie, IndirectRef iref) {
  if (kDebugIRT) {
    LOG(INFO) << "+++ Remove: cookie=" << cookie << " last_prev=" << last_known_prev_top_index_
              << " topIndex=" << segment_state_
              << " holes=" << current_num_holes_;
  }

  uint32_t topIndex = segment_state_;
  uint32_t bottomIndex = cookie;

  RecoverHoles(bottomIndex);
  CheckHoleCount(table_, current_num_holes_, bottomIndex, topIndex);

  DCHECK(table_ != nullptr);

  if (GetIndirectRefKind(iref) == kHandleScopeOrInvalid) {
    auto* self = Thread::Current();
    if (self->HandleScopeContains(reinterpret_cast<jobject>(iref))) {
      auto* env = self->GetJniEnv();
      DCHECK(env != nullptr);
      if (env->check_jni) {
        ScopedObjectAccess soa(self);
        LOG(WARNING) << "Attempt to remove non-JNI local reference, dumping thread";
        if (kDumpStackOnNonLocalReference) {
          self->Dump(LOG_STREAM(WARNING));
        }
      }
      return true;
    }
  }
  const uint32_t idx = ExtractIndex(iref);
  if (idx < bottomIndex) {
    // Wrong segment.
    LOG(WARNING) << "Attempt to remove index outside index area (" << idx
                 << " vs " << bottomIndex << "-" << topIndex << ")";
    return false;
  }
  if (idx >= topIndex) {
    // Bad --- stale reference?
    LOG(WARNING) << "Attempt to remove invalid index " << idx
                 << " (bottom=" << bottomIndex << " top=" << topIndex << ")";
    return false;
  }

  if (idx == topIndex - 1) {
    // Top-most entry.  Scan up and consume holes.

    if (!CheckEntry("remove", iref, idx)) {
      return false;
    }

    *table_[idx].GetReference() = GcRoot<mirror::Object>(nullptr);
    if (current_num_holes_ != 0) {
      while (--topIndex > bottomIndex && current_num_holes_ != 0) {
        if (kDebugIRT) {
          ScopedObjectAccess soa(Thread::Current());
          LOG(INFO) << "+++ checking for hole at " << topIndex - 1
                    << " (cookie=" << cookie << ") val="
                    << table_[topIndex - 1].GetReference()->Read<kWithoutReadBarrier>();
        }
        if (!table_[topIndex - 1].GetReference()->IsNull()) {
          break;
        }
        if (kDebugIRT) {
          LOG(INFO) << "+++ ate hole at " << (topIndex - 1);
        }
        current_num_holes_--;
      }
      segment_state_ = topIndex;

      CheckHoleCount(table_, current_num_holes_, cookie, topIndex);
    } else {
      segment_state_ = topIndex-1;
      if (kDebugIRT) {
        LOG(INFO) << "+++ ate last entry " << topIndex - 1;
      }
    }
  } else {
    // Not the top-most entry.  This creates a hole.  We null out the entry to prevent somebody
    // from deleting it twice and screwing up the hole count.
    if (table_[idx].GetReference()->IsNull()) {
      LOG(INFO) << "--- WEIRD: removing null entry " << idx;
      return false;
    }
    if (!CheckEntry("remove", iref, idx)) {
      return false;
    }

    *table_[idx].GetReference() = GcRoot<mirror::Object>(nullptr);
    current_num_holes_++;
    CheckHoleCount(table_, current_num_holes_, cookie, topIndex);
    if (kDebugIRT) {
      LOG(INFO) << "+++ left hole at " << idx << ", holes=" << current_num_holes_;
    }
    // Mark the new hole in this segment.
    hole_at_or_above_ = bottomIndex;
  }

  return true;
}

void IndirectReferenceTable::Trim() {
  ScopedTrace trace(__PRETTY_FUNCTION__);
  const size_t top_index = Capacity();
  auto* release_start = AlignUp(reinterpret_cast<uint8_t*>(&table_[top_index]), kPageSize);
  uint8_t* release_end = table_mem_map_->End();
  madvise(release_start, release_end - release_start, MADV_DONTNEED);
}

void IndirectReferenceTable::VisitRoots(RootVisitor* visitor, const RootInfo& root_info) {
  BufferedRootVisitor<kDefaultBufferedRootCount> root_visitor(visitor, root_info);
  for (auto ref : *this) {
    if (!ref->IsNull()) {
      root_visitor.VisitRoot(*ref);
      DCHECK(!ref->IsNull());
    }
  }
}

void IndirectReferenceTable::Dump(std::ostream& os) const {
  os << kind_ << " table dump:\n";
  ReferenceTable::Table entries;
  for (size_t i = 0; i < Capacity(); ++i) {
    ObjPtr<mirror::Object> obj = table_[i].GetReference()->Read<kWithoutReadBarrier>();
    if (obj != nullptr) {
      obj = table_[i].GetReference()->Read();
      entries.push_back(GcRoot<mirror::Object>(obj));
    }
  }
  ReferenceTable::Dump(os, entries);
}

void IndirectReferenceTable::SetSegmentState(IRTSegmentState new_state) {
  if (kDebugIRT) {
    LOG(INFO) << "Setting segment state: " << segment_state_ << " -> " << new_state;
  }
  segment_state_ = new_state;
}

}  // namespace art
