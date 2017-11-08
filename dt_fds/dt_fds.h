// Copyright (C) 2017 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef ART_DT_FDS_DT_FDS_H_
#define ART_DT_FDS_DT_FDS_H_

#include <android-base/logging.h>
#include <atomic>
#include <string>

#include <arpa/inet.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <poll.h>

#include <jni.h>
#include <jvmti.h>
#include <jdwpTransport.h>

namespace dt_fds {

static constexpr uint8_t kReplyFlag = 0x80;
// Macro and constexpr to make error values less annoying to write.
#define ERR(e) JDWPTRANSPORT_ERROR_ ## e
static constexpr jdwpTransportError OK = ERR(NONE);

static constexpr const char kJdwpHandshake[14] = {
    'J', 'D', 'W', 'P', '-', 'H', 'a', 'n', 'd', 's', 'h', 'a', 'k', 'e'
};  // "JDWP-Handshake"

enum class TransportState { kOpen, kListening, kClosed, };

enum class IOResult {
  kOk, kInterrupt, kError, kEOF,
};

class ScopedWriteLockHolder;
class PacketReader;
class PacketWriter;

class ScopedRawMonitorLock {
 public:
  ScopedRawMonitorLock(jvmtiEnv* env, jrawMonitorID mon) : env_(env), mon_(mon) {
    env_->RawMonitorEnter(mon_);
  }

  ~ScopedRawMonitorLock() {
    env_->RawMonitorExit(mon_);
  }

 private:
  jvmtiEnv* env_;
  jrawMonitorID mon_;
};

// TODO It might be useful to set the fd's to O_NONBLOCK or verify that they already are.
class DtFdsTransport : public jdwpTransportEnv {
 public:
  DtFdsTransport(jvmtiEnv* jvmti,
                 jrawMonitorID mon_read,
                 jrawMonitorID mon_write,
                 jdwpTransportCallback* cb);
  ~DtFdsTransport();

  jdwpTransportError PerformAttach(int sock, int accept_event_fd);
  jdwpTransportError SetupListen(int sock, int accept_event_fd);
  jdwpTransportError StopListening();

  jboolean IsOpen();

  jdwpTransportError WritePacket(const jdwpPacket* pkt);
  jdwpTransportError ReadPacket(jdwpPacket* pkt);
  jdwpTransportError Close();
  jdwpTransportError Accept();
  jdwpTransportError GetLastError(/*out*/char** description);

 private:
  void SetLastError(const std::string& desc);

  bool ChangeState(TransportState old_state, TransportState new_state) {
    return state_.compare_exchange_strong(old_state, new_state);
  }

  // Send a wakeup event to stop an paused read/write.
  void SendWakeup(int fd);
  // acknowledge the wakeup.
  void ClearWakeup(int fd);

  // Gain a lock on the sock_fd_ using control_fd_
  IOResult AcquireControlFd();
  void ReleaseControlFd();

  IOResult WriteFullyWithInterrupt(const void* data, size_t ndata, int interrupt_fd);
  IOResult ReadFullyWithInterrupt(void* data, size_t ndata, int interrupt_fd);
  // IOResult readInterruptable(/*out*/void* data, size_t ndata, /*out*/size_t* read_data_size);

 public:
  // The allocation/deallocation functions.
  jdwpTransportCallback mem_;

 private:
  // FDs used to communicate with the client.
  int sock_fd_;

  // FD used to wakeup an in-progress attach/accept. It is an eventfd.
  int attach_cancel_fd_;

  // FD used to wakeup an in-progress read/write packet. It is an eventfd.
  int write_cancel_fd_;

  // eventfd(EFD_SEMAPHORE) used to syncronize with another thread sharing the fd_sock for
  // multiplexing.  If this is set to a non-negative number then we will wrap any writes to the
  // sock_fd_ with a acquire (read) and release (write(1)).
  int control_fd_;

  // FD we will use in 'attach' to get the real communication channel.
  int listen_fd_;

  // FD we will use to communicate to the plugin that we are actively listening. It is an eventfd.
  // If it is non-zero and the plugin reads from it we will take an fd from the listen_fd_.
  int accept_event_fd_;

  std::atomic<TransportState> state_;

  jvmtiEnv* jvmti_;

  jrawMonitorID read_monitor_;
  jrawMonitorID write_monitor_;

  friend class ScopedWriteLockHolder;  // For Acquire/ReleaseControlFd
  friend class PacketReader;  // For ReadFullyWithInterrupt
  friend class PacketWriter;  // For WriteFullyWithInterrupt
};

class ScopedWriteLockHolder {
 public:
  ScopedWriteLockHolder(DtFdsTransport* t, /*out*/IOResult* result);
  ~ScopedWriteLockHolder();

 private:
  DtFdsTransport* transport_;
  bool locked_;
};

}  // namespace dt_fds

#endif  // ART_DT_FDS_DT_FDS_H_
