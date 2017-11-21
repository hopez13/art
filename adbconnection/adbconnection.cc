/*
 * Copyright (C) 2017 The Android Open Source Project
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


#include "adbconnection.h"

#include "android-base/endian.h"

#include "base/logging.h"
#include "base/macros.h"
#include "base/mutex.h"

#include "runtime-inl.h"
#include "runtime_callbacks.h"

namespace adbconnection {

static AdbConnectionState* gState;

struct AdbConnectionDebuggerController : public art::DebuggerControlCallback {
  // Begin running the debugger.
  void StartDebugger() {
    StartDebuggerThreads();
  }
  // The debugger should begin shutting down since the runtime is ending.
  void StopDebugger() {
    StopDebuggerThreads();
  }
};

struct AdbConnectionDdmCallback : public art::DdmCallback {
  void DdmPublishChunk(uint32_t type,
                       const ArrayRef<const uint8_t>& data)
      REQUIRES_SHARED(Locks::mutator_lock_) {
        // TODO
  }
};

// The plugin initialization function.
extern "C" bool ArtPlugin_Initialize() REQUIRES_SHARED(art::Locks::mutator_lock_) {
  art::Runtime* runtime = art::Runtime::Current();
  gState = new AdbConnectionState;
  CHECK(gState != nullptr);
  if (runtime->IsStarted()) {
    gState->InitLate();
  } else {
    gState->InitEarly();
  }
  return true;
}

extern "C" bool ArtPlugin_Deinitialize() {
  CHECK(gState != nullptr);
  gState->Shutdown();
  delete gState;
  return true;
}

}  // namespace adbconnection
