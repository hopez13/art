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

#ifndef ART_SIMULATOR_CODE_SIMULATOR_ARM64_H_
#define ART_SIMULATOR_CODE_SIMULATOR_ARM64_H_

#include "memory"

// TODO(VIXL): Make VIXL compile with -Wshadow.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#include "aarch64/disasm-aarch64.h"
#include "aarch64/macro-assembler-aarch64.h"
#include "aarch64/simulator-aarch64.h"
#pragma GCC diagnostic pop

#include "arch/instruction_set.h"
#include "code_simulator.h"
#include "entrypoints/quick/quick_entrypoints.h"

namespace art {
namespace arm64 {

class CustomSimulator;

class BasicCodeSimulatorArm64 : public BasicCodeSimulator {
 public:
  virtual ~BasicCodeSimulatorArm64() {}
  static BasicCodeSimulatorArm64* CreateBasicCodeSimulatorArm64();

  void RunFrom(intptr_t code_buffer) override;

  bool GetCReturnBool() const override;
  int32_t GetCReturnInt32() const override;
  int64_t GetCReturnInt64() const override;

 protected:
  BasicCodeSimulatorArm64();
  void InitInstructionSimulator();

  // We currently support only 64-bit hosts as the simulator is designed around the fact
  // that host/target pointer size is the same.
  // TODO: Enable for more host ISAs once Simulator supports them.
  static constexpr bool kCanSimulate = (kRuntimeISA == InstructionSet::kX86_64);

  std::unique_ptr<vixl::aarch64::Decoder> decoder_;
  std::unique_ptr<vixl::aarch64::Simulator> simulator_;

 private:
  virtual vixl::aarch64::Simulator* CreateNewInstructionSimualtor(
      vixl::aarch64::SimStack::Allocated&& stack);

  DISALLOW_COPY_AND_ASSIGN(BasicCodeSimulatorArm64);
};

#ifdef ART_USE_SIMULATOR

class CodeSimulatorArm64 : public CodeSimulator, public BasicCodeSimulatorArm64 {
 public:
  static CodeSimulatorArm64* CreateCodeSimulatorArm64();

  void Invoke(ArtMethod* method,
              uint32_t* args,
              uint32_t args_size,
              Thread* self,
              JValue* result,
              const char* shorty,
              bool isStatic) override REQUIRES_SHARED(Locks::mutator_lock_);

  uint8_t* GetStackBase() override;
  size_t GetStackSize() override;

 private:
  CodeSimulatorArm64();
  vixl::aarch64::Simulator* CreateNewInstructionSimualtor(
      vixl::aarch64::SimStack::Allocated&& stack) override;

  CustomSimulator* GetSimulator();

  DISALLOW_COPY_AND_ASSIGN(CodeSimulatorArm64);
};

class SimulatorEntryPointsManagerArm64 : public SimulatorEntryPointsManager {
 public:
  ~SimulatorEntryPointsManagerArm64();

  static SimulatorEntryPointsManagerArm64* CreateSimulatorEntryPointsManagerArm64();

  void UpdateOthersEntryPoints(QuickEntryPoints* others_entry_points) const override;
  void InitCustomEntryPoints() override;

  // Simulator is now only supported on x86_64.
  static constexpr bool kCanSimulate = (kRuntimeISA == InstructionSet::kX86_64);

 private:
  template <typename T>
  static void DeleteEntryPointBuffer(T entry_point) {
    uintptr_t func_raw_address = reinterpret_cast<uintptr_t>(entry_point);

    if (func_raw_address != GetUnimplementedEntryPoint()) {
      char* func_buffer = reinterpret_cast<char*>(entry_point);
      delete[] func_buffer;
    }
  }
};

#endif

}  // namespace arm64
}  // namespace art

#endif  // ART_SIMULATOR_CODE_SIMULATOR_ARM64_H_
