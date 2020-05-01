/*
 * Copyright (C) 2020 The Android Open Source Project
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

#ifndef ART_DEX2OAT_INTERPRETER_ACTIVE_TRANSACTION_CHECKER_H_
#define ART_DEX2OAT_INTERPRETER_ACTIVE_TRANSACTION_CHECKER_H_

#include "gc/heap.h"
#include "mirror/class.h"
#include "runtime.h"
#include "transaction.h"

namespace art {
namespace interpreter {

// Helper class for checking constraints in transactional interpreter.
// The interface must be identical to the InactiveTransactionChecker counterpart in runtime.
class ActiveTransactionChecker {
 public:
  ALWAYS_INLINE static bool CheckWriteConstraint(Thread* self, ObjPtr<mirror::Object> obj)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    Runtime* runtime = Runtime::Current();
    if (runtime->GetTransaction()->WriteConstraint(self, obj)) {
      DCHECK(runtime->GetHeap()->ObjectIsInBootImageSpace(obj) || obj->IsClass());
      const char* extra = runtime->GetHeap()->ObjectIsInBootImageSpace(obj) ? "boot image " : "";
      Runtime::AbortTransactionF(self,
                                 "Can't set fields of %s%s",
                                 extra,
                                 obj->PrettyTypeOf().c_str());
      return false;
    }
    return true;
  }

  ALWAYS_INLINE static bool CheckWriteValueConstraint(Thread* self, ObjPtr<mirror::Object> value)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    Runtime* runtime = Runtime::Current();
    if (runtime->GetTransaction()->WriteValueConstraint(self, value)) {
      DCHECK(value != nullptr);
      const char* description = value->IsClass() ? "class" : "instance of";
      std::string descriptor =
          (value->IsClass() ? value->AsClass() : value->GetClass())->PrettyDescriptor();
      Runtime::AbortTransactionF(self,
                                 "Can't store reference to %s %s",
                                 description,
                                 descriptor.c_str());
      return false;
    }
    return true;
  }

  ALWAYS_INLINE static bool CheckAllocationConstraint(Thread* self, ObjPtr<mirror::Class> klass)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (klass->IsFinalizable()) {
      Runtime::AbortTransactionF(self,
                                 "Allocating finalizable object in transaction: %s",
                                 klass->PrettyDescriptor().c_str());
      return false;  // Pending exception.
    }
    return true;
  }
};

}  // namespace interpreter
}  // namespace art

#endif  // ART_DEX2OAT_INTERPRETER_ACTIVE_TRANSACTION_CHECKER_H_
