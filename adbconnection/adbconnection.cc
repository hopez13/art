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

  // Begin running the debugger.
void AdbConnectionDebuggerController::StartDebugger() {
  connection_->StartDebuggerThreads();
}

// The debugger should begin shutting down since the runtime is ending.
void AdbConnectionDebuggerController::StopDebugger() {
  connection_->StopDebuggerThreads();
}

void AdbConnectionDdmCallback::DdmPublishChunk(uint32_t type, const ArrayRef<const uint8_t>& data) {
  // TODO
}

AdbConnectionState::AdbConnectionState()
  : controller_(this),
    ddm_callback_(this),
    control_sock_(-1),
    shutting_down_(false),
    state_lock_("AdbConnection State Lock", art::LockLevel::kJdwpAdbStateLock),
    local_debugger_sock_(-1),
    debugger_state_(DebuggerState::kNotLoaded),
    link_(nullptr) /* TODO More */ {
  // Setup the addr.
  control_addr_.controlAddrUn.sun_family = AF_UNIX;
  control_addr_len_ = sizeof(control_addr_.controlAddrUn.sun_family) + sizeof(kJdwpControlName) - 1;
  memcpy(control_addr_.controlAddrUn.sun_path, kJdwpControlName, sizeof(kJdwpControlName) - 1);

  // Add the startup callback.
  art::ScopedObjectAccess soa(art::Thread::Current());
  art::Runtime::Current()->GetRuntimeCallbacks()->AddDebuggerControlCallback(&controller_);
}

static jthread CreateAdbConnectionThread(art::Thread* thr) {
  JNIEnv* env = thr->GetJniEnv();
  // Move to native state to talk with the jnienv api.
  art::ScopedThreadStateChange stsc(self, art::kNative);
  return env->NewObject(art::WellKnownClasses::java_lang_Thread,
                        art::WellKnownClasses::java_lang_Thread_init,
                        env->GetStaticObjectField(
                            art::WellKnownClasses::java_lang_ThreadGroup,
                            art::WellKnownClasses::java_lang_ThreadGroup_mainThreadGroup),
                        env->NewString(kAdbConnectionThreadName,
                                       sizeof(kAdbConnectionThreadName) - 1),
                        /*Priority*/ 0,
                        /*Daemon*/ true);
}

struct CallbackData {
  AdbConnectionState this_;
  jthread thr_;
};

static void* CallbackFunction(void* vdata) {
  std::unique_ptr<CallbackData> data(reinterpret_cast<CallbackData*>(vdata));
  art::Thread* self = art::Thread::Attach(kAdbConnectionThreadName,
                                          true,
                                          data->thr_);
  CHECK(self != nullptr) << "threads_being_born_ should have ensured thread could be attached.";
  // The name in Attach() is only for logging. Set the thread name. This is important so
  // that the thread is no longer seen as starting up.
  {
    art::ScopedObjectAccess soa(self);
    self->SetThreadName(kAdbConnectionThreadName);
  }

  // Release the peer.
  JNIEnv* env = self->GetJniEnv();
  env->DeleteGlobalRef(data->thr_);
  data->thr_= nullptr;
  {
    // The StartThreadBirth was called in the parent thread. We let the runtime know we are up
    // before going into the provided code.
    art::MutexLock mu(self, *art::Locks::runtime_shutdown_lock_);
    art::Runtime::Current()->EndThreadBirth();
  }
  data->this_->BeginListening(self);
  int detach_result = art::Runtime::Current()->GetJavaVM()->DetachCurrentThread();
  CHECK_EQ(detach_result, 0);

  return nullptr;
}

void AdbConnectionState::StartDebuggerThreads() {
  {
    art::Runtime* runtime = art::Runtime::Current();
    art::MutexLock mu(art::Thread::Current(), *art::Locks::runtime_shutdown_lock_);
    if (runtime->IsShuttingDownLocked()) {
      // The runtime is shutting down so we cannot create new threads. This shouldn't really happen.
      return;
    }
    runtime->StartThreadBirth();
  }
  ScopedLocalRef<jthread> thr(soa.Env(), CreateAdbConnectionThread(soa.Self()));
  pthread_t pthread;
  std::unique_ptr<CallbackData> data(new CallbackData { this, env->NewGlobalRef(thr.get()) });
  int pthread_create_result = pthread_create(&pthread,
                                             nullptr,
                                             &CallbackFunction,
                                             data.get());
  if (pthread_create_result != 0) {
    // If the create succeeded the other thread will call EndThreadBirth.
    art::Runtime* runtime = art::Runtime::Current();
    art::MutexLock mu(art::Thread::Current(), *art::Locks::runtime_shutdown_lock_);
    runtime->EndThreadBirth();
    return;
  }
  data.release();
}

PollResult AdbConnectionState::WaitForData() {
  size_t num_fds;
  bool has_remote = remote_.IsActive();
  bool has_link = link_.IsActive();
  DCHECK(!has_link || has_remote) << "link_.IsActive(): " << has_link << " remote_.IsActive(): "
                                  << has_remote << " having a link to the agent without an adb "
                                  << "connection should not happen.";
  PollResult res;
  memset(&res, 0, sizeof(res));

  std::array<struct pollfd, 6> pollfds = {
    { control_sock_, POLLIN, 0 },
    { sleep_event_fd_, POLLIN, 0 },
    // TODO Put this in.
    { link_sock_fd_, POLLIN, 0 },
    { has_remote ? remote_.GetFd() : -1, POLLIN | POLLHUP | POLLERR, 0 }
    { has_link ? link_.GetOutputFd() : -1, POLLIN | POLLHUP | POLLERR, 0 }
    { has_link ? link_.GetInputFd() : -1, POLLHUP | POLLERR, 0 }
  };

  if (has_remote && has_link) {
    num_fds = 6;
    res.has_link_fd_results_ = true;
    res.has_remote_fd_result_ = true;
  } else if (has_remote && !has_link) {
    num_fds = 4;
    res.has_remote_fd_result_ = true;
  } else {
    DCHECK(!has_remote && !has_link);
    num_fds = 3;
  }
  int retval = TEMP_FAILURE_RETRY(poll(pollfds.data(), num_fds, 0));
  // TODO Handle this failure!
  CHECK_GT(retval, 0) << "Something went wrong polling for data";

  res.control_sock_result_   = pollfds[0].revents;
  res.sleep_event_fd_result_ = pollfds[1].revents;
  res.link_sock_result_      = pollfds[2].revents;
  res.remote_fd_result_      = pollfds[3].revents;
  res.link_output_result_    = pollfds[4].revents;
  res.link_input_result_     = pollfds[5].revents;
  return res;
}

template<typename T>
bool FlagsSet(T data, T flags) {
  return (data & flags) == flags;
}
void AdbConnectionState::BeginListening(art::Thread* self) {
  CHECK_EQ(self->GetState(), art::kNative);
  art::Locks::mutator_lock_->AssertNotHeld(self);
  self->SetState(art::kWaitingInMainDebuggerLoop);
  while (!shutting_down_) {
    // Wait to see what all of the fds get.
    PollResult result = WaitForData();
    if (FlagsSet(result.sleep_event_fd_result_, POLLIN)) {
      // Something changed. Retry.
      ClearSleepEvent();
      continue;
    }

    if 
  }
  // Loop:
  //   set_to_wait_readable = [control addr, wakeup-event]
  //   set_to_wait_exception = []
  //   if link != nullptr:
  //     set_to_wait_readable.append(link.input) // one pipe
  //     set_to_wait_exception.append(link.input, link.output)
  //   if remote != nullptr:
  //     set_to_wait_readable.append(remote.output)
  //     set_to_wait_exception.append(remote.input, remote.output)
  //   elif link != nullptr:
  //     # we can only send remote pipes down if we actually have a link to adb
  //     set_to_wait_readable.append(remote.comm_socket)
  //   ready_fds, is_exception = poll(set_to_wait):
  //   if wakeup-event in ready_Fds:
  //     clear_wakeup
  //     # Something might have changed. Abandon this loop and try again.
  //     continue;
  //   if is_exception:
  //     if ready_fd contains link.input || ready_fd contains link.output:
  //       # Link went away suddenly. close the pipes if we have them. Clean up link
  //       remote.close()
  //       remote = null
  //     if ready_fd contains remote.input || ready_fd contains remote.output:
  //       # remote pipes had other ends closed. Don't kill the full connection.
  //       remote = nullptr;
  //   if ready_fd contains remote.comm_socket:
  //     # pass fds down to ageng
  //   if ready_fd contains remote.output:
  //     remote.write_fd.Lock();  # make sure there is a full packet boundary.
  //     # Use ioctl and splice(2).
  //     copy from remote.output to link.output
  //     remote.write_fd.Unlock();
  //   if ready_fd contains link.input:
  //     if agent is loaded:
  //       # Use ioctl and splice(2).
  //       copy from link to remote.input
  //     else:
  //       load agent
  //       retry
}

void AdbConnectionState::StopDebuggerThreads() {
  art::MutexLock mu(art::Thread::Current(), state_lock_);
  // The regular agent system will take care of unloading the agent (if needed).
  shutting_down_ = true;
  Wakeup();
  // TODO Close the sockets?
}

// The plugin initialization function.
extern "C" bool ArtPlugin_Initialize() REQUIRES_SHARED(art::Locks::mutator_lock_) {
  art::Runtime* runtime = art::Runtime::Current();
  gState = new AdbConnectionState;
  CHECK(gState != nullptr);
  return true;
}

extern "C" bool ArtPlugin_Deinitialize() {
  CHECK(gState != nullptr);
  // Just do this a second time?
  // TODO I don't think this should be needed.
  gState->StopDebuggerThreads();
  delete gState;
  return true;
}

}  // namespace adbconnection
