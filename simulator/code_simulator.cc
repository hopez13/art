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

#include "code_simulator.h"

#include "code_simulator_arm64.h"

namespace art {

BasicCodeSimulator* CreateBasicCodeSimulator(InstructionSet target_isa) {
  switch (target_isa) {
    case InstructionSet::kArm64:
      return arm64::BasicCodeSimulatorArm64::CreateBasicCodeSimulatorArm64();
    default:
      return nullptr;
  }
}

#ifdef ART_USE_SIMULATOR

CodeSimulator* CreateCodeSimulator(InstructionSet target_isa) {
  switch (target_isa) {
    case InstructionSet::kArm64:
      return arm64::CodeSimulatorArm64::CreateCodeSimulatorArm64();
    default:
      return nullptr;
  }
}

SimulatorEntryPointsManager::SimulatorEntryPointsManager() {
  uintptr_t* begin = reinterpret_cast<uintptr_t*>(&entry_points_);
  uintptr_t* end = reinterpret_cast<uintptr_t*>(
      reinterpret_cast<uint8_t*>(&entry_points_) + sizeof(entry_points_));
  for (uintptr_t* it = begin; it != end; ++it) {
    *it = GetUnimplementedEntryPoint();
  }

  SetStubToUnimplemented(&invoke_stub_);
  SetStubToUnimplemented(&invoke_static_stub_);
  SetStubToUnimplemented(&long_jump_stub_);
  SetStubToUnimplemented(&pending_exception_stub_);
  SetStubToUnimplemented(&deoptimize_stub_);

  SetStubToUnimplemented(&instrumentation_exit_stub_);
  SetStubToUnimplemented(&instrumentation_entry_stub_);
  SetStubToUnimplemented(&proxy_invoke_stub_);
  SetStubToUnimplemented(&invoke_obsolete_stub_);
}

SimulatorEntryPointsManager* CreateSimulatorEntryPointsManager(InstructionSet target_isa) {
  switch (target_isa) {
    case InstructionSet::kArm64:
      return arm64::SimulatorEntryPointsManagerArm64::CreateSimulatorEntryPointsManagerArm64();
    default:
      return nullptr;
  }
}

#endif

}  // namespace art
