/*
 * Copyright (C) 2011 The Android Open Source Project
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

// #include "trace_compiler-inl.h"
#include "trace_compiler.h"

#include <android-base/endian.h>

#include <string.h>
#include <sys/mman.h>  // For the PROT_* and MAP_* constants.

#include "thread-inl.h"
#include "base/macros.h"
#include "base/mem_map.h"
#include "base/bit_utils.h"

namespace art {

TraceCompiler* TraceCompiler::Create() {
  return new TraceCompiler;
}

// Where the entrypoint template starts and stop.
extern "C" void* art_quick_ttrace_entrypoint_template();
extern "C" void* art_quick_ttrace_entrypoint_template_end();

// TODO Get these from somewhere authoritative.
static constexpr size_t kCodeAlignment = 16;
static constexpr size_t kNumPages = 16;

void* TraceCompiler::GetTrampolineTo(uintptr_t target) {
  MutexLock mu(art::Thread::Current(), lock_);
  auto it = target_to_trampoline_map_.find(target);
  if (it != target_to_trampoline_map_.end()) {
    return it->second;
  }
  size_t entrypoint_size =
      reinterpret_cast<uintptr_t>(art_quick_ttrace_entrypoint_template_end) -
      reinterpret_cast<uintptr_t>(art_quick_ttrace_entrypoint_template);

  // Copy the template code in.
  ArrayRef<uint8_t> data(AllocateCode(entrypoint_size), entrypoint_size);
  memcpy(data.data(),
         reinterpret_cast<void*>(&art_quick_ttrace_entrypoint_template),
         entrypoint_size);

  // Write the target in.
  WriteTraceTarget(data, target);

  // Save the trampoline.
  target_to_trampoline_map_.Put(target, &data.front());

  return data.data();
}

// TODO
void TraceCompiler::WriteTraceTarget(ArrayRef<uint8_t> trampoline, uintptr_t target) const {
#if defined(__x86_64__)
  // TODO Need to update this once we have the real placements. Currently the first instruction is a
  // movabs which is <2-byte instruction & dest> <8 byte immediate>
  target = htole64(target);
  memcpy(trampoline.SubArray(2).data(), &target, sizeof(target));
#else
  UNUSED(trampoline);
  UNUSED(target);
#endif
}

uint8_t* TraceCompiler::AllocateCode(size_t size) {
  size = RoundUp(size, kCodeAlignment);
  if (exec_pages_.empty()) {
    DCHECK(next_location_ == nullptr);
    AllocateMoreSpace();
  }
  CHECK_GE(next_location_, exec_pages_.back()->Begin());
  if (next_location_ + size > exec_pages_.back()->End()) {
    AllocateMoreSpace();
  }
  CHECK_EQ(AlignUp(next_location_, kCodeAlignment), next_location_);
  uint8_t* res = next_location_;
  next_location_ += size;
  return res;
}

void TraceCompiler::AllocateMoreSpace() {
  std::string err;
  MemMap* new_map = MemMap::MapAnonymous("trampoline trace memory",
                                         /* addr */nullptr,
                                         /* byte_count */kNumPages * kPageSize,
                                         /* prot */ PROT_READ | PROT_WRITE | PROT_EXEC,
                                         /* low_4gb */ false,
                                         /* resuse */ false,
                                         /* error_msg */ &err);
  // TODO Don't do this.
  CHECK(new_map != nullptr) << err;
  exec_pages_.push_back(new_map);
  next_location_ = AlignUp(new_map->Begin(), kCodeAlignment);
}

}  // namespace art
