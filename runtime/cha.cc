/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "cha.h"

#include "jit/jit.h"
#include "jit/jit_code_cache.h"
#include "runtime.h"
#include "scoped_thread_state_change-inl.h"
#include "stack.h"
#include "thread.h"
#include "thread_list.h"
#include "thread_pool.h"

namespace art {

void ClassHierarchyAnalysis::AddDependency(ArtMethod* method,
                                           ArtMethod* dependent_method,
                                           OatQuickMethodHeader* dependent_header) {
  auto it = cha_dependency_map_.find(method);
  if (it == cha_dependency_map_.end()) {
    cha_dependency_map_[method] =
        new std::vector<std::pair<art::ArtMethod*, art::OatQuickMethodHeader*>>();
  } else {
    DCHECK(it->second != nullptr);
  }
  cha_dependency_map_[method]->push_back(std::make_pair(dependent_method, dependent_header));
}

std::vector<std::pair<ArtMethod*, OatQuickMethodHeader*>>*
    ClassHierarchyAnalysis::GetDependents(ArtMethod* method) {
  auto it = cha_dependency_map_.find(method);
  if (it != cha_dependency_map_.end()) {
    DCHECK(it->second != nullptr);
    return it->second;
  }
  return nullptr;
}

void ClassHierarchyAnalysis::RemoveDependencyFor(ArtMethod* method) {
  auto it = cha_dependency_map_.find(method);
  if (it != cha_dependency_map_.end()) {
    auto dependents = it->second;
    cha_dependency_map_.erase(it);
    delete dependents;
  }
}

// This stack visitor walks the stack and for compiled code with certain method
// headers, sets the should_deoptimize flag on stack to 1.
// TODO: also set the register value to 1 when should_deoptimize is allocated in
// a register.
class CHAStackVisitor FINAL  : public StackVisitor {
 public:
  CHAStackVisitor(Thread* thread_in,
                  Context* context,
                  const std::unordered_set<OatQuickMethodHeader*>& method_headers)
      : StackVisitor(thread_in, context, StackVisitor::StackWalkKind::kSkipInlinedFrames),
        method_headers_(method_headers) {
  }

  bool VisitFrame() OVERRIDE REQUIRES_SHARED(Locks::mutator_lock_) {
    ArtMethod* method = GetMethod();
    if (method == nullptr || method->IsRuntimeMethod() || method->IsNative()) {
      return true;
    }
    if (GetCurrentQuickFrame() == nullptr) {
      // Not compiled code.
      return true;
    }
    // Method may have multiple versions of compiled code. Check
    // the method header to see if it has should_deoptimize flag.
    const OatQuickMethodHeader* method_header = GetCurrentOatQuickMethodHeader();
    if (!method_header->HasShouldDeoptimizeFlag()) {
      // This compiled version doesn't have should_deoptimize flag. Skip.
      return true;
    }
    auto it = std::find(method_headers_.begin(), method_headers_.end(), method_header);
    if (it == method_headers_.end()) {
      // Not in the list of method headers that should be deoptimized.
      return true;
    }

    // Need to deoptimize. Set should_deoptimize flag to true.
    QuickMethodFrameInfo frame_info = GetCurrentQuickFrameInfo();
    size_t frame_size = frame_info.FrameSizeInBytes();
    uint8_t* sp = reinterpret_cast<uint8_t*>(GetCurrentQuickFrame());
    size_t core_spill_size = POPCOUNT(frame_info.CoreSpillMask()) *
        GetBytesPerGprSpillLocation(kRuntimeISA);
    size_t fpu_spill_size = POPCOUNT(frame_info.FpSpillMask()) *
        GetBytesPerFprSpillLocation(kRuntimeISA);
    size_t offset = frame_size - core_spill_size - fpu_spill_size - kShouldDeoptimizeFlagSize;
    uint8_t* should_deoptimize_addr = sp + offset;
    // Set deoptimization flag to 1.
    DCHECK(*should_deoptimize_addr == 0 || *should_deoptimize_addr == 1);
    *should_deoptimize_addr = 1;
    return true;
  }

 private:
  // List of method headers for compiled code that should be deoptimized.
  const std::unordered_set<OatQuickMethodHeader*>& method_headers_;

  DISALLOW_COPY_AND_ASSIGN(CHAStackVisitor);
};

class CHACheckpoint FINAL : public Closure {
 public:
  explicit CHACheckpoint(const std::unordered_set<OatQuickMethodHeader*>& method_headers)
      : barrier_(0),
        method_headers_(method_headers) {}

  void Run(Thread* thread) OVERRIDE {
    // Note thread and self may not be equal if thread was already suspended at
    // the point of the request.
    Thread* self = Thread::Current();
    ScopedObjectAccess soa(self);
    CHAStackVisitor visitor(thread, nullptr, method_headers_);
    visitor.WalkStack();
    barrier_.Pass(self);
  }

  void WaitForThreadsToRunThroughCheckpoint(size_t threads_running_checkpoint) {
    Thread* self = Thread::Current();
    ScopedThreadStateChange tsc(self, kWaitingForCheckPointsToRun);
    barrier_.Increment(self, threads_running_checkpoint);
  }

 private:
  // The barrier to be passed through and for the requestor to wait upon.
  Barrier barrier_;
  // List of method headers for invalidated compiled code.
  const std::unordered_set<OatQuickMethodHeader*>& method_headers_;

  DISALLOW_COPY_AND_ASSIGN(CHACheckpoint);
};

void ClassHierarchyAnalysis::CheckSingleImplementationInfo(
    Handle<mirror::Class> klass,
    ArtMethod* virtual_method,
    ArtMethod* method_in_super,
    std::unordered_set<ArtMethod*>& invalidated_single_impl_methods) {
  // TODO: if klass is not instantiable, virtual_method isn't invocable yet so
  // even if it overrides, it doesn't invalidate single-implementation
  // assumption.

  DCHECK_NE(virtual_method, method_in_super);
  DCHECK(method_in_super->GetDeclaringClass()->IsResolved()) << "class isn't resolved";
  // If virtual_method doesn't come from a default interface method, it should
  // be supplied by klass.
  DCHECK(virtual_method->IsCopied() ||
         virtual_method->GetDeclaringClass() == klass.Get());

  // Basically a new virtual_method should set method_in_super to
  // non-single-implementation (if not set already).
  // We don't grab cha_lock_. Single-implementation flag won't be set to true
  // again once it's set to false.
  if (!method_in_super->HasSingleImplementation()) {
    // method_in_super already has multiple implementations. All methods in the
    // same vtable slots in its super classes should have
    // non-single-implementation already.
    if (kIsDebugBuild) {
      // Verify all methods in the same slot before method_in_super doesn't have
      // single-implementation. Grab cha_lock_ to make sure all
      // single-implementation updates are seen.
      PointerSize image_pointer_size =
          Runtime::Current()->GetClassLinker()->GetImagePointerSize();
      MutexLock cha_mu(Thread::Current(), *Locks::cha_lock_);
      mirror::Class* verify_class = klass->GetSuperClass()->GetSuperClass();
      uint16_t verify_index = method_in_super->GetMethodIndex();
      while (verify_class != nullptr) {
        if (verify_index >= verify_class->GetVTableLength()) {
          break;
        }
        ArtMethod* verify_method =
            verify_class->GetVTableEntry(verify_index, image_pointer_size);
        DCHECK(!verify_method->HasSingleImplementation())
            << "class: " << art::PrettyClass(klass.Get())
            << " verify_method: " << art::PrettyMethod(verify_method, true);
        verify_class = verify_class->GetSuperClass();
      }
    }
    return;
  }

  // Native methods don't have single-implementation flag set.
  DCHECK(!method_in_super->IsNative());
  // Invalidate method_in_super's single-implementation status.
  invalidated_single_impl_methods.insert(method_in_super);
}

void ClassHierarchyAnalysis::InitSingleImplementationFlag(Handle<mirror::Class> klass,
                                                          ArtMethod* method) {
  DCHECK(method->IsCopied() || method->GetDeclaringClass() == klass.Get());
  if (method->IsNative()) {
    // Skip native method since we use the native entry point to
    // keep single implementation info, and native method's invocation
    // overhead is already high and it cannot be inlined anyway.
    DCHECK(!method->HasSingleImplementation());
  } else {
    method->SetHasSingleImplementation(true);
    if (method->IsAbstract()) {
      // There is no real implementation yet.
      // TODO: implement single-implementation logic for abstract methods.
      DCHECK(method->GetSingleImplementation() == nullptr);
    } else {
      // Single implementation of non-abstract method is itself.
      DCHECK_EQ(method->GetSingleImplementation(), method);
    }
  }
}

void ClassHierarchyAnalysis::Update(Handle<mirror::Class> klass) {
  if (klass->IsInterface()) {
    return;
  }
  mirror::Class* super_class = klass->GetSuperClass();
  if (super_class == nullptr) {
    return;
  }

  // Keeps track of all methods whose single-implementation assumption
  // is invalidated by linking `klass`.
  std::unordered_set<ArtMethod*> invalidated_single_impl_methods;

  PointerSize image_pointer_size = Runtime::Current()->GetClassLinker()->GetImagePointerSize();
  for (int32_t i = 0; i < super_class->GetVTableLength(); ++i) {
    ArtMethod* method = klass->GetVTableEntry(i, image_pointer_size);
    ArtMethod* method_in_super = super_class->GetVTableEntry(i, image_pointer_size);
    if (method == method_in_super) {
      // vtable slot entry is inherited from super class.
      continue;
    }
    InitSingleImplementationFlag(klass, method);
    CheckSingleImplementationInfo(klass,
                                  method,
                                  method_in_super,
                                  invalidated_single_impl_methods);
  }

  // For new virtual methods that don't override.
  for (int32_t i = super_class->GetVTableLength(); i < klass->GetVTableLength(); ++i) {
    ArtMethod* method = klass->GetVTableEntry(i, image_pointer_size);
    InitSingleImplementationFlag(klass, method);
  }

  Runtime* const runtime = Runtime::Current();
  if (!invalidated_single_impl_methods.empty()) {
    Thread *self = Thread::Current();
    // Method headers for compiled code to be invalidated.
    std::unordered_set<OatQuickMethodHeader*> dependent_method_headers;

    {
      // We do this under cha_lock_. Committing code also grabs this lock to
      // make sure the code is only committed when all single-implementation
      // assumptions are still true.
      MutexLock cha_mu(self, *Locks::cha_lock_);
      // Invalidate compiled methods that assume some virtual calls have only
      // single implementations.
      for (ArtMethod* invalidated : invalidated_single_impl_methods) {
        if (!invalidated->HasSingleImplementation()) {
          // It might have been invalidated already when other class linking is
          // going on.
          continue;
        }
        invalidated->SetHasSingleImplementation(false);

        if (runtime->IsAotCompiler()) {
          // No need to invalidate any compiled code as the AotCompiler doesn't
          // run any code.
          continue;
        }

        // Invalidate all dependents.
        auto dependents = GetDependents(invalidated);
        if (dependents == nullptr) {
          continue;
        }
        for (const auto& dependent : *dependents) {
          ArtMethod* method = dependent.first;;
          OatQuickMethodHeader* method_header = dependent.second;
          VLOG(class_linker) << "CHA invalidated compiled code for " << PrettyMethod(method);
          DCHECK(runtime->UseJitCompilation());
          runtime->GetJit()->GetCodeCache()->InvalidateCompiledCodeFor(
              method, method_header);
          dependent_method_headers.insert(method_header);
        }
        RemoveDependencyFor(invalidated);
      }
    }

    if (dependent_method_headers.empty()) {
      return;
    }
    // Deoptimze compiled code on stack that should have been invalidated.
    CHACheckpoint checkpoint(dependent_method_headers);
    size_t threads_running_checkpoint = runtime->GetThreadList()->RunCheckpoint(&checkpoint);
    if (threads_running_checkpoint != 0) {
      checkpoint.WaitForThreadsToRunThroughCheckpoint(threads_running_checkpoint);
    }
  }
}

}  // namespace art
