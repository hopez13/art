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

#include "common_runtime_test.h"
#include "verification.h"
#include "mirror/string.h"
#include "scoped_thread_state_change-inl.h"

namespace art {
namespace gc {

class VerificationTest : public CommonRuntimeTest {
 protected:
};

TEST_F(VerificationTest, IsValidHeapObjectAddress) {
  ScopedObjectAccess soa(Thread::Current());
  const Verification* const v = Runtime::Current()->GetHeap()->GetVerification();
  EXPECT_FALSE(v->IsValidHeapObjectAddress(reinterpret_cast<const void*>(1)));
  EXPECT_FALSE(v->IsValidHeapObjectAddress(reinterpret_cast<const void*>(4)));
  EXPECT_FALSE(v->IsValidHeapObjectAddress(nullptr));
  StackHandleScope<1> hs(soa.Self());
  Handle<mirror::String> string(
      hs.NewHandle(mirror::String::AllocFromModifiedUtf8(soa.Self(), "test")));
  EXPECT_TRUE(v->IsValidHeapObjectAddress(string.Get()));
  EXPECT_TRUE(v->IsValidHeapObjectAddress(string->GetClass()));
  const uintptr_t uint_klass = reinterpret_cast<uintptr_t>(string->GetClass());
  // Not actually a valid object but the verification can't know that. Guaranteed to be inside a
  // heap space.
  EXPECT_TRUE(v->IsValidHeapObjectAddress(
      reinterpret_cast<const void*>(uint_klass + kObjectAlignment)));
  EXPECT_FALSE(v->IsValidHeapObjectAddress(
      reinterpret_cast<const void*>(&uint_klass)));
}

TEST_F(VerificationTest, IsValidClass) {
  ScopedObjectAccess soa(Thread::Current());
  const Verification* const v = Runtime::Current()->GetHeap()->GetVerification();
  EXPECT_FALSE(v->IsValidClass(reinterpret_cast<const void*>(1)));
  EXPECT_FALSE(v->IsValidClass(reinterpret_cast<const void*>(4)));
  EXPECT_FALSE(v->IsValidClass(nullptr));
  StackHandleScope<1> hs(soa.Self());
  Handle<mirror::String> string(
      hs.NewHandle(mirror::String::AllocFromModifiedUtf8(soa.Self(), "test")));
  EXPECT_FALSE(v->IsValidClass(string.Get()));
  EXPECT_TRUE(v->IsValidClass(string->GetClass()));
  const uintptr_t uint_klass = reinterpret_cast<uintptr_t>(string->GetClass());
  EXPECT_FALSE(v->IsValidClass(reinterpret_cast<const void*>(uint_klass - kObjectAlignment)));
  EXPECT_FALSE(v->IsValidClass(reinterpret_cast<const void*>(&uint_klass)));
}

TEST_F(VerificationTest, DumpObjectInfo) {
  ScopedLogSeverity sls(LogSeverity::INFO);
  ScopedObjectAccess soa(Thread::Current());
  const Verification* const v = Runtime::Current()->GetHeap()->GetVerification();
  LOG(INFO) << v->DumpObjectInfo(reinterpret_cast<const void*>(1), "obj");
  LOG(INFO) << v->DumpObjectInfo(reinterpret_cast<const void*>(4), "obj");
  LOG(INFO) << v->DumpObjectInfo(nullptr, "obj");
  StackHandleScope<1> hs(soa.Self());
  Handle<mirror::String> string(
      hs.NewHandle(mirror::String::AllocFromModifiedUtf8(soa.Self(), "obj")));
  LOG(INFO) << v->DumpObjectInfo(string.Get(), "test");
  LOG(INFO) << v->DumpObjectInfo(string->GetClass(), "obj");
  const uintptr_t uint_klass = reinterpret_cast<uintptr_t>(string->GetClass());
  LOG(INFO) << v->DumpObjectInfo(reinterpret_cast<const void*>(uint_klass - kObjectAlignment),
                                 "obj");
  LOG(INFO) << v->DumpObjectInfo(reinterpret_cast<const void*>(&uint_klass), "obj");
}

}  // namespace gc
}  // namespace art
