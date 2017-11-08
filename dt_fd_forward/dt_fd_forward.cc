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

#include "dt_fd_forward.h"

#include <android-base/logging.h>
#include <atomic>
#include <string>
#include <vector>

#include <arpa/inet.h>
#include <sys/ioctl.h>
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
  int read_fd_;
  int w;
};

// TODO Include line
#define DT_IO_ERROR_OTHER(trans, f) trans->SetLastError(__FILE__ ":"  f)

#define DT_IO_ERROR(f) DT_IO_ERROR_OTHER((this), f)

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

DtFdsTransport::DtFdsTransport(jdwpTransportCallback* cb)
      : mem_(*cb),
        read_fd_(-1),
        write_fd_(-1),
        wakeup_fd_(eventfd(0, EFD_NONBLOCK)),
        listen_fd_(-1),
        state_(TransportState::kClosed) {}

DtFdsTransport::~DtFdsTransport() { }

bool DtFdsTransport::ChangeState(TransportState old_state, TransportState new_state) {
  if (old_state == state_) {
    state_ = new_state;
    state_cv_.notify_all();
    return true;
  } else {
    return false;
  }
}

jdwpTransportError DtFdsTransport::PerformAttach(int listen_fd) {
  jdwpTransportError err = SetupListen(listen_fd);
  if (err != OK) {
    return OK;
  }
  return Accept();
}

jdwpTransportError DtFdsTransport::SetupListen(int listen_fd) {
  std::lock_guard<std::mutex> lk(state_mutex_);
  if (!ChangeState(TransportState::kClosed, TransportState::kListenSetup)) {
    return ERR(ILLEGAL_STATE);
  } else {
    listen_fd_ = listen_fd;
    CHECK(ChangeState(TransportState::kListenSetup, TransportState::kListening));
    return OK;
  }
}

jdwpTransportError DtFdsTransport::StopListening() {
  std::lock_guard<std::mutex> lk(state_mutex_);
  if (ChangeState(TransportState::kListening, TransportState::kClosed)) {
    listen_fd_ = -1;
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

IOResult DtFdsTransport::ReadFullyWithoutChecks(void* data, size_t ndata) {
  uint8_t* bdata = reinterpret_cast<uint8_t*>(data);
  size_t nbytes = 0;
  while (nbytes < ndata) {
    int res = TEMP_FAILURE_RETRY(read(read_fd_, bdata + nbytes, ndata - nbytes));
    if (res < 0) {
      DT_IO_ERROR("failed read()");
      return IOResult::kError;
    } else if (res == 0) {
      return IOResult::kEOF;
    } else {
      nbytes += res;
    }
  }
  return IOResult::kOk;
}

IOResult DtFdsTransport::ReadUpToMax(void* data, size_t ndata, /*out*/size_t* read_amount) {
  int avail;
  int res = ioctl(read_fd_, FIONREAD, &avail);
  if (res < 0) {
    DT_IO_ERROR("Failed ioctl(read_fd_, FIONREAD, &avail)");
    return IOResult::kError;
  }
  size_t to_read = std::min(static_cast<size_t>(avail), ndata);
  *read_amount = to_read;
  if (read_amount == 0) {
    // Check if the read would cause an EOF. Poll without any events.
    struct pollfd pollfd = { read_fd_, 0, 0 };
    res = poll(&pollfd, /*nfds*/1, /*timeout*/0);
    if (res < 0) {
      DT_IO_ERROR("Failed poll on read fd.");
      return IOResult::kError;
    }
    return ((pollfd.revents & POLLHUP) == POLLHUP) ? IOResult::kEOF : IOResult::kOk;
  }

  return ReadFullyWithoutChecks(data, to_read);
}

IOResult DtFdsTransport::ReadFully(void* data, size_t ndata) {
  uint64_t current_seq = pipe_seq_num_;
  size_t nbytes = 0;
  while (nbytes < ndata) {
    if (current_seq != pipe_seq_num_) {
      // We missed a close.
      return IOResult::kInterrupt;
    }
    size_t written;
    {
      std::lock_guard<std::mutex> lk(state_mutex_);
      // Nothing in here can cause any unbounded pause.
      if (state_ != TransportState::kOpen) {
        return IOResult::kInterrupt;
      }
      IOResult res = ReadUpToMax(reinterpret_cast<uint8_t*>(data) + nbytes,
                                 ndata - nbytes,
                                 /*out*/&written);
      if (res != IOResult::kOk) {
        return res;
      } else {
        nbytes += written;
      }
    }
    if (written == 0) {
      // No more data. Sleep without locks until more is available. We don't actually check for any
      // errors since possible ones are (1) the read_fd_ is closed or wakeup happens which are both
      // fine since the wakeup_fd_ or the poll failing will wake us up.
      struct pollfd pollfds[2] = { { read_fd_, POLLIN, 0 }, { wakeup_fd_, POLLIN, 0 }, };
      poll(pollfds, 2, -1);
      uint64_t val;
      int unused = read(wakeup_fd_, &val, sizeof(val));
      DCHECK(unused == sizeof(val) || errno == EAGAIN);
    }
  }
  return IOResult::kOk;
}

class ScopedEventFdLock {
 public:
  explicit ScopedEventFdLock(int fd) : fd_(fd), data_(0) {
    if (fd_ >= 0) {
      TEMP_FAILURE_RETRY(read(fd_, &data_, sizeof(data_)));
    }
  }

  ~ScopedEventFdLock() {
    if (fd_ >= 0) {
      TEMP_FAILURE_RETRY(write(fd_, &data_, sizeof(data_)));
    }
  }

 private:
  int fd_;
  uint64_t data_;
};

IOResult DtFdsTransport::WriteFullyWithoutChecks(const void* data, size_t ndata) {
  ScopedEventFdLock sefdl(write_lock_fd_);
  const uint8_t* bdata = reinterpret_cast<const uint8_t*>(data);
  size_t nbytes = 0;
  while (nbytes < ndata) {
    int res = TEMP_FAILURE_RETRY(write(write_fd_, bdata + nbytes, ndata - nbytes));
    if (res < 0) {
      DT_IO_ERROR("failed write()");
      return IOResult::kError;
    } else if (res == 0) {
      return IOResult::kEOF;
    } else {
      nbytes += res;
    }
  }
  return IOResult::kOk;
}

IOResult DtFdsTransport::WriteFully(const void* data, size_t ndata) {
  std::lock_guard<std::mutex> lk(state_mutex_);
  if (state_ != TransportState::kOpen) {
    return IOResult::kInterrupt;
  }
  return WriteFullyWithoutChecks(data, ndata);
}

IOResult DtFdsTransport::RecieveFdsFromSocket() {
  struct FdSet {
    int read_fd_;
    int write_fd_;
    int write_lock_fd_;
  };
  union {
    cmsghdr cm;
    uint8_t buffer[CMSG_SPACE(sizeof(FdSet))];
  } msg_union;
  // We don't actually care about the data. Only FDs. We need to have an iovec anyway to tell if we
  // got the values or not though.
  char dummy[1] = { '\0' };
  iovec iov;
  iov.iov_base = dummy;
  iov.iov_len  = sizeof(dummy);

  msghdr msg;
  memset(&msg, 0, sizeof(msg));
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = msg_union.buffer;
  msg.msg_controllen = sizeof(msg_union.buffer);

  cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
  cmsg->cmsg_len   = msg.msg_controllen;
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type  = SCM_RIGHTS;
  memset(reinterpret_cast<FdSet*>(CMSG_DATA(cmsg)), -1, sizeof(FdSet));

  int res = TEMP_FAILURE_RETRY(recvmsg(listen_fd_, &msg, 0));
  if (res <= 0) {
    DT_IO_ERROR("Failed to receive fds!");
    return IOResult::kError;
  }
  FdSet* out_fds = reinterpret_cast<FdSet*>(CMSG_DATA(cmsg));
  if (out_fds->read_fd_ < 0 || out_fds->write_fd_ < 0 || out_fds->write_lock_fd_ < 0) {
    DT_IO_ERROR("Received fds were invalid!");
    return IOResult::kError;
  }
  read_fd_ = out_fds->read_fd_;
  write_fd_ = out_fds->write_fd_;
  write_lock_fd_ = out_fds->write_lock_fd_;
  return IOResult::kOk;
}

jdwpTransportError DtFdsTransport::Accept() {
  std::unique_lock<std::mutex> lk(state_mutex_);
  while (!ChangeState(TransportState::kListening, TransportState::kOpening)) {
    if (state_ == TransportState::kClosed ||
        state_ == TransportState::kOpen) {
      return ERR(ILLEGAL_STATE);
    }
    state_cv_.wait(lk);
  }

  DCHECK_NE(listen_fd_, -1);
  if (RecieveFdsFromSocket() != IOResult::kOk) {
    CHECK(ChangeState(TransportState::kOpening, TransportState::kListening));
    return ERR(IO_ERROR);
  }

  // Moved to the opening state.
  char handshake_recv[sizeof(kJdwpHandshake)];
  memset(handshake_recv, 0, sizeof(handshake_recv));
  IOResult res = ReadFullyWithoutChecks(handshake_recv, sizeof(handshake_recv));
  if (res != IOResult::kOk ||
      strncmp(handshake_recv, kJdwpHandshake, sizeof(kJdwpHandshake)) != 0) {
    DT_IO_ERROR("Failed to read handshake");
    CHECK(ChangeState(TransportState::kOpening, TransportState::kListening));
    return ERR(IO_ERROR);
  }
  res = WriteFullyWithoutChecks(kJdwpHandshake, sizeof(kJdwpHandshake));
  if (res != IOResult::kOk) {
    DT_IO_ERROR("Failed to write handshake");
    CHECK(ChangeState(TransportState::kOpening, TransportState::kListening));
    return ERR(IO_ERROR);
  }
  CHECK(ChangeState(TransportState::kOpening, TransportState::kOpen));
  return OK;
}

jdwpTransportError DtFdsTransport::Close() {
  std::unique_lock<std::mutex> lk(state_mutex_);
  jdwpTransportError res =
      ChangeState(TransportState::kOpen, TransportState::kClosed) ? OK : ERR(ILLEGAL_STATE);
  // Send a wakeup after changing the state.
  uint64_t data = 1;
  TEMP_FAILURE_RETRY(write(wakeup_fd_, &data, sizeof(data)));
  if (res == OK) {
    // All access to these is either (1) in the kOpening state which we aren't in or (2) locked
    // under the state_mutex_ so we are safe to close these.
    close(read_fd_);
    close(write_fd_);
    close(write_lock_fd_);
    read_fd_ = -1;
    write_fd_ = -1;
    write_lock_fd_ = -1;
  }
  return res;
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
      DT_IO_ERROR_OTHER(transport_, "Packet with len < 11 received!");
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
        is_err_ = true;
        return fail;
      case IOResult::kOk:
        return val;
      case IOResult::kEOF:
        is_eof_ = true;
        pkt_->type.cmd.len = 0;
        return fail;
      case IOResult::kInterrupt:
        DT_IO_ERROR_OTHER(transport_, "Failed to read, concurrent close!");
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
      out = reinterpret_cast<jbyte*>(transport_->Alloc(rem));
      IOResult res = transport_->ReadFully(out, rem);
      jbyte* ret = HandleResult(res, out, static_cast<jbyte*>(nullptr));
      if (ret != out) {
        transport_->Free(out);
      }
      return ret;
    }
  }

  jbyte ReadByte() {
    if (is_eof_ || is_err_) {
      return -1;
    }
    jbyte out;
    IOResult res = transport_->ReadFully(&out, sizeof(out));
    return HandleResult(res, NetworkToHost(out), static_cast<jbyte>(-1));
  }

  jshort ReadInt16() {
    if (is_eof_ || is_err_) {
      return -1;
    }
    jshort out;
    IOResult res = transport_->ReadFully(&out, sizeof(out));
    return HandleResult(res, NetworkToHost(out), static_cast<jshort>(-1));
  }

  jshort ReadInt32() {
    if (is_eof_ || is_err_) {
      return -1;
    }
    jint out;
    IOResult res = transport_->ReadFully(&out, sizeof(out));
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
    IOResult res = transport_->WriteFully(data_.data(), data_.size());
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

void* DtFdsTransport::Alloc(size_t s) {
  return mem_.alloc(s);
}
void DtFdsTransport::Free(void* data) {
  mem_.free(data);
}
jdwpTransportError DtFdsTransport::GetLastError(/*out*/char** err) {
  std::string data = global_last_error_;
  *err = reinterpret_cast<char*>(Alloc(data.size() + 1));
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
                                       /*out*/int* listen_sock) {
  const char* numbers = "1234567890";
  if (addr.find_first_not_of(numbers) != std::string::npos) {
    LOG(ERROR) << "address format is <fd_num> not " << addr;
    return ERR(ILLEGAL_ARGUMENT);
  }
  *listen_sock = std::stoi(addr, 0, 10);
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
    int listen_fd;
    jdwpTransportError err = ParseAddress(address, &listen_fd);
    if (err != OK) {
      return err;
    }
    return AsDtFds(env)->PerformAttach(listen_fd);
  }

  static jdwpTransportError StartListening(jdwpTransportEnv* env,
                                           const char* address,
                                           /*out*/ char** actual_address) {
    if (address == nullptr || *address == '\0') {
      return ERR(ILLEGAL_ARGUMENT);
    }
    int listen_fd;
    jdwpTransportError err = ParseAddress(address, &listen_fd);
    if (err != OK) {
      return err;
    }
    err = AsDtFds(env)->SetupListen(listen_fd);
    if (err != OK) {
      return err;
    }
    if (actual_address != nullptr) {
      *actual_address = reinterpret_cast<char*>(AsDtFds(env)->Alloc(strlen(address) + 1));
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

extern "C"
JNIEXPORT jint JNICALL jdwpTransport_OnLoad(JavaVM* vm ATTRIBUTE_UNUSED,
                                            jdwpTransportCallback* cb,
                                            jint version,
                                            jdwpTransportEnv** /*out*/env) {
  if (version != JDWPTRANSPORT_VERSION_1_0) {
    LOG(ERROR) << "unknown version " << version;
    return JNI_ERR;
  }
  DtFdsTransport* transport = new (cb->alloc(sizeof(DtFdsTransport))) DtFdsTransport(cb);
  transport->functions = &gTransportInterface;
  *env = transport;
  return JNI_OK;
}

}  // namespace dt_fds
