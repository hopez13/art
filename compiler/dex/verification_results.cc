/*
 * Copyright (C) 2013 The Android Open Source Project
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

#include "verification_results.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/mutex-inl.h"
#include "driver/compiler_driver.h"
#include "driver/compiler_options.h"
#include "thread.h"
#include "thread-inl.h"
#include "verified_method.h"
#include "verifier/method_verifier-inl.h"

namespace art {

VerificationResults::VerificationResults(const CompilerOptions* compiler_options)
    : compiler_options_(compiler_options),
      verified_methods_lock_("compiler verified methods lock"),
      rejected_classes_lock_("compiler rejected classes lock") {}

VerificationResults::~VerificationResults() {
  WriterMutexLock mu(Thread::Current(), verified_methods_lock_);
  DeleteResults(preregistered_dex_files_);
  DeleteResults(dex_files_);
}

void VerificationResults::ProcessVerifiedMethod(verifier::MethodVerifier* method_verifier) {
  DCHECK(method_verifier != nullptr);
  MethodReference ref = method_verifier->GetMethodReference();
  bool compile = IsCandidateForCompilation(ref, method_verifier->GetAccessFlags());
  std::unique_ptr<const VerifiedMethod> verified_method(
      VerifiedMethod::Create(method_verifier, compile));
  if (verified_method == nullptr) {
    // We'll punt this later.
    return;
  }
  DexFileMethodArray* const array = GetMethodArray(*ref.dex_file);
  DCHECK(array != nullptr);
  Atomic<const VerifiedMethod*>* slot = &(*array)[ref.dex_method_index];
  if (slot->CompareExchangeStrongSequentiallyConsistent(nullptr, verified_method.get())) {
    // Successfully added, release the unique_ptr since we no longer have ownership.
    DCHECK_EQ(GetVerifiedMethod(ref), verified_method.get());
    verified_method.release();
  } else {
    // TODO: Investigate why are we doing the work again for this method and try to avoid it.
    LOG(WARNING) << "Method processed more than once: " << ref.PrettyMethod();
    if (!Runtime::Current()->UseJitCompilation()) {
      const VerifiedMethod* existing = slot->LoadSequentiallyConsistent();
      DCHECK_EQ(existing->GetDevirtMap().size(), verified_method->GetDevirtMap().size());
      DCHECK_EQ(existing->GetSafeCastSet().size(), verified_method->GetSafeCastSet().size());
    }
    // Let the unique_ptr delete the new verified method since there was already an existing one
    // registered. It is unsafe to replace the existing one since the JIT may be using it to
    // generate a native GC map.
  }
}

const VerifiedMethod* VerificationResults::GetVerifiedMethod(MethodReference ref) {
  DexFileMethodArray* array = GetMethodArray(*ref.dex_file);
  DCHECK(array != nullptr);
  return (*array)[ref.dex_method_index].LoadRelaxed();
}

void VerificationResults::AddRejectedClass(ClassReference ref) {
  {
    WriterMutexLock mu(Thread::Current(), rejected_classes_lock_);
    rejected_classes_.insert(ref);
  }
  DCHECK(IsClassRejected(ref));
}

bool VerificationResults::IsClassRejected(ClassReference ref) {
  ReaderMutexLock mu(Thread::Current(), rejected_classes_lock_);
  return (rejected_classes_.find(ref) != rejected_classes_.end());
}

bool VerificationResults::IsCandidateForCompilation(MethodReference&,
                                                    const uint32_t access_flags) {
  if (!compiler_options_->IsBytecodeCompilationEnabled()) {
    return false;
  }
  // Don't compile class initializers unless kEverything.
  if ((compiler_options_->GetCompilerFilter() != CompilerFilter::kEverything) &&
     ((access_flags & kAccConstructor) != 0) && ((access_flags & kAccStatic) != 0)) {
    return false;
  }
  return true;
}

void VerificationResults::PreRegisterDexFile(const DexFile& dex_file) {
  preregistered_dex_files_.emplace_back(
      &dex_file,
      std::make_unique<DexFileMethodArray>(dex_file.NumMethodIds()));
}

void VerificationResults::DeleteResults(DexFileResults& array) {
  for (auto& pair : array) {
    DexFileMethodArray* method_array = pair.second.get();
    if (method_array != nullptr) {
      for (Atomic<const VerifiedMethod*>& method : *method_array) {
        delete method.LoadSequentiallyConsistent();
      }
    }
  }
  array.clear();
}

VerificationResults::DexFileMethodArray* VerificationResults::GetMethodArray(
    const DexFile& dex_file) {
  for (auto& pair : preregistered_dex_files_) {
    if (pair.first == &dex_file) {
      DCHECK(pair.second != nullptr);
      return pair.second.get();
    }
  }
  WriterMutexLock mu(Thread::Current(), verified_methods_lock_);
  for (auto& pair : dex_files_) {
    if (pair.first == &dex_file) {
      DCHECK(pair.second != nullptr);
      return pair.second.get();
    }
  }
  dex_files_.emplace_back(
      &dex_file,
      std::make_unique<DexFileMethodArray>(dex_file.NumMethodIds()));
  return dex_files_.back().second.get();
}

}  // namespace art
