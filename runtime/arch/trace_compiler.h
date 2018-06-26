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

#ifndef ART_RUNTIME_ARCH_TRACE_COMPILER_H_
#define ART_RUNTIME_ARCH_TRACE_COMPILER_H_

#include <unordered_map>
#include <vector>

#include <stddef.h>
#include <stdint.h>

#include "base/array_ref.h"
#include "base/safe_map.h"
#include "base/macros.h"
#include "base/mem_map.h"
#include "base/mutex.h"

namespace art {

class TraceCompiler {
 public:
  static TraceCompiler* Create();

  const void* GetTrampolineTo(uintptr_t target);
  const void* GetTrampolineTarget(const void* trampoline);

  // TODO Do something.
  ~TraceCompiler() {}

 private:
  TraceCompiler()
      : lock_("Trace Compiler Lock", LockLevel::kGenericBottomLock),
        next_location_(nullptr) {}

  // Called with an already copied version of
  // [art_quick_ttrace_entrypoint_template,art_quick_ttrace_entrypoint_template_end). The
  // art_quick_ttrace_entrypoint_template must be PIC. This function will modify the trampoline
  // slice in place so that when executed the call will be to 'target'.
  void WriteTraceTarget(ArrayRef<uint8_t> trampoline, uintptr_t target) const;

  uint8_t* AllocateCode(size_t size) REQUIRES(lock_);

  void AllocateMoreSpace() REQUIRES(lock_);

  Mutex lock_ BOTTOM_MUTEX_ACQUIRED_AFTER;
  SafeMap<uintptr_t, const void*> target_to_trampoline_map_ GUARDED_BY(lock_);
  SafeMap<const void*, uintptr_t> trampoline_to_target_map_ GUARDED_BY(lock_);
  std::vector<MemMap*> exec_pages_ GUARDED_BY(lock_);
  uint8_t* next_location_;
};

}  // namespace art

#endif  // ART_RUNTIME_ARCH_TRACE_COMPILER_H_

