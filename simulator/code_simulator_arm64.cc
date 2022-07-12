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

#include "code_simulator_container.h"

using namespace vixl::aarch64;  // NOLINT(build/namespaces)

namespace art {
namespace arm64 {

BasicCodeSimulatorArm64* BasicCodeSimulatorArm64::CreateBasicCodeSimulatorArm64() {
  if (kCanSimulate) {
    BasicCodeSimulatorArm64* simulator = new BasicCodeSimulatorArm64();
    simulator->InitInstructionSimulator();
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


Simulator* BasicCodeSimulatorArm64::CreateNewInstructionSimualtor(SimStack::Allocated&& stack) {
  return new Simulator(decoder_.get(), stdout, std::move(stack));
}

void BasicCodeSimulatorArm64::InitInstructionSimulator() {
  SimStack stack_builder;
  stack_builder.SetLimitGuardSize(0x4000);
  stack_builder.SetUsableSize(0x400000);
  SimStack::Allocated stack = stack_builder.Allocate();

  simulator_.reset(CreateNewInstructionSimualtor(std::move(stack)));
  if (VLOG_IS_ON(simulator)) {
    simulator_->SetColouredTrace(true);
    simulator_->SetTraceParameters(LOG_DISASM | LOG_WRITE | LOG_REGS);
  }
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
       Simulator(decoder, stream, std::move(stack)) {}
  virtual ~CustomSimulator() {}

  uint8_t* GetStackBase() {
    return reinterpret_cast<uint8_t*>(memory_.GetStack().GetBase());
  }

  size_t GetStackSize() {
    return memory_.GetStack().GetBase() - memory_.GetStack().GetLimit();
  }

  // TODO(simulator): Maybe integrate these into vixl?
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
};

CodeSimulatorArm64* CodeSimulatorArm64::CreateCodeSimulatorArm64() {
  if (kCanSimulate) {
    CodeSimulatorArm64* simulator = new CodeSimulatorArm64;
    simulator->InitInstructionSimulator();
    return simulator;
  } else {
    return nullptr;
  }
}

CodeSimulatorArm64::CodeSimulatorArm64() : BasicCodeSimulatorArm64() {}

CustomSimulator* CodeSimulatorArm64::GetSimulator() {
  return reinterpret_cast<CustomSimulator*>(simulator_.get());
}

Simulator* CodeSimulatorArm64::CreateNewInstructionSimualtor(SimStack::Allocated&& stack) {
  return new CustomSimulator(decoder_.get(), stdout, std::move(stack));
}

uint8_t* CodeSimulatorArm64::GetStackBase() {
  return GetSimulator()->GetStackBase();
}

size_t CodeSimulatorArm64::GetStackSize() {
  return GetSimulator()->GetStackSize();
}

SimulatorEntryPointsManagerArm64*
SimulatorEntryPointsManagerArm64::CreateSimulatorEntryPointsManagerArm64() {
  if (kCanSimulate) {
    return new SimulatorEntryPointsManagerArm64();
  } else {
    return nullptr;
  }
}

SimulatorEntryPointsManagerArm64::~SimulatorEntryPointsManagerArm64() {
}

void SimulatorEntryPointsManagerArm64::InitCustomEntryPoints() {
}

void SimulatorEntryPointsManagerArm64::UpdateOthersEntryPoints(
    QuickEntryPoints* others_entry_points) const {
  // No entrypoints to update (yet).
  USE(others_entry_points);
}

#endif

}  // namespace arm64
}  // namespace art
