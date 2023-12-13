/*
 * Copyright (C) 2023 The Android Open Source Project
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

#include "profiling_info_builder.h"

#include "art_method-inl.h"
#include "driver/compiler_options.h"
#include "dex/code_item_accessors-inl.h"
#include "jit/profiling_info.h"
#include "optimizing_compiler_stats.h"
#include "scoped_thread_state_change-inl.h"

namespace art HIDDEN {

void ProfilingInfoBuilder::Run() {
  DCHECK_EQ(GetGraph()->GetProfilingInfo(), nullptr);
  // Order does not matter.
  for (HBasicBlock* block : GetGraph()->GetReversePostOrder()) {
    // No need to visit the phis.
    for (HInstructionIteratorHandleChanges inst_it(block->GetInstructions()); !inst_it.Done();
         inst_it.Advance()) {
      inst_it.Current()->Accept(this);
    }
  }

  ScopedObjectAccess soa(Thread::Current());
  GetGraph()->SetProfilingInfo(
      ProfilingInfo::Create(soa.Self(), GetGraph()->GetArtMethod(), inline_caches_));
}

static uint32_t ComputeDexPc(ArtMethod* method, HInvoke* invoke)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (invoke->GetEnvironment()->IsFromInlinedInvoke()) {
    uint32_t dex_pc = method->DexInstructions().InsnsSizeInCodeUnits();
    for (HEnvironment* env = invoke->GetEnvironment(); env != nullptr; env = env->GetParent()) {
      dex_pc += env->GetDexPc();
    }
    return dex_pc;
  } else {
    return invoke->GetDexPc();
  }
}

void ProfilingInfoBuilder::HandleInvoke(HInvoke* invoke) {
  if (invoke->IsIntrinsic()) {
    return;
  }

  if (invoke->InputAt(0)->GetReferenceTypeInfo().IsExact()) {
    return;
  }

  ScopedObjectAccess soa(Thread::Current());
  if (invoke->GetResolvedMethod() != nullptr) {
    if (invoke->GetResolvedMethod()->IsFinal() ||
        invoke->GetResolvedMethod()->GetDeclaringClass()->IsFinal()) {
      return;
    }
  }
  if (invoke->GetEnvironment()->IsFromInlinedInvoke()) {
//    LOG(ERROR) << "CREATING A CACHE AT " << ComputeDexPc(GetGraph()->GetArtMethod(), invoke) << " FOR";
//    LOG(ERROR) <<   "    " << invoke->GetResolvedMethod()->PrettyMethod();
    for (HEnvironment* env = invoke->GetEnvironment(); env != nullptr; env = env->GetParent()) {
//      LOG(ERROR) << "    " << env->GetMethod()->PrettyMethod();
    }
  }
  inline_caches_.push_back(ComputeDexPc(GetGraph()->GetArtMethod(), invoke));
}

void ProfilingInfoBuilder::VisitInvokeInterface(HInvokeInterface* invoke) {
  HandleInvoke(invoke);
}

void ProfilingInfoBuilder::VisitInvokeVirtual(HInvokeVirtual* invoke) {
  HandleInvoke(invoke);
}

InlineCache* ProfilingInfoBuilder::GetInlineCache(ProfilingInfo* info, HInvoke* instruction) {
  ScopedObjectAccess soa(Thread::Current());
  return info->GetInlineCache(ComputeDexPc(info->GetMethod(), instruction));
}

}  // namespace art
