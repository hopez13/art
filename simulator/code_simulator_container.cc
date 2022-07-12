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

#include <dlfcn.h>

#include "code_simulator_container.h"

#include "base/globals.h"
#include "base/logging.h"  // For VLOG.
#include "code_simulator.h"

namespace art {

CodeSimulatorContainer::CodeSimulatorContainer(InstructionSet target_isa)
    : libart_simulator_handle_(nullptr),
      entry_points_manager_(nullptr),
      target_isa_(target_isa) {
  const char* libart_simulator_so_name =
      kIsDebugBuild ? "libartd-simulator.so" : "libart-simulator.so";
  libart_simulator_handle_ = dlopen(libart_simulator_so_name, RTLD_NOW);
  // It is not a real error when libart-simulator does not exist, e.g., on target.
  if (libart_simulator_handle_ == nullptr) {
    VLOG(simulator) << "Could not load " << libart_simulator_so_name << ": " << dlerror();
  }
}

void CodeSimulatorContainer::InitEntryPointsManager() {
  using CreateSimulatorEntryPointsManagerPtr = SimulatorEntryPointsManager*(*)(InstructionSet);
  CreateSimulatorEntryPointsManagerPtr  create_qpoints_manager =
      reinterpret_cast<CreateSimulatorEntryPointsManagerPtr >(
          dlsym(libart_simulator_handle_, "CreateSimulatorEntryPointsManager"));
  CHECK(create_qpoints_manager != nullptr) <<
      "Fail to find symbol of CreateSimulatorEntryPointsManager: " <<
      dlerror();
  entry_points_manager_ = create_qpoints_manager(target_isa_);
}

BasicCodeSimulator* CodeSimulatorContainer::CreateBasicExecutor() {
  using CreateCodeSimulatorPtr = BasicCodeSimulator*(*)(InstructionSet);
  CreateCodeSimulatorPtr create_code_simulator =
      reinterpret_cast<CreateCodeSimulatorPtr>(
          dlsym(libart_simulator_handle_, "CreateBasicCodeSimulator"));
  CHECK(create_code_simulator != nullptr) << "Fail to find symbol of CreateBasicCodeSimulator: "
      << dlerror();
  return create_code_simulator(target_isa_);
}

CodeSimulator* CodeSimulatorContainer::CreateExecutor() {
  using CreateCodeSimulatorPtr = CodeSimulator*(*)(InstructionSet);
  CreateCodeSimulatorPtr create_code_simulator =
      reinterpret_cast<CreateCodeSimulatorPtr>(
          dlsym(libart_simulator_handle_, "CreateCodeSimulator"));
  CHECK(create_code_simulator != nullptr) << "Fail to find symbol of CreateCodeSimulator: "
      << dlerror();
  return create_code_simulator(target_isa_);
}

CodeSimulatorContainer::~CodeSimulatorContainer() {
#ifdef ART_USE_SIMULATOR
  // Free entrypoints manager object before closing libart-simulator because
  // CodeSimulatorContainer's destructor lives in it.
  if (entry_points_manager_ != nullptr) {
    delete entry_points_manager_;
  }
#endif
  if (libart_simulator_handle_ != nullptr) {
    dlclose(libart_simulator_handle_);
  }
}

}  // namespace art
