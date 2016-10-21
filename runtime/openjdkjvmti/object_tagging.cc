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

#include "object_tagging.h"

#include "art_jvmti.h"
#include "base/logging.h"
#include "events-inl.h"
#include "gc/allocation_listener.h"
#include "instrumentation.h"
#include "jni_env_ext-inl.h"
#include "mirror/class.h"
#include "mirror/object.h"
#include "runtime.h"
#include "ScopedLocalRef.h"

namespace openjdkjvmti {

void ObjectTagTable::UpdateTable() {
  update_since_last_sweep_ = true;

  // TODO: Implement.
}

bool ObjectTagTable::GetTagSlowPath(art::Thread* self, art::mirror::Object* obj, jlong* result) {
  UpdateTable();
  return GetTagLocked(self, obj, result);
}

void ObjectTagTable::Add(art::mirror::Object* obj, jlong tag) {
  // Same as Set(), as we don't have duplicates in an unordered_map.
  Set(obj, tag);
}

bool ObjectTagTable::Remove(art::mirror::Object* obj, jlong* tag) {
  art::Thread* self = art::Thread::Current();
  art::MutexLock mu(self, allow_disallow_lock_);
  Wait(self);

  return RemoveLocked(self, obj, tag);
}

bool ObjectTagTable::RemoveLocked(art::Thread* self, art::mirror::Object* obj, jlong* tag) {
  auto it = tagged_objects_.find(art::GcRoot<art::mirror::Object>(obj));
  if (it != tagged_objects_.end()) {
    if (tag != nullptr) {
      *tag = it->second;
    }
    tagged_objects_.erase(it);
    return true;
  }

  if (art::kUseReadBarrier && self->GetIsGcMarking() && !update_since_last_sweep_) {
    // Update the table.
    UpdateTable();

    // And try again.
    return RemoveLocked(self, obj, tag);
  }

  // Not in here.
  return false;
}

bool ObjectTagTable::Set(art::mirror::Object* obj, jlong new_tag) {
  art::Thread* self = art::Thread::Current();
  art::MutexLock mu(self, allow_disallow_lock_);
  Wait(self);

  return SetLocked(self, obj, new_tag);
}

bool ObjectTagTable::SetLocked(art::Thread* self, art::mirror::Object* obj, jlong new_tag) {
  auto it = tagged_objects_.find(art::GcRoot<art::mirror::Object>(obj));
  if (it != tagged_objects_.end()) {
    it->second = new_tag;
    return true;
  }

  if (art::kUseReadBarrier && self->GetIsGcMarking() && !update_since_last_sweep_) {
    // Update the table.
    UpdateTable();

    // And try again.
    return SetLocked(self, obj, new_tag);
  }

  // New element.
  auto insert_it = tagged_objects_.emplace(art::GcRoot<art::mirror::Object>(obj), new_tag);
  DCHECK(insert_it.second);
  return false;
}

void ObjectTagTable::Sweep(art::IsMarkedVisitor* visitor) {
  if (event_handler_->IsEventEnabledAnywhere(JVMTI_EVENT_OBJECT_FREE)) {
    SweepImpl<true>(visitor);
  } else {
    SweepImpl<false>(visitor);
  }
  update_since_last_sweep_ = false;
}

template <bool kHandleNull>
void ObjectTagTable::SweepImpl(art::IsMarkedVisitor* visitor) {
  art::Thread* self = art::Thread::Current();
  art::MutexLock mu(self, allow_disallow_lock_);

  // As we update keys, iterators could be invalidated. Thus, do thinks in bulk.
  std::vector<std::pair<art::mirror::Object*, art::mirror::Object*>> changes;
  for (auto it = tagged_objects_.begin(); it != tagged_objects_.end(); ++it) {
    if (!it->first.IsNull()) {
      art::mirror::Object* original_obj = it->first.Read<art::kWithoutReadBarrier>();
      art::mirror::Object* target_obj = visitor->IsMarked(original_obj);
      if (original_obj != target_obj) {
        changes.emplace_back(original_obj, target_obj);
      }
    }
  }

  for (auto& change : changes) {
    auto it = tagged_objects_.find(art::GcRoot<art::mirror::Object>(change.first));
    DCHECK(it != tagged_objects_.end());

    jlong tag = it->second;
    tagged_objects_.erase(it);

    if (change.second != nullptr) {
      // Update the map.
      auto update = tagged_objects_.emplace(art::GcRoot<art::mirror::Object>(change.second), tag);
      DCHECK(update.second);
    } else if (kHandleNull) {
      HandleNullSweep(tag);
    }
  }
}

void ObjectTagTable::HandleNullSweep(jlong tag) {
  event_handler_->DispatchEvent(nullptr, JVMTI_EVENT_OBJECT_FREE, tag);
}

}  // namespace openjdkjvmti
