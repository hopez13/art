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

#include "code_simulator_arm64.h"

#include "arch/arm64/asm_support_arm64.h"
#include "arch/instruction_set.h"
#include "base/memory_region.h"
#include "base/utils.h"
#include "entrypoints/quick/runtime_entrypoints_list.h"
#include "fault_handler.h"

#include <sys/ucontext.h>

#include "code_simulator_container.h"

using namespace vixl::aarch64;  // NOLINT(build/namespaces)

namespace art {

// Enable the simulator debugger, disabled by default.
static constexpr bool kSimDebuggerEnabled = false;

namespace jit {
class OsrData;
}

extern "C" const void* GetQuickInvokeStub();
extern "C" const void* GetQuickInvokeStaticStub();
extern "C" const void* GetQuickOsrStub();
extern "C" const void* GetQuickThrowNullPointerExceptionFromSignal();
extern "C" const void* GetQuickThrowStackOverflow();

extern "C" const char* NterpGetShorty(ArtMethod* method)
    REQUIRES_SHARED(Locks::mutator_lock_);
extern "C" const char* NterpGetShortyFromMethodId(ArtMethod* caller, uint32_t method_index)
    REQUIRES_SHARED(Locks::mutator_lock_);
extern "C" const char* NterpGetShortyFromInvokePolymorphic(ArtMethod* caller, uint16_t* dex_pc_ptr)
    REQUIRES_SHARED(Locks::mutator_lock_);
extern "C" const char* NterpGetShortyFromInvokeCustom(ArtMethod* caller, uint16_t* dex_pc_ptr)
    REQUIRES_SHARED(Locks::mutator_lock_);

FLATTEN
extern "C" size_t NterpGetMethod(Thread* self, ArtMethod* caller, const uint16_t* dex_pc_ptr)
    REQUIRES_SHARED(Locks::mutator_lock_);
extern "C" size_t NterpGetStaticField(Thread* self,
                                      ArtMethod* caller,
                                      const uint16_t* dex_pc_ptr,
                                      size_t resolve_field_type)  // Resolve if not zero
    REQUIRES_SHARED(Locks::mutator_lock_);
extern "C" uint32_t NterpGetInstanceFieldOffset(Thread* self,
                                                ArtMethod* caller,
                                                const uint16_t* dex_pc_ptr,
                                                size_t resolve_field_type)  // Resolve if not zero
    REQUIRES_SHARED(Locks::mutator_lock_);
extern "C" mirror::Object* NterpGetClass(Thread* self, ArtMethod* caller, uint16_t* dex_pc_ptr)
    REQUIRES_SHARED(Locks::mutator_lock_);
extern "C" mirror::Object* NterpAllocateObject(Thread* self,
                                               ArtMethod* caller,
                                               uint16_t* dex_pc_ptr)
    REQUIRES_SHARED(Locks::mutator_lock_);
extern "C" mirror::Object* NterpLoadObject(Thread* self, ArtMethod* caller, uint16_t* dex_pc_ptr)
    REQUIRES_SHARED(Locks::mutator_lock_);
extern "C" mirror::Object* NterpFilledNewArray(Thread* self,
                                               ArtMethod* caller,
                                               uint32_t* registers,
                                               uint16_t* dex_pc_ptr)
    REQUIRES_SHARED(Locks::mutator_lock_);
extern "C" mirror::Object* NterpFilledNewArrayRange(Thread* self,
                                                    ArtMethod* caller,
                                                    uint32_t* registers,
                                                    uint16_t* dex_pc_ptr)
    REQUIRES_SHARED(Locks::mutator_lock_);
extern "C" jit::OsrData* NterpHotMethod(ArtMethod* method, uint16_t* dex_pc_ptr, uint32_t* vregs)
    REQUIRES_SHARED(Locks::mutator_lock_);
extern "C" ssize_t NterpDoPackedSwitch(const uint16_t* switchData, int32_t testVal)
    REQUIRES_SHARED(Locks::mutator_lock_);
extern "C" ssize_t NterpDoSparseSwitch(const uint16_t* switchData, int32_t testVal)
    REQUIRES_SHARED(Locks::mutator_lock_);
extern "C" void NterpFree(void* val);
extern "C" float NterpFmodF(float x, float y);
extern "C" double NterpFmod(double x, double y);

namespace arm64 {

BasicCodeSimulatorArm64* BasicCodeSimulatorArm64::CreateBasicCodeSimulatorArm64(size_t stack_size) {
  if (kCanSimulate) {
    BasicCodeSimulatorArm64* simulator = new BasicCodeSimulatorArm64();
    simulator->InitInstructionSimulator(stack_size);
    return simulator;
  } else {
    return nullptr;
  }
}

BasicCodeSimulatorArm64::BasicCodeSimulatorArm64()
    : BasicCodeSimulator(), decoder_(nullptr), simulator_(nullptr) {
  CHECK(kCanSimulate);
  decoder_.reset(new Decoder());
}


Simulator* BasicCodeSimulatorArm64::CreateNewInstructionSimulator(SimStack::Allocated&& stack) {
  return new Simulator(decoder_.get(), stdout, std::move(stack));
}

void BasicCodeSimulatorArm64::InitInstructionSimulator(size_t stack_size) {
  SimStack stack_builder;
  stack_builder.SetUsableSize(stack_size);

  // Protected regions are added for the simulator in Thread::InstallSimulatorImplicitProtection()
  // so disable them for the simulator here.
  stack_builder.SetLimitGuardSize(0);
  stack_builder.SetBaseGuardSize(0);

  // Align the stack to a page so we can install protected regions using mprotect.
  stack_builder.AlignToBytesLog2(log2(MemMap::GetPageSize()));

  SimStack::Allocated stack = stack_builder.Allocate();
  simulator_.reset(CreateNewInstructionSimulator(std::move(stack)));

  // VIXL simulator will print a warning by default if it gets an instruction with any special
  // behavior in terms of memory model - not only those with exclusive access.
  //
  // TODO: Update this once the behavior is resolved in VIXL.
  simulator_->SilenceExclusiveAccessWarning();

  if (VLOG_IS_ON(simulator)) {
    // Only trace the main thread. Multiple threads tracing simulation at the same time can ruin
    // the output trace, making it difficult to read.
    // TODO(Simulator): Support tracing multiple threads at the same time.
    if (::art::GetTid() == static_cast<uint32_t>(getpid())) {
      simulator_->SetTraceParameters(LOG_DISASM | LOG_WRITE | LOG_REGS);
    }
  }

  simulator_->SetColouredTrace(true);
  simulator_->SetDebuggerEnabled(kSimDebuggerEnabled);
}

void BasicCodeSimulatorArm64::RunFrom(intptr_t code_buffer) {
  simulator_->RunFrom(reinterpret_cast<const vixl::aarch64::Instruction*>(code_buffer));
}

bool BasicCodeSimulatorArm64::GetCReturnBool() const {
  return simulator_->ReadWRegister(0);
}

int32_t BasicCodeSimulatorArm64::GetCReturnInt32() const {
  return simulator_->ReadWRegister(0);
}

int64_t BasicCodeSimulatorArm64::GetCReturnInt64() const {
  return simulator_->ReadXRegister(0);
}

#ifdef ART_USE_SIMULATOR

//
// Special registers defined in asm_support_arm64.s.
//

// Frame Pointer.
static const unsigned kFp = 29;
// Stack Pointer.
static const unsigned kSp = 31;

class CustomSimulator final: public Simulator {
 public:
  CustomSimulator(Decoder* decoder, FILE* stream, SimStack::Allocated stack) :
       Simulator(decoder, stream, std::move(stack)) {
    // Setup all C++ entrypoint functions to be intercepted.
    RegisterBranchInterception(artQuickResolutionTrampoline);
    RegisterBranchInterception(artQuickToInterpreterBridge);
    RegisterBranchInterception(artQuickGenericJniTrampoline);
    RegisterBranchInterception(artThrowDivZeroFromCode);
    RegisterBranchInterception(artDeliverPendingExceptionFromCode);
    RegisterBranchInterception(artContextCopyForLongJump);
    RegisterBranchInterception(artQuickProxyInvokeHandler);
    RegisterBranchInterception(artInvokeObsoleteMethod);
    RegisterBranchInterception(artMethodExitHook);
    RegisterBranchInterception(artAllocArrayFromCodeResolvedRosAlloc);
    RegisterBranchInterception(artTestSuspendFromCode);
    RegisterBranchInterception(artAllocObjectFromCodeInitializedRosAlloc);
    RegisterBranchInterception(artAllocObjectFromCodeResolvedRosAlloc);
    RegisterBranchInterception(artResolveTypeFromCode);
    RegisterBranchInterception(artThrowClassCastExceptionForObject);
    RegisterBranchInterception(artInstanceOfFromCode);
    RegisterBranchInterception(artThrowArrayBoundsFromCode);
    RegisterBranchInterception(artThrowNullPointerExceptionFromCode);
    RegisterBranchInterception(artThrowStringBoundsFromCode);
    RegisterBranchInterception(artDeoptimizeFromCompiledCode);
    RegisterBranchInterception(artResolveTypeAndVerifyAccessFromCode);
    RegisterBranchInterception(artIsAssignableFromCode);
    RegisterBranchInterception(artThrowArrayStoreException);
    RegisterBranchInterception(artInitializeStaticStorageFromCode);
    RegisterBranchInterception(artResolveStringFromCode);
    RegisterBranchInterception(artAllocObjectFromCodeWithChecksRosAlloc);
    RegisterBranchInterception(artInvokePolymorphic);
    RegisterBranchInterception(artLockObjectFromCode);
    RegisterBranchInterception(artUnlockObjectFromCode);
    RegisterBranchInterception(artDeliverExceptionFromCode);
    RegisterBranchInterception(artStringBuilderAppend);
    RegisterBranchInterception(fmodf);
    RegisterBranchInterception(fmod);
    RegisterBranchInterception(free);
    RegisterBranchInterception(artAllocArrayFromCodeResolvedRosAllocInstrumented);
    RegisterBranchInterception(artAllocObjectFromCodeInitializedRosAllocInstrumented);
    RegisterBranchInterception(artAllocObjectFromCodeWithChecksRosAllocInstrumented);
    RegisterBranchInterception(artAllocObjectFromCodeResolvedRosAllocInstrumented);
    RegisterBranchInterception(artResolveTypeAndVerifyAccessFromCode);
    RegisterBranchInterception(artGetByteStaticFromCompiledCode);
    RegisterBranchInterception(artGetCharStaticFromCompiledCode);
    RegisterBranchInterception(artGet32StaticFromCompiledCode);
    RegisterBranchInterception(artGet64StaticFromCompiledCode);
    RegisterBranchInterception(artGetObjStaticFromCompiledCode);
    RegisterBranchInterception(artGetByteInstanceFromCompiledCode);
    RegisterBranchInterception(artGetCharInstanceFromCompiledCode);
    RegisterBranchInterception(artGet32InstanceFromCompiledCode);
    RegisterBranchInterception(artGet64InstanceFromCompiledCode);
    RegisterBranchInterception(artGetObjInstanceFromCompiledCode);
    RegisterBranchInterception(artSet8StaticFromCompiledCode);
    RegisterBranchInterception(artSet16StaticFromCompiledCode);
    RegisterBranchInterception(artSet32StaticFromCompiledCode);
    RegisterBranchInterception(artSet64StaticFromCompiledCode);
    RegisterBranchInterception(artSetObjStaticFromCompiledCode);
    RegisterBranchInterception(artSet8InstanceFromCompiledCode);
    RegisterBranchInterception(artSet16InstanceFromCompiledCode);
    RegisterBranchInterception(artSet32InstanceFromCompiledCode);
    RegisterBranchInterception(artSet64InstanceFromCompiledCode);
    RegisterBranchInterception(artSetObjInstanceFromCompiledCode);
    RegisterBranchInterception(artResolveMethodHandleFromCode);
    RegisterBranchInterception(artResolveMethodTypeFromCode);
    RegisterBranchInterception(artAllocStringObjectRosAlloc);
    RegisterBranchInterception(artDeoptimizeIfNeeded);
    RegisterBranchInterception(artInvokeCustom);
    RegisterBranchInterception(artAllocStringFromBytesFromCodeRosAlloc);
    RegisterBranchInterception(artAllocStringFromCharsFromCodeRosAlloc);
    RegisterBranchInterception(artAllocStringFromStringFromCodeRosAlloc);
    RegisterBranchInterception(artThrowNullPointerExceptionFromSignal);
    RegisterBranchInterception(artDeoptimize);
    RegisterBranchInterception(artReadBarrierMark);
    RegisterBranchInterception(artHandleFillArrayDataFromCode);
    RegisterBranchInterception(artIsAssignableFromCode);
    RegisterBranchInterception(artThrowArrayStoreException);
    RegisterBranchInterception(artThrowStackOverflowFromCode);
    RegisterBranchInterception(artCompileOptimized);
    RegisterBranchInterception(artAllocStringObjectRosAllocInstrumented);
    RegisterBranchInterception(artMethodEntryHook);

    // ART has a number of math entrypoints which operate on double type (see
    // quick_entrypoints_list.h, entrypoints_init_arm64.cc); we need to intercept C functions
    // called from those EPs.
    //
    // The C library provides function implementations for both double and float, so we have
    // to explicitly choose the type for the interception templates - double.
    RegisterBranchInterception<double, double>(cos);
    RegisterBranchInterception<double, double>(sin);
    RegisterBranchInterception<double, double>(acos);
    RegisterBranchInterception<double, double>(asin);
    RegisterBranchInterception<double, double>(atan);
    RegisterBranchInterception<double, double, double>(atan2);
    RegisterBranchInterception<double, double, double>(pow);
    RegisterBranchInterception<double, double>(cbrt);
    RegisterBranchInterception<double, double>(cosh);
    RegisterBranchInterception<double, double>(exp);
    RegisterBranchInterception<double, double>(expm1);
    RegisterBranchInterception<double, double, double>(hypot);
    RegisterBranchInterception<double, double>(log);
    RegisterBranchInterception<double, double>(log10);
    RegisterBranchInterception<double, double, double>(nextafter);
    RegisterBranchInterception<double, double>(sinh);
    RegisterBranchInterception<double, double>(tan);
    RegisterBranchInterception<double, double>(tanh);

    // Nterp methods.
    RegisterBranchInterception(NterpGetShorty);
    RegisterBranchInterception(NterpGetShortyFromMethodId);
    RegisterBranchInterception(NterpGetShortyFromInvokePolymorphic);
    RegisterBranchInterception(NterpGetShortyFromInvokeCustom);
    RegisterBranchInterception(NterpGetMethod);
    RegisterBranchInterception(NterpGetStaticField);
    RegisterBranchInterception(NterpGetInstanceFieldOffset);
    RegisterBranchInterception(NterpGetClass);
    RegisterBranchInterception(NterpAllocateObject);
    RegisterBranchInterception(NterpLoadObject);
    RegisterBranchInterception(NterpFilledNewArray);
    RegisterBranchInterception(NterpFilledNewArrayRange);
    RegisterBranchInterception(NterpHotMethod);
    RegisterBranchInterception(NterpDoPackedSwitch);
    RegisterBranchInterception(NterpDoSparseSwitch);
    RegisterBranchInterception(NterpFree);
    RegisterBranchInterception(NterpFmod);
    RegisterBranchInterception(NterpFmodF);

    RegisterTwoWordReturnInterception(artInvokeSuperTrampolineWithAccessCheck);
    RegisterTwoWordReturnInterception(artInvokeStaticTrampolineWithAccessCheck);
    RegisterTwoWordReturnInterception(artInvokeInterfaceTrampoline);
    RegisterTwoWordReturnInterception(artInvokeVirtualTrampolineWithAccessCheck);
    RegisterTwoWordReturnInterception(artInvokeDirectTrampolineWithAccessCheck);
    RegisterTwoWordReturnInterception(artInvokeInterfaceTrampolineWithAccessCheck);

    RegisterBranchInterception(artArm64SimulatorGenericJNIPlaceholder,
                               [this]([[maybe_unused]] uint64_t addr)
                               REQUIRES_SHARED(Locks::mutator_lock_) {
      uint64_t native_code_ptr = static_cast<uint64_t>(ReadXRegister(0));
      ArtMethod** simulated_reserved_area = reinterpret_cast<ArtMethod**>(ReadXRegister(1));
      Thread* self = reinterpret_cast<Thread*>(ReadXRegister(2));

      uint64_t fp_result = 0.0;
      int64_t gpr_result = artQuickGenericJniTrampolineSimulator(
          native_code_ptr,
          reinterpret_cast<void*>(simulated_reserved_area),
          reinterpret_cast<void*>(&fp_result));

      jvalue jval;
      jval.j = gpr_result;
      uint64_t result_end = artQuickGenericJniEndTrampoline(self, jval, fp_result);

      WriteXRegister(0, result_end);
      WriteDRegister(0, bit_cast<double>(result_end));
    });
  }

  virtual ~CustomSimulator() {}

  uint8_t* GetStackBase() {
    return reinterpret_cast<uint8_t*>(memory_.GetStack().GetBase());
  }


  // TODO(Simulator): Maybe integrate these into vixl?
  int64_t get_sp() const {
    return ReadRegister<int64_t>(kSp, Reg31IsStackPointer);
  }

  int64_t get_x(int32_t n) const {
    return ReadRegister<int64_t>(n, Reg31IsStackPointer);
  }

  int64_t get_lr() const {
    return ReadRegister<int64_t>(kLinkRegCode);
  }

  int64_t get_fp() const {
    return ReadXRegister(kFp);
  }

  // Register a branch interception to a function which returns TwoWordReturn. VIXL does not
  // currently support returning composite types from runtime calls so this is a specialised case.
  template <typename... P>
  void RegisterTwoWordReturnInterception(TwoWordReturn (*func)(P...)) {
    RegisterBranchInterception(reinterpret_cast<void (*)()>(func),
                               [this, func]([[maybe_unused]] uint64_t addr) {
      ABI abi;
      std::tuple<P...> arguments{
          ReadGenericOperand<P>(abi.GetNextParameterGenericOperand<P>())...};

      TwoWordReturn res = DoRuntimeCall(func, arguments, __local_index_sequence_for<P...>{});

      // Method pointer.
      WriteXRegister(0, res.lo);
      // Code pointer.
      WriteXRegister(1, res.hi);
    });
  }
};

CodeSimulatorArm64* CodeSimulatorArm64::CreateCodeSimulatorArm64(size_t stack_size) {
  if (kCanSimulate) {
    CodeSimulatorArm64* simulator = new CodeSimulatorArm64;
    simulator->InitInstructionSimulator(stack_size);

    return simulator;
  } else {
    return nullptr;
  }
}

CodeSimulatorArm64::CodeSimulatorArm64() : BasicCodeSimulatorArm64() {}

CustomSimulator* CodeSimulatorArm64::GetSimulator() {
  return reinterpret_cast<CustomSimulator*>(simulator_.get());
}

Simulator* CodeSimulatorArm64::CreateNewInstructionSimulator(SimStack::Allocated&& stack) {
  return new CustomSimulator(decoder_.get(), stdout, std::move(stack));
}

void CodeSimulatorArm64::Invoke(ArtMethod* method,
                                uint32_t* args,
                                uint32_t args_size_in_bytes,
                                Thread* self,
                                JValue* result,
                                const char* shorty,
                                bool isStatic)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  // ARM64 simulator only supports 64-bit host machines. Because:
  //   1) vixl simulator is not tested on 32-bit host machines.
  //   2) Data structures in ART have different representations for 32/64-bit machines.
  DCHECK(sizeof(args) == sizeof(int64_t));

  if (VLOG_IS_ON(simulator)) {
    VLOG(simulator) << "\nVIXL_SIMULATOR simulate: " << method->PrettyMethod();
  }

  /*  extern "C"
   *     void art_quick_invoke_static_stub(ArtMethod *method,   x0
   *                                       uint32_t  *args,     x1
   *                                       uint32_t argsize,    w2
   *                                       Thread *self,        x3
   *                                       JValue *result,      x4
   *                                       char   *shorty);     x5 */
  CustomSimulator* simulator = GetSimulator();
  size_t arg_no = 0;
  simulator->WriteXRegister(arg_no++, reinterpret_cast<uint64_t>(method));
  simulator->WriteXRegister(arg_no++, reinterpret_cast<uint64_t>(args));
  simulator->WriteWRegister(arg_no++, args_size_in_bytes);
  simulator->WriteXRegister(arg_no++, reinterpret_cast<uint64_t>(self));
  simulator->WriteXRegister(arg_no++, reinterpret_cast<uint64_t>(result));
  simulator->WriteXRegister(arg_no++, reinterpret_cast<uint64_t>(shorty));

  // The simulator will stop (and return from RunFrom) when it encounters pc == 0.
  simulator->WriteLr(0);

  int64_t quick_code;

  if (isStatic) {
    quick_code = reinterpret_cast<int64_t>(GetQuickInvokeStaticStub());
  } else {
    quick_code = reinterpret_cast<int64_t>(GetQuickInvokeStub());
  }

  DCHECK_NE(quick_code, 0);
  RunFrom(quick_code);
}

void CodeSimulatorArm64::DoOsr(void** stack,
                               size_t stack_size_in_bytes,
                               const uint8_t* native_pc,
                               JValue* result,
                               const char* shorty,
                               Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  /*  extern "C"
   *     void art_quick_osr_stub(void** stack,                x0
   *                             size_t stack_size_in_bytes,  x1
   *                             const uint8_t* native_pc,    x2
   *                             JValue* result,              x3
   *                             const char* shorty,          x4
   *                             Thread* self);               x5 */
  CustomSimulator* simulator = GetSimulator();
  size_t arg_no = 0;
  simulator->WriteXRegister(arg_no++, reinterpret_cast<uint64_t>(stack));
  simulator->WriteXRegister(arg_no++, stack_size_in_bytes);
  simulator->WriteXRegister(arg_no++, reinterpret_cast<uint64_t>(native_pc));
  simulator->WriteXRegister(arg_no++, reinterpret_cast<uint64_t>(result));
  simulator->WriteXRegister(arg_no++, reinterpret_cast<uint64_t>(shorty));
  simulator->WriteXRegister(arg_no++, reinterpret_cast<uint64_t>(self));

  // The simulator will stop (and return from RunFrom) when it encounters pc == 0.
  simulator->WriteLr(0);

  int64_t quick_code = reinterpret_cast<int64_t>(GetQuickOsrStub());

  DCHECK_NE(quick_code, 0);
  RunFrom(quick_code);
}

int64_t CodeSimulatorArm64::GetStackPointer() {
  return GetSimulator()->get_sp();
}

uint8_t* CodeSimulatorArm64::GetStackBaseInternal() {
  return GetSimulator()->GetStackBase();
}

uintptr_t CodeSimulatorArm64::GetPc() {
  return reinterpret_cast<uintptr_t>(GetSimulator()->ReadPc());
}

#ifdef __x86_64__
//
// Simulator Fault Handlers.
//
// These fault handlers are based on their respective Arm64 fault handlers and should remain
// aligned with those functions. This alignment is because all implicit check fault handlers are
// called from and return to quick code and so should be aligned with the kRuntimeQuickCodeISA
// fault handler, which in the case of the simulator is Arm64.
//
// In general these fault handlers should mirror their respective Arm64 fault handlers except in
// the following ways:
//   - There is an additional check that the fault came from the simulator.
//   - If the faulting address is needed, it is first replaced by the actual address by the
//     simulator. For more details see vixl::aarch64::Simulator::ReplaceFaultAddress.
//   - Native registers in the context are replaced with their equivalent simulated registers.
//   - The native context is set up to return to the simulator.
//

// This handler is based on the Arm64 NullPointerHandler::Action and should remain aligned with
// that function.
bool CodeSimulatorArm64::HandleNullPointer([[maybe_unused]] int sig,
                                           siginfo_t* siginfo,
                                           void* context) {
  CustomSimulator* sim = GetSimulator();

  // Did the signal come from the simulator?
  ucontext_t* uc = reinterpret_cast<ucontext_t*>(context);
  uintptr_t fault_pc = uc->uc_mcontext.gregs[REG_RIP];
  if (!sim->IsSimulatedMemoryAccess(fault_pc)) {
    return false;
  }

  // If we use siginfo->si_addr we need to ensure it is at the correct address as the address
  // reported by the kernel could be wrong due to how VIXL probes memory for implicit checks.
  sim->ReplaceFaultAddress(siginfo, context);
  uintptr_t fault_address = reinterpret_cast<uintptr_t>(siginfo->si_addr);
  if (!NullPointerHandler::IsValidFaultAddress(fault_address)) {
    return false;
  }

  // For null checks in compiled code we insert a stack map that is immediately
  // after the load/store instruction that might cause the fault and we need to
  // pass the return PC to the handler. For null checks in Nterp, we similarly
  // need the return PC to recognize that this was a null check in Nterp, so
  // that the handler can get the needed data from the Nterp frame.

  ArtMethod** sp = reinterpret_cast<ArtMethod**>(sim->get_sp());
  uintptr_t return_pc = reinterpret_cast<uintptr_t>(sim->ReadPc()->GetNextInstruction());
  if (!NullPointerHandler::IsValidMethod(*sp) ||
      !NullPointerHandler::IsValidReturnPc(sp, return_pc)) {
    return false;
  }

  // Push the return PC to the stack and pass the fault address in LR.
  sim->WriteSp(sim->get_sp() - sizeof(uintptr_t));
  *reinterpret_cast<uintptr_t*>(sim->get_sp()) = return_pc;
  sim->WriteLr(fault_address);

  // Return to the VIXL memory access continuation point, which is also the next instruction, after
  // this handler.
  uc->uc_mcontext.gregs[REG_RIP] = sim->GetSignalReturnAddress();
  // Return that the memory access failed.
  uc->uc_mcontext.gregs[REG_RAX] = static_cast<greg_t>(MemoryAccessResult::Failure);
  // Set the address where we want to continue simulating.
  sim->WritePc(reinterpret_cast<const vixl::aarch64::Instruction*>(
      GetQuickThrowNullPointerExceptionFromSignal()));

  VLOG(signals) << "Generating null pointer exception";
  return true;
}

// This handler is based on the Arm64 StackOverflowHandler::Action and should remain aligned with
// that function.
bool CodeSimulatorArm64::HandleStackOverflow([[maybe_unused]] int sig,
                                             siginfo_t* siginfo,
                                             void* context) {
  CustomSimulator* sim = GetSimulator();

  ucontext_t* uc = reinterpret_cast<ucontext_t*>(context);
  mcontext_t* mc = reinterpret_cast<mcontext_t*>(&uc->uc_mcontext);
  VLOG(signals) << "stack overflow handler with sp at " << std::hex << &uc;
  VLOG(signals) << "sigcontext: " << std::hex << mc;

  uintptr_t fault_pc = uc->uc_mcontext.gregs[REG_RIP];
  if (!sim->IsSimulatedMemoryAccess(fault_pc)) {
    return false;
  }

  uintptr_t sp = sim->get_sp();
  VLOG(signals) << "sp: " << std::hex << sp;

  // If we use siginfo->si_addr we need to ensure it is at the correct address as the address
  // reported by the kernel could be wrong due to how VIXL probes memory for implicit checks.
  sim->ReplaceFaultAddress(siginfo, context);
  uintptr_t fault_addr = reinterpret_cast<uintptr_t>(siginfo->si_addr);
  VLOG(signals) << "fault_addr: " << std::hex << fault_addr;
  VLOG(signals) << "checking for stack overflow, sp: " << std::hex << sp <<
      ", fault_addr: " << fault_addr;

  uintptr_t overflow_addr = sp - GetStackOverflowReservedBytes(InstructionSet::kArm64);

  // Check that the fault address is the value expected for a stack overflow.
  if (fault_addr != overflow_addr) {
    VLOG(signals) << "Not a stack overflow";
    return false;
  }

  VLOG(signals) << "Stack overflow found";

  // Return to the VIXL memory access continuation point, which is also the next instruction, after
  // this handler.
  uc->uc_mcontext.gregs[REG_RIP] = sim->GetSignalReturnAddress();
  // Return that the memory access failed.
  uc->uc_mcontext.gregs[REG_RAX] = static_cast<greg_t>(MemoryAccessResult::Failure);

  // Now arrange for the signal handler to return to art_quick_throw_stack_overflow.
  // The value of LR must be the same as it was when we entered the code that
  // caused this fault. This will be inserted into a callee save frame by
  // the function to which this handler returns (art_quick_throw_stack_overflow).
  sim->WritePc(reinterpret_cast<const vixl::aarch64::Instruction*>(
      GetQuickThrowStackOverflow()));

  // The kernel will now return to the simulator.
  return true;
}
#else
bool CodeSimulatorArm64::HandleNullPointer([[maybe_unused]] int sig,
                                           [[maybe_unused]] siginfo_t* siginfo,
                                           [[maybe_unused]] void* context) {
  LOG(FATAL) << "Unreachable";
  UNREACHABLE();
}

bool CodeSimulatorArm64::HandleStackOverflow([[maybe_unused]] int sig,
                                             [[maybe_unused]] siginfo_t* siginfo,
                                             [[maybe_unused]] void* context) {
  LOG(FATAL) << "Unreachable";
  UNREACHABLE();
}
#endif

#endif

}  // namespace arm64
}  // namespace art
