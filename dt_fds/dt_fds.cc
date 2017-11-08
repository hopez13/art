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

#include "dt_fds.h"

#include <android-base/logging.h>
#include <atomic>
#include <string>
#include <vector>

#include <arpa/inet.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <poll.h>

#include <jni.h>
#include <jvmti.h>
#include <jdwpTransport.h>

namespace dt_fds {

struct DtFdData {
  int sock_fd_;
  int control_fd_;
};

// TODO SOCK_SEQPACKET AF_LOCAL socketpair.
extern const jdwpTransportNativeInterface_ gTransportInterface;

template <typename T> static T HostToNetwork(T in);
template <typename T> static T NetworkToHost(T in);

// template<> uint8_t HostToNetwork(uint8_t in) { return in; }
// template<> uint8_t NetworkToHost(uint8_t in) { return in; }
template<> int8_t HostToNetwork(int8_t in) { return in; }
template<> int8_t NetworkToHost(int8_t in) { return in; }

template<> uint16_t HostToNetwork(uint16_t in) { return htons(in); }
template<> uint16_t NetworkToHost(uint16_t in) { return ntohs(in); }
template<> int16_t HostToNetwork(int16_t in) {
  return static_cast<int16_t>(HostToNetwork(static_cast<uint16_t>(in)));
}
template<> int16_t NetworkToHost(int16_t in) {
  return static_cast<int16_t>(NetworkToHost(static_cast<uint16_t>(in)));
}
template<> uint32_t HostToNetwork(uint32_t in) { return htonl(in); }
template<> uint32_t NetworkToHost(uint32_t in) { return ntohl(in); }
template<> int32_t HostToNetwork(int32_t in) {
  return static_cast<int32_t>(HostToNetwork(static_cast<uint32_t>(in)));
}
template<> int32_t NetworkToHost(int32_t in) {
  return static_cast<int32_t>(NetworkToHost(static_cast<uint32_t>(in)));
}

ScopedWriteLockHolder::ScopedWriteLockHolder(DtFdsTransport* t, /*out*/IOResult* result)
    : transport_(t), locked_(false) {
  *result = transport_->AcquireControlFd();
  locked_ = (*result == IOResult::kOk);
}

ScopedWriteLockHolder::~ScopedWriteLockHolder() {
  if (locked_) {
    transport_->ReleaseControlFd();
  }
}

IOResult DtFdsTransport::AcquireControlFd() {
  if (control_fd_ < 0) {
    return IOResult::kOk;
  }
  while (true) {
    constexpr int nfds = 2;
    struct pollfd pollfds[nfds] = {
      { write_cancel_fd_, POLLIN, 0 },
      { control_fd_, POLLIN, 0 },
    };
    int retval = poll(pollfds, nfds, 0);
    if (retval < 0) {
      return IOResult::kError;
    } else if ((pollfds[0].revents & POLLIN) == POLLIN) {
      ClearWakeup(write_cancel_fd_);
      return IOResult::kInterrupt;
    } else {
      CHECK_EQ(pollfds[1].revents & POLLIN, POLLIN);
      uint64_t val;
      if (TEMP_FAILURE_RETRY(read(control_fd_, &val, sizeof(val))) != sizeof(val)) {
        if (errno == EAGAIN) {
          continue;
        } else {
          return IOResult::kError;
        }
      } else {
        return IOResult::kOk;
      }
    }
  }
}

void DtFdsTransport::ReleaseControlFd() {
  if (control_fd_ < 0) {
    return;
  }
  uint64_t val = 1;
  TEMP_FAILURE_RETRY(write(control_fd_, &val, sizeof(val)));
}

void DtFdsTransport::SendWakeup(int fd) {
  uint64_t data = 1;
  CHECK(TEMP_FAILURE_RETRY(write(fd, &data, sizeof(data))) > 0);
}

void DtFdsTransport::ClearWakeup(int fd) {
  uint64_t data = 0;
  // Ignore failures.
  TEMP_FAILURE_RETRY(read(fd, &data, sizeof(data)));
}

// TODO It might be useful to set the fd's to O_NONBLOCK or verify that they already are.
DtFdsTransport::DtFdsTransport(jvmtiEnv* jvmti,
                               jrawMonitorID mon_read,
                               jrawMonitorID mon_write,
                               jdwpTransportCallback* cb)
      : mem_(*cb),
        sock_fd_(-1),
        attach_cancel_fd_(eventfd(0, EFD_NONBLOCK)),
        write_cancel_fd_(eventfd(0, EFD_NONBLOCK)),
        control_fd_(-1),
        listen_fd_(-1),
        state_(TransportState::kClosed),
        jvmti_(jvmti),
        read_monitor_(mon_read),
        write_monitor_(mon_write) { }

DtFdsTransport::~DtFdsTransport() { }

jdwpTransportError DtFdsTransport::PerformAttach(int comm_sock, int accept_event_fd) {
  jdwpTransportError err = SetupListen(comm_sock, accept_event_fd);
  if (err != OK) {
    return OK;
  }
  return Accept();
}

// Just record the listen-socket we will actually get the fds in attach.
jdwpTransportError DtFdsTransport::SetupListen(int sock, int accept_event_fd) {
  ScopedRawMonitorLock lk1(jvmti_, read_monitor_);
  ScopedRawMonitorLock lk2(jvmti_, write_monitor_);
  if (!ChangeState(TransportState::kClosed, TransportState::kListening)) {
    return ERR(ILLEGAL_STATE);
  }
  CHECK_EQ(sock_fd_, -1);
  CHECK_EQ(control_fd_, -1);
  listen_fd_ = sock;
  accept_event_fd_ = accept_event_fd_;
  uint64_t data = 1;
  // Tell the other side we are listening.
  TEMP_FAILURE_RETRY(write(accept_event_fd_, &data, sizeof(data)));
  return OK;
}

jdwpTransportError DtFdsTransport::StopListening() {
  // Don't actually do anything if this isn't in listen mode as required by spec.
  if (ChangeState(TransportState::kListening, TransportState::kClosed)) {
    // These are cached in Accept() for this exact reason.
    listen_fd_ = -1;
    accept_event_fd_ = -1;
    SendWakeup(attach_cancel_fd_);
  }
  return OK;
}

// Last error message.
thread_local std::string global_last_error_;

void DtFdsTransport::SetLastError(const std::string& desc) {
  PLOG(ERROR) << desc;
  // TODO More than this.
  global_last_error_ = desc;
}

IOResult DtFdsTransport::ReadFullyWithInterrupt(void* data, size_t ndata, int interrupt_fd) {
  uint8_t* bdata = reinterpret_cast<uint8_t*>(data);
  size_t nbytes = 0;
  constexpr int nfds = 2;
  constexpr int event_idx = 1;
  while (nbytes < ndata) {
    struct pollfd pollfds[nfds] = {
      { sock_fd_, POLLIN, 0 },
      { interrupt_fd, POLLIN, 0 },
    };
    int retval = poll(pollfds, nfds, 0);
    if (retval < 0) {
      SetLastError("POLL Failed");
      return IOResult::kError;
    } else if ((pollfds[event_idx].revents & POLLIN) == POLLIN) {
      // we got a wakeup. Clear it and retry. We might have had listening canceled.
      ClearWakeup(interrupt_fd);
      return IOResult::kInterrupt;
    }
    int res = TEMP_FAILURE_RETRY(recv(sock_fd_, bdata + nbytes, ndata - nbytes, 0));
    if (res < 0) {
      SetLastError("Failed during recv");
      return IOResult::kError;
    } else if (res == 0) {
      return IOResult::kEOF;
    } else {
      nbytes += res;
    }
  }
  return IOResult::kOk;
}

IOResult DtFdsTransport::WriteFullyWithInterrupt(const void* data, size_t ndata, int interrupt_fd) {
  const uint8_t* bdata = reinterpret_cast<const uint8_t*>(data);
  size_t nbytes = 0;
  constexpr int nfds = 2;
  constexpr int event_idx = 1;
  while (nbytes < ndata) {
    struct pollfd pollfds[nfds] = {
      { sock_fd_, POLLOUT, 0 },
      { interrupt_fd, POLLIN, 0 },
    };
    int retval = poll(pollfds, nfds, 0);
    if (retval < 0) {
      SetLastError("POLL Failed");
      return IOResult::kError;
    } else if ((pollfds[event_idx].revents & POLLIN) == POLLIN) {
      // we got a wakeup. Clear it and retry. We might have had listening canceled.
      ClearWakeup(interrupt_fd);
      return IOResult::kInterrupt;
    }
    int res = TEMP_FAILURE_RETRY(write(sock_fd_, bdata + nbytes, ndata - nbytes));
    if (res < 0) {
      SetLastError("Failed to write fully to sock_fd_");
      return IOResult::kError;
    } else if (res == 0) {
      return IOResult::kEOF;
    } else {
      nbytes += res;
    }
  }
  return IOResult::kOk;
}

jdwpTransportError DtFdsTransport::Accept() {
  // TODO Need to clear accept_event_fd_.
  while (true) {
    ScopedRawMonitorLock lk1(jvmti_, read_monitor_);
    ScopedRawMonitorLock lk2(jvmti_, write_monitor_);
    ClearWakeup(attach_cancel_fd_);
    if (state_ != TransportState::kListening) {
      return ERR(ILLEGAL_STATE);
    }
    // Read from the listen_fd_ to get a pair of fd's. A read/write socket for comms and a
    // EFD_SEMAPHORE for sync with writing.
    constexpr int nfds = 2;
    constexpr int listen_idx = 0;
    constexpr int event_idx = 1;
    struct pollfd pollfds[nfds] = {
      { listen_fd_, POLLIN, 0 },
      { attach_cancel_fd_, POLLIN, 0 },
    };
    int retval = poll(pollfds, nfds, 0);
    if (retval < 0) {
      SetLastError("POLL Failed");
      return ERR(IO_ERROR);
    } else if ((pollfds[event_idx].revents & POLLIN) == POLLIN) {
      // we got a wakeup. Retry. We might have had listening canceled and missed the last clear.
      continue;
    }
    // From here on we cannot retry. Pull the fd's out of the pipe.
    CHECK_EQ((pollfds[listen_idx].revents & POLLIN), POLLIN);
    struct DtFdData data{ -1, -1 };
    if (TEMP_FAILURE_RETRY(read(listen_fd_, &data, sizeof(data))) != sizeof(data)) {
      SetLastError("Unable to receive fds.");
      return ERR(IO_ERROR);
    }

    sock_fd_ = data.sock_fd_;
    control_fd_ = data.control_fd_;

    char handshake_recv[sizeof(kJdwpHandshake)];
    memset(handshake_recv, 0, sizeof(handshake_recv));
    IOResult res = ReadFullyWithInterrupt(handshake_recv,
                                          sizeof(handshake_recv),
                                          attach_cancel_fd_);
    if (res == IOResult::kInterrupt) {
      return ERR(ILLEGAL_STATE);
    } else if (res != IOResult::kOk ||
               strncmp(handshake_recv, kJdwpHandshake, sizeof(kJdwpHandshake)) != 0) {
      return ERR(IO_ERROR);
    }
    res = WriteFullyWithInterrupt(kJdwpHandshake, sizeof(kJdwpHandshake), attach_cancel_fd_);
    if (res == IOResult::kInterrupt) {
      return ERR(ILLEGAL_STATE);
    } else if (res != IOResult::kOk) {
      return ERR(IO_ERROR);
    }
    if (!ChangeState(TransportState::kListening, TransportState::kOpen)) {
      return ERR(ILLEGAL_STATE);
    }
    return OK;
  }
}

jdwpTransportError DtFdsTransport::Close() {
  state_ = TransportState::kClosed;
  SendWakeup(attach_cancel_fd_);
  SendWakeup(write_cancel_fd_);
  return OK;
}

class PacketReader {
 public:
  PacketReader(DtFdsTransport* transport, jdwpPacket* pkt)
      : transport_(transport),
        pkt_(pkt),
        is_eof_(false),
        is_err_(false) {}
  bool ReadFully() {
    // Zero out.
    memset(pkt_, 0, sizeof(jdwpPacket));
    int32_t len = ReadInt32();         // read len
    if (is_err_) {
      return false;
    } else if (is_eof_) {
      return true;
    } else if (len < 11) {
      transport_->SetLastError("Packet with len < 11 received!");
      return false;
    }
    pkt_->type.cmd.len = len;
    pkt_->type.cmd.id = ReadInt32();    // read id
    pkt_->type.cmd.flags = ReadByte();  // read flags
    if (is_err_) {
      return false;
    } else if (is_eof_) {
      return true;
    } else if ((pkt_->type.reply.flags & kReplyFlag) == kReplyFlag) {
      ReadReplyPacket();
    } else {
      ReadCmdPacket();
    }
    if (is_err_) {
      return false;
    } else {
      return true;
    }
  }

 private:
  void ReadReplyPacket() {
    pkt_->type.reply.errorCode = ReadInt16();
    pkt_->type.reply.data = ReadRemaining();
  }

  void ReadCmdPacket() {
    pkt_->type.cmd.cmdSet = ReadByte();
    pkt_->type.cmd.cmd = ReadByte();
    pkt_->type.cmd.data = ReadRemaining();
  }

  template <typename T>
  T HandleResult(IOResult res, T val, T fail) {
    switch (res) {
      case IOResult::kError:
        transport_->SetLastError("Failed to read");
        is_err_ = true;
        return fail;
      case IOResult::kOk:
        return val;
      case IOResult::kEOF:
        is_eof_ = true;
        pkt_->type.cmd.len = 0;
        return fail;
      case IOResult::kInterrupt:
        transport_->SetLastError("Failed to read, concurrent close!");
        is_err_ = true;
        return fail;
    }
  }

  jbyte* ReadRemaining() {
    if (is_eof_ || is_err_) {
      return nullptr;
    }
    jbyte* out = nullptr;
    jint rem = pkt_->type.cmd.len - 11;
    CHECK_GE(rem, 0);
    if (rem == 0) {
      return nullptr;
    } else {
      out = reinterpret_cast<jbyte*>(transport_->mem_.alloc(rem));
      IOResult res = transport_->ReadFullyWithInterrupt(out, rem, transport_->write_cancel_fd_);
      jbyte* ret = HandleResult(res, out, static_cast<jbyte*>(nullptr));
      if (ret != out) {
        transport_->mem_.free(out);
      }
      return ret;
    }
  }

  jbyte ReadByte() {
    if (is_eof_ || is_err_) {
      return -1;
    }
    jbyte out;
    IOResult res =
        transport_->ReadFullyWithInterrupt(&out, sizeof(out), transport_->write_cancel_fd_);
    return HandleResult(res, NetworkToHost(out), static_cast<jbyte>(-1));
  }

  jshort ReadInt16() {
    if (is_eof_ || is_err_) {
      return -1;
    }
    jshort out;
    IOResult res =
        transport_->ReadFullyWithInterrupt(&out, sizeof(out), transport_->write_cancel_fd_);
    return HandleResult(res, NetworkToHost(out), static_cast<jshort>(-1));
  }

  jshort ReadInt32() {
    if (is_eof_ || is_err_) {
      return -1;
    }
    jint out;
    IOResult res =
        transport_->ReadFullyWithInterrupt(&out, sizeof(out), transport_->write_cancel_fd_);
    return HandleResult(res, NetworkToHost(out), -1);
  }

  DtFdsTransport* transport_;
  jdwpPacket* pkt_;
  bool is_eof_;
  bool is_err_;
};

jdwpTransportError DtFdsTransport::ReadPacket(jdwpPacket* pkt) {
  if (pkt == nullptr) {
    return ERR(ILLEGAL_ARGUMENT);
  }
  // Only one thread can have a packet read at a time.
  ScopedRawMonitorLock lk(jvmti_, read_monitor_);
  ClearWakeup(write_cancel_fd_);
  if (state_ != TransportState::kOpen) {
    return ERR(ILLEGAL_STATE);
  }
  PacketReader reader(this, pkt);
  if (reader.ReadFully()) {
    return OK;
  } else {
    return ERR(IO_ERROR);
  }
}

class PacketWriter {
 public:
  PacketWriter(DtFdsTransport* transport, const jdwpPacket* pkt)
      : transport_(transport), pkt_(pkt), data_() {}

  bool WriteFully() {
    PushInt32(pkt_->type.cmd.len);
    PushInt32(pkt_->type.cmd.id);
    PushByte(pkt_->type.cmd.flags);
    if ((pkt_->type.reply.flags & kReplyFlag) == kReplyFlag) {
      PushInt16(pkt_->type.reply.errorCode);
      PushData(pkt_->type.reply.data, pkt_->type.reply.len - 11);
    } else {
      PushByte(pkt_->type.cmd.cmdSet);
      PushByte(pkt_->type.cmd.cmd);
      PushData(pkt_->type.cmd.data, pkt_->type.cmd.len - 11);
    }
    IOResult res = transport_->WriteFullyWithInterrupt(data_.data(),
                                                       data_.size(),
                                                       transport_->write_cancel_fd_);
    return res == IOResult::kOk;
  }

 private:
  void PushInt32(int32_t data) {
    data = HostToNetwork(data);
    PushData(&data, sizeof(data));
  }
  void PushInt16(int16_t data) {
    data = HostToNetwork(data);
    PushData(&data, sizeof(data));
  }
  void PushByte(jbyte data) {
    data_.push_back(HostToNetwork(data));
  }

  void PushData(void* d, size_t size) {
    uint8_t* bytes = reinterpret_cast<uint8_t*>(d);
    for (size_t i = 0; i < size; i++) {
      data_.push_back(bytes[i]);
    }
  }

  DtFdsTransport* transport_;
  const jdwpPacket* pkt_;
  std::vector<uint8_t> data_;
};

jdwpTransportError DtFdsTransport::WritePacket(const jdwpPacket* pkt) {
  if (pkt == nullptr) {
    return ERR(ILLEGAL_ARGUMENT);
  }
  // Only one thread can have a packet write at a time.
  ScopedRawMonitorLock lk(jvmti_, write_monitor_);
  ClearWakeup(write_cancel_fd_);
  if (state_ != TransportState::kOpen) {
    return ERR(ILLEGAL_STATE);
  }
  IOResult res;
  ScopedWriteLockHolder swlh(this, &res);
  if (res != IOResult::kOk) {
    return ERR(IO_ERROR);
  }
  PacketWriter writer(this, pkt);
  if (writer.WriteFully()) {
    return OK;
  } else {
    return ERR(IO_ERROR);
  }
}

jboolean DtFdsTransport::IsOpen() {
  return state_ == TransportState::kOpen;
}

jdwpTransportError DtFdsTransport::GetLastError(/*out*/char** err) {
  std::string data = global_last_error_;
  *err = reinterpret_cast<char*>(mem_.alloc(data.size() + 1));
  strcpy(*err, data.c_str());
  return OK;
}

static DtFdsTransport* AsDtFds(jdwpTransportEnv* env) {
  return reinterpret_cast<DtFdsTransport*>(env);
}

// TODO Need a second event-fd to send if we are actually listening now.
// Address is <fd_number>:<accept_event_fd>. The fd is a pipe that will be set to the pair of
// socket_fd & control_fd by the art_plugin that manages this connection. The accept_event_fd is a
// event-fd that this transport will ping to indicate that it is listening for fd's from the pipe.
//
// Note everything with this must be done in the same process.
static jdwpTransportError ParseAddress(const std::string& addr,
                                       /*out*/int* sock_fd,
                                       /*out*/int* accept_event_fd) {
  size_t sep = addr.find(':');
  auto fd_num_addr = addr.substr(0, sep);
  auto event_fd_addr = addr.substr(sep + 1);
  // TODO...
  const char* numbers = "1234567890";
  if (fd_num_addr.find_first_not_of(numbers) != std::string::npos ||
      event_fd_addr.find_first_not_of(numbers) != std::string::npos) {
    LOG(ERROR) << "address format is <fd_num>:<accept_event_fd> not " << addr;
    return ERR(ILLEGAL_ARGUMENT);
  }
  *sock_fd = std::stoi(fd_num_addr, 0, 10);
  *accept_event_fd = std::stoi(event_fd_addr, 0, 10);
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

  // Address is <sock_fd>
  static jdwpTransportError Attach(jdwpTransportEnv* env,
                                   const char* address,
                                   jlong attach_timeout ATTRIBUTE_UNUSED,
                                   jlong handshake_timeout ATTRIBUTE_UNUSED) {
    if (address == nullptr || *address == '\0') {
      return ERR(ILLEGAL_ARGUMENT);
    }
    int sock_fd_num;
    int event_fd;
    jdwpTransportError err = ParseAddress(address, &sock_fd_num, &event_fd);
    if (err != OK) {
      return err;
    }
    return AsDtFds(env)->PerformAttach(sock_fd_num, event_fd);
  }

  static jdwpTransportError StartListening(jdwpTransportEnv* env,
                                           const char* address,
                                           /*out*/ char** actual_address) {
    if (address == nullptr || *address == '\0') {
      return ERR(ILLEGAL_ARGUMENT);
    }
    int sock_fd_num;
    int event_fd;
    jdwpTransportError err = ParseAddress(address, &sock_fd_num, &event_fd);
    if (err != OK) {
      return err;
    }
    err = AsDtFds(env)->SetupListen(sock_fd_num, event_fd);
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
    return AsDtFds(env)->IsOpen();
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

  static jdwpTransportError GetLastError(jdwpTransportEnv* env, char** error) {
    // TODO Actually implement this? The tls requirements make it somewhat annoying.
    return AsDtFds(env)->GetLastError(error);
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
  jvmtiEnv* jvmti;
  if (vm->GetEnv(reinterpret_cast<void**>(&jvmti), JVMTI_VERSION_1_2) != JNI_OK) {
    LOG(ERROR) << "Failed to get a jvmtiEnv for this transport!";
    return JNI_ERR;
  }
  jrawMonitorID mon_write;
  if (jvmti->CreateRawMonitor("jdwpTransport: dt_fds write monitor",
                              &mon_write) != JVMTI_ERROR_NONE) {
    LOG(ERROR) << "Unable to create raw-monitor for jdwp transport";
    return JNI_ERR;
  }
  jrawMonitorID mon_read;
  if (jvmti->CreateRawMonitor("jdwpTransport: dt_fds read monitor",
                              &mon_read) != JVMTI_ERROR_NONE) {
    LOG(ERROR) << "Unable to create raw-monitor for jdwp transport";
    return JNI_ERR;
  }
  DtFdsTransport* transport =
      new (cb->alloc(sizeof(DtFdsTransport))) DtFdsTransport(jvmti, mon_read, mon_write, cb);
  *env = transport;
  return JNI_OK;
}

}  // namespace dt_fds
