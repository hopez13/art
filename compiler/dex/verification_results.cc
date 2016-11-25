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
}

void VerificationResults::ProcessVerifiedMethod(verifier::MethodVerifier* method_verifier) {
  DCHECK(method_verifier != nullptr);
  DCHECK(Runtime::Current()->IsAotCompiler());
  MethodReference ref = method_verifier->GetMethodReference();
  DexFileMethodArray* const array = GetMethodArray(ref.dex_file);
  if (array == nullptr) {
    // A classpath or a boot classpath method, that we are not going to compile.
    return;
  }
  DCHECK((*array)[ref.dex_method_index] == nullptr);
  (*array)[ref.dex_method_index] = VerifiedMethod::Create(method_verifier);
}

const VerifiedMethod* VerificationResults::GetVerifiedMethod(MethodReference ref) {
  DexFileMethodArray* array = GetMethodArray(ref.dex_file);
  if (array == nullptr) {
    return nullptr;
  }
  return (*array)[ref.dex_method_index];
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

void VerificationResults::PreRegisterDexFile(const DexFile* dex_file) {
  CHECK(preregistered_dex_files_.find(dex_file) == preregistered_dex_files_.end())
      << dex_file->GetLocation();
  DexFileMethodArray array(dex_file->NumMethodIds());
  WriterMutexLock mu(Thread::Current(), verified_methods_lock_);
  preregistered_dex_files_.emplace(dex_file, std::move(array));
}

void VerificationResults::DeleteResults(DexFileResults& array) {
  for (auto& pair : array) {
    for (const VerifiedMethod* method : pair.second) {
      delete method;
    }
  }
  array.clear();
}

VerificationResults::DexFileMethodArray* VerificationResults::GetMethodArray(
    const DexFile* dex_file) {
  auto it = preregistered_dex_files_.find(dex_file);
  if (it != preregistered_dex_files_.end()) {
    return &it->second;
  }
  return nullptr;
}

}  // namespace art
