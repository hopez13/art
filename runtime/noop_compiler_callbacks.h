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

#ifndef ART_RUNTIME_NOOP_COMPILER_CALLBACKS_H_
#define ART_RUNTIME_NOOP_COMPILER_CALLBACKS_H_

#include "compiler_callbacks.h"

#include "base/macros.h"
#include "base/logging.h"

namespace art {

class NoopCompilerCallbacks final : public CompilerCallbacks {
 public:
  NoopCompilerCallbacks() : CompilerCallbacks(CompilerCallbacks::CallbackMode::kCompileApp) {}
  ~NoopCompilerCallbacks() {}

  void MethodVerified(verifier::MethodVerifier* verifier ATTRIBUTE_UNUSED) override {
  }

  void ClassRejected(ClassReference ref ATTRIBUTE_UNUSED) override {}

  verifier::VerifierDeps* GetVerifierDeps() const override { return nullptr; }

  // Transactional interpreter support.

  InterpreterPointer GetTransactionalInterpreter() override {
    LOG(FATAL) << "UNREACHABLE";
    UNREACHABLE();
  }

  InterpreterPointer GetTransactionalInterpreterWithAccessChecks() override {
    LOG(FATAL) << "UNREACHABLE";
    UNREACHABLE();
  }

  bool CheckTransactionWriteConstraint(Thread* self ATTRIBUTE_UNUSED,
                                       ObjPtr<mirror::Object> obj ATTRIBUTE_UNUSED) override
        REQUIRES_SHARED(Locks::mutator_lock_) {
    LOG(FATAL) << "UNREACHABLE";
    UNREACHABLE();
  }

  bool CheckTransactionWriteValueConstraint(Thread* self ATTRIBUTE_UNUSED,
                                            ObjPtr<mirror::Object> value ATTRIBUTE_UNUSED) override
      REQUIRES_SHARED(Locks::mutator_lock_) {
    LOG(FATAL) << "UNREACHABLE";
    UNREACHABLE();
  }

  bool CheckTransactionAllocationConstraint(Thread* self ATTRIBUTE_UNUSED,
                                            ObjPtr<mirror::Class> klass ATTRIBUTE_UNUSED) override
      REQUIRES_SHARED(Locks::mutator_lock_) {
    LOG(FATAL) << "UNREACHABLE";
    UNREACHABLE();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NoopCompilerCallbacks);
};

}  // namespace art

#endif  // ART_RUNTIME_NOOP_COMPILER_CALLBACKS_H_
