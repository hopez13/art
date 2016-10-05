/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef ART_RUNTIME_INDIRECT_REFERENCE_TABLE_INL_H_
#define ART_RUNTIME_INDIRECT_REFERENCE_TABLE_INL_H_

#include "indirect_reference_table.h"

#include "base/dumpable.h"
#include "gc_root-inl.h"
#include "obj_ptr-inl.h"
#include "runtime-inl.h"
#include "verify_object-inl.h"

namespace art {
namespace mirror {
class Object;
}  // namespace mirror

// Verifies that the indirect table lookup is valid.
// Returns "false" if something looks bad.
inline bool IndirectReferenceTable::GetChecked(IndirectRef iref) const {
  if (UNLIKELY(iref == nullptr)) {
    LOG(WARNING) << "Attempt to look up nullptr " << kind_;
    return false;
  }
  if (UNLIKELY(GetIndirectRefKind(iref) == kHandleScopeOrInvalid)) {
    AbortIfNoCheckJNI(StringPrintf("JNI ERROR (app bug): invalid %s %p",
                                   GetIndirectRefKindString(kind_),
                                   iref));
    return false;
  }
  const int topIndex = segment_state_.parts.topIndex;
  int idx = ExtractIndex(iref);
  if (UNLIKELY(idx >= topIndex)) {
    std::string msg = StringPrintf(
        "JNI ERROR (app bug): accessed stale %s %p  (index %d in a table of size %d)",
        GetIndirectRefKindString(kind_),
        iref,
        idx,
        topIndex);
    AbortIfNoCheckJNI(msg);
    return false;
  }
  if (UNLIKELY(table_[idx].GetReference()->IsNull())) {
    AbortIfNoCheckJNI(StringPrintf("JNI ERROR (app bug): accessed deleted %s %p",
                                   GetIndirectRefKindString(kind_),
                                   iref));
    return false;
  }
  if (UNLIKELY(!CheckEntry("use", iref, idx))) {
    return false;
  }
  return true;
}

// Make sure that the entry at "idx" is correctly paired with "iref".
inline bool IndirectReferenceTable::CheckEntry(const char* what, IndirectRef iref, int idx) const {
  IndirectRef checkRef = ToIndirectRef(idx);
  if (UNLIKELY(checkRef != iref)) {
    std::string msg = StringPrintf(
        "JNI ERROR (app bug): attempt to %s stale %s %p (should be %p)",
        what,
        GetIndirectRefKindString(kind_),
        iref,
        checkRef);
    AbortIfNoCheckJNI(msg);
    return false;
  }
  return true;
}

template<ReadBarrierOption kReadBarrierOption>
inline ObjPtr<mirror::Object> IndirectReferenceTable::Get(IndirectRef iref) const {
  if (!GetChecked(iref)) {
    return nullptr;
  }
  uint32_t idx = ExtractIndex(iref);
  ObjPtr<mirror::Object> obj = table_[idx].GetReference()->Read<kReadBarrierOption>();
  VerifyObject(obj.Ptr());
  return obj;
}

inline void IndirectReferenceTable::Update(IndirectRef iref, ObjPtr<mirror::Object> obj) {
  if (!GetChecked(iref)) {
    LOG(WARNING) << "IndirectReferenceTable Update failed to find reference " << iref;
    return;
  }
  uint32_t idx = ExtractIndex(iref);
  table_[idx].SetReference(obj);
}

inline IndirectRef IndirectReferenceTable::Add(uint32_t cookie, ObjPtr<mirror::Object> obj) {
  IRTSegmentState prevState;
  prevState.all = cookie;
  size_t topIndex = segment_state_.parts.topIndex;

  CHECK(obj != nullptr);
  VerifyObject(obj.Ptr());
  DCHECK(table_ != nullptr);
  DCHECK_GE(segment_state_.parts.numHoles, prevState.parts.numHoles);

  if (topIndex == max_entries_) {
    LOG(FATAL) << "JNI ERROR (app bug): " << kind_ << " table overflow "
               << "(max=" << max_entries_ << ")\n"
               << MutatorLockedDumpable<IndirectReferenceTable>(*this);
  }

  // We know there's enough room in the table.  Now we just need to find
  // the right spot.  If there's a hole, find it and fill it; otherwise,
  // add to the end of the list.
  IndirectRef result;
  int numHoles = segment_state_.parts.numHoles - prevState.parts.numHoles;
  size_t index;
  if (numHoles > 0) {
    DCHECK_GT(topIndex, 1U);
    // Find the first hole; likely to be near the end of the list.
    IrtEntry* pScan = &table_[topIndex - 1];
    DCHECK(!pScan->GetReference()->IsNull());
    --pScan;
    while (!pScan->GetReference()->IsNull()) {
      DCHECK_GE(pScan, table_ + prevState.parts.topIndex);
      --pScan;
    }
    index = pScan - table_;
    segment_state_.parts.numHoles--;
  } else {
    // Add to the end.
    index = topIndex++;
    segment_state_.parts.topIndex = topIndex;
  }
  table_[index].Add(obj);
  result = ToIndirectRef(index);
  if ((false)) {
    LOG(INFO) << "+++ added at " << ExtractIndex(result) << " top=" << segment_state_.parts.topIndex
              << " holes=" << segment_state_.parts.numHoles;
  }

  DCHECK(result != nullptr);
  return result;
}

inline void IrtEntry::Add(ObjPtr<mirror::Object> obj) {
  ++serial_;
  if (serial_ == kIRTPrevCount) {
    serial_ = 0;
  }
  references_[serial_] = GcRoot<mirror::Object>(obj);
}

inline void IrtEntry::SetReference(ObjPtr<mirror::Object> obj) {
  DCHECK_LT(serial_, kIRTPrevCount);
  references_[serial_] = GcRoot<mirror::Object>(obj);
}

}  // namespace art

#endif  // ART_RUNTIME_INDIRECT_REFERENCE_TABLE_INL_H_
