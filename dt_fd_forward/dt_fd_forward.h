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

#ifndef ART_DT_FD_FORWARD_DT_FD_FORWARD_H_
#define ART_DT_FD_FORWARD_DT_FD_FORWARD_H_

#include <android-base/logging.h>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>

#include <arpa/inet.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <poll.h>

#include <jni.h>
#include <jvmti.h>
#include <jdwpTransport.h>

#include "fd_transport.h"

namespace dt_fds {

static constexpr uint8_t kReplyFlag = 0x80;
// Macro and constexpr to make error values less annoying to write.
#define ERR(e) JDWPTRANSPORT_ERROR_ ## e
static constexpr jdwpTransportError OK = ERR(NONE);

static constexpr const char kJdwpHandshake[14] = {
    'J', 'D', 'W', 'P', '-', 'H', 'a', 'n', 'd', 's', 'h', 'a', 'k', 'e'
};  // "JDWP-Handshake"

enum class TransportState {
  kClosed,       // Main state.
  kListenSetup,  // Transient, wait for the state to change before proceeding.
  kListening,    // Main state.
  kOpening,      // Transient, wait for the state to change before proceeding.
  kOpen,         // Main state.
  kClosing,      // Transient, wait for the state to change before proceeding.
};

enum class IOResult {
  kOk, kInterrupt, kError, kEOF,
};

class PacketReader;
class PacketWriter;

// TODO It might be useful to set the fd's to O_NONBLOCK or verify that they already are.
class DtFdsTransport : public jdwpTransportEnv {
 public:
  explicit DtFdsTransport(jdwpTransportCallback* cb);
  ~DtFdsTransport();

  jdwpTransportError PerformAttach(int listen_fd);
  jdwpTransportError SetupListen(int listen_fd);
  jdwpTransportError StopListening();

  jboolean IsOpen();

  jdwpTransportError WritePacket(const jdwpPacket* pkt);
  jdwpTransportError ReadPacket(jdwpPacket* pkt);
  jdwpTransportError Close();
  jdwpTransportError Accept();
  jdwpTransportError GetLastError(/*out*/char** description);

  void* Alloc(size_t data);
  void Free(void* data);

 private:
  void SetLastError(const std::string& desc);

  bool ChangeState(TransportState old_state, TransportState new_state);

  IOResult RecieveFdsFromSocket();

  IOResult WriteFully(const void* data, size_t ndata);
  IOResult WriteFullyWithoutChecks(const void* data, size_t ndata);
  IOResult ReadFully(void* data, size_t ndata);
  IOResult ReadUpToMax(void* data, size_t ndata, /*out*/size_t* amount_read);
  IOResult ReadFullyWithoutChecks(void* data, size_t ndata);
  // IOResult readInterruptable(/*out*/void* data, size_t ndata, /*out*/size_t* read_data_size);

  // The allocation/deallocation functions.
  jdwpTransportCallback mem_;

  // Input from the server;
  int read_fd_;
  // Output to the server;
  int write_fd_;

  // an eventfd passed with the write_fd to the transport that we will 'read' from to get a lock on
  // the write_fd_. The other side must not hold it for unbounded time.
  int write_lock_fd_;

  // Eventfd we will use to wake-up paused reads for close().
  int wakeup_fd_;

  // Socket we will get the read/write fd's from.
  int listen_fd_;

  std::atomic<uint64_t> pipe_seq_num_;

  TransportState state_;  // Protected by state_mutex_

  std::mutex state_mutex_;
  std::condition_variable state_cv_;

  // std::mutex read_mutex_;
  // std::mutex write_mutex_;

  friend class PacketReader;  // For ReadFullyWithInterrupt
  friend class PacketWriter;  // For WriteFullyWithInterrupt
};

}  // namespace dt_fds

#endif  // ART_DT_FD_FORWARD_DT_FD_FORWARD_H_
