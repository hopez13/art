/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "backtrace_helper.h"

#if defined(__linux__)

#include <backtrace/Backtrace.h>
#include <backtrace/BacktraceMap.h>

#include "unwindstack/Unwinder.h"
#include "unwindstack/RegsGetLocal.h"

#include <unistd.h>
#include <sys/types.h>

#include "thread-inl.h"

#else

// For UNUSED
#include "base/macros.h"

#endif

namespace art {

static struct LogMem {
  ~LogMem() {
    // LOG(ERROR) << "VmPeak: " << GetProcessStatus("VmPeak");
  }
} log_mem;

// We only really support libbacktrace on linux which is unfortunate but since this is only for
// gcstress this isn't a huge deal.
#if defined(__linux__)

static const char* kBacktraceCollectorTlsKey = "BacktraceCollectorTlsKey";

struct BacktraceMapHolder : public TLSData {
  BacktraceMapHolder() : map_(BacktraceMap::Create(getpid())) {}

  std::unique_ptr<BacktraceMap> map_;
};

static BacktraceMap* GetMap(Thread* self) {
  BacktraceMapHolder* map_holder =
      reinterpret_cast<BacktraceMapHolder*>(self->GetCustomTLS(kBacktraceCollectorTlsKey));
  if (map_holder == nullptr) {
    map_holder = new BacktraceMapHolder;
    // We don't care about the function names. Turning this off makes everything significantly
    // faster.
    map_holder->map_->SetResolveNames(false);
    // Only created and queried on Thread::Current so no sync needed.
    self->SetCustomTLS(kBacktraceCollectorTlsKey, map_holder);
  }

  return map_holder->map_.get();
}

struct UnwindHelper : public TLSData {
  static constexpr const char* kTlsKey = "UnwindHelper::kTlsKey";

  explicit UnwindHelper(size_t max_depth) {
    Init(max_depth);
  }

  void Init(size_t max_depth) {
    maps_.reset(new unwindstack::LocalMaps());
    CHECK(maps_->Parse());
    memory_.reset(new unwindstack::MemoryLocal());
    jit_.reset(new unwindstack::JitDebug(memory_));
    dex_.reset(new unwindstack::DexFiles(memory_));
    unwinder_.reset(new unwindstack::Unwinder(max_depth, maps_.get(), memory_));
    unwinder_->SetJitDebug(jit_.get(), unwindstack::Regs::CurrentArch());
    unwinder_->SetDexFiles(dex_.get(), unwindstack::Regs::CurrentArch());
    unwinder_->SetResolveNames(false);
    unwindstack::Elf::SetCachingEnabled(true);
  }

  static UnwindHelper* Get(Thread* self, size_t max_depth) {
    UnwindHelper* unwinder_holder =
        reinterpret_cast<UnwindHelper*>(self->GetCustomTLS(UnwindHelper::kTlsKey));
    if (unwinder_holder == nullptr) {
      unwinder_holder = new UnwindHelper(max_depth);
      self->SetCustomTLS(UnwindHelper::kTlsKey, unwinder_holder);
    }
    return unwinder_holder;
  }

  unwindstack::Unwinder* Unwinder() { return unwinder_.get(); }

 private:
  std::unique_ptr<unwindstack::LocalMaps> maps_;
  std::shared_ptr<unwindstack::Memory> memory_;
  std::unique_ptr<unwindstack::JitDebug> jit_;
  std::unique_ptr<unwindstack::DexFiles> dex_;
  std::unique_ptr<unwindstack::Unwinder> unwinder_;
};

void BacktraceCollector::Collect() {
  std::unique_ptr<Backtrace> backtrace(Backtrace::Create(BACKTRACE_CURRENT_PROCESS,
                                                         BACKTRACE_CURRENT_THREAD,
                                                         GetMap(Thread::Current())));
  backtrace->SetSkipFrames(true);
  if (!backtrace->Unwind(0 * skip_count_, nullptr)) {
    return;
  }
  for (Backtrace::const_iterator it = backtrace->begin();
       max_depth_ > num_frames_ && it != backtrace->end();
       ++it) {
    out_frames_[num_frames_++] = static_cast<uintptr_t>(it->pc);
  }
  CHECK_GT(num_frames_, 1u);

  auto* helper = UnwindHelper::Get(Thread::Current(), max_depth_);
  auto* unwinder = helper->Unwinder();
  std::unique_ptr<unwindstack::Regs> regs(unwindstack::Regs::CreateFromLocal());
  RegsGetLocal(regs.get());
  unwinder->SetRegs(regs.get());
  unwinder->Unwind();
  size_t i = 0;
  for (auto it : unwinder->ConsumeFrames()) {
    if (i != 0 && i < num_frames_) {
      CHECK_EQ(it.pc, out_frames_[i]) << i;
    }
    // if (i != num_frames_ - 1 && it.function_name == "") {
    //   LOG(ERROR) << "VmPeak: " << GetProcessStatus("VmPeak");
    //   LOG(FATAL) << i << ": " << std::hex << it.pc;
    // }
    i++;
  }
  CHECK_EQ(i, num_frames_);
}

#else

#pragma clang diagnostic push
#pragma clang diagnostic warning "-W#warnings"
#warning "Backtrace collector is not implemented. GCStress cannot be used."
#pragma clang diagnostic pop

// We only have an implementation for linux. On other plaforms just return nothing. This is not
// really correct but we only use this for hashing and gcstress so it's not too big a deal.
void BacktraceCollector::Collect() {
  UNUSED(skip_count_);
  UNUSED(out_frames_);
  UNUSED(max_depth_);
  num_frames_ = 0;
}

#endif

}  // namespace art
