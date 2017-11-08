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
#include <android-base/thread_annotations.h>
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

namespace dt_fd_forward {

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
};

enum class IOResult {
  kOk, kInterrupt, kError, kEOF,
};

class PacketReader;
class PacketWriter;

// TODO It would be good to get the thread-safety analysis checks working but first we would need to
// move away from the std::mutex.
class FdForwardTransport : public jdwpTransportEnv {
 public:
  explicit FdForwardTransport(jdwpTransportCallback* cb);
  ~FdForwardTransport();

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

  bool ChangeState(TransportState old_state, TransportState new_state);  // REQUIRES(state_mutex_);

  IOResult RecieveFdsFromSocket();

  IOResult WriteFully(const void* data, size_t ndata);  // REQUIRES(!state_mutex_);
  IOResult WriteFullyWithoutChecks(const void* data, size_t ndata);  // REQUIRES(state_mutex_);
  IOResult ReadFully(void* data, size_t ndata);  // REQUIRES(!state_mutex_);
  IOResult ReadUpToMax(void* data, size_t ndata, /*out*/size_t* amount_read);
      // REQUIRES(state_mutex_);
  IOResult ReadFullyWithoutChecks(void* data, size_t ndata);  // REQUIRES(state_mutex_);

  // The allocation/deallocation functions.
  jdwpTransportCallback mem_;

  // Input from the server;
  int read_fd_;  // GUARDED_BY(state_mutex_);
  // Output to the server;
  int write_fd_; // GUARDED_BY(state_mutex_);

  // an eventfd passed with the write_fd to the transport that we will 'read' from to get a lock on
  // the write_fd_. The other side must not hold it for unbounded time.
  int write_lock_fd_;  // GUARDED_BY(state_mutex_);

  // Eventfd we will use to wake-up paused reads for close().
  int wakeup_fd_;

  // Socket we will get the read/write fd's from.
  int listen_fd_;

  TransportState state_;  // GUARDED_BY(state_mutex_);

  std::mutex state_mutex_;
  std::condition_variable state_cv_;

  // A counter that we use to make sure we don't do half a read on one and half on another fd.
  std::atomic<uint64_t> current_seq_num_;

  friend class PacketReader;  // For ReadFullyWithInterrupt
  friend class PacketWriter;  // For WriteFullyWithInterrupt
};

}  // namespace dt_fd_forward

#endif  // ART_DT_FD_FORWARD_DT_FD_FORWARD_H_
