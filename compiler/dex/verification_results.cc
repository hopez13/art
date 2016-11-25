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
#include "utils/atomic_method_ref_map-inl.h"
#include "verified_method.h"
#include "verifier/method_verifier-inl.h"

namespace art {

VerificationResults::VerificationResults(const CompilerOptions* compiler_options)
    : compiler_options_(compiler_options),
      rejected_classes_lock_("compiler rejected classes lock") {}

VerificationResults::~VerificationResults() {
  verified_methods_.Visit([](const MethodReference& ref ATTRIBUTE_UNUSED,
                             const VerifiedMethod* method) {
    delete method;
  });
}

void VerificationResults::ProcessVerifiedMethod(verifier::MethodVerifier* method_verifier) {
  DCHECK(method_verifier != nullptr);
  DCHECK(Runtime::Current()->IsAotCompiler());
  MethodReference ref = method_verifier->GetMethodReference();
  std::unique_ptr<const VerifiedMethod> verified_method(VerifiedMethod::Create(method_verifier));
  if (verified_method == nullptr) {
    // We'll punt this later.
    return;
  }
  AtomicMap::InsertResult result = verified_methods_.Insert(ref,
                                                            /*expected*/ nullptr,
                                                            verified_method.get());
  CHECK(result != AtomicMap::kInsertResultCASFailure);
  if (result == AtomicMap::kInsertResultInvalidDexFile) {
    // A classpath or a boot classpath method, that we are not going to compile.
    return;
  }
  DCHECK_EQ(GetVerifiedMethod(ref), verified_method.get());
  DCHECK(result == AtomicMap::kInsertResultSuccess);
  // Successfully inserted the verified method, release the unique pointer since ownership has
  // been transferred.
  verified_method.release();
}

const VerifiedMethod* VerificationResults::GetVerifiedMethod(MethodReference ref) {
  const VerifiedMethod* ret = nullptr;
  if (!verified_methods_.Get(ref, &ret)) {
    DCHECK(ret == nullptr);
  }
  return ret;
}

void VerificationResults::CreateVerifiedMethodFor(MethodReference ref) {
  // This method should only be called for classes verified at compile time,
  // which have no verifier error, nor has methods that we know will throw
  // at runtime.
  AtomicMap::InsertResult result = verified_methods_.Insert(
      ref,
      /*expected*/ nullptr,
      new VerifiedMethod(/* encountered_error_types */ 0, /* has_runtime_throw */ false));
  DCHECK_EQ(result, AtomicMap::kInsertResultSuccess);
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

void VerificationResults::AddDexFile(const DexFile* dex_file) {
  if (!verified_methods_.HaveDexFile(dex_file)) {
    verified_methods_.AddDexFile(dex_file);
  }
}

}  // namespace art
