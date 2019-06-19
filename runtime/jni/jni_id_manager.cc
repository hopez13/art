/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include "jni_id_manager.h"

#include "android-base/macros.h"
#include "art_field-inl.h"
#include "art_method-inl.h"
#include "base/enums.h"
#include "base/globals.h"
#include "base/locks.h"
#include "base/mutex.h"
#include "gc/allocation_listener.h"
#include "gc/heap.h"
#include "jni/jni_internal.h"
#include "mirror/array-inl.h"
#include "mirror/class-inl.h"
#include "mirror/class.h"
#include "mirror/class_ext.h"
#include "mirror/object-inl.h"
#include "obj_ptr-inl.h"
#include "thread-inl.h"
#include "thread.h"
#include <algorithm>
#include <cstdint>
#include <type_traits>

namespace art {
namespace jni {

constexpr bool kTraceIds = false;

// TODO This whole thing could be done lock & wait free (since we never remove anything from the
// ids list). It's not clear this would be worthwile though.

namespace {

static constexpr size_t IdToIndex(uintptr_t id) {
  return id >> 1;
}

static constexpr uintptr_t IndexToId(size_t index) {
  return (index << 1) + 1;
}

template <typename ArtType>
ObjPtr<mirror::PointerArray>
GetOrCreateIds(Thread* self, ObjPtr<mirror::Class> k, ArtType* t, bool* allocation_failure)
    REQUIRES_SHARED(Locks::mutator_lock_);

template <>
ObjPtr<mirror::PointerArray>
GetOrCreateIds(Thread* self, ObjPtr<mirror::Class> k, ArtField* field, bool* allocation_failure) {
  ScopedExceptionStorage ses(self);
  ObjPtr<mirror::PointerArray> res;
  if (Locks::mutator_lock_->IsExclusiveHeld(self)) {
    res = field->IsStatic() ? k->GetStaticFieldIds() : k->GetInstanceFieldIds();
  } else {
    res = field->IsStatic() ? k->GetOrCreateStaticFieldIds() : k->GetOrCreateInstanceFieldIds();
  }
  if (self->IsExceptionPending()) {
    self->AssertPendingOOMException();
    ses.SuppressOldException("Failed to allocate maps for jmethodIDs. ");
    *allocation_failure = true;
  } else {
    *allocation_failure = false;
  }
  return res;
}

template <>
ObjPtr<mirror::PointerArray>
GetOrCreateIds(Thread* self, ObjPtr<mirror::Class> k, ArtMethod* method, bool* allocation_failure) {
  if (method->IsObsolete()) {
    if (kTraceIds) {
      LOG(INFO) << "jmethodID for Obsolete method " << method->PrettyMethod() << " requested!";
    }
    // No ids array for obsolete methods. Just do a linear scan.
    *allocation_failure = false;
    return nullptr;
  }
  ObjPtr<mirror::PointerArray> res;
  if (Locks::mutator_lock_->IsExclusiveHeld(self) || !Locks::mutator_lock_->IsSharedHeld(self)) {
    res = k->GetMethodIds();
  } else {
    res = k->GetOrCreateMethodIds();
  }
  if (self->IsExceptionPending()) {
    self->AssertPendingOOMException();
    *allocation_failure = true;
  } else {
    *allocation_failure = false;
  }
  return res;
}

template <typename ArtType>
size_t GetIdOffset(ObjPtr<mirror::Class> k, ArtType* t, PointerSize pointer_size)
    REQUIRES_SHARED(Locks::mutator_lock_);

template <>
size_t
GetIdOffset(ObjPtr<mirror::Class> k, ArtField* field, PointerSize pointer_size ATTRIBUTE_UNUSED) {
  return field->IsStatic() ? k->GetStaticFieldIdOffset(field) : k->GetInstanceFieldIdOffset(field);
}

template <>
size_t GetIdOffset(ObjPtr<mirror::Class> k, ArtMethod* method, PointerSize pointer_size) {
  return method->IsObsolete() ? -1 : k->GetMethodIdOffset(method, pointer_size);
}

template <typename ArtType>
std::string PrettyGeneric(ArtType* t) REQUIRES_SHARED(Locks::mutator_lock_);

template <> std::string PrettyGeneric(ArtMethod* f) {
  return f->PrettyMethod();
}

template <> std::string PrettyGeneric(ArtField* f) {
  return f->PrettyField();
}

template <typename ArtType> bool IsObsolete(ArtType* t) REQUIRES_SHARED(Locks::mutator_lock_);
template <> bool IsObsolete(ArtField* t ATTRIBUTE_UNUSED) {
  return false;
}
template <> bool IsObsolete(ArtMethod* t) {
  return t->IsObsolete();
}

template <typename ArtType> ArtType* Canonicalize(ArtType* t) REQUIRES_SHARED(Locks::mutator_lock_);
template <> ArtField* Canonicalize(ArtField* t) {
  return t;
}
template <> ArtMethod* Canonicalize(ArtMethod* t) {
  if (UNLIKELY(t->IsCopied())) {
    t = t->GetCanonicalMethod();
  }
  return t;
}
};  // namespace

template <> uintptr_t JniIdManager::GetNextId<ArtField>() {
  uintptr_t res = next_field_id_;
  next_field_id_ += 2;
  return res;
}

template <> uintptr_t JniIdManager::GetNextId<ArtMethod>() {
  uintptr_t res = next_method_id_;
  next_method_id_ += 2;
  return res;
}
template <> std::vector<ArtField*>& JniIdManager::GetGenericMap<ArtField>() {
  return field_id_map_;
}

template <> std::vector<ArtMethod*>& JniIdManager::GetGenericMap<ArtMethod>() {
  return method_id_map_;
}
template <> size_t JniIdManager::GetLinearSearchStartId<ArtField>(ArtField* t ATTRIBUTE_UNUSED) {
  return deferred_allocation_field_id_start_;
}

template <> size_t JniIdManager::GetLinearSearchStartId<ArtMethod>(ArtMethod* m) {
  if (m->IsObsolete()) {
    return 1;
  } else {
    return deferred_allocation_method_id_start_;
  }
}

template <typename ArtType> uintptr_t JniIdManager::EncodeGenericId(ArtType* t) {
  if (!Runtime::Current()->JniIdsAreIndicies() || t == nullptr) {
    return reinterpret_cast<uintptr_t>(t);
  }
  Thread* self = Thread::Current();
  ScopedExceptionStorage ses(self);
  t = Canonicalize(t);
  ObjPtr<mirror::Class> klass = t->GetDeclaringClass();
  DCHECK(!klass.IsNull()) << "Null declaring class " << PrettyGeneric(t);
  size_t off = GetIdOffset(klass, t, kRuntimePointerSize);
  bool allocation_failure = false;
  ObjPtr<mirror::PointerArray> ids(GetOrCreateIds(self, klass, t, &allocation_failure));
  if (allocation_failure) {
    self->AssertPendingOOMException();
    ses.SuppressOldException("OOM exception while trying to allocate JNI ids.");
    return 0u;
  }
  uintptr_t cur_id = 0;
  if (!ids.IsNull()) {
    DCHECK_GT(ids->GetLength(), static_cast<int32_t>(off)) << " is " << PrettyGeneric(t);
    cur_id = ids->GetElementPtrSize<uintptr_t>(off, kRuntimePointerSize);
  }
  if (cur_id != 0) {
    return cur_id;
  }
  WriterMutexLock mu(self, *Locks::jni_id_lock_);
  // Check the ids array for a racing id.
  if (!ids.IsNull()) {
    cur_id = ids->GetElementPtrSize<uintptr_t>(off, kRuntimePointerSize);
    if (cur_id != 0) {
      // We were racing some other thread and lost.
      return cur_id;
    }
  } else {
    // We cannot allocate anything here or don't have an ids array (we might be an obsolete method).
    // Check to see if we raced and lost to another thread.
    const std::vector<ArtType*>& vec = GetGenericMap<ArtType>();
    bool found = false;
    // simple count-while.
    size_t search_start_index = IdToIndex(GetLinearSearchStartId(t));
    size_t index = std::count_if(vec.cbegin() + search_start_index,
                                 vec.cend(),
                                 [&found, t](const ArtType* candidate) {
                                   found = found || candidate == t;
                                   return !found;
                                 }) +
                   search_start_index;
    if (found) {
      // We were either racing some other thread and lost or this thread was asked to encode the
      // same method multiple times while holding the mutator lock.
      DCHECK_EQ(vec[index], t) << "Expected: " << PrettyGeneric(vec[index]) << " got "
                               << PrettyGeneric(t) << " at index " << index
                               << " (id: " << IndexToId(index) << ").";
      return IndexToId(index);
    }
  }
  cur_id = GetNextId<ArtType>();
  size_t cur_index = IdToIndex(cur_id);
  std::vector<ArtType*>& vec = GetGenericMap<ArtType>();
  vec.reserve(cur_index + 1);
  vec.resize(std::max(vec.size(), cur_index + 1), nullptr);
  vec[cur_index] = t;
  if (ids.IsNull()) {
    if (kIsDebugBuild && !IsObsolete(t)) {
      CHECK_NE(deferred_allocation_refcount_, 0u)
          << "Failed to allocate ids array despite not being forbidden from doing so!";
      Locks::mutator_lock_->AssertExclusiveHeld(self);
    }
  } else {
    ids->SetElementPtrSize(off, reinterpret_cast<void*>(cur_id), kRuntimePointerSize);
  }
  return cur_id;
}

jfieldID JniIdManager::EncodeFieldId(ArtField* field) {
  auto* res = reinterpret_cast<jfieldID>(EncodeGenericId(field));
  if (kTraceIds && field != nullptr) {
    LOG(INFO) << "Returning " << res << " for field " << field->PrettyField();
  }
  return res;
}
jmethodID JniIdManager::EncodeMethodId(ArtMethod* method) {
  auto* res = reinterpret_cast<jmethodID>(EncodeGenericId(method));
  if (kTraceIds && method != nullptr) {
    LOG(INFO) << "Returning " << res << " for method " << method->PrettyMethod();
  }
  return res;
}

template <typename ArtType> ArtType* JniIdManager::DecodeGenericId(uintptr_t t) {
  if (Runtime::Current()->JniIdsAreIndicies() && (t % 2) == 1) {
    ReaderMutexLock mu(Thread::Current(), *Locks::jni_id_lock_);
    size_t index = IdToIndex(t);
    DCHECK_GT(GetGenericMap<ArtType>().size(), index);
    return GetGenericMap<ArtType>().at(index);
  } else {
    DCHECK_EQ((t % 2), 0u) << "id: " << t;
    return reinterpret_cast<ArtType*>(t);
  }
}

ArtMethod* JniIdManager::DecodeMethodId(jmethodID method) {
  return DecodeGenericId<ArtMethod>(reinterpret_cast<uintptr_t>(method));
}

ArtField* JniIdManager::DecodeFieldId(jfieldID field) {
  return DecodeGenericId<ArtField>(reinterpret_cast<uintptr_t>(field));
}

void JniIdManager::StartDefer() {
  Thread* self = Thread::Current();
  WriterMutexLock mu(self, *Locks::jni_id_lock_);
  if (deferred_allocation_refcount_++ == 0) {
    deferred_allocation_field_id_start_ = next_field_id_;
    deferred_allocation_method_id_start_ = next_method_id_;
  }
}

void JniIdManager::EndDefer() {
  // Fixup the method->id map.
  Thread* self = Thread::Current();
  auto set_id = [&](auto* t, uintptr_t id) REQUIRES_SHARED(Locks::mutator_lock_) {
    if (t == nullptr) {
      return;
    }
    ObjPtr<mirror::Class> klass(t->GetDeclaringClass());
    size_t off = GetIdOffset(klass, t, kRuntimePointerSize);
    bool alloc_failure = false;
    ObjPtr<mirror::PointerArray> ids = GetOrCreateIds(self, klass, t, &alloc_failure);
    CHECK(!alloc_failure) << "Could not allocate jni ids array!";
    if (ids.IsNull()) {
      return;
    }
    if (kIsDebugBuild) {
      uintptr_t old_id = ids->GetElementPtrSize<uintptr_t, kRuntimePointerSize>(off);
      if (old_id != 0) {
        DCHECK_EQ(old_id, id);
      }
    }
    ids->SetElementPtrSize(off, reinterpret_cast<void*>(id), kRuntimePointerSize);
  };
  // To ensure eventual consistency this depends on the fact that the method_id_map_ and
  // field_id_map_ are the ultimate source of truth and no id is ever reused to be valid. It also
  // relies on all threads always getting calling StartDefer if they are going to be allocating jni
  // ids while suspended. If a thread tries to do so while it doesn't have a scope we could miss
  // ids.
  // TODO We should use roles or something to verify that this requirement is not broken.
  //
  // If another thread comes along and adds more methods to the list after
  // copying either (1) the id-maps are already present for the method and everything is fine, (2)
  // the thread is not suspended and so can create the ext-data and id lists or, (3) the thread also
  // suspended everything and incremented the deferred_allocation_refcount_ so it will fix up new
  // ids when it finishes.
  Locks::mutator_lock_->AssertNotExclusiveHeld(self);
  Locks::mutator_lock_->AssertSharedHeld(self);
  std::vector<ArtMethod*> method_id_copy;
  std::vector<ArtField*> field_id_copy;
  uintptr_t method_start_id;
  uintptr_t field_start_id;
  {
    ReaderMutexLock mu(self, *Locks::jni_id_lock_);
    method_id_copy = method_id_map_;
    field_id_copy = field_id_map_;
    method_start_id = deferred_allocation_method_id_start_;
    field_start_id = deferred_allocation_field_id_start_;
  }

  for (size_t index = kIsDebugBuild ? 0 : IdToIndex(method_start_id); index < method_id_copy.size();
       ++index) {
    set_id(method_id_copy[index], IndexToId(index));
  }
  for (size_t index = kIsDebugBuild ? 0 : IdToIndex(field_start_id); index < field_id_copy.size();
       ++index) {
    set_id(field_id_copy[index], IndexToId(index));
  }
  WriterMutexLock mu(self, *Locks::jni_id_lock_);
  DCHECK_GE(deferred_allocation_refcount_, 1u);
  if (--deferred_allocation_refcount_ == 0) {
    deferred_allocation_field_id_start_ = 0;
    deferred_allocation_method_id_start_ = 0;
  }
}

ScopedEnableSuspendAllJniIdQueries::ScopedEnableSuspendAllJniIdQueries()
    : manager_(Runtime::Current()->GetJniIdManager()) {
  manager_->StartDefer();
}

ScopedEnableSuspendAllJniIdQueries::~ScopedEnableSuspendAllJniIdQueries() {
  manager_->EndDefer();
}

};  // namespace jni
};  // namespace art
