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

#ifndef ART_ADBCONNECTION_ADBCONNECTION_H_
#define ART_ADBCONNECTION_ADBCONNECTION_H_

#include <stdint.h>
#include <vector>
#include <limits>

#include "base/mutex.h"
#include "base/util/fd_file.h"
#include "runtime_callbacks.h"

#include "write_lock_fd.h"

namespace adbconnection {

static constexpr char kJdwpControlName[] = "\0jdwp-control";
static constexpr char kAdbConnectionThreadName = "ADB-JDWP Connection Thread";

struct AdbConnectionDebuggerController : public art::DebuggerControlCallback {
  explicit AdbConnectionDebuggerController(AdbConnection* connection) : connection_(connection) {}

  // Begin running the debugger.
  void StartDebugger();

  // The debugger should begin shutting down since the runtime is ending.
  void StopDebugger();

 private:
  AdbConnectionState* connection_;
};

struct AdbConnectionDdmCallback : public art::DdmCallback {
  explicit AdbConnectionDdmCallback(AdbConnection* connection) : connection_(connection) {}

  void DdmPublishChunk(uint32_t type,
                       const ArrayRef<const uint8_t>& data)
      REQUIRES_SHARED(Locks::mutator_lock_);

 private:
  AdbConnectionState* connection_;
};

class PacketBuilder {
 public:
  PacketBuilder() {}

 private:
  std::vector<uint8_t> data_;
};

enum class DebuggerState {
  kError,
  kUnloaded,
  kListeningUnloaded,
  kListeningLoaded,
  kLoaded,
};

struct PollResult {
  short control_sock_result_;
  short sleep_event_fd_result_;

  bool has_remote_fd_result_;
  short remote_fd_result_;

  bool has_link_fd_results_;
  short link_output_result_;
  short link_input_result_;
};

class AdbConnectionState {
 public:
  AdbConnection();
  ~AdbConnection();

  bool CanSendData();

  // Called on the listening thread to start dealing with new input. thr is used to attach the new
  // thread to the runtime.
  void BeginListening(art::Thread* self);

 private:
  PollResult WaitForData();

  AdbConnectionDebuggerController controller_;
  AdbConnectionDdmCallback ddm_callback_;

  // Eventfd used to allow the StopDebuggerThreads function to wake up speeping threads
  int sleep_event_fd_;
  int control_sock_;
  bool shutting_down_;
  bool agent_loaded_ GUARDED_BY(state_lock_);

  // Lock has level kJdwpAdbStateLock.
  art::Mutex state_lock_ ACQUIRED_AFTER(art::Locks::jni_function_table_lock_);

  DebuggerState debugger_state_;

  // The agent side of the socketpair that we use to tell the debugger server what the connection
  // file descriptors are after we've set it up. We don't use this for anything but bookkeeping.
  int debugger_sock_;

  // This holds the fd that we have to the ADB server
  AdbConnection remote_;

  // This holds the fd's we sent to the agent.
  PipeConnection link_;

  socklen_t control_addr_len_;
  union {
    sockaddr_un controlAddrUn;
    sockaddr controlAddrPlain;
  } control_addr_;

  friend struct AdbConnectionDebuggerController;
};

}  // namespace adbconnection

#endif  // ART_ADBCONNECTION_ADBCONNECTION_H_
