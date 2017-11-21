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

class AdbConnectionState {
 public:
  AdbConnection();
  ~AdbConnection();

  bool CanSendData();

 private:
  AdbConnectionDebuggerController controller_;
  AdbConnectionDdmCallback ddm_callback_;

  int control_sock_ GUARDED_BY(state_lock_);
  bool shutting_down_ GUARDED_BY(state_lock_);

  // Lock has level kJdwpAdbStateLock.
  art::Mutex state_lock_ ACQUIRED_AFTER(art::Locks::jni_function_table_lock_);

  socklen_t control_addr_len_;
  union {
    sockaddr_un controlAddrUn;
    sockaddr controlAddrPlain;
  } control_addr_;

  // The local side of the socketpair that we use to tell the debugger server what the connection
  // file descriptors are.
  int local_debugger_sock_;

  // Active connection to debugger client. This is going out towards ADB
  int client_sock_;

  // An eventfd we share with the dt_fd_forward transport to control write access to the
  // client_sock_. Since we never read from it no read lock is needed.
  WriteLockFd write_lock_;
};

}  // namespace adbconnection

#endif  // ART_ADBCONNECTION_ADBCONNECTION_H_
