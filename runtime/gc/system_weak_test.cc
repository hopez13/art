/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "system_weak.h"

#include <stdint.h>
#include <stdio.h>
#include <memory>

#include "base/mutex.h"
#include "common_runtime_test.h"
#include "handle_scope-inl.h"
#include "heap.h"
#include "mirror/string.h"
#include "scoped_thread_state_change.h"
#include "thread_list.h"

namespace art {
namespace gc {

class SystemWeakTest : public CommonRuntimeTest {
};

struct CountingSystemWeakHolder : public SystemWeakHolder {
  CountingSystemWeakHolder()
      : SystemWeakHolder(kAllocTrackerLock),
        allow_count_(0),
        disallow_count_(0),
        sweep_count_(0) {}

  void AllowNewSystemWeaks() OVERRIDE
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!allow_disallow_lock_) {
    SystemWeakHolder::AllowNewSystemWeaks();

    allow_count_++;
  }

  void DisallowNewSystemWeaks() OVERRIDE
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!allow_disallow_lock_) {
    SystemWeakHolder::DisallowNewSystemWeaks();

    disallow_count_++;
  }

  void BroadcastForNewSystemWeaks() OVERRIDE
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!allow_disallow_lock_) {
    SystemWeakHolder::BroadcastForNewSystemWeaks();

    allow_count_++;
  }

  void SweepWeaks(IsMarkedVisitor* visitor) OVERRIDE REQUIRES_SHARED(Locks::mutator_lock_) {
    mirror::Object* old_object = weak_.Read<kWithoutReadBarrier>();
    mirror::Object* new_object = old_object == nullptr ? nullptr : visitor->IsMarked(old_object);
    weak_ = GcRoot<mirror::Object>(new_object);

    sweep_count_++;
  }

  size_t allow_count_;
  size_t disallow_count_;
  size_t sweep_count_;
  GcRoot<mirror::Object> weak_;
};

TEST_F(SystemWeakTest, Keep) {
  CountingSystemWeakHolder cswh;

  {
    ScopedSuspendAll ssa("Install holder");
    Runtime::Current()->AddSystemWeakHolder(&cswh);
  }

  ScopedObjectAccess soa(Thread::Current());

  StackHandleScope<1> hs(soa.Self());

  // We use Strings because they are very easy to allocate.
  Handle<mirror::String> s(hs.NewHandle(mirror::String::AllocFromModifiedUtf8(soa.Self(), "ABC")));
  cswh.weak_ = GcRoot<mirror::Object>(s.Get());

  // Trigger a GC.
  Runtime::Current()->GetHeap()->CollectGarbage(false);

  // Expect the holder to have been called.
  EXPECT_EQ(1U, cswh.allow_count_);
  EXPECT_EQ(1U, cswh.disallow_count_);
  EXPECT_EQ(1U, cswh.sweep_count_);

  // Expect the weak to not be cleared.
  EXPECT_TRUE(cswh.weak_.Read() != nullptr);
  EXPECT_EQ(cswh.weak_.Read(), s.Get());
}

TEST_F(SystemWeakTest, Discard) {
  CountingSystemWeakHolder cswh;

  {
    ScopedSuspendAll ssa("Install holder");
    Runtime::Current()->AddSystemWeakHolder(&cswh);
  }

  ScopedObjectAccess soa(Thread::Current());

  cswh.weak_ = GcRoot<mirror::Object>(mirror::String::AllocFromModifiedUtf8(soa.Self(), "ABC"));

  // Trigger a GC.
  Runtime::Current()->GetHeap()->CollectGarbage(false);

  // Expect the holder to have been called.
  EXPECT_EQ(1U, cswh.allow_count_);
  EXPECT_EQ(1U, cswh.disallow_count_);
  EXPECT_EQ(1U, cswh.sweep_count_);

  // Expect the weak to be cleared.
  EXPECT_TRUE(cswh.weak_.Read() == nullptr);
}

}  // namespace gc
}  // namespace art
