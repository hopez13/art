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
  ResolvedFields().Clear();
  ResolvedMethods().Clear();
}

void DexCache::InitializeNativeFields(const DexFile* dex_file) {
  CHECK(GetDexFile() == nullptr);
  SetDexFile(dex_file);
  if (resolved_call_sites_ == 0) {
    auto* resolved_call_sites = new std::atomic<GcRoot<CallSite>>[GetDexFile()->NumCallSiteIds()];
    std::fill_n(resolved_call_sites, GetDexFile()->NumCallSiteIds(), GcRoot<CallSite>());
    resolved_call_sites_ = reinterpret_cast<uintptr_t>(resolved_call_sites);
  }
  ResolvedFields().Initialize();
  ResolvedMethods().Initialize();
  ResolvedMethodTypes().Initialize();
  ResolvedTypes().Initialize();
  ResolvedStrings().Initialize();
}

void DexCache::Clear() {
  DCHECK(GetDexFile() != nullptr);
  std::fill_n(GetResolvedCallSites(), GetDexFile()->NumCallSiteIds(), GcRoot<CallSite>());
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

void FreeDexCacheArray(void* ptr) {
  Thread* self = Thread::Current();
  MutexLock mu(self, *Locks::thread_list_lock_);
  MutexLock mu2(self, *Locks::thread_suspend_count_lock_);
  struct FreeDexCacheArrayClosure : Closure {
    FreeDexCacheArrayClosure(void* ptr, int32_t count) : ptr_(ptr), count_(count) {}
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
  // Initialize to 1 (representing current thread) to ensure we don't free the memory just yet.
  FreeDexCacheArrayClosure* closure = new FreeDexCacheArrayClosure(ptr, 1);
  for (Thread* thread : Runtime::Current()->GetThreadList()->GetList()) {
    if (thread != self) {
      closure->count_.fetch_add(1);
      if (!thread->RequestCheckpoint(closure)) {
        closure->Run(thread);
      }
    }
  }
  // Decrement the initial 1 (currnet thread) which allows us to eventually free the memory.
  closure->Run(self);
}

void DexCache::SetLocation(ObjPtr<mirror::String> location) {
  SetFieldObject<false>(OFFSET_OF_OBJECT_MEMBER(DexCache, location_), location);
}

void DexCache::SetClassLoader(ObjPtr<ClassLoader> class_loader) {
  SetFieldObject<false>(OFFSET_OF_OBJECT_MEMBER(DexCache, class_loader_), class_loader);
}

}  // namespace mirror
}  // namespace art
