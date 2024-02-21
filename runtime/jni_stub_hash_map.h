/*
 * Copyright (C) 2024 The Android Open Source Project
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

#ifndef ART_RUNTIME_JNI_STUB_HASH_MAP_H_
#define ART_RUNTIME_JNI_STUB_HASH_MAP_H_

#include <memory>
#include <string_view>

#include "arch/instruction_set.h"
#include "art_method.h"
#include "base/hash_map.h"

namespace art {

class JniStubKey;

using JniStubKeyHashFunction = size_t (*)(const JniStubKey& key);
using JniStubKeyEqualsFunction = bool (*)(const JniStubKey& lhs, const JniStubKey& rhs);

template <InstructionSet kIsa>
size_t JniStubKeyOptimizedHash(const JniStubKey& key);
template <InstructionSet kIsa>
size_t JniStubKeyGenericHash(const JniStubKey& key);

template <InstructionSet kIsa>
bool JniStubKeyOptimizedEquals(const JniStubKey& lhs, const JniStubKey& rhs);
template <InstructionSet kIsa>
bool JniStubKeyGenericEquals(const JniStubKey& lhs, const JniStubKey& rhs);

class JniStubKey {
 public:
  JniStubKey() = default;
  JniStubKey(const JniStubKey& other) = default;
  JniStubKey& operator=(const JniStubKey& other) = default;

  JniStubKey(uint32_t flags, std::string_view shorty) : shorty_(shorty) {
    DCHECK(ArtMethod::IsNative(flags));
    flags_ = flags & (kAccStatic | kAccSynchronized | kAccFastNative | kAccCriticalNative);
  }

  JniStubKey(ArtMethod* method) REQUIRES_SHARED(Locks::mutator_lock_) {
    uint32_t flags = method->GetAccessFlags();
    DCHECK(ArtMethod::IsNative(flags));
    flags_ = flags & (kAccStatic | kAccSynchronized | kAccFastNative | kAccCriticalNative);
    shorty_ = method->GetShortyView();
  }

  uint32_t Flags() const {
    return flags_;
  }

  std::string_view Shorty() const {
    return shorty_;
  }

  bool IsEmpty() const {
    return Shorty().empty();
  }

  void MakeEmpty() {
    shorty_ = {};
  }

 private:
  uint32_t flags_;
  std::string_view shorty_;
};

template <typename Value>
class JniStubKeyEmpty {
 public:
  bool IsEmpty(const std::pair<JniStubKey, Value>& pair) const {
    return pair.first.IsEmpty();
  }

  void MakeEmpty(std::pair<JniStubKey, Value>& pair) {
    pair.first.MakeEmpty();
  }
};

class JniStubKeyHash {
 public:
  explicit JniStubKeyHash(InstructionSet isa) {
    switch (isa) {
      case InstructionSet::kArm64:
        hash_func_ = JniStubKeyOptimizedHash<InstructionSet::kArm64>;
        break;
      case InstructionSet::kX86_64:
        hash_func_ = JniStubKeyOptimizedHash<InstructionSet::kX86_64>;
        break;
      case InstructionSet::kRiscv64:
        hash_func_ = JniStubKeyGenericHash<InstructionSet::kRiscv64>;
        break;
      case InstructionSet::kArm:
        hash_func_ = JniStubKeyGenericHash<InstructionSet::kArm>;
        break;
      case InstructionSet::kX86:
        hash_func_ = JniStubKeyGenericHash<InstructionSet::kX86>;
        break;
      default:
        UNREACHABLE();
    }
  }

  size_t operator()(const JniStubKey& key) const {
    return hash_func_(key);
  }

 private:
  JniStubKeyHashFunction hash_func_;
};

class JniStubKeyEquals {
 public:
  explicit JniStubKeyEquals(InstructionSet isa) {
    switch (isa) {
      case InstructionSet::kArm64:
        equals_func_ = JniStubKeyOptimizedEquals<InstructionSet::kArm64>;
        break;
      case InstructionSet::kX86_64:
        equals_func_ = JniStubKeyOptimizedEquals<InstructionSet::kX86_64>;
        break;
      case InstructionSet::kRiscv64:
        equals_func_ = JniStubKeyGenericEquals<InstructionSet::kRiscv64>;
        break;
      case InstructionSet::kArm:
        equals_func_ = JniStubKeyGenericEquals<InstructionSet::kArm>;
        break;
      case InstructionSet::kX86:
        equals_func_ = JniStubKeyGenericEquals<InstructionSet::kX86>;
        break;
      default:
        UNREACHABLE();
    }
  }

  bool operator()(const JniStubKey& lhs, const JniStubKey& rhs) const {
    return equals_func_(lhs, rhs);
  }

 private:
  JniStubKeyEqualsFunction equals_func_;
};

template <typename Value,
          class Alloc = std::allocator<std::pair<JniStubKey, Value>>>
using JniStubHashMap =
    HashMap<JniStubKey, Value, JniStubKeyEmpty<Value>, JniStubKeyHash, JniStubKeyEquals>;

} // namespace art

#endif // ART_RUNTIME_JNI_STUB_HASH_MAP_H_
