/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include "dex_cache-inl.h"

#include "art_method-inl.h"
#include "class_linker.h"
#include "gc/accounting/card_table-inl.h"
#include "gc/heap.h"
#include "linear_alloc.h"
#include "oat_file.h"
#include "object-inl.h"
#include "object.h"
#include "object_array-inl.h"
#include "reflective_value_visitor.h"
#include "runtime.h"
#include "runtime_globals.h"
#include "string.h"
#include "thread.h"
#include "write_barrier.h"

namespace art {
namespace mirror {

void DexCache::VisitReflectiveTargets(ReflectiveValueVisitor* visitor ATTRIBUTE_UNUSED) {
  // Used by JVMTI, it is easiest to just clear the arrays.
  ResolvedFields().Clear();
  ResolvedMethods().Clear();
}

void DexCache::Initialize(const DexFile* dex_file,
                          ObjPtr<ClassLoader> class_loader,
                          LinearAlloc* linear_alloc) {
  DCHECK(GetDexFile() == nullptr);
  Thread* self = Thread::Current();
  SetDexFile(dex_file);
  SetClassLoader(class_loader);

  // Allocate fixed sized array, since we can not randomly clear/drop callsites.
  // The first atomically set call site wins and is persisted.
  DCHECK_EQ(resolved_call_sites_, 0u);
  num_resolved_call_sites_ = dex_file->NumCallSiteIds();
  size_t size = sizeof(std::atomic<GcRoot<CallSite>>) * num_resolved_call_sites_;
  void* resolved_call_sites = linear_alloc->AllocAlign16(self, RoundUp(size, 16));
  resolved_call_sites_ = reinterpret_cast<uintptr_t>(resolved_call_sites);

  // Allocate all other arrays lazily, when they are first needed.
  DCHECK_EQ(resolved_fields_, 0u);
  DCHECK_EQ(resolved_method_types_, 0u);
  DCHECK_EQ(resolved_methods_, 0u);
  DCHECK_EQ(resolved_types_, 0u);
  DCHECK_EQ(strings_, 0u);
}

void DexCache::Clear() {
  DCHECK(GetDexFile() != nullptr);
  std::fill_n(GetResolvedCallSites(), num_resolved_call_sites_, GcRoot<CallSite>());
  ResolvedFields().Clear();
  ResolvedMethods().Clear();
  ResolvedMethodTypes().Clear();
  ResolvedTypes().Clear();
  ResolvedStrings().Clear();
}

void DexCache::ResetNativeFields() {
  dex_file_ = 0;
  preresolved_strings_ = 0;
  resolved_call_sites_ = 0;
  resolved_fields_ = 0;
  resolved_method_types_ = 0;
  resolved_methods_ = 0;
  resolved_types_ = 0;
  strings_ = 0;
}

// Allocate native memory, and track it in class linker so it can be freed.
// (DexCache is weak heap objected which can be GCed at any point)
void* DexCache::AllocArray(size_t alignment, size_t size) {
  void* ptr = aligned_alloc(alignment, size);
  WriterMutexLock mu(Thread::Current(), *Locks::classlinker_classes_lock_);
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  auto* data = class_linker->FindClassLoaderDataLocked(GetClassLoader());
  if (data != nullptr) {
    data->dex_cache_arrays.emplace(ptr);
  } else {
    class_linker->dex_cache_arrays_.emplace(ptr);
  }
  return ptr;
}

void DexCache::FreeArray(void* ptr) {
  Thread* self = Thread::Current();
  {
    // Remove the reference from class linker to avoid later double free.
    WriterMutexLock mu(self, *Locks::classlinker_classes_lock_);
    ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
    auto* data = class_linker->FindClassLoaderDataLocked(GetClassLoader());
    if (data != nullptr) {
      size_t num_removed = data->dex_cache_arrays.erase(ptr);
      DCHECK_EQ(num_removed, 1u);
    } else {
      size_t num_removed = class_linker->dex_cache_arrays_.erase(ptr);
      DCHECK_EQ(num_removed, 1u);
    }
  }

  // Closure to postpone the free until after all threads pass suspend point.
  // This is needed as other threads might be still using the memory concurrently.
  struct FreeDexCacheArrayClosure : Closure {
    FreeDexCacheArrayClosure(void* ptr, int32_t count)
        : ptr_(ptr), count_(count) {}
    void Run(Thread* thread ATTRIBUTE_UNUSED) override {
      int32_t count = count_.fetch_sub(1) - 1;
      if (count == 0) {
        free(ptr_);
        delete this;
      }
    }
    void* ptr_;
    std::atomic_int32_t count_;
  };

  // Initialize to 1 to ensure we don't free the memory while threads are still being added.
  FreeDexCacheArrayClosure* closure = new FreeDexCacheArrayClosure(ptr, 1);
  MutexLock mu2(self, *Locks::thread_list_lock_);
  MutexLock mu3(self, *Locks::thread_suspend_count_lock_);
  for (Thread* thread : Runtime::Current()->GetThreadList()->GetList()) {
    if (thread != self) {
      closure->count_.fetch_add(1);
      if (!thread->RequestCheckpoint(closure)) {
        closure->Run(thread);
      }
    }
  }
  closure->Run(self);  // Decrement the initial 1.
}

void DexCache::SetLocation(ObjPtr<mirror::String> location) {
  SetFieldObject<false>(OFFSET_OF_OBJECT_MEMBER(DexCache, location_), location);
}

void DexCache::SetClassLoader(ObjPtr<ClassLoader> class_loader) {
  SetFieldObject<false>(OFFSET_OF_OBJECT_MEMBER(DexCache, class_loader_), class_loader);
}

}  // namespace mirror
}  // namespace art
