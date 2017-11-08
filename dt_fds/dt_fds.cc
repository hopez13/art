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
//

#include <android-base/logging.h>
#include <atomic>
#include <string>

#include <arpa/inet.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <poll.h>

#include <jni.h>
#include <jdwpTransport.h>

namespace dt_fds {

// TODO SOCK_SEQPACKET AF_LOCAL socketpair.
extern const jdwpTransportNativeInterface_ gTransportInterface;

// Macro and constexpr to make error values less annoying to write.
#define ERR(e) JDWPTRANSPORT_ERROR_ ## e
static constexpr jdwpTransportError OK = ERR(NONE);

static constexpr const char* kJdwpHandshake = "JDWP-Handshake";
static constexpr size_t kJdwpHandshakeLen = sizeof(kJdwpHandshake) - 1;

enum class TransportState {
  kOpen, kAttaching, kListenSetup, kListening, kClosed,
};

enum class IOResult {
  kWritten, kIntterupt, kError,
};

template <typename T> static T HostToNetwork(T in);
template <typename T> static T NetworkToHost(T in);

template<> static uint8_t HostToNetwork(uint8_t in) { return in; }
template<> static uint8_t NetworkToHost(uint8_t in) { return in; }
template<> static int8_t HostToNetwork(int8_t in) { return in; }
template<> static int8_t NetworkToHost(int8_t in) { return in; }

template<> static uint16_t HostToNetwork(uint16_t in) { return htons(in); }
template<> static uint16_t NetworkToHost(uint16_t in) { return ntohs(in); }
template<> static int16_t HostToNetwork(int16_t in) {
  return reinterpret_cast<int16_t>(HostToNetwork(reinterpret_cast<uint16_t>(in)));
}
template<> static int16_t NetworkToHost(int16_t in) {
  return reinterpret_cast<int16_t>(NetworkToHost(reinterpret_cast<uint16_t>(in)));
}
template<> static uint32_t HostToNetwork(uint32_t in) { return htonl(in); }
template<> static uint32_t NetworkToHost(uint32_t in) { return ntohl(in); }
template<> static int32_t HostToNetwork(int32_t in) {
  return reinterpret_cast<int32_t>(HostToNetwork(reinterpret_cast<uint32_t>(in)));
}
template<> static int32_t NetworkToHost(int32_t in) {
  return reinterpret_cast<int32_t>(NetworkToHost(reinterpret_cast<uint32_t>(in)));
}

// TODO It might be useful to set the fd's to O_NONBLOCK or verify that they already are.
struct DtFdsTransport : public jdwpTransportEnv {
  DtFdsTransport(JavaVM* vm ATTRIBUTE_UNUSED, jdwpTransportCallback* cb)
      : mem_(*cb),
        fd_sock_(-1),
        wakeup_fd_(eventfd(0, EFD_NONBLOCK)),
        state_(TransportState::kClosed) { }

  ~DtFdsTransport() {
    CHECK(state_ == TransportState::kClosed);
    close(wakeup_fd_);
    wakeup_fd_ = -1;
  }

  bool ChangeState(TransportState old, TransportState new_state) {
    return state_.compare_exchange_strong(old, new_state);
  }
  // Returns true if data was written successfully. False if it was interrupted
  IOResult writeInterruptable(const void* data, size_t ndata) {
    constexpr int nfds = 2;
    constexpr int write_idx = 0;
    constexpr int event_idx = 1;
    struct pollfd pollfds[nfds];
    while (true) {
      pollfds[write_idx] = { fd_sock_, POLLOUT, 0 };
      pollfds[event_idx] = { wakeup_fd_, POLLIN, 0 };
      int retval = poll(pollfds, nfds, 0);

      if (retval < 0) {
        return IOResult::kError;
      } else if ((pollfds[event_idx].revents & POLLIN) == POLLIN) {
        // we got a wakeup. Clear it and return. The caller can retry if it's not important.
        ClearWakeup();
        return IOResult::kIntterupt;
      } else if (state_ == TransportState::kClosed) {
        return IOResult::kIntterupt;
      } else if ((pollfds[write_idx].revents & POLLOUT) == POLLOUT) {
        int size;
        if ((size = TEMP_FAILURE_RETRY(write(fd_sock_, data, ndata))) < 0) {
          // partial write or failure.
          return IOResult::kError;
        } else {
          return ndata == static_cast<size_t>(size) ? IOResult::kWritten : IOResult::kError;
        }
      }
    }
  }

  template <typename T>
  IOResult readInterruptable(/*out*/T* data, size_t ndata, /*out*/size_t* read_data_size) {
    // TODO Deal with partial reads.
    DCHECK(read_data_size != nullptr);
    constexpr int nfds = 2;
    constexpr int read_idx = 0;
    constexpr int event_idx = 1;
    struct pollfd pollfds[nfds];
    while (true) {
      pollfds[read_idx] = { fd_sock_, POLLIN, 0 };
      pollfds[event_idx] = { wakeup_fd_, POLLIN, 0 };
      int read_fd = fd_sock_;
      int retval = poll(pollfds, nfds, 0);

      if (retval < 0) {
        return IOResult::kError;
      } else if ((pollfds[event_idx].revents & POLLIN) == POLLIN) {
        // we got a wakeup. Clear it and return. The caller can retry if it's not important.
        ClearWakeup();
        return IOResult::kIntterupt;
      } else if (state_ == TransportState::kClosed) {
        return IOResult::kIntterupt;
      } else if ((pollfds[read_idx].revents & POLLIN) == POLLIN) {
        int read_size;
        if ((read_size = TEMP_FAILURE_RETRY(read(read_fd, data, ndata))) < 0) {
          // error occurred.
          return IOResult::kError;
        } else {
          *read_data_size = static_cast<size_t>(read_size);
          return IOResult::kWritten;
        }
      }
    }
  }

  jdwpTransportError PerformAttach(int sock) {
    if (!ChangeState(TransportState::kClosed, TransportState::kAttaching)) {
      return ERR(ILLEGAL_STATE);
    }
    while (state_ == TransportState::kAttaching) {
      fd_sock_ = read;
      IOResult res = writeInterruptable(kJdwpHandshake, kJdwpHandshakeLen);
      if (res == IOResult::kError) {
        return ERR(IO_ERROR);
      } else if (res == IOResult::kIntterupt) {
        continue;
      } else {
        break;
      }
    }
    while (state_ == TransportState::kAttaching) {
      char handshake_res[kJdwpHandshakeLen];
      size_t read_size = 0;
      IOResult res = readInterruptable(handshake_res, sizeof(handshake_res), &read_size);
      if (res == IOResult::kIntterupt) {
        // Check if we got closed.
        continue;
      }
      if (res == IOResult::kError ||
          read_size != strlen(handshake_res) ||
          strncmp(handshake_res, kJdwpHandshake, kJdwpHandshakeLen) != 0) {
        return ERR(IO_ERROR);
      }
      // We connected. Change the state.
      ChangeState(TransportState::kAttaching, TransportState::kOpen);
      return OK;
    }
    return ERR(IO_ERROR);
  }

  jdwpTransportError SetupListen(int sock) {
    if (!ChangeState(TransportState::kClosed, TransportState::kListenSetup)) {
      return ERR(ILLEGAL_STATE);
    }
    fd_sock_ = sock;
    ChangeState(TransportState::kListenSetup, TransportState::kListening);
    return OK;
  }

  jdwpTransportError StopListening() {
    if (state_ != TransportState::kListenSetup &&
        state_ != TransportState::kListening) {
      return ERR(ILLEGAL_STATE);
    }
    while (state_ == TransportState::kListenSetup) {
      // Let other thread finish listen setup.
      sched_yield();
    }
    if (!ChangeState(TransportState::kListening, TransportState::kListenSetup)) {
      return ERR(ILLEGAL_STATE);
    }
    fd_sock_ = -1;
    // Move to closed
    ChangeState(TransportState::kListenSetup, TransportState::kClosed);
    // wakeup any sleeping attach calls.
    SendWakeup();
    return OK;
  }

  jdwpTransportError Accept() {
    while (state_ == TransportState::kListenSetup) {
      // Let other thread finish listen setup.
      sched_yield();
    }
    while (state_ == TransportState::kListening) {
      size_t read_size = 0;
      char handshake_res[kJdwpHandshakeLen];
      IOResult res = readInterruptable(handshake_res, kJdwpHandshakeLen, &read_size);
      if (res == IOResult::kIntterupt) {
        continue;
      } else if (res == IOResult::kError ||
                 read_size != kJdwpHandshakeLen ||
                 strncmp(handshake_res, kJdwpHandshake, kJdwpHandshakeLen) != 0) {
        return ERR(IO_ERROR);
      } else {
        break;
      }
    }
    while (state_ == TransportState::kAttaching) {
      IOResult res = writeInterruptable(kJdwpHandshake, kJdwpHandshakeLen);
      if (res == IOResult::kIntterupt) {
        // Check if we got closed.
        continue;
      } else if (res == IOResult::kError) {
        return ERR(IO_ERROR);
      }
      // We connected. Change the state.
      ChangeState(TransportState::kAttaching, TransportState::kOpen);
      return OK;
    }
    return ERR(IO_ERROR);
  }

  jdwpTransportError Close() {
    state_ = TransportState::kClosed;
    SendWakeup();
    return OK;
  }

  jdwpTransportError ReadPacket(jdwpPacket* pkt) {
    if (state_ != TransportState::kOpen) {
      return ERR(ILLEGAL_STATE);
    }
    memset(pkt, 0, sizeof(jdwpPacket));
    while (state_ == TransportState::kOpen) {
      size_t size = 0;
      uint32_t len = 0;
      IOResult res = readInterruptable(&len, 1, &size);
      // Convert to host long.
      len = ntohl(len);
      if (res == IOResult::kIntterupt) {
        // Try again.
        continue;
      } else if (res == IOResult::kError) {
        return ERR(IO_ERROR);
      } else if (size == 0) {
        // EOF
        return OK;
      } else if (size < sizeof(len) || len < 11) {
        return ERR(IO_ERROR);
      } else {

        // We can no longer be interrupted until the reads finish.
        return OK;
      }
    }
    return ERR(IO_ERROR);
    // TODO
  }

  jdwpTransportError WritePacket(const jdwpPacket* pkt ATTRIBUTE_UNUSED) {
    return OK;
    // TODO
  }

  void SendWakeup() {
    uint64_t data = 1;
    CHECK(write(wakeup_fd_, &data, sizeof(data)) > 0);
  }

  void ClearWakeup() {
    uint64_t data = 0;
    CHECK(read(wakeup_fd_, &data, sizeof(data)) > 0);
  }

  // The allocation/deallocation functions.
  jdwpTransportCallback mem_;

  // FDs used to communicate with the client.
  int fd_sock_;

  // FD used to wakeup an in-progress attach/accept. It is an eventfd (EFD_SEMAPHORE).
  int wakeup_fd_;

  std::atomic<TransportState> state_;
};

static DtFdsTransport* AsDtFds(jdwpTransportEnv* env) {
  return reinterpret_cast<DtFdsTransport*>(env);
}

static jdwpTransportError ParseAddress(const std::string& addr,
                                       /*out*/int* sock_fd) {
  const char* numbers = "1234567890";
  if (addr.find_first_not_of(numbers) != std::string::npos) {
    return ERR(ILLEGAL_ARGUMENT);
  }
  *sock_fd = std::stoi(addr, 0, 10);
  return OK;
}

class JdwpTransportFunctions {
 public:
  static jdwpTransportError GetCapabilities(jdwpTransportEnv* env ATTRIBUTE_UNUSED,
                                            /*out*/ JDWPTransportCapabilities* capabilities_ptr) {
    // We don't support any of the optional capabilities (can_timeout_attach, can_timeout_accept,
    // can_timeout_handshake) so just return a zeroed capabilities ptr.
    memset(capabilities_ptr, 0, sizeof(JDWPTransportCapabilities));
    return OK;
  }

  // Address is <read_FD>:<write_FD>
  static jdwpTransportError Attach(jdwpTransportEnv* env,
                                   const char* address,
                                   jlong attach_timeout ATTRIBUTE_UNUSED,
                                   jlong handshake_timeout ATTRIBUTE_UNUSED) {
    int sock_fd_num;
    jdwpTransportError err = ParseAddress(address, &sock_fd_num);
    if (err != OK) {
      return err;
    }
    return AsDtFds(env)->PerformAttach(sock_fd_num);
  }

  static jdwpTransportError StartListening(jdwpTransportEnv* env,
                                           const char* address,
                                           /*out*/ char** actual_address) {
    int sock_fd_num;
    jdwpTransportError err = ParseAddress(address, &sock_fd_num);
    if (err != OK) {
      return err;
    }
    err = AsDtFds(env)->SetupListen(read_fd_num, write_fd_num);
    if (err != OK) {
      return err;
    }
    if (actual_address != nullptr) {
      *actual_address = reinterpret_cast<char*>(AsDtFds(env)->mem_.alloc(strlen(address) + 1));
      memcpy(*actual_address, address, strlen(address) + 1);
    }
    return OK;
  }

  static jdwpTransportError StopListening(jdwpTransportEnv* env) {
    return AsDtFds(env)->StopListening();
  }

  static jdwpTransportError Accept(jdwpTransportEnv* env,
                                   jlong accept_timeout ATTRIBUTE_UNUSED,
                                   jlong handshake_timeout ATTRIBUTE_UNUSED) {
    return AsDtFds(env)->Accept();
  }

  static jboolean IsOpen(jdwpTransportEnv* env) {
    return AsDtFds(env)->state_ == TransportState::kOpen;
  }

  static jdwpTransportError Close(jdwpTransportEnv* env) {
    return AsDtFds(env)->Close();
  }

  static jdwpTransportError ReadPacket(jdwpTransportEnv* env, jdwpPacket *pkt) {
    return AsDtFds(env)->ReadPacket(pkt);
  }

  static jdwpTransportError WritePacket(jdwpTransportEnv* env, const jdwpPacket* pkt) {
    return AsDtFds(env)->WritePacket(pkt);
  }

  static jdwpTransportError GetLastError(jdwpTransportEnv* env ATTRIBUTE_UNUSED,
                                         char** error ATTRIBUTE_UNUSED) {
    // TODO Actually implement this? The tls requirements make it somewhat annoying.
    return ERR(MSG_NOT_AVAILABLE);
  }
};

// The actual struct holding all the entrypoints into the jdwpTransport interface.
const jdwpTransportNativeInterface_ gTransportInterface = {
  nullptr,  // reserved1
  JdwpTransportFunctions::GetCapabilities,
  JdwpTransportFunctions::Attach,
  JdwpTransportFunctions::StartListening,
  JdwpTransportFunctions::StopListening,
  JdwpTransportFunctions::Accept,
  JdwpTransportFunctions::IsOpen,
  JdwpTransportFunctions::Close,
  JdwpTransportFunctions::ReadPacket,
  JdwpTransportFunctions::WritePacket,
  JdwpTransportFunctions::GetLastError,
};

JNIEXPORT jint JNICALL jdwpTransport_OnLoad(JavaVM* vm,
                                            jdwpTransportCallback* cb,
                                            jint version,
                                            jdwpTransportEnv** /*out*/env) {
  if (version != JDWPTRANSPORT_VERSION_1_0) {
    LOG(ERROR) << "unknown version " << version;
    return JNI_ERR;
  }
  DtFdsTransport* transport = new (cb->alloc(sizeof(DtFdsTransport))) DtFdsTransport(vm, cb);
  if (transport->wakeup_fd_ < 0) {
    PLOG(ERROR) << "Failed to make event fd!";
    return JNI_ERR;
  }
  *env = transport;
  return JNI_OK;
}

}  // namespace dt_fds
