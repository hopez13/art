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

#ifndef ART_SIMULATOR_CODE_SIMULATOR_CONTAINER_H_
#define ART_SIMULATOR_CODE_SIMULATOR_CONTAINER_H_

#include <android-base/logging.h>

#include "arch/instruction_set.h"
#include "code_simulator.h"

namespace art {

class CodeSimulator;
class SimulatorEntryPointsManager;

// This class represents a runtime simulator root concept - one per Runtime instance.
// It also acts as a container - dynamically opens and closes libart-simulator.
class CodeSimulatorContainer {
 public:
  explicit CodeSimulatorContainer(InstructionSet target_isa);
  ~CodeSimulatorContainer();

  // Creates a basic simulator executor.
  BasicCodeSimulator* CreateBasicExecutor();

  // Creates an ART runtime aware simulator executor.
  CodeSimulator* CreateExecutor();

  // Creates an EntryPointsManager for the simulator container.
  void InitEntryPointsManager();

  SimulatorEntryPointsManager* GetEntryPointsManager() { return entry_points_manager_; }

 private:
  // A handle for the libart_simulator dynamic library.
  void* libart_simulator_handle_;
  // Entrypoint manager - is used to operate with custom simulator's entrypoints.
  SimulatorEntryPointsManager* entry_points_manager_;
  InstructionSet target_isa_;
  DISALLOW_COPY_AND_ASSIGN(CodeSimulatorContainer);
};

}  // namespace art

#endif  // ART_SIMULATOR_CODE_SIMULATOR_CONTAINER_H_
