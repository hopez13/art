/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "art_method-inl.h"
#include "arch/trace_compiler.h"
#include "class_linker-inl.h"
#include "gc/scoped_gc_critical_section.h"
#include "handle.h"
#include "instrumentation.h"
#include "jit/jit.h"
#include "jit/jit_code_cache.h"
#include "mirror/class-inl.h"
#include "runtime.h"
#include "runtime_callbacks.h"
#include "scoped_thread_state_change-inl.h"
#include "thread-inl.h"
#include "thread_list.h"

namespace tracefast {

#if 0
#if ((!defined(TRACEFAST_INTERPRETER) && !defined(TRACEFAST_TRAMPOLINE)) || \
     (defined(TRACEFAST_INTERPRETER) && defined(TRACEFAST_TRAMPOLINE)))
#error Must set one of TRACEFAST_TRAMPOLINE or TRACEFAST_INTERPRETER during build
#endif
#endif


enum class TraceType {
  Interpreter,
  Trampoline,
  TTrace,
};

#ifdef TRACEFAST_INTERPRETER
static constexpr const char* kTracerInstrumentationKey = "tracefast_INTERPRETER";
static constexpr bool kNeedsInterpreter = true;
static constexpr TraceType kTraceType = TraceType::Interpreter;
#elif defined(TRACEFAST_TRAMPOLINE)
static constexpr const char* kTracerInstrumentationKey = "tracefast_TRAMPOLINE";
static constexpr bool kNeedsInterpreter = false;
static constexpr TraceType kTraceType = TraceType::Trampoline;
#else  // defined(TRACEFAST_TTRACE)
static constexpr const char* kTracerInstrumentationKey = "tracefast_TTRACE";
static constexpr bool kNeedsInterpreter = false;
static constexpr TraceType kTraceType = TraceType::TTrace;
#endif  // TRACEFAST_INITERPRETER

class Tracer FINAL : public art::instrumentation::InstrumentationListener,
                     public art::ClassLoadCallback {
 public:
  Tracer() {}

  void ClassPrepare(art::Handle<art::mirror::Class> tk ATTRIBUTE_UNUSED,
                    art::Handle<art::mirror::Class> k) REQUIRES_SHARED(art::Locks::mutator_lock_) {
    for (art::ArtMethod& method : k->GetMethods(art::kRuntimePointerSize)) {
      method.SetEntryPointFromQuickCompiledCode(
          art::Runtime::Current()->GetTraceCompiler()->GetTrampolineTo(
              reinterpret_cast<uintptr_t>(method.GetEntryPointFromQuickCompiledCode())));
    }
  }

  // Not used, we might not have entrypoints yet!
  void ClassLoad(art::Handle<art::mirror::Class> k ATTRIBUTE_UNUSED) {}

  void MethodEntered(art::Thread* thread ATTRIBUTE_UNUSED,
                     art::Handle<art::mirror::Object> this_object ATTRIBUTE_UNUSED,
                     art::ArtMethod* method ATTRIBUTE_UNUSED,
                     uint32_t dex_pc ATTRIBUTE_UNUSED)
      OVERRIDE REQUIRES_SHARED(art::Locks::mutator_lock_) { }

  void MethodExited(art::Thread* thread ATTRIBUTE_UNUSED,
                    art::Handle<art::mirror::Object> this_object ATTRIBUTE_UNUSED,
                    art::ArtMethod* method ATTRIBUTE_UNUSED,
                    uint32_t dex_pc ATTRIBUTE_UNUSED,
                    art::Handle<art::mirror::Object> return_value ATTRIBUTE_UNUSED)
      OVERRIDE REQUIRES_SHARED(art::Locks::mutator_lock_) { }

  void MethodExited(art::Thread* thread ATTRIBUTE_UNUSED,
                    art::Handle<art::mirror::Object> this_object ATTRIBUTE_UNUSED,
                    art::ArtMethod* method ATTRIBUTE_UNUSED,
                    uint32_t dex_pc ATTRIBUTE_UNUSED,
                    const art::JValue& return_value ATTRIBUTE_UNUSED)
      OVERRIDE REQUIRES_SHARED(art::Locks::mutator_lock_) { }

  void MethodUnwind(art::Thread* thread ATTRIBUTE_UNUSED,
                    art::Handle<art::mirror::Object> this_object ATTRIBUTE_UNUSED,
                    art::ArtMethod* method ATTRIBUTE_UNUSED,
                    uint32_t dex_pc ATTRIBUTE_UNUSED)
      OVERRIDE REQUIRES_SHARED(art::Locks::mutator_lock_) { }

  void DexPcMoved(art::Thread* thread ATTRIBUTE_UNUSED,
                  art::Handle<art::mirror::Object> this_object ATTRIBUTE_UNUSED,
                  art::ArtMethod* method ATTRIBUTE_UNUSED,
                  uint32_t new_dex_pc ATTRIBUTE_UNUSED)
      OVERRIDE REQUIRES_SHARED(art::Locks::mutator_lock_) { }

  void FieldRead(art::Thread* thread ATTRIBUTE_UNUSED,
                 art::Handle<art::mirror::Object> this_object ATTRIBUTE_UNUSED,
                 art::ArtMethod* method ATTRIBUTE_UNUSED,
                 uint32_t dex_pc ATTRIBUTE_UNUSED,
                 art::ArtField* field ATTRIBUTE_UNUSED)
      OVERRIDE REQUIRES_SHARED(art::Locks::mutator_lock_) { }

  void FieldWritten(art::Thread* thread ATTRIBUTE_UNUSED,
                    art::Handle<art::mirror::Object> this_object ATTRIBUTE_UNUSED,
                    art::ArtMethod* method ATTRIBUTE_UNUSED,
                    uint32_t dex_pc ATTRIBUTE_UNUSED,
                    art::ArtField* field ATTRIBUTE_UNUSED,
                    art::Handle<art::mirror::Object> field_value ATTRIBUTE_UNUSED)
      OVERRIDE REQUIRES_SHARED(art::Locks::mutator_lock_) { }

  void FieldWritten(art::Thread* thread ATTRIBUTE_UNUSED,
                    art::Handle<art::mirror::Object> this_object ATTRIBUTE_UNUSED,
                    art::ArtMethod* method ATTRIBUTE_UNUSED,
                    uint32_t dex_pc ATTRIBUTE_UNUSED,
                    art::ArtField* field ATTRIBUTE_UNUSED,
                    const art::JValue& field_value ATTRIBUTE_UNUSED)
      OVERRIDE REQUIRES_SHARED(art::Locks::mutator_lock_) { }

  void ExceptionThrown(art::Thread* thread ATTRIBUTE_UNUSED,
                       art::Handle<art::mirror::Throwable> exception_object ATTRIBUTE_UNUSED)
      OVERRIDE REQUIRES_SHARED(art::Locks::mutator_lock_) { }

  void ExceptionHandled(art::Thread* self ATTRIBUTE_UNUSED,
                        art::Handle<art::mirror::Throwable> throwable ATTRIBUTE_UNUSED)
      OVERRIDE REQUIRES_SHARED(art::Locks::mutator_lock_) { }

  void Branch(art::Thread* thread ATTRIBUTE_UNUSED,
              art::ArtMethod* method ATTRIBUTE_UNUSED,
              uint32_t dex_pc ATTRIBUTE_UNUSED,
              int32_t dex_pc_offset ATTRIBUTE_UNUSED)
      OVERRIDE REQUIRES_SHARED(art::Locks::mutator_lock_) { }

  void InvokeVirtualOrInterface(art::Thread* thread ATTRIBUTE_UNUSED,
                                art::Handle<art::mirror::Object> this_object ATTRIBUTE_UNUSED,
                                art::ArtMethod* caller ATTRIBUTE_UNUSED,
                                uint32_t dex_pc ATTRIBUTE_UNUSED,
                                art::ArtMethod* callee ATTRIBUTE_UNUSED)
      OVERRIDE REQUIRES_SHARED(art::Locks::mutator_lock_) { }

  void WatchedFramePop(art::Thread* thread ATTRIBUTE_UNUSED,
                       const art::ShadowFrame& frame ATTRIBUTE_UNUSED)
      OVERRIDE REQUIRES_SHARED(art::Locks::mutator_lock_) { }

 private:
  DISALLOW_COPY_AND_ASSIGN(Tracer);
};

Tracer gEmptyTracer;

static void StartTracing() REQUIRES(!art::Locks::mutator_lock_,
                                    !art::Locks::thread_list_lock_,
                                    !art::Locks::thread_suspend_count_lock_) {
  art::Thread* self = art::Thread::Current();
  art::Runtime* runtime = art::Runtime::Current();
  art::gc::ScopedGCCriticalSection gcs(self,
                                       art::gc::kGcCauseInstrumentation,
                                       art::gc::kCollectorTypeInstrumentation);
  art::ScopedSuspendAll ssa("starting fast tracing");
  switch (kTraceType) {
    case TraceType::Interpreter:
    case TraceType::Trampoline:
      runtime->GetInstrumentation()->AddListener(
          &gEmptyTracer, (art::instrumentation::Instrumentation::kMethodEntered |
                          art::instrumentation::Instrumentation::kMethodExited |
                          art::instrumentation::Instrumentation::kMethodUnwind));
      runtime->GetInstrumentation()->EnableMethodTracing(kTracerInstrumentationKey,
                                                         kNeedsInterpreter);
      break;
    case TraceType::TTrace: {
      if (runtime->GetJit() != nullptr) {
        // Not dealing with that BS
        runtime->GetJit()->GetCodeCache()->SetGarbageCollectCode(false);
      }
      struct ReplaceAllEntrypoints : public art::ClassVisitor {
       public:
        bool operator()(art::ObjPtr<art::mirror::Class> klass)
            OVERRIDE REQUIRES_SHARED(art::Locks::mutator_lock_) {
          for (art::ArtMethod& method : klass->GetMethods(art::kRuntimePointerSize)) {
            method.SetEntryPointFromQuickCompiledCode(
                art::Runtime::Current()->GetTraceCompiler()->GetTrampolineTo(
                    reinterpret_cast<uintptr_t>(method.GetEntryPointFromQuickCompiledCode())));
          }
          return true;
        }
      };
      ReplaceAllEntrypoints visitor;
      runtime->GetClassLinker()->VisitClasses(&visitor);
      runtime->GetRuntimeCallbacks()->AddClassLoadCallback(&gEmptyTracer);
      return;
    }
  }
}

class TraceFastPhaseCB : public art::RuntimePhaseCallback {
 public:
  TraceFastPhaseCB() {}

  void NextRuntimePhase(art::RuntimePhaseCallback::RuntimePhase phase)
      OVERRIDE REQUIRES_SHARED(art::Locks::mutator_lock_) {
    if (phase == art::RuntimePhaseCallback::RuntimePhase::kInit) {
      art::ScopedThreadSuspension sts(art::Thread::Current(),
                                      art::ThreadState::kWaitingForMethodTracingStart);
      StartTracing();
    }
  }
};
TraceFastPhaseCB gPhaseCallback;

// The plugin initialization function.
extern "C" bool ArtPlugin_Initialize() REQUIRES_SHARED(art::Locks::mutator_lock_) {
  art::Runtime* runtime = art::Runtime::Current();
  art::ScopedThreadSuspension stsc(art::Thread::Current(),
                                   art::ThreadState::kWaitingForMethodTracingStart);
  art::ScopedSuspendAll ssa("Add phase callback");
  runtime->GetRuntimeCallbacks()->AddRuntimePhaseCallback(&gPhaseCallback);
  return true;
}

extern "C" bool ArtPlugin_Deinitialize() {
  // Don't need to bother doing anything.
  return true;
}

}  // namespace tracefast
