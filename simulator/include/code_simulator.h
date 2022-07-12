/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef ART_SIMULATOR_INCLUDE_CODE_SIMULATOR_H_
#define ART_SIMULATOR_INCLUDE_CODE_SIMULATOR_H_

#include "arch/instruction_set.h"
#include "runtime.h"

namespace art {

class ArtMethod;
union JValue;
class Thread;
struct QuickEntryPoints;

//
// Classes in this file model simulator executors - a per thread entities with their own contexts:
// simulated stack, registers, etc.
//

// This class implements a basic simulator executor which is capable of executing sequences
// of simulated ISA instructions. It is not aware of ART runtime so it can manage only trivial
// ART methods.
//
// Currently only used by code generator tests.
class BasicCodeSimulator {
 public:
  BasicCodeSimulator() {}
  virtual ~BasicCodeSimulator() {}

  // Starts simulating instructions of a target isa from a buffer.
  virtual void RunFrom(intptr_t code_buffer) = 0;

  // Get return value according to C ABI.
  virtual bool GetCReturnBool() const = 0;
  virtual int32_t GetCReturnInt32() const = 0;
  virtual int64_t GetCReturnInt64() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(BasicCodeSimulator);
};

// Returns a null pointer if a simulator cannot be found for target_isa.
extern "C" BasicCodeSimulator* CreateBasicCodeSimulator(InstructionSet target_isa);

#ifdef ART_USE_SIMULATOR

// This class implements a ART runtime aware simulator executor which can execute all* quick ABI
// code: is aware of ART entrypoints, ABI/ISA transitions, etc.
class CodeSimulator {
 public:
  CodeSimulator() {}
  virtual ~CodeSimulator() {}

  // Invokes (starts to simulate) a method; this should follow the semantics of
  // art_quick_invoke_stub.
  virtual void Invoke(ArtMethod* method,
                      uint32_t* args,
                      uint32_t args_size,
                      Thread* self,
                      JValue* result,
                      const char* shorty,
                      bool isStatic) REQUIRES_SHARED(Locks::mutator_lock_) = 0;

  // Returns the "highest addressable byte" of the stack.
  virtual uint8_t* GetStackBase() = 0;

  // Returns the size of the stack.
  virtual size_t GetStackSize() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(CodeSimulator);
};

extern "C" CodeSimulator* CreateCodeSimulator(InstructionSet target_isa);

// This class helps to create/manage/update quick entrypoints which should have
// a special behavour in simulator mode, e.g. those whose code is of different ISA.
class SimulatorEntryPointsManager {
 public:
  SimulatorEntryPointsManager();
  virtual ~SimulatorEntryPointsManager() {}

  // Updates someone's entrypoints (e.g. Thread's TLS ones) with custom simulator ones.
  virtual void UpdateOthersEntryPoints(QuickEntryPoints* others_entry_points) const = 0;

  // Inits and prepares simulator custom entrypoints.
  virtual void InitCustomEntryPoints() = 0;

  const QuickEntryPoints* GetEntryPoints() const { return &entry_points_; }

  using ArtInvokeStubType = void (*)(ArtMethod*,
                                     uint32_t*,
                                     uint32_t,
                                     Thread*,
                                     JValue*,
                                     const char*);
  using ArtLongJumpStubType = void (*)(uint64_t*, uint64_t*);
  using ArtPendingExceptionStubType = void (*)();
  using ArtDeoptimizeStubType = void (*)();
  using ArtInstrumentationExitStubType = void (*)();
  using ArtInstrumentationEntryStubType = void (*)(void*);
  using ArtProxyInvokeStubType = void (*)();
  using ArtInvokeObsoleteStubType = void (*)(ArtMethod*);

  ArtInvokeStubType GetInvokeStub() const { return invoke_stub_; }
  ArtInvokeStubType GetInvokeStaticStub() const { return invoke_static_stub_; }
  ArtLongJumpStubType GetLongJumpStub() const { return long_jump_stub_; }
  ArtPendingExceptionStubType GetPendingExceptionStub() const { return pending_exception_stub_; }
  ArtDeoptimizeStubType GetDeoptimizeStub() const { return deoptimize_stub_; }
  ArtProxyInvokeStubType GetProxyInvokeStub() const { return proxy_invoke_stub_; }
  ArtInvokeObsoleteStubType GetInvokeObsoleteStub() const { return invoke_obsolete_stub_; }

  ArtInstrumentationExitStubType GetInstrumentationExitStub() const {
    return instrumentation_exit_stub_;
  }

  ArtInstrumentationEntryStubType GetInstrumentationEntryStub() const {
    return instrumentation_entry_stub_;
  }

  // Returns an unimplemented entrypoint, e.g. to use for those entrypoints which simulator
  // doesn't care about.
  static uintptr_t GetUnimplementedEntryPoint() {
    return 0;
  }

  // Sets the entrypoint/stub pointer to unimplemented state.
  template <typename FuncType>
  static void SetStubToUnimplemented(FuncType* func) {
    *func = reinterpret_cast<FuncType>(0);
  }

 protected:
  // Custom simulator entrypoints.
  QuickEntryPoints entry_points_;

  // Pointer to a simulator version of art_quick_invoke_stub.
  ArtInvokeStubType invoke_stub_;
  // Pointer to a simulator version of art_quick_invoke_static_stub.
  ArtInvokeStubType invoke_static_stub_;
  // Pointer to a simulator version of art_quick_do_long_jump.
  ArtLongJumpStubType long_jump_stub_;
  // Pointer to a simulator version of art_deliver_pending_exception.
  ArtPendingExceptionStubType pending_exception_stub_;
  // Pointer to a simulator version of art_quick_deoptimize.
  ArtDeoptimizeStubType deoptimize_stub_;
  // Pointer to a simulator version of art_quick_instrumentation_exit.
  // Note: exit stub should be generated before entry as the latter uses it.
  ArtInstrumentationExitStubType instrumentation_exit_stub_;
  // Pointer to a simulator version of art_quick_instrumentation_entry.
  ArtInstrumentationEntryStubType instrumentation_entry_stub_;
  // Pointer to a simulator version of art_quick_proxy_invoke_handler.
  ArtProxyInvokeStubType proxy_invoke_stub_;
  // Pointer to a simulator version of art_quick_invoke_obsolete_stub.
  ArtInvokeObsoleteStubType invoke_obsolete_stub_;
};

extern "C" SimulatorEntryPointsManager* CreateSimulatorEntryPointsManager(
    InstructionSet target_isa);

#endif

}  // namespace art

#endif  // ART_SIMULATOR_INCLUDE_CODE_SIMULATOR_H_
