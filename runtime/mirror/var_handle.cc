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

#include "var_handle.h"

#include "array-inl.h"
#include "art_field-inl.h"
#include "class-inl.h"
#include "class_linker.h"
#include "gc_root-inl.h"
#include "jni_internal.h"
#include "jvalue-inl.h"
#include "method_handles.h"
#include "method_type.h"
#include "well_known_classes.h"

namespace art {
namespace mirror {

namespace {

struct VarHandleAccessorToAccessModeEntry {
  const char* method_name;
  VarHandle::AccessMode access_mode;

  // Binary predicate function for finding access_mode by
  // method_name. The access_mode field is ignored.
  static bool CompareName(const VarHandleAccessorToAccessModeEntry& lhs,
                          const VarHandleAccessorToAccessModeEntry& rhs) {
    return strcmp(lhs.method_name, rhs.method_name) < 0;
  }
};

// Map of VarHandle accessor method names to access mode values. The list is alpha-sorted to support
// binary search. For the usage scenario - lookups in the verifier - a linear scan would likely
// suffice since we expect VarHandles to be a lesser encountered class. We could use a std::hashmap
// here and this would be easier to maintain if new values are added here. However, this entails
// CPU cycles initializing the structure on every execution and uses O(N) more memory for
// intermediate nodes and makes that memory dirty. Compile-time magic using constexpr is possible
// here, but that's a tax when this code is recompiled.
const VarHandleAccessorToAccessModeEntry kAccessorToAccessMode[VarHandle::kNumberOfAccessModes] = {
  { "compareAndExchange", VarHandle::AccessMode::kCompareAndExchange },
  { "compareAndExchangeAcquire", VarHandle::AccessMode::kCompareAndExchangeAcquire },
  { "compareAndExchangeRelease", VarHandle::AccessMode::kCompareAndExchangeRelease },
  { "compareAndSet", VarHandle::AccessMode::kCompareAndSet },
  { "get", VarHandle::AccessMode::kGet },
  { "getAcquire", VarHandle::AccessMode::kGetAcquire },
  { "getAndAdd", VarHandle::AccessMode::kGetAndAdd },
  { "getAndAddAcquire", VarHandle::AccessMode::kGetAndAddAcquire },
  { "getAndAddRelease", VarHandle::AccessMode::kGetAndAddRelease },
  { "getAndBitwiseAnd", VarHandle::AccessMode::kGetAndBitwiseAnd },
  { "getAndBitwiseAndAcquire", VarHandle::AccessMode::kGetAndBitwiseAndAcquire },
  { "getAndBitwiseAndRelease", VarHandle::AccessMode::kGetAndBitwiseAndRelease },
  { "getAndBitwiseOr", VarHandle::AccessMode::kGetAndBitwiseOr },
  { "getAndBitwiseOrAcquire", VarHandle::AccessMode::kGetAndBitwiseOrAcquire },
  { "getAndBitwiseOrRelease", VarHandle::AccessMode::kGetAndBitwiseOrRelease },
  { "getAndBitwiseXor", VarHandle::AccessMode::kGetAndBitwiseXor },
  { "getAndBitwiseXorAcquire", VarHandle::AccessMode::kGetAndBitwiseXorAcquire },
  { "getAndBitwiseXorRelease", VarHandle::AccessMode::kGetAndBitwiseXorRelease },
  { "getAndSet", VarHandle::AccessMode::kGetAndSet },
  { "getAndSetAcquire", VarHandle::AccessMode::kGetAndSetAcquire },
  { "getAndSetRelease", VarHandle::AccessMode::kGetAndSetRelease },
  { "getOpaque", VarHandle::AccessMode::kGetOpaque },
  { "getVolatile", VarHandle::AccessMode::kGetVolatile },
  { "set", VarHandle::AccessMode::kSet },
  { "setOpaque", VarHandle::AccessMode::kSetOpaque },
  { "setRelease", VarHandle::AccessMode::kSetRelease },
  { "setVolatile", VarHandle::AccessMode::kSetVolatile },
  { "weakCompareAndSet", VarHandle::AccessMode::kWeakCompareAndSet },
  { "weakCompareAndSetAcquire", VarHandle::AccessMode::kWeakCompareAndSetAcquire },
  { "weakCompareAndSetPlain", VarHandle::AccessMode::kWeakCompareAndSetPlain },
  { "weakCompareAndSetRelease", VarHandle::AccessMode::kWeakCompareAndSetRelease },
};

// Enumeration for describing the parameter and return types of an AccessMode.
enum class AccessModeTemplate : uint32_t {
  kGet,                 // T Op(C0..CN)
  kSet,                 // void Op(C0..CN, T)
  kCompareAndSet,       // boolean Op(C0..CN, T, T)
  kCompareAndExchange,  // T Op(C0..CN, T, T)
  kGetAndUpdate,        // T Op(C0..CN, T)
};

// Look up the AccessModeTemplate for a given VarHandle
// AccessMode. This simplifies finding the correct signature for a
// VarHandle accessor method.
AccessModeTemplate GetAccessModeTemplate(VarHandle::AccessMode access_mode) {
  switch (access_mode) {
    case VarHandle::AccessMode::kGet:
      return AccessModeTemplate::kGet;
    case VarHandle::AccessMode::kSet:
      return AccessModeTemplate::kSet;
    case VarHandle::AccessMode::kGetVolatile:
      return AccessModeTemplate::kGet;
    case VarHandle::AccessMode::kSetVolatile:
      return AccessModeTemplate::kSet;
    case VarHandle::AccessMode::kGetAcquire:
      return AccessModeTemplate::kGet;
    case VarHandle::AccessMode::kSetRelease:
      return AccessModeTemplate::kSet;
    case VarHandle::AccessMode::kGetOpaque:
      return AccessModeTemplate::kGet;
    case VarHandle::AccessMode::kSetOpaque:
      return AccessModeTemplate::kSet;
    case VarHandle::AccessMode::kCompareAndSet:
      return AccessModeTemplate::kCompareAndSet;
    case VarHandle::AccessMode::kCompareAndExchange:
      return AccessModeTemplate::kCompareAndExchange;
    case VarHandle::AccessMode::kCompareAndExchangeAcquire:
      return AccessModeTemplate::kCompareAndExchange;
    case VarHandle::AccessMode::kCompareAndExchangeRelease:
      return AccessModeTemplate::kCompareAndExchange;
    case VarHandle::AccessMode::kWeakCompareAndSetPlain:
      return AccessModeTemplate::kCompareAndSet;
    case VarHandle::AccessMode::kWeakCompareAndSet:
      return AccessModeTemplate::kCompareAndSet;
    case VarHandle::AccessMode::kWeakCompareAndSetAcquire:
      return AccessModeTemplate::kCompareAndSet;
    case VarHandle::AccessMode::kWeakCompareAndSetRelease:
      return AccessModeTemplate::kCompareAndSet;
    case VarHandle::AccessMode::kGetAndSet:
      return AccessModeTemplate::kGetAndUpdate;
    case VarHandle::AccessMode::kGetAndSetAcquire:
      return AccessModeTemplate::kGetAndUpdate;
    case VarHandle::AccessMode::kGetAndSetRelease:
      return AccessModeTemplate::kGetAndUpdate;
    case VarHandle::AccessMode::kGetAndAdd:
      return AccessModeTemplate::kGetAndUpdate;
    case VarHandle::AccessMode::kGetAndAddAcquire:
      return AccessModeTemplate::kGetAndUpdate;
    case VarHandle::AccessMode::kGetAndAddRelease:
      return AccessModeTemplate::kGetAndUpdate;
    case VarHandle::AccessMode::kGetAndBitwiseOr:
      return AccessModeTemplate::kGetAndUpdate;
    case VarHandle::AccessMode::kGetAndBitwiseOrRelease:
      return AccessModeTemplate::kGetAndUpdate;
    case VarHandle::AccessMode::kGetAndBitwiseOrAcquire:
      return AccessModeTemplate::kGetAndUpdate;
    case VarHandle::AccessMode::kGetAndBitwiseAnd:
      return AccessModeTemplate::kGetAndUpdate;
    case VarHandle::AccessMode::kGetAndBitwiseAndRelease:
      return AccessModeTemplate::kGetAndUpdate;
    case VarHandle::AccessMode::kGetAndBitwiseAndAcquire:
      return AccessModeTemplate::kGetAndUpdate;
    case VarHandle::AccessMode::kGetAndBitwiseXor:
      return AccessModeTemplate::kGetAndUpdate;
    case VarHandle::AccessMode::kGetAndBitwiseXorRelease:
      return AccessModeTemplate::kGetAndUpdate;
    case VarHandle::AccessMode::kGetAndBitwiseXorAcquire:
      return AccessModeTemplate::kGetAndUpdate;
  }
}

size_t GetNumberOfVarTypeParameters(AccessModeTemplate access_mode_template) {
  switch (access_mode_template) {
    case AccessModeTemplate::kGet:
      return 0u;
    case AccessModeTemplate::kSet:
    case AccessModeTemplate::kGetAndUpdate:
      return 1u;
    case AccessModeTemplate::kCompareAndSet:
    case AccessModeTemplate::kCompareAndExchange:
      return 2u;
  }
  UNREACHABLE();
}

// Returns the number of parameters associated with an
// AccessModeTemplate and the supplied coordinate types.
int32_t GetParameterCount(AccessModeTemplate access_mode_template,
                          ObjPtr<Class> coordinateType0,
                          ObjPtr<Class> coordinateType1) {
  int32_t count = 0;
  if (!coordinateType0.IsNull()) {
    count++;
    if (!coordinateType1.IsNull()) {
      count++;
    }
  }
  return count + static_cast<int32_t>(GetNumberOfVarTypeParameters(access_mode_template));
}

// Writes the parameter types associated with the AccessModeTemplate
// into an array. The parameter types are derived from the specified
// variable type and coordinate types. Returns the number of
// parameters written.
int32_t BuildParameterArray(ObjPtr<Class> (&parameters)[VarHandle::kMaxAccessorParameters],
                            AccessModeTemplate access_mode_template,
                            ObjPtr<Class> varType,
                            ObjPtr<Class> coordinateType0,
                            ObjPtr<Class> coordinateType1)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  DCHECK(varType != nullptr);
  int32_t index = 0;
  if (!coordinateType0.IsNull()) {
    parameters[index++] = coordinateType0;
    if (!coordinateType1.IsNull()) {
      parameters[index++] = coordinateType1;
    }
  } else {
    DCHECK(coordinateType1.IsNull());
  }

  switch (access_mode_template) {
    case AccessModeTemplate::kCompareAndExchange:
    case AccessModeTemplate::kCompareAndSet:
      parameters[index++] = varType;
      parameters[index++] = varType;
      return index;
    case AccessModeTemplate::kGet:
      return index;
    case AccessModeTemplate::kGetAndUpdate:
    case AccessModeTemplate::kSet:
      parameters[index++] = varType;
      return index;
  }
  return -1;
}

// Returns the return type associated with an AccessModeTemplate based
// on the template and the variable type specified.
Class* GetReturnType(AccessModeTemplate access_mode_template, ObjPtr<Class> varType)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  DCHECK(varType != nullptr);
  switch (access_mode_template) {
    case AccessModeTemplate::kCompareAndSet:
      return Runtime::Current()->GetClassLinker()->FindPrimitiveClass('Z');
    case AccessModeTemplate::kCompareAndExchange:
    case AccessModeTemplate::kGet:
    case AccessModeTemplate::kGetAndUpdate:
      return varType.Ptr();
    case AccessModeTemplate::kSet:
      return Runtime::Current()->GetClassLinker()->FindPrimitiveClass('V');
  }
  return nullptr;
}

ObjectArray<Class>* NewArrayOfClasses(Thread* self, int count)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  Runtime* const runtime = Runtime::Current();
  ClassLinker* const class_linker = runtime->GetClassLinker();
  ObjPtr<mirror::Class> class_type = mirror::Class::GetJavaLangClass();
  ObjPtr<mirror::Class> array_of_class = class_linker->FindArrayClass(self, &class_type);
  return ObjectArray<Class>::Alloc(Thread::Current(), array_of_class, count);
}

// Method to insert a read barrier for accessors to reference fields.
inline void ReadBarrierForVarHandleAccess(ObjPtr<Object> obj, MemberOffset field_offset)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (kUseReadBarrier) {
    // We need ensure that the reference stored in the field is a to-space one before attempting
    // the CompareAndSet/CompareAndExchange/Exchange operation otherwise it will fail incorrectly
    // if obj is in the process of being moved.
    uint8_t* raw_field_addr = reinterpret_cast<uint8_t*>(obj.Ptr()) + field_offset.SizeValue();
    auto field_addr = reinterpret_cast<mirror::HeapReference<mirror::Object>*>(raw_field_addr);
    // Note that the read barrier load does NOT need to be volatile.
    static constexpr bool kIsVolatile = false;
    static constexpr bool kAlwaysUpdateField = true;
    ReadBarrier::Barrier<mirror::Object, kIsVolatile, kWithReadBarrier, kAlwaysUpdateField>(
        obj.Ptr(),
        MemberOffset(field_offset),
        field_addr);
  }
}

//
// Helper methods for storing results from atomic operations into
// JValue instances.
//

inline void StoreResult(uint8_t value, JValue* result) {
  result->SetZ(value);
}

inline void StoreResult(int8_t value, JValue* result) {
  result->SetB(value);
}

inline void StoreResult(uint16_t value, JValue* result) {
  result->SetC(value);
}

inline void StoreResult(int16_t value, JValue* result) {
  result->SetS(value);
}

inline void StoreResult(int32_t value, JValue* result) {
  result->SetI(value);
}

inline void StoreResult(int64_t value, JValue* result) {
  result->SetJ(value);
}

inline void StoreResult(float value, JValue* result) {
  result->SetF(value);
}

inline void StoreResult(double value, JValue* result) {
  result->SetD(value);
}

inline void StoreResult(ObjPtr<Object> value, JValue* result)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  result->SetL(value);
}

//
// Helper class for byte-swapping value that has been stored in a JValue.
//

template <typename T>
class JValueByteSwapper FINAL {
 public:
  static void ByteSwap(JValue* value);
};

template <>
void JValueByteSwapper<uint16_t>::ByteSwap(JValue* value) {
  value->SetC(BSWAP(value->GetC()));
}

template <>
void JValueByteSwapper<int16_t>::ByteSwap(JValue* value) {
  value->SetS(BSWAP(value->GetS()));
}

template <>
void JValueByteSwapper<int32_t>::ByteSwap(JValue* value) {
  value->SetI(BSWAP(value->GetI()));
}

template <>
void JValueByteSwapper<int64_t>::ByteSwap(JValue* value) {
  value->SetJ(BSWAP(value->GetJ()));
}

//
// Accessor implementations, shared across all VarHandle types.
//

template <typename T, std::memory_order MO>
class AtomicGetAccessor : public Object::Accessor<T> {
 public:
  explicit AtomicGetAccessor(JValue* result) : result_(result) {}
  void Access(T* addr) OVERRIDE {
    std::atomic<T>* atom = reinterpret_cast<std::atomic<T>*>(addr);
    StoreResult(atom->load(MO), result_);
  }
 private:
  JValue* result_;
};

template <typename T, std::memory_order MO>
class AtomicSetAccessor : public Object::Accessor<T> {
 public:
  explicit AtomicSetAccessor(T new_value) : new_value_(new_value) {}
  void Access(T* addr) OVERRIDE {
    std::atomic<T>* atom = reinterpret_cast<std::atomic<T>*>(addr);
    atom->store(new_value_, MO);
  }
 private:
  T new_value_;
};

template <typename T> using GetAccessor = AtomicGetAccessor<T, std::memory_order_relaxed>;

template <typename T> using SetAccessor = AtomicSetAccessor<T, std::memory_order_relaxed>;

template <typename T>
using GetVolatileAccessor = AtomicGetAccessor<T, std::memory_order_seq_cst>;

template <typename T>
using SetVolatileAccessor = AtomicSetAccessor<T, std::memory_order_seq_cst>;

template <typename T>
using GetAcquireAccessor = AtomicGetAccessor<T, std::memory_order_acquire>;

template <typename T>
using SetReleaseAccessor = AtomicSetAccessor<T, std::memory_order_release>;

template <typename T>
using GetOpaqueAccessor = AtomicGetAccessor<T, std::memory_order_relaxed>;

template <typename T>
using SetOpaqueAccessor = AtomicSetAccessor<T, std::memory_order_relaxed>;

template <typename T, std::memory_order MOS, std::memory_order MOF>
class AtomicStrongCompareAndSetAccessor : public Object::Accessor<T> {
 public:
  AtomicStrongCompareAndSetAccessor(T expected_value, T desired_value, JValue* result)
      : expected_value_(expected_value), desired_value_(desired_value), result_(result) {}
  void Access(T* addr) OVERRIDE {
    std::atomic<T>* atom = reinterpret_cast<std::atomic<T>*>(addr);
    bool success = atom->compare_exchange_strong(expected_value_, desired_value_, MOS, MOF);
    StoreResult(static_cast<uint8_t>(success), result_);
  }
 private:
  T expected_value_;
  T desired_value_;
  JValue* result_;
};

template<typename T>
using CompareAndSetAccessor =
    AtomicStrongCompareAndSetAccessor<T, std::memory_order_seq_cst, std::memory_order_seq_cst>;

template <typename T, std::memory_order MOS, std::memory_order MOF>
class AtomicStrongCompareAndExchangeAccessor : public Object::Accessor<T> {
 public:
  AtomicStrongCompareAndExchangeAccessor(T expected_value, T desired_value, JValue* result)
      : expected_value_(expected_value), desired_value_(desired_value), result_(result) {}
  void Access(T* addr) OVERRIDE {
    std::atomic<T>* atom = reinterpret_cast<std::atomic<T>*>(addr);
    atom->compare_exchange_strong(expected_value_, desired_value_, MOS, MOF);
    StoreResult(expected_value_, result_);
  }
 private:
  T expected_value_;
  T desired_value_;
  JValue* result_;
};

template <typename T>
using CompareAndExchangeAccessor =
    AtomicStrongCompareAndExchangeAccessor<T, std::memory_order_seq_cst, std::memory_order_seq_cst>;

template <typename T>
using CompareAndExchangeAcquireAccessor =
    AtomicStrongCompareAndExchangeAccessor<T, std::memory_order_acquire, std::memory_order_acquire>;

template <typename T>
using CompareAndExchangeReleaseAccessor =
    AtomicStrongCompareAndExchangeAccessor<T, std::memory_order_release, std::memory_order_relaxed>;

template <typename T, std::memory_order MOS, std::memory_order MOF>
class AtomicWeakCompareAndSetAccessor : public Object::Accessor<T> {
 public:
  AtomicWeakCompareAndSetAccessor(T expected_value, T desired_value, JValue* result)
      : expected_value_(expected_value), desired_value_(desired_value), result_(result) {}
  void Access(T* addr) OVERRIDE {
    std::atomic<T>* atom = reinterpret_cast<std::atomic<T>*>(addr);
    bool success = atom->compare_exchange_weak(expected_value_, desired_value_, MOS, MOF);
    StoreResult(static_cast<uint8_t>(success), result_);
  }
 private:
  T expected_value_;
  T desired_value_;
  JValue* result_;
};

template <typename T>
using WeakCompareAndSetPlainAccessor =
    AtomicWeakCompareAndSetAccessor<T, std::memory_order_relaxed, std::memory_order_relaxed>;

template <typename T>
using WeakCompareAndSetAccessor =
    AtomicWeakCompareAndSetAccessor<T, std::memory_order_seq_cst, std::memory_order_seq_cst>;

template <typename T>
using WeakCompareAndSetAcquireAccessor =
    AtomicWeakCompareAndSetAccessor<T, std::memory_order_acquire, std::memory_order_acquire>;

template <typename T>
using WeakCompareAndSetReleaseAccessor =
    AtomicWeakCompareAndSetAccessor<T, std::memory_order_release, std::memory_order_relaxed>;

template <typename T, std::memory_order MO>
class AtomicGetAndSetAccessor : public Object::Accessor<T> {
 public:
  explicit AtomicGetAndSetAccessor(T new_value, JValue* result)
      : new_value_(new_value), result_(result) {}
  void Access(T* addr) OVERRIDE {
    std::atomic<T>* atom = reinterpret_cast<std::atomic<T>*>(addr);
    T old_value = atom->exchange(new_value_, MO);
    StoreResult(old_value, result_);
  }
 private:
  T new_value_;
  JValue* result_;
};

template <typename T>
using GetAndSetAccessor = AtomicGetAndSetAccessor<T, std::memory_order_seq_cst>;

template <typename T>
using GetAndSetAcquireAccessor = AtomicGetAndSetAccessor<T, std::memory_order_acquire>;

template <typename T>
using GetAndSetReleaseAccessor = AtomicGetAndSetAccessor<T, std::memory_order_release>;

template <typename T, std::memory_order MO>
class AtomicGetAndAddAccessor : public Object::Accessor<T> {
 public:
  explicit AtomicGetAndAddAccessor(T value, JValue* result)
      : value_(value), result_(result) {}
  void Access(T* addr) OVERRIDE {
    std::atomic<T>* atom = reinterpret_cast<std::atomic<T>*>(addr);
    T old_value = atom->fetch_add(value_, MO);
    StoreResult(old_value, result_);
  }
 private:
  T value_;
  JValue* result_;
};

template <typename T>
using GetAndAddAccessor = AtomicGetAndAddAccessor<T, std::memory_order_seq_cst>;

template <typename T>
using GetAndAddAcquireAccessor = AtomicGetAndAddAccessor<T, std::memory_order_acquire>;

template <typename T>
using GetAndAddReleaseAccessor = AtomicGetAndAddAccessor<T, std::memory_order_release>;

template <typename T, std::memory_order MO>
class AtomicGetAndBitwiseOrAccessor : public Object::Accessor<T> {
 public:
  explicit AtomicGetAndBitwiseOrAccessor(T value, JValue* result)
      : value_(value), result_(result) {}
  void Access(T* addr) OVERRIDE {
    std::atomic<T>* atom = reinterpret_cast<std::atomic<T>*>(addr);
    T old_value = atom->fetch_or(value_, MO);
    StoreResult(old_value, result_);
  }
 private:
  T value_;
  JValue* result_;
};

template <typename T>
using GetAndBitwiseOrAccessor = AtomicGetAndBitwiseOrAccessor<T, std::memory_order_seq_cst>;

template <typename T>
using GetAndBitwiseOrAcquireAccessor = AtomicGetAndBitwiseOrAccessor<T, std::memory_order_acquire>;

template <typename T>
using GetAndBitwiseOrReleaseAccessor = AtomicGetAndBitwiseOrAccessor<T, std::memory_order_release>;

template <typename T, std::memory_order MO>
class AtomicGetAndBitwiseAndAccessor : public Object::Accessor<T> {
 public:
  explicit AtomicGetAndBitwiseAndAccessor(T value, JValue* result)
      : value_(value), result_(result) {}
  void Access(T* addr) OVERRIDE {
    std::atomic<T>* atom = reinterpret_cast<std::atomic<T>*>(addr);
    T old_value = atom->fetch_and(value_, MO);
    StoreResult(old_value, result_);
  }
 private:
  T value_;
  JValue* result_;
};

template <typename T>
using GetAndBitwiseAndAccessor =
    AtomicGetAndBitwiseAndAccessor<T, std::memory_order_seq_cst>;

template <typename T>
using GetAndBitwiseAndAcquireAccessor =
    AtomicGetAndBitwiseAndAccessor<T, std::memory_order_acquire>;

template <typename T>
using GetAndBitwiseAndReleaseAccessor =
    AtomicGetAndBitwiseAndAccessor<T, std::memory_order_release>;

template <typename T, std::memory_order MO>
class AtomicGetAndBitwiseXorAccessor : public Object::Accessor<T> {
 public:
  explicit AtomicGetAndBitwiseXorAccessor(T value, JValue* result)
      : value_(value), result_(result) {}
  void Access(T* addr) OVERRIDE {
    std::atomic<T>* atom = reinterpret_cast<std::atomic<T>*>(addr);
    T old_value = atom->fetch_xor(value_, MO);
    StoreResult(old_value, result_);
  }
 private:
  T value_;
  JValue* result_;
};

template <typename T>
using GetAndBitwiseXorAccessor = AtomicGetAndBitwiseXorAccessor<T, std::memory_order_seq_cst>;

template <typename T>
using GetAndBitwiseXorAcquireAccessor =
    AtomicGetAndBitwiseXorAccessor<T, std::memory_order_acquire>;

template <typename T>
using GetAndBitwiseXorReleaseAccessor =
    AtomicGetAndBitwiseXorAccessor<T, std::memory_order_release>;

//
// Unreachable access modes.
//

NO_RETURN void UnreachableAccessMode(const char* access_mode, const char* type_name) {
  LOG(FATAL) << "Unreachable access mode :" << access_mode << " for type " << type_name;
  UNREACHABLE();
}

#define UNREACHABLE_ACCESS_MODE(ACCESS_MODE, TYPE)             \
template<> void ACCESS_MODE ## Accessor<TYPE>::Access(TYPE*) { \
  UnreachableAccessMode(#ACCESS_MODE, #TYPE);                  \
}

UNREACHABLE_ACCESS_MODE(GetAndAdd, float);
UNREACHABLE_ACCESS_MODE(GetAndAddAcquire, float);
UNREACHABLE_ACCESS_MODE(GetAndAddRelease, float);
UNREACHABLE_ACCESS_MODE(GetAndBitwiseOr, float);
UNREACHABLE_ACCESS_MODE(GetAndBitwiseOrAcquire, float);
UNREACHABLE_ACCESS_MODE(GetAndBitwiseOrRelease, float);
UNREACHABLE_ACCESS_MODE(GetAndBitwiseAnd, float);
UNREACHABLE_ACCESS_MODE(GetAndBitwiseAndAcquire, float);
UNREACHABLE_ACCESS_MODE(GetAndBitwiseAndRelease, float);
UNREACHABLE_ACCESS_MODE(GetAndBitwiseXor, float);
UNREACHABLE_ACCESS_MODE(GetAndBitwiseXorAcquire, float);
UNREACHABLE_ACCESS_MODE(GetAndBitwiseXorRelease, float);

UNREACHABLE_ACCESS_MODE(GetAndAdd, double);
UNREACHABLE_ACCESS_MODE(GetAndAddAcquire, double);
UNREACHABLE_ACCESS_MODE(GetAndAddRelease, double);
UNREACHABLE_ACCESS_MODE(GetAndBitwiseOr, double);
UNREACHABLE_ACCESS_MODE(GetAndBitwiseOrAcquire, double);
UNREACHABLE_ACCESS_MODE(GetAndBitwiseOrRelease, double);
UNREACHABLE_ACCESS_MODE(GetAndBitwiseAnd, double);
UNREACHABLE_ACCESS_MODE(GetAndBitwiseAndAcquire, double);
UNREACHABLE_ACCESS_MODE(GetAndBitwiseAndRelease, double);
UNREACHABLE_ACCESS_MODE(GetAndBitwiseXor, double);
UNREACHABLE_ACCESS_MODE(GetAndBitwiseXorAcquire, double);
UNREACHABLE_ACCESS_MODE(GetAndBitwiseXorRelease, double);

// A helper class for object field accesses for floats and
// doubles. The object interface deals with Field32 and Field64. The
// former is used for both integers and floats, the latter for longs
// and doubles. This class provides the necessary coercion.
template <typename T, typename U>
class TypeAdaptorAccessor : public Object::Accessor<T> {
 public:
  explicit TypeAdaptorAccessor(Object::Accessor<U>* inner_accessor)
      : inner_accessor_(inner_accessor) {}
  virtual void Access(T* addr) {
    static_assert(sizeof(T) == sizeof(U), "bad conversion");
    inner_accessor_->Access(reinterpret_cast<U*>(addr));
  }
 private:
  Object::Accessor<U>* inner_accessor_;
};

template <typename T>
class FieldAccessViaAccessor {
 public:
  typedef Object::Accessor<T> Accessor;

  // Apply an Accessor to get a field in an object.
  static void Get(ObjPtr<Object> obj,
                  MemberOffset field_offset,
                  Accessor* accessor)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Apply an Accessor to update a field in an object.
  static void Update(ObjPtr<Object> obj,
                     MemberOffset field_offset,
                     Accessor* accessor)
      REQUIRES_SHARED(Locks::mutator_lock_);
};

template <>
void FieldAccessViaAccessor<uint8_t>::Get(ObjPtr<Object> obj,
                                          MemberOffset field_offset,
                                          Accessor* accessor)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  obj->GetFieldBooleanViaAccessor(field_offset, accessor);
}

template <>
inline void FieldAccessViaAccessor<int8_t>::Get(ObjPtr<Object> obj,
                                                MemberOffset field_offset,
                                                Accessor* accessor)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  obj->GetFieldByteViaAccessor(field_offset, accessor);
}

template <>
inline void FieldAccessViaAccessor<uint16_t>::Get(ObjPtr<Object> obj,
                                                  MemberOffset field_offset,
                                                  Accessor* accessor)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  obj->GetFieldCharViaAccessor(field_offset, accessor);
}

template <>
inline void FieldAccessViaAccessor<int16_t>::Get(ObjPtr<Object> obj,
                                                 MemberOffset field_offset,
                                                 Accessor* accessor)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  obj->GetFieldShortViaAccessor(field_offset, accessor);
}

template <>
inline void FieldAccessViaAccessor<int32_t>::Get(ObjPtr<Object> obj,
                                                 MemberOffset field_offset,
                                                 Accessor* accessor)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  obj->GetField32ViaAccessor(field_offset, accessor);
}

template <>
inline void FieldAccessViaAccessor<int64_t>::Get(ObjPtr<Object> obj,
                                                 MemberOffset field_offset,
                                                 Accessor* accessor)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  obj->GetField64ViaAccessor(field_offset, accessor);
}

template <>
inline void FieldAccessViaAccessor<float>::Get(ObjPtr<Object> obj,
                                               MemberOffset field_offset,
                                               Accessor* accessor)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  TypeAdaptorAccessor<int32_t, float> float_to_int_accessor(accessor);
  obj->GetField32ViaAccessor(field_offset, &float_to_int_accessor);
}

template <>
inline void FieldAccessViaAccessor<double>::Get(ObjPtr<Object> obj,
                                                MemberOffset field_offset,
                                                Accessor* accessor)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  TypeAdaptorAccessor<int64_t, double> double_to_int_accessor(accessor);
  obj->GetField64ViaAccessor(field_offset, &double_to_int_accessor);
}

template <>
void FieldAccessViaAccessor<uint8_t>::Update(ObjPtr<Object> obj,
                                             MemberOffset field_offset,
                                             Accessor* accessor)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (Runtime::Current()->IsActiveTransaction()) {
    obj->UpdateFieldBooleanViaAccessor</*kTransactionActive*/true>(field_offset, accessor);
  } else {
    obj->UpdateFieldBooleanViaAccessor</*kTransactionActive*/false>(field_offset, accessor);
  }
}

template <>
void FieldAccessViaAccessor<int8_t>::Update(ObjPtr<Object> obj,
                                            MemberOffset field_offset,
                                            Accessor* accessor)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (Runtime::Current()->IsActiveTransaction()) {
    obj->UpdateFieldByteViaAccessor</*kTransactionActive*/true>(field_offset, accessor);
  } else {
    obj->UpdateFieldByteViaAccessor</*kTransactionActive*/false>(field_offset, accessor);
  }
}

template <>
void FieldAccessViaAccessor<uint16_t>::Update(ObjPtr<Object> obj,
                                              MemberOffset field_offset,
                                              Accessor* accessor)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (Runtime::Current()->IsActiveTransaction()) {
    obj->UpdateFieldCharViaAccessor</*kTransactionActive*/true>(field_offset, accessor);
  } else {
    obj->UpdateFieldCharViaAccessor</*kTransactionActive*/false>(field_offset, accessor);
  }
}

template <>
void FieldAccessViaAccessor<int16_t>::Update(ObjPtr<Object> obj,
                                              MemberOffset field_offset,
                                              Accessor* accessor)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (Runtime::Current()->IsActiveTransaction()) {
    obj->UpdateFieldShortViaAccessor</*kTransactionActive*/true>(field_offset, accessor);
  } else {
    obj->UpdateFieldShortViaAccessor</*kTransactionActive*/false>(field_offset, accessor);
  }
}

template <>
void FieldAccessViaAccessor<int32_t>::Update(ObjPtr<Object> obj,
                                             MemberOffset field_offset,
                                             Accessor* accessor)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (Runtime::Current()->IsActiveTransaction()) {
    obj->UpdateField32ViaAccessor</*kTransactionActive*/true>(field_offset, accessor);
  } else {
    obj->UpdateField32ViaAccessor</*kTransactionActive*/false>(field_offset, accessor);
  }
}

template <>
void FieldAccessViaAccessor<int64_t>::Update(ObjPtr<Object> obj,
                                             MemberOffset field_offset,
                                             Accessor* accessor)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (Runtime::Current()->IsActiveTransaction()) {
    obj->UpdateField64ViaAccessor</*kTransactionActive*/true>(field_offset, accessor);
  } else {
    obj->UpdateField64ViaAccessor</*kTransactionActive*/false>(field_offset, accessor);
  }
}

template <>
void FieldAccessViaAccessor<float>::Update(ObjPtr<Object> obj,
                                           MemberOffset field_offset,
                                           Accessor* accessor)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  TypeAdaptorAccessor<int32_t, float> float_to_int_accessor(accessor);
  if (Runtime::Current()->IsActiveTransaction()) {
    obj->UpdateField32ViaAccessor</*kTransactionActive*/true>(field_offset,
                                                               &float_to_int_accessor);
  } else {
    obj->UpdateField32ViaAccessor</*kTransactionActive*/false>(field_offset,
                                                                &float_to_int_accessor);
  }
}

template <>
void FieldAccessViaAccessor<double>::Update(ObjPtr<Object> obj,
                                            MemberOffset field_offset,
                                            Accessor* accessor)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  TypeAdaptorAccessor<int64_t, double> double_to_int_accessor(accessor);
  if (Runtime::Current()->IsActiveTransaction()) {
    obj->UpdateField64ViaAccessor</*kTransactionActive*/true>(field_offset,
                                                               &double_to_int_accessor);
  } else {
    obj->UpdateField64ViaAccessor</*kTransactionActive*/false>(field_offset,
                                                                &double_to_int_accessor);
  }
}

// Helper class that gets values from a shadow frame with appropriate type coercion.
template <typename T>
class ValueGetter {
 public:
  static T Get(ShadowFrameGetter* getter) REQUIRES_SHARED(Locks::mutator_lock_);
};

template <>
int8_t ValueGetter<int8_t>::Get(ShadowFrameGetter* getter) {
  uint32_t raw_value = getter->Get();
  return static_cast<int8_t>(raw_value);
}

template <>
uint8_t ValueGetter<uint8_t>::Get(ShadowFrameGetter* getter) {
  uint32_t raw_value = getter->Get();
  return static_cast<uint8_t>(raw_value);
}

template <>
int16_t ValueGetter<int16_t>::Get(ShadowFrameGetter* getter) {
  uint32_t raw_value = getter->Get();
  return static_cast<int16_t>(raw_value);
}

template <>
uint16_t ValueGetter<uint16_t>::Get(ShadowFrameGetter* getter) {
  uint32_t raw_value = getter->Get();
  return static_cast<uint16_t>(raw_value);
}

template <>
int32_t ValueGetter<int32_t>::Get(ShadowFrameGetter* getter) {
  uint32_t raw_value = getter->Get();
  return static_cast<int32_t>(raw_value);
}

template <>
uint32_t ValueGetter<uint32_t>::Get(ShadowFrameGetter* getter) {
  return getter->Get();
}

template <>
int64_t ValueGetter<int64_t>::Get(ShadowFrameGetter* getter) {
  return getter->GetLong();
}

template <>
float ValueGetter<float>::Get(ShadowFrameGetter* getter) {
  uint32_t raw_value = getter->Get();
  return *reinterpret_cast<float*>(&raw_value);
}

template <>
double ValueGetter<double>::Get(ShadowFrameGetter* getter) {
  int64_t raw_value = getter->GetLong();
  return *reinterpret_cast<double*>(&raw_value);
}

template <>
ObjPtr<Object> ValueGetter<ObjPtr<Object>>::Get(ShadowFrameGetter* getter) {
  return getter->GetReference();
}

// Class for accessing fields of Object instances
template <typename T>
class FieldAccessor {
 public:
  static bool Dispatch(VarHandle::AccessMode access_mode,
                       ObjPtr<Object> obj,
                       MemberOffset field_offset,
                       ShadowFrameGetter* getter,
                       JValue* result)
      REQUIRES_SHARED(Locks::mutator_lock_);
};

// Dispatch implementation for primitive fields.
template <typename T>
bool FieldAccessor<T>::Dispatch(VarHandle::AccessMode access_mode,
                                ObjPtr<Object> obj,
                                MemberOffset field_offset,
                                ShadowFrameGetter* getter,
                                JValue* result) {
  switch (access_mode) {
    case VarHandle::AccessMode::kGet: {
      GetAccessor<T> accessor(result);
      FieldAccessViaAccessor<T>::Get(obj, field_offset, &accessor);
      break;
    }
    case VarHandle::AccessMode::kSet: {
      T new_value = ValueGetter<T>::Get(getter);
      SetAccessor<T> accessor(new_value);
      FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
      break;
    }
    case VarHandle::AccessMode::kGetVolatile: {
      GetVolatileAccessor<T> accessor(result);
      FieldAccessViaAccessor<T>::Get(obj, field_offset, &accessor);
      break;
    }
    case VarHandle::AccessMode::kSetVolatile: {
      T new_value = ValueGetter<T>::Get(getter);
      SetVolatileAccessor<T> accessor(new_value);
      FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
      break;
    }
    case VarHandle::AccessMode::kGetAcquire: {
      GetAcquireAccessor<T> accessor(result);
      FieldAccessViaAccessor<T>::Get(obj, field_offset, &accessor);
      break;
    }
    case VarHandle::AccessMode::kSetRelease: {
      T new_value = ValueGetter<T>::Get(getter);
      SetReleaseAccessor<T> accessor(new_value);
      FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
      break;
    }
    case VarHandle::AccessMode::kGetOpaque: {
      GetOpaqueAccessor<T> accessor(result);
      FieldAccessViaAccessor<T>::Get(obj, field_offset, &accessor);
      break;
    }
    case VarHandle::AccessMode::kSetOpaque: {
      T new_value = ValueGetter<T>::Get(getter);
      SetOpaqueAccessor<T> accessor(new_value);
      FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
      break;
    }
    case VarHandle::AccessMode::kCompareAndSet: {
      T expected_value = ValueGetter<T>::Get(getter);
      T desired_value = ValueGetter<T>::Get(getter);
      CompareAndSetAccessor<T> accessor(expected_value, desired_value, result);
      FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
      break;
    }
    case VarHandle::AccessMode::kCompareAndExchange: {
      T expected_value = ValueGetter<T>::Get(getter);
      T desired_value = ValueGetter<T>::Get(getter);
      CompareAndExchangeAccessor<T> accessor(expected_value, desired_value, result);
      FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
      break;
    }
    case VarHandle::AccessMode::kCompareAndExchangeAcquire: {
      T expected_value = ValueGetter<T>::Get(getter);
      T desired_value = ValueGetter<T>::Get(getter);
      CompareAndExchangeAcquireAccessor<T> accessor(expected_value, desired_value, result);
      FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
      break;
    }
    case VarHandle::AccessMode::kCompareAndExchangeRelease: {
      T expected_value = ValueGetter<T>::Get(getter);
      T desired_value = ValueGetter<T>::Get(getter);
      CompareAndExchangeReleaseAccessor<T> accessor(expected_value, desired_value, result);
      FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
      break;
    }
    case VarHandle::AccessMode::kWeakCompareAndSetPlain: {
      T expected_value = ValueGetter<T>::Get(getter);
      T desired_value = ValueGetter<T>::Get(getter);
      WeakCompareAndSetPlainAccessor<T> accessor(expected_value, desired_value, result);
      FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
      break;
    }
    case VarHandle::AccessMode::kWeakCompareAndSet: {
      T expected_value = ValueGetter<T>::Get(getter);
      T desired_value = ValueGetter<T>::Get(getter);
      WeakCompareAndSetAccessor<T> accessor(expected_value, desired_value, result);
      FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
      break;
    }
    case VarHandle::AccessMode::kWeakCompareAndSetAcquire: {
      T expected_value = ValueGetter<T>::Get(getter);
      T desired_value = ValueGetter<T>::Get(getter);
      WeakCompareAndSetAcquireAccessor<T> accessor(expected_value, desired_value, result);
      FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
      break;
    }
    case VarHandle::AccessMode::kWeakCompareAndSetRelease: {
      T expected_value = ValueGetter<T>::Get(getter);
      T desired_value = ValueGetter<T>::Get(getter);
      WeakCompareAndSetReleaseAccessor<T> accessor(expected_value, desired_value, result);
      FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
      break;
    }
    case VarHandle::AccessMode::kGetAndSet: {
      T new_value = ValueGetter<T>::Get(getter);
      GetAndSetAccessor<T> accessor(new_value, result);
      FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
      break;
    }
    case VarHandle::AccessMode::kGetAndSetAcquire: {
      T new_value = ValueGetter<T>::Get(getter);
      GetAndSetAcquireAccessor<T> accessor(new_value, result);
      FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
      break;
    }
    case VarHandle::AccessMode::kGetAndSetRelease: {
      T new_value = ValueGetter<T>::Get(getter);
      GetAndSetReleaseAccessor<T> accessor(new_value, result);
      FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
      break;
    }
    case VarHandle::AccessMode::kGetAndAdd: {
      T value = ValueGetter<T>::Get(getter);
      GetAndAddAccessor<T> accessor(value, result);
      FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
      break;
    }
    case VarHandle::AccessMode::kGetAndAddAcquire: {
      T value = ValueGetter<T>::Get(getter);
      GetAndAddAcquireAccessor<T> accessor(value, result);
      FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
      break;
    }
    case VarHandle::AccessMode::kGetAndAddRelease: {
      T value = ValueGetter<T>::Get(getter);
      GetAndAddReleaseAccessor<T> accessor(value, result);
      FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
      break;
    }
    case VarHandle::AccessMode::kGetAndBitwiseOr: {
      T value = ValueGetter<T>::Get(getter);
      GetAndBitwiseOrAccessor<T> accessor(value, result);
      FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
      break;
    }
    case VarHandle::AccessMode::kGetAndBitwiseOrAcquire: {
      T value = ValueGetter<T>::Get(getter);
      GetAndBitwiseOrAcquireAccessor<T> accessor(value, result);
      FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
      break;
    }
    case VarHandle::AccessMode::kGetAndBitwiseOrRelease: {
      T value = ValueGetter<T>::Get(getter);
      GetAndBitwiseOrReleaseAccessor<T> accessor(value, result);
      FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
      break;
    }
    case VarHandle::AccessMode::kGetAndBitwiseAnd: {
      T value = ValueGetter<T>::Get(getter);
      GetAndBitwiseAndAccessor<T> accessor(value, result);
      FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
      break;
    }
    case VarHandle::AccessMode::kGetAndBitwiseAndAcquire: {
      T value = ValueGetter<T>::Get(getter);
      GetAndBitwiseAndAcquireAccessor<T> accessor(value, result);
      FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
      break;
    }
    case VarHandle::AccessMode::kGetAndBitwiseAndRelease: {
      T value = ValueGetter<T>::Get(getter);
      GetAndBitwiseAndReleaseAccessor<T> accessor(value, result);
      FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
      break;
    }
    case VarHandle::AccessMode::kGetAndBitwiseXor: {
      T value = ValueGetter<T>::Get(getter);
      GetAndBitwiseXorAccessor<T> accessor(value, result);
      FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
      break;
    }
    case VarHandle::AccessMode::kGetAndBitwiseXorAcquire: {
      T value = ValueGetter<T>::Get(getter);
      GetAndBitwiseXorAcquireAccessor<T> accessor(value, result);
      FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
      break;
    }
    case VarHandle::AccessMode::kGetAndBitwiseXorRelease: {
      T value = ValueGetter<T>::Get(getter);
      GetAndBitwiseXorReleaseAccessor<T> accessor(value, result);
      FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
      break;
    }
  }
  return true;
}

// Dispatch implementation for reference fields.
template <>
bool FieldAccessor<ObjPtr<Object>>::Dispatch(VarHandle::AccessMode access_mode,
                                             ObjPtr<Object> obj,
                                             MemberOffset field_offset,
                                             ShadowFrameGetter* getter,
                                             JValue* result)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  // To keep things simple, use the minimum strongest existing
  // field accessor for Object fields. This may be the most
  // straightforward strategy in general for the interpreter.
  switch (access_mode) {
    case VarHandle::AccessMode::kGet: {
      StoreResult(obj->GetFieldObject<Object>(field_offset), result);
      break;
    }
    case VarHandle::AccessMode::kSet: {
      ObjPtr<Object> new_value = ValueGetter<ObjPtr<Object>>::Get(getter);
      if (Runtime::Current()->IsActiveTransaction()) {
        obj->SetFieldObject</*kTransactionActive*/true>(field_offset, new_value);
      } else {
        obj->SetFieldObject</*kTransactionActive*/false>(field_offset, new_value);
      }
      break;
    }
    case VarHandle::AccessMode::kGetAcquire:
    case VarHandle::AccessMode::kGetOpaque:
    case VarHandle::AccessMode::kGetVolatile: {
      StoreResult(obj->GetFieldObjectVolatile<Object>(field_offset), result);
      break;
    }
    case VarHandle::AccessMode::kSetOpaque:
    case VarHandle::AccessMode::kSetRelease:
    case VarHandle::AccessMode::kSetVolatile: {
      ObjPtr<Object> new_value = ValueGetter<ObjPtr<Object>>::Get(getter);
      if (Runtime::Current()->IsActiveTransaction()) {
        obj->SetFieldObjectVolatile</*kTransactionActive*/true>(field_offset, new_value);
      } else {
        obj->SetFieldObjectVolatile</*kTransactionActive*/false>(field_offset, new_value);
      }
      break;
    }
    case VarHandle::AccessMode::kCompareAndSet: {
      ReadBarrierForVarHandleAccess(obj, field_offset);
      ObjPtr<Object> expected_value = ValueGetter<ObjPtr<Object>>::Get(getter);
      ObjPtr<Object> desired_value = ValueGetter<ObjPtr<Object>>::Get(getter);
      bool cas_result;
      if (Runtime::Current()->IsActiveTransaction()) {
        cas_result = obj->CasFieldStrongSequentiallyConsistentObject</*kTransactionActive*/true>(
            field_offset,
            expected_value,
            desired_value);
      } else {
        cas_result = obj->CasFieldStrongSequentiallyConsistentObject</*kTransactionActive*/false>(
            field_offset,
            expected_value,
            desired_value);
      }
      StoreResult(cas_result, result);
      break;
    }
    case VarHandle::AccessMode::kWeakCompareAndSet:
    case VarHandle::AccessMode::kWeakCompareAndSetAcquire:
    case VarHandle::AccessMode::kWeakCompareAndSetPlain:
    case VarHandle::AccessMode::kWeakCompareAndSetRelease: {
      ReadBarrierForVarHandleAccess(obj, field_offset);
      ObjPtr<Object> expected_value = ValueGetter<ObjPtr<Object>>::Get(getter);
      ObjPtr<Object> desired_value = ValueGetter<ObjPtr<Object>>::Get(getter);
      bool cas_result;
      if (Runtime::Current()->IsActiveTransaction()) {
        cas_result = obj->CasFieldWeakSequentiallyConsistentObject</*kTransactionActive*/true>(
            field_offset,
            expected_value,
            desired_value);
      } else {
        cas_result = obj->CasFieldWeakSequentiallyConsistentObject</*kTransactionActive*/false>(
            field_offset,
            expected_value,
            desired_value);
      }
      StoreResult(cas_result, result);
      break;
    }
    case VarHandle::AccessMode::kCompareAndExchange:
    case VarHandle::AccessMode::kCompareAndExchangeAcquire:
    case VarHandle::AccessMode::kCompareAndExchangeRelease: {
      ReadBarrierForVarHandleAccess(obj, field_offset);
      ObjPtr<Object> expected_value = ValueGetter<ObjPtr<Object>>::Get(getter);
      ObjPtr<Object> desired_value = ValueGetter<ObjPtr<Object>>::Get(getter);
      ObjPtr<Object> witness_value;
      if (Runtime::Current()->IsActiveTransaction()) {
        witness_value = obj->CompareAndExchangeFieldObject</*kTransactionActive*/true>(
            field_offset,
            expected_value,
            desired_value);
      } else {
        witness_value = obj->CompareAndExchangeFieldObject</*kTransactionActive*/false>(
            field_offset,
            expected_value,
            desired_value);
      }
      StoreResult(witness_value, result);
      break;
    }
    case VarHandle::AccessMode::kGetAndSet:
    case VarHandle::AccessMode::kGetAndSetAcquire:
    case VarHandle::AccessMode::kGetAndSetRelease: {
      ReadBarrierForVarHandleAccess(obj, field_offset);
      ObjPtr<Object> new_value = ValueGetter<ObjPtr<Object>>::Get(getter);
      ObjPtr<Object> old_value;
      if (Runtime::Current()->IsActiveTransaction()) {
        old_value = obj->ExchangeFieldObject</*kTransactionActive*/true>(field_offset, new_value);
      } else {
        old_value = obj->ExchangeFieldObject</*kTransactionActive*/false>(field_offset, new_value);
      }
      StoreResult(old_value, result);
      break;
    }
    case VarHandle::AccessMode::kGetAndAdd:
    case VarHandle::AccessMode::kGetAndAddAcquire:
    case VarHandle::AccessMode::kGetAndAddRelease:
    case VarHandle::AccessMode::kGetAndBitwiseOr:
    case VarHandle::AccessMode::kGetAndBitwiseOrAcquire:
    case VarHandle::AccessMode::kGetAndBitwiseOrRelease:
    case VarHandle::AccessMode::kGetAndBitwiseAnd:
    case VarHandle::AccessMode::kGetAndBitwiseAndAcquire:
    case VarHandle::AccessMode::kGetAndBitwiseAndRelease:
    case VarHandle::AccessMode::kGetAndBitwiseXor:
    case VarHandle::AccessMode::kGetAndBitwiseXorAcquire:
    case VarHandle::AccessMode::kGetAndBitwiseXorRelease: {
      size_t index = static_cast<size_t>(access_mode);
      const char* access_mode_name = kAccessorToAccessMode[index].method_name;
      UnreachableAccessMode(access_mode_name, "Object");
    }
  }
  return true;
}

// Class for accessing primitive array elements.
template <typename T>
class PrimitiveArrayElementAccessor {
 public:
  static T* GetElementAddress(ObjPtr<Array> target_array, int target_element)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    auto primitive_array = ObjPtr<PrimitiveArray<T>>::DownCast(target_array);
    DCHECK(primitive_array->CheckIsValidIndex(target_element));
    return &primitive_array->GetData()[target_element];
  }

  static bool Dispatch(VarHandle::AccessMode access_mode,
                       ObjPtr<Array> target_array,
                       int target_element,
                       ShadowFrameGetter* getter,
                       JValue* result)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    T* element_address = GetElementAddress(target_array, target_element);
    switch (access_mode) {
      case VarHandle::AccessMode::kGet: {
        GetAccessor<T> accessor(result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kSet: {
        T new_value = ValueGetter<T>::Get(getter);
        SetAccessor<T> accessor(new_value);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kGetVolatile: {
        GetVolatileAccessor<T> accessor(result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kSetVolatile: {
        T new_value = ValueGetter<T>::Get(getter);
        SetVolatileAccessor<T> accessor(new_value);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kGetAcquire: {
        GetAcquireAccessor<T> accessor(result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kSetRelease: {
        T new_value = ValueGetter<T>::Get(getter);
        SetReleaseAccessor<T> accessor(new_value);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kGetOpaque: {
        GetOpaqueAccessor<T> accessor(result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kSetOpaque: {
        T new_value = ValueGetter<T>::Get(getter);
        SetOpaqueAccessor<T> accessor(new_value);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kCompareAndSet: {
        T expected_value = ValueGetter<T>::Get(getter);
        T desired_value = ValueGetter<T>::Get(getter);
        CompareAndSetAccessor<T> accessor(expected_value, desired_value, result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kCompareAndExchange: {
        T expected_value = ValueGetter<T>::Get(getter);
        T desired_value = ValueGetter<T>::Get(getter);
        CompareAndExchangeAccessor<T> accessor(expected_value, desired_value, result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kCompareAndExchangeAcquire: {
        T expected_value = ValueGetter<T>::Get(getter);
        T desired_value = ValueGetter<T>::Get(getter);
        CompareAndExchangeAcquireAccessor<T> accessor(expected_value, desired_value, result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kCompareAndExchangeRelease: {
        T expected_value = ValueGetter<T>::Get(getter);
        T desired_value = ValueGetter<T>::Get(getter);
        CompareAndExchangeReleaseAccessor<T> accessor(expected_value, desired_value, result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kWeakCompareAndSetPlain: {
        T expected_value = ValueGetter<T>::Get(getter);
        T desired_value = ValueGetter<T>::Get(getter);
        WeakCompareAndSetPlainAccessor<T> accessor(expected_value, desired_value, result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kWeakCompareAndSet: {
        T expected_value = ValueGetter<T>::Get(getter);
        T desired_value = ValueGetter<T>::Get(getter);
        WeakCompareAndSetAccessor<T> accessor(expected_value, desired_value, result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kWeakCompareAndSetAcquire: {
        T expected_value = ValueGetter<T>::Get(getter);
        T desired_value = ValueGetter<T>::Get(getter);
        WeakCompareAndSetAcquireAccessor<T> accessor(expected_value, desired_value, result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kWeakCompareAndSetRelease: {
        T expected_value = ValueGetter<T>::Get(getter);
        T desired_value = ValueGetter<T>::Get(getter);
        WeakCompareAndSetReleaseAccessor<T> accessor(expected_value, desired_value, result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kGetAndSet: {
        T new_value = ValueGetter<T>::Get(getter);
        GetAndSetAccessor<T> accessor(new_value, result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kGetAndSetAcquire: {
        T new_value = ValueGetter<T>::Get(getter);
        GetAndSetAcquireAccessor<T> accessor(new_value, result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kGetAndSetRelease: {
        T new_value = ValueGetter<T>::Get(getter);
        GetAndSetReleaseAccessor<T> accessor(new_value, result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kGetAndAdd: {
        T value = ValueGetter<T>::Get(getter);
        GetAndAddAccessor<T> accessor(value, result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kGetAndAddAcquire: {
        T value = ValueGetter<T>::Get(getter);
        GetAndAddAcquireAccessor<T> accessor(value, result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kGetAndAddRelease: {
        T value = ValueGetter<T>::Get(getter);
        GetAndAddReleaseAccessor<T> accessor(value, result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kGetAndBitwiseOr: {
        T value = ValueGetter<T>::Get(getter);
        GetAndBitwiseOrAccessor<T> accessor(value, result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kGetAndBitwiseOrAcquire: {
        T value = ValueGetter<T>::Get(getter);
        GetAndBitwiseOrAcquireAccessor<T> accessor(value, result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kGetAndBitwiseOrRelease: {
        T value = ValueGetter<T>::Get(getter);
        GetAndBitwiseOrReleaseAccessor<T> accessor(value, result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kGetAndBitwiseAnd: {
        T value = ValueGetter<T>::Get(getter);
        GetAndBitwiseAndAccessor<T> accessor(value, result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kGetAndBitwiseAndAcquire: {
        T value = ValueGetter<T>::Get(getter);
        GetAndBitwiseAndAcquireAccessor<T> accessor(value, result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kGetAndBitwiseAndRelease: {
        T value = ValueGetter<T>::Get(getter);
        GetAndBitwiseAndReleaseAccessor<T> accessor(value, result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kGetAndBitwiseXor: {
        T value = ValueGetter<T>::Get(getter);
        GetAndBitwiseXorAccessor<T> accessor(value, result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kGetAndBitwiseXorAcquire: {
        T value = ValueGetter<T>::Get(getter);
        GetAndBitwiseXorAcquireAccessor<T> accessor(value, result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kGetAndBitwiseXorRelease: {
        T value = ValueGetter<T>::Get(getter);
        GetAndBitwiseXorReleaseAccessor<T> accessor(value, result);
        accessor.Access(element_address);
        break;
      }
    }
    return true;
  }
};

// Class for accessing primitive array elements.
template <typename T>
class ByteArrayViewAccessor {
 public:
  static inline bool IsAccessAligned(int8_t* data, int data_index) {
    static_assert(IsPowerOfTwo(sizeof(T)), "unexpected size");
    static_assert(std::is_arithmetic<T>::value, "unexpected type");
    uintptr_t alignment_mask = sizeof(T) - 1;
    uintptr_t address = reinterpret_cast<uintptr_t>(data + data_index);
    return (address & alignment_mask) == 0;
  }

  static inline void MaybeByteSwap(bool byte_swap, T* const value) {
    if (byte_swap) {
      *value = BSWAP(*value);
    }
  }

  static bool Dispatch(const VarHandle::AccessMode access_mode,
                       int8_t* const data,
                       const int data_index,
                       const bool byte_swap,
                       ShadowFrameGetter* const getter,
                       JValue* const result)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    const bool is_aligned = IsAccessAligned(data, data_index);
    if (!is_aligned) {
      switch (access_mode) {
        case VarHandle::AccessMode::kGet: {
          T value;
          memcpy(&value, data + data_index, sizeof(T));
          MaybeByteSwap(byte_swap, &value);
          StoreResult(value, result);
          return true;
        }
        case VarHandle::AccessMode::kSet: {
          T new_value = ValueGetter<T>::Get(getter);
          MaybeByteSwap(byte_swap, &new_value);
          memcpy(data + data_index, &new_value, sizeof(T));
          return true;
        }
        default:
          // No other access modes support unaligned access.
          ThrowIllegalStateException("Unaligned access not supported.");
          return false;
      }
    }

    T* const element_address = reinterpret_cast<T*>(data + data_index);
    switch (access_mode) {
      case VarHandle::AccessMode::kGet: {
        GetAccessor<T> accessor(result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kSet: {
        T new_value = ValueGetter<T>::Get(getter);
        MaybeByteSwap(byte_swap, &new_value);
        SetAccessor<T> accessor(new_value);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kGetVolatile: {
        GetVolatileAccessor<T> accessor(result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kSetVolatile: {
        T new_value = ValueGetter<T>::Get(getter);
        MaybeByteSwap(byte_swap, &new_value);
        SetVolatileAccessor<T> accessor(new_value);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kGetAcquire: {
        GetAcquireAccessor<T> accessor(result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kSetRelease: {
        T new_value = ValueGetter<T>::Get(getter);
        MaybeByteSwap(byte_swap, &new_value);
        SetReleaseAccessor<T> accessor(new_value);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kGetOpaque: {
        GetOpaqueAccessor<T> accessor(result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kSetOpaque: {
        T new_value = ValueGetter<T>::Get(getter);
        MaybeByteSwap(byte_swap, &new_value);
        SetOpaqueAccessor<T> accessor(new_value);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kCompareAndSet: {
        T expected_value = ValueGetter<T>::Get(getter);
        T desired_value = ValueGetter<T>::Get(getter);
        MaybeByteSwap(byte_swap, &expected_value);
        MaybeByteSwap(byte_swap, &desired_value);
        CompareAndSetAccessor<T> accessor(expected_value, desired_value, result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kCompareAndExchange: {
        T expected_value = ValueGetter<T>::Get(getter);
        T desired_value = ValueGetter<T>::Get(getter);
        MaybeByteSwap(byte_swap, &expected_value);
        MaybeByteSwap(byte_swap, &desired_value);
        CompareAndExchangeAccessor<T> accessor(expected_value, desired_value, result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kCompareAndExchangeAcquire: {
        T expected_value = ValueGetter<T>::Get(getter);
        T desired_value = ValueGetter<T>::Get(getter);
        MaybeByteSwap(byte_swap, &expected_value);
        MaybeByteSwap(byte_swap, &desired_value);
        CompareAndExchangeAcquireAccessor<T> accessor(expected_value, desired_value, result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kCompareAndExchangeRelease: {
        T expected_value = ValueGetter<T>::Get(getter);
        T desired_value = ValueGetter<T>::Get(getter);
        MaybeByteSwap(byte_swap, &expected_value);
        MaybeByteSwap(byte_swap, &desired_value);
        CompareAndExchangeReleaseAccessor<T> accessor(expected_value, desired_value, result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kWeakCompareAndSetPlain: {
        T expected_value = ValueGetter<T>::Get(getter);
        T desired_value = ValueGetter<T>::Get(getter);
        MaybeByteSwap(byte_swap, &expected_value);
        MaybeByteSwap(byte_swap, &desired_value);
        WeakCompareAndSetPlainAccessor<T> accessor(expected_value, desired_value, result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kWeakCompareAndSet: {
        T expected_value = ValueGetter<T>::Get(getter);
        T desired_value = ValueGetter<T>::Get(getter);
        MaybeByteSwap(byte_swap, &expected_value);
        MaybeByteSwap(byte_swap, &desired_value);
        WeakCompareAndSetAccessor<T> accessor(expected_value, desired_value, result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kWeakCompareAndSetAcquire: {
        T expected_value = ValueGetter<T>::Get(getter);
        T desired_value = ValueGetter<T>::Get(getter);
        MaybeByteSwap(byte_swap, &expected_value);
        MaybeByteSwap(byte_swap, &desired_value);
        WeakCompareAndSetAcquireAccessor<T> accessor(expected_value, desired_value, result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kWeakCompareAndSetRelease: {
        T expected_value = ValueGetter<T>::Get(getter);
        T desired_value = ValueGetter<T>::Get(getter);
        MaybeByteSwap(byte_swap, &expected_value);
        MaybeByteSwap(byte_swap, &desired_value);
        WeakCompareAndSetReleaseAccessor<T> accessor(expected_value, desired_value, result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kGetAndSet: {
        T new_value = ValueGetter<T>::Get(getter);
        MaybeByteSwap(byte_swap, &new_value);
        GetAndSetAccessor<T> accessor(new_value, result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kGetAndSetAcquire: {
        T new_value = ValueGetter<T>::Get(getter);
        MaybeByteSwap(byte_swap, &new_value);
        GetAndSetAcquireAccessor<T> accessor(new_value, result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kGetAndSetRelease: {
        T new_value = ValueGetter<T>::Get(getter);
        MaybeByteSwap(byte_swap, &new_value);
        GetAndSetReleaseAccessor<T> accessor(new_value, result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kGetAndAdd: {
        // TODO(oth): GetAndAdd* need to be performed in host order if a byte-swap is request.
        T value = ValueGetter<T>::Get(getter);
        MaybeByteSwap(byte_swap, &value);
        GetAndAddAccessor<T> accessor(value, result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kGetAndAddAcquire: {
        T value = ValueGetter<T>::Get(getter);
        MaybeByteSwap(byte_swap, &value);
        GetAndAddAcquireAccessor<T> accessor(value, result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kGetAndAddRelease: {
        T value = ValueGetter<T>::Get(getter);
        MaybeByteSwap(byte_swap, &value);
        GetAndAddReleaseAccessor<T> accessor(value, result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kGetAndBitwiseOr: {
        T value = ValueGetter<T>::Get(getter);
        MaybeByteSwap(byte_swap, &value);
        GetAndBitwiseOrAccessor<T> accessor(value, result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kGetAndBitwiseOrAcquire: {
        T value = ValueGetter<T>::Get(getter);
        MaybeByteSwap(byte_swap, &value);
        GetAndBitwiseOrAcquireAccessor<T> accessor(value, result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kGetAndBitwiseOrRelease: {
        T value = ValueGetter<T>::Get(getter);
        MaybeByteSwap(byte_swap, &value);
        GetAndBitwiseOrReleaseAccessor<T> accessor(value, result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kGetAndBitwiseAnd: {
        T value = ValueGetter<T>::Get(getter);
        MaybeByteSwap(byte_swap, &value);
        GetAndBitwiseAndAccessor<T> accessor(value, result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kGetAndBitwiseAndAcquire: {
        T value = ValueGetter<T>::Get(getter);
        MaybeByteSwap(byte_swap, &value);
        GetAndBitwiseAndAcquireAccessor<T> accessor(value, result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kGetAndBitwiseAndRelease: {
        T value = ValueGetter<T>::Get(getter);
        MaybeByteSwap(byte_swap, &value);
        GetAndBitwiseAndReleaseAccessor<T> accessor(value, result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kGetAndBitwiseXor: {
        T value = ValueGetter<T>::Get(getter);
        MaybeByteSwap(byte_swap, &value);
        GetAndBitwiseXorAccessor<T> accessor(value, result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kGetAndBitwiseXorAcquire: {
        T value = ValueGetter<T>::Get(getter);
        MaybeByteSwap(byte_swap, &value);
        GetAndBitwiseXorAcquireAccessor<T> accessor(value, result);
        accessor.Access(element_address);
        break;
      }
      case VarHandle::AccessMode::kGetAndBitwiseXorRelease: {
        T value = ValueGetter<T>::Get(getter);
        MaybeByteSwap(byte_swap, &value);
        GetAndBitwiseXorReleaseAccessor<T> accessor(value, result);
        accessor.Access(element_address);
        break;
      }
    }
    if (byte_swap) {
      JValueByteSwapper<T>::ByteSwap(result);
    }
    return true;
  }
};

}  // namespace

Class* VarHandle::GetVarType() {
  return GetFieldObject<Class>(VarTypeOffset());
}

Class* VarHandle::GetCoordinateType0() {
  return GetFieldObject<Class>(CoordinateType0Offset());
}

Class* VarHandle::GetCoordinateType1() {
  return GetFieldObject<Class>(CoordinateType1Offset());
}

int32_t VarHandle::GetAccessModesBitMask() {
  return GetField32(AccessModesBitMaskOffset());
}

bool VarHandle::IsMethodTypeCompatible(AccessMode access_mode, MethodType* method_type) {
  StackHandleScope<3> hs(Thread::Current());
  Handle<Class> mt_rtype(hs.NewHandle(method_type->GetRType()));
  Handle<VarHandle> vh(hs.NewHandle(this));
  Handle<Class> var_type(hs.NewHandle(vh->GetVarType()));
  AccessModeTemplate access_mode_template = GetAccessModeTemplate(access_mode);

  // Check return type first.
  if (mt_rtype->GetPrimitiveType() == Primitive::Type::kPrimVoid) {
    // The result of the operation will be discarded. The return type
    // of the VarHandle is immaterial.
  } else {
    ObjPtr<Class> vh_rtype(GetReturnType(access_mode_template, var_type.Get()));
    if (!IsReturnTypeConvertible(vh_rtype, mt_rtype.Get())) {
      return false;
    }
  }

  // Check the number of parameters matches.
  ObjPtr<Class> vh_ptypes[VarHandle::kMaxAccessorParameters];
  const int32_t vh_ptypes_count = BuildParameterArray(vh_ptypes,
                                                      access_mode_template,
                                                      var_type.Get(),
                                                      GetCoordinateType0(),
                                                      GetCoordinateType1());
  if (vh_ptypes_count != method_type->GetPTypes()->GetLength()) {
    return false;
  }

  // Check the parameter types are compatible.
  ObjPtr<ObjectArray<Class>> mt_ptypes = method_type->GetPTypes();
  for (int32_t i = 0; i < vh_ptypes_count; ++i) {
    if (!IsParameterTypeConvertible(mt_ptypes->Get(i), vh_ptypes[i])) {
      return false;
    }
  }
  return true;
}

MethodType* VarHandle::GetMethodTypeForAccessMode(Thread* self,
                                                  ObjPtr<VarHandle> var_handle,
                                                  AccessMode access_mode) {
  // This is a static as the var_handle might be moved by the GC during it's execution.
  AccessModeTemplate access_mode_template = GetAccessModeTemplate(access_mode);

  StackHandleScope<3> hs(self);
  Handle<VarHandle> vh = hs.NewHandle(var_handle);
  Handle<Class> rtype = hs.NewHandle(GetReturnType(access_mode_template, vh->GetVarType()));
  const int32_t ptypes_count =
      GetParameterCount(access_mode_template, vh->GetCoordinateType0(), vh->GetCoordinateType1());
  Handle<ObjectArray<Class>> ptypes = hs.NewHandle(NewArrayOfClasses(self, ptypes_count));
  if (ptypes == nullptr) {
    return nullptr;
  }

  ObjPtr<Class> ptypes_array[VarHandle::kMaxAccessorParameters];
  BuildParameterArray(ptypes_array,
                      access_mode_template,
                      vh->GetVarType(),
                      vh->GetCoordinateType0(),
                      vh->GetCoordinateType1());
  for (int32_t i = 0; i < ptypes_count; ++i) {
    ptypes->Set(i, ptypes_array[i].Ptr());
  }
  return MethodType::Create(self, rtype, ptypes);
}

MethodType* VarHandle::GetMethodTypeForAccessMode(Thread* self, AccessMode access_mode) {
  return GetMethodTypeForAccessMode(self, this, access_mode);
}

size_t VarHandle::GetNumberOfCoordinateTypes() {
  if (GetCoordinateType0() == nullptr) {
    DCHECK(GetClass() == FieldVarHandle::StaticClass());
    return 0u;
  }
  if (GetCoordinateType1() == nullptr) {
    DCHECK(GetClass() == FieldVarHandle::StaticClass());
    return 1u;
  }
  DCHECK(GetClass() == ArrayElementVarHandle::StaticClass() ||
         GetClass() == ByteArrayViewVarHandle::StaticClass() ||
         GetClass() == ByteBufferViewVarHandle::StaticClass());
  return 2u;
}

bool VarHandle::Access(AccessMode access_mode,
                       ShadowFrame* shadow_frame,
                       InstructionOperands* operands,
                       JValue* result) {
  Class* klass = GetClass();
  if (klass == FieldVarHandle::StaticClass()) {
    auto vh = reinterpret_cast<FieldVarHandle*>(this);
    return vh->Access(access_mode, shadow_frame, operands, result);
  } else if (klass == ArrayElementVarHandle::StaticClass()) {
    auto vh = reinterpret_cast<ArrayElementVarHandle*>(this);
    return vh->Access(access_mode, shadow_frame, operands, result);
  } else if (klass == ByteArrayViewVarHandle::StaticClass()) {
    auto vh = reinterpret_cast<ByteArrayViewVarHandle*>(this);
    return vh->Access(access_mode, shadow_frame, operands, result);
  } else if (klass == ByteBufferViewVarHandle::StaticClass()) {
    auto vh = reinterpret_cast<ByteBufferViewVarHandle*>(this);
    return vh->Access(access_mode, shadow_frame, operands, result);
  } else {
    LOG(FATAL) << "Unknown varhandle kind";
    UNREACHABLE();
  }
}

const char* VarHandle::GetReturnTypeDescriptor(const char* accessor_name) {
  AccessMode access_mode;
  if (!GetAccessModeByMethodName(accessor_name, &access_mode)) {
    return nullptr;
  }
  AccessModeTemplate access_mode_template = GetAccessModeTemplate(access_mode);
  switch (access_mode_template) {
    case AccessModeTemplate::kGet:
    case AccessModeTemplate::kCompareAndExchange:
    case AccessModeTemplate::kGetAndUpdate:
      return "Ljava/lang/Object;";
    case AccessModeTemplate::kCompareAndSet:
      return "Z";
    case AccessModeTemplate::kSet:
      return "V";
  }
}

bool VarHandle::GetAccessModeByMethodName(const char* method_name, AccessMode* access_mode) {
  if (method_name == nullptr) {
    return false;
  }
  VarHandleAccessorToAccessModeEntry target = { method_name, /*dummy*/VarHandle::AccessMode::kGet };
  auto last = std::cend(kAccessorToAccessMode);
  auto it = std::lower_bound(std::cbegin(kAccessorToAccessMode),
                             last,
                             target,
                             VarHandleAccessorToAccessModeEntry::CompareName);
  if (it == last || strcmp(it->method_name, method_name) != 0) {
    return false;
  }
  *access_mode = it->access_mode;
  return true;
}

Class* VarHandle::StaticClass() REQUIRES_SHARED(Locks::mutator_lock_) {
  return static_class_.Read();
}

void VarHandle::SetClass(Class* klass) {
  CHECK(static_class_.IsNull()) << static_class_.Read() << " " << klass;
  CHECK(klass != nullptr);
  static_class_ = GcRoot<Class>(klass);
}

void VarHandle::ResetClass() {
  CHECK(!static_class_.IsNull());
  static_class_ = GcRoot<Class>(nullptr);
}

void VarHandle::VisitRoots(RootVisitor* visitor) {
  static_class_.VisitRootIfNonNull(visitor, RootInfo(kRootStickyClass));
}

GcRoot<Class> VarHandle::static_class_;

ArtField* FieldVarHandle::GetField() {
  uintptr_t opaque_field = static_cast<uintptr_t>(GetField64(ArtFieldOffset()));
  return reinterpret_cast<ArtField*>(opaque_field);
}

bool FieldVarHandle::Access(AccessMode access_mode,
                            ShadowFrame* shadow_frame,
                            InstructionOperands* operands,
                            JValue* result) {
  ShadowFrameGetter getter(*shadow_frame, operands);
  ArtField* field = GetField();
  ObjPtr<Object> ct0;
  if (field->IsStatic()) {
    DCHECK_LE(operands->GetNumberOfOperands(), 2u);
    ct0 = field->GetDeclaringClass();
  } else {
    DCHECK_GE(operands->GetNumberOfOperands(), 1u);
    DCHECK_LE(operands->GetNumberOfOperands(), 3u);
    ct0 = getter.GetReference();
  }
  DCHECK(!ct0.IsNull());

  const MemberOffset offset = field->GetOffset();
  const Primitive::Type primitive_type = GetVarType()->GetPrimitiveType();
  switch (primitive_type) {
    case Primitive::Type::kPrimNot:
      return FieldAccessor<ObjPtr<Object>>::Dispatch(access_mode, ct0, offset, &getter, result);
    case Primitive::kPrimBoolean:
      return FieldAccessor<uint8_t>::Dispatch(access_mode, ct0, offset, &getter, result);
    case Primitive::kPrimByte:
      return FieldAccessor<int8_t>::Dispatch(access_mode, ct0, offset, &getter, result);
    case Primitive::kPrimChar:
      return FieldAccessor<uint16_t>::Dispatch(access_mode, ct0, offset, &getter, result);
    case Primitive::kPrimShort:
      return FieldAccessor<int16_t>::Dispatch(access_mode, ct0, offset, &getter, result);
    case Primitive::kPrimInt:
      return FieldAccessor<int32_t>::Dispatch(access_mode, ct0, offset, &getter, result);
    case Primitive::kPrimFloat:
      return FieldAccessor<float>::Dispatch(access_mode,  ct0, offset, &getter, result);
    case Primitive::kPrimLong:
      return FieldAccessor<int64_t>::Dispatch(access_mode, ct0, offset, &getter, result);
    case Primitive::kPrimDouble:
      return FieldAccessor<double>::Dispatch(access_mode, ct0, offset, &getter, result);
    case Primitive::kPrimVoid:
      break;
  }
  LOG(FATAL) << "Unreachable: Unexpected primitive " << primitive_type;
  UNREACHABLE();
}

Class* FieldVarHandle::StaticClass() REQUIRES_SHARED(Locks::mutator_lock_) {
  return static_class_.Read();
}

void FieldVarHandle::SetClass(Class* klass) {
  CHECK(static_class_.IsNull()) << static_class_.Read() << " " << klass;
  CHECK(klass != nullptr);
  static_class_ = GcRoot<Class>(klass);
}

void FieldVarHandle::ResetClass() {
  CHECK(!static_class_.IsNull());
  static_class_ = GcRoot<Class>(nullptr);
}

void FieldVarHandle::VisitRoots(RootVisitor* visitor) {
  static_class_.VisitRootIfNonNull(visitor, RootInfo(kRootStickyClass));
}

GcRoot<Class> FieldVarHandle::static_class_;

bool ArrayElementVarHandle::Access(AccessMode access_mode,
                                   ShadowFrame* shadow_frame,
                                   InstructionOperands* operands,
                                   JValue* result) {
  ShadowFrameGetter getter(*shadow_frame, operands);

  // The target array is the first co-ordinate type preceeding var type arguments.
  ObjPtr<Array> target_array(getter.GetReference()->AsArray());
  if (target_array == nullptr) {
    ThrowNullPointerException("target array is null");
    return false;
  }

  // The target array element is the second co-ordinate type preceeding var type arguments.
  const int target_element = getter.Get();
  if (!target_array->CheckIsValidIndex(target_element)) {
    DCHECK(Thread::Current()->IsExceptionPending());
    return false;
  }

  const Primitive::Type primitive_type = GetVarType()->GetPrimitiveType();
  switch (primitive_type) {
    case Primitive::Type::kPrimNot: {
      MemberOffset target_element_offset =
          target_array->AsObjectArray<Object>()->OffsetOfElement(target_element);
      return FieldAccessor<ObjPtr<Object>>::Dispatch(access_mode,
                                                     target_array,
                                                     target_element_offset,
                                                     &getter,
                                                     result);
    }
    case Primitive::Type::kPrimBoolean:
      return PrimitiveArrayElementAccessor<uint8_t>::Dispatch(access_mode,
                                                              target_array,
                                                              target_element,
                                                              &getter,
                                                              result);
    case Primitive::Type::kPrimByte:
      return PrimitiveArrayElementAccessor<int8_t>::Dispatch(access_mode,
                                                             target_array,
                                                             target_element,
                                                             &getter,
                                                             result);
    case Primitive::Type::kPrimChar:
      return PrimitiveArrayElementAccessor<uint16_t>::Dispatch(access_mode,
                                                               target_array,
                                                               target_element,
                                                               &getter,
                                                               result);
    case Primitive::Type::kPrimShort:
      return PrimitiveArrayElementAccessor<int16_t>::Dispatch(access_mode,
                                                              target_array,
                                                              target_element,
                                                              &getter,
                                                              result);
    case Primitive::Type::kPrimInt:
      return PrimitiveArrayElementAccessor<int32_t>::Dispatch(access_mode,
                                                              target_array,
                                                              target_element,
                                                              &getter,
                                                              result);
    case Primitive::Type::kPrimLong:
      return PrimitiveArrayElementAccessor<int64_t>::Dispatch(access_mode,
                                                              target_array,
                                                              target_element,
                                                              &getter,
                                                              result);
    case Primitive::Type::kPrimFloat:
      return PrimitiveArrayElementAccessor<float>::Dispatch(access_mode,
                                                            target_array,
                                                            target_element,
                                                            &getter,
                                                            result);
    case Primitive::Type::kPrimDouble:
      return PrimitiveArrayElementAccessor<double>::Dispatch(access_mode,
                                                             target_array,
                                                             target_element,
                                                             &getter,
                                                             result);
    case Primitive::Type::kPrimVoid:
      break;
  }
  LOG(FATAL) << "Unreachable: Unexpected primitive " << primitive_type;
  UNREACHABLE();
}

Class* ArrayElementVarHandle::StaticClass() REQUIRES_SHARED(Locks::mutator_lock_) {
  return static_class_.Read();
}

void ArrayElementVarHandle::SetClass(Class* klass) {
  CHECK(static_class_.IsNull()) << static_class_.Read() << " " << klass;
  CHECK(klass != nullptr);
  static_class_ = GcRoot<Class>(klass);
}

void ArrayElementVarHandle::ResetClass() {
  CHECK(!static_class_.IsNull());
  static_class_ = GcRoot<Class>(nullptr);
}

void ArrayElementVarHandle::VisitRoots(RootVisitor* visitor) {
  static_class_.VisitRootIfNonNull(visitor, RootInfo(kRootStickyClass));
}

GcRoot<Class> ArrayElementVarHandle::static_class_;

bool ByteArrayViewVarHandle::GetNativeByteOrder() {
  return GetFieldBoolean(NativeByteOrderOffset());
}

bool ByteArrayViewVarHandle::Access(AccessMode access_mode,
                                    ShadowFrame* shadow_frame,
                                    InstructionOperands* operands,
                                    JValue* result) {
  ShadowFrameGetter getter(*shadow_frame, operands);

  // The byte array is the first co-ordinate type preceeding var type arguments.
  ObjPtr<ByteArray> byte_array(getter.GetReference()->AsByteArray());
  if (byte_array == nullptr) {
    ThrowNullPointerException("target array is null");
    return false;
  }

  const Primitive::Type primitive_type = GetVarType()->GetPrimitiveType();

  // The offset in the byte array element is the second co-ordinate type.
  int32_t element_width = Primitive::ComponentSize(primitive_type);
  const int32_t data_offset = getter.Get();
  if (!byte_array->CheckIsValidIndex(data_offset) ||
      !byte_array->CheckIsValidIndex(data_offset + element_width - 1)) {
    DCHECK(Thread::Current()->IsExceptionPending());
    return false;
  }

  int8_t* const data = byte_array->GetData();
  bool byte_swap = !GetNativeByteOrder();
  switch (primitive_type) {
    case Primitive::Type::kPrimNot:
    case Primitive::kPrimBoolean:
    case Primitive::kPrimByte:
    case Primitive::kPrimVoid:
      // These are not supported for byte array views and not constructible.
      break;
    case Primitive::kPrimChar:
      return ByteArrayViewAccessor<uint16_t>::Dispatch(access_mode,
                                                       data,
                                                       data_offset,
                                                       byte_swap,
                                                       &getter,
                                                       result);
    case Primitive::kPrimShort:
      return ByteArrayViewAccessor<int16_t>::Dispatch(access_mode,
                                                      data,
                                                      data_offset,
                                                      byte_swap,
                                                      &getter,
                                                      result);
    case Primitive::kPrimInt:
      return ByteArrayViewAccessor<int32_t>::Dispatch(access_mode,
                                                      data,
                                                      data_offset,
                                                      byte_swap,
                                                      &getter,
                                                      result);
    case Primitive::kPrimFloat:
      // Treated as a bitwise representation. See javadoc comments for
      // java.lang.invoke.MethodHandles.byteArrayViewVarHandle().
      return ByteArrayViewAccessor<int32_t>::Dispatch(access_mode,
                                                      data,
                                                      data_offset,
                                                      byte_swap,
                                                      &getter,
                                                      result);
    case Primitive::kPrimLong:
      return ByteArrayViewAccessor<int64_t>::Dispatch(access_mode,
                                                      data,
                                                      data_offset,
                                                      byte_swap,
                                                      &getter,
                                                      result);
    case Primitive::kPrimDouble:
      // Treated as a bitwise representation. See javadoc comments for
      // java.lang.invoke.MethodHandles.byteArrayViewVarHandle().
      return ByteArrayViewAccessor<int64_t>::Dispatch(access_mode,
                                                      data,
                                                      data_offset,
                                                      byte_swap,
                                                      &getter,
                                                      result);
  }
  LOG(FATAL) << "Unreachable: Unexpected primitive " << primitive_type;
  UNREACHABLE();
}

Class* ByteArrayViewVarHandle::StaticClass() REQUIRES_SHARED(Locks::mutator_lock_) {
  return static_class_.Read();
}

void ByteArrayViewVarHandle::SetClass(Class* klass) {
  CHECK(static_class_.IsNull()) << static_class_.Read() << " " << klass;
  CHECK(klass != nullptr);
  static_class_ = GcRoot<Class>(klass);
}

void ByteArrayViewVarHandle::ResetClass() {
  CHECK(!static_class_.IsNull());
  static_class_ = GcRoot<Class>(nullptr);
}

void ByteArrayViewVarHandle::VisitRoots(RootVisitor* visitor) {
  static_class_.VisitRootIfNonNull(visitor, RootInfo(kRootStickyClass));
}

GcRoot<Class> ByteArrayViewVarHandle::static_class_;

bool ByteBufferViewVarHandle::GetNativeByteOrder() {
  return GetFieldBoolean(NativeByteOrderOffset());
}

bool ByteBufferViewVarHandle::Access(AccessMode access_mode,
                                     ShadowFrame* shadow_frame,
                                     InstructionOperands* operands,
                                     JValue* result) {
  ShadowFrameGetter getter(*shadow_frame, operands);

  // The byte buffer is the first co-ordinate type preceeding var type arguments.
  ObjPtr<Object> byte_buffer(getter.GetReference());
  if (byte_buffer == nullptr) {
    ThrowNullPointerException("target buffer is null");
    return false;
  }

  ArtField* const address_field =
      jni::DecodeArtField(WellKnownClasses::java_nio_DirectByteBuffer_effectiveDirectAddress);
  if (!address_field->GetDeclaringClass()->IsAssignableFrom(byte_buffer->GetClass())) {
    ThrowIllegalStateException("Not a direct byte buffer");
    return false;
  }

  const int64_t raw_address = byte_buffer->GetField64(address_field->GetOffset());
  if (raw_address == 0ll) {
    ThrowIllegalStateException("Direct buffer is null");
    return false;
  }

  ArtField* capacity_field =
      jni::DecodeArtField(WellKnownClasses::java_nio_DirectByteBuffer_capacity);
  const int32_t capacity = byte_buffer->GetField32(capacity_field->GetOffset());
  DCHECK_GE(capacity, 0);

  const Primitive::Type primitive_type = GetVarType()->GetPrimitiveType();
  // The offset in the byte buffer element is the second co-ordinate type.
  int32_t element_width = Primitive::ComponentSize(primitive_type);
  const int32_t data_offset = getter.Get();
  if (data_offset < 0 || data_offset + element_width > capacity) {
    ThrowIndexOutOfBoundsException(data_offset, capacity);
    return false;
  }

  // TODO(oth): This is cut-and-paste from ByteArrayViewHandle::Dispatch().
  int8_t* const data = reinterpret_cast<int8_t*>(raw_address);
  bool byte_swap = !GetNativeByteOrder();
  switch (primitive_type) {
    case Primitive::Type::kPrimNot:
    case Primitive::kPrimBoolean:
    case Primitive::kPrimByte:
    case Primitive::kPrimVoid:
      // These are not supported for byte array views and not constructible.
      break;
    case Primitive::kPrimChar:
      return ByteArrayViewAccessor<uint16_t>::Dispatch(access_mode,
                                                       data,
                                                       data_offset,
                                                       byte_swap,
                                                       &getter,
                                                       result);
    case Primitive::kPrimShort:
      return ByteArrayViewAccessor<int16_t>::Dispatch(access_mode,
                                                      data,
                                                      data_offset,
                                                      byte_swap,
                                                      &getter,
                                                      result);
    case Primitive::kPrimInt:
      return ByteArrayViewAccessor<int32_t>::Dispatch(access_mode,
                                                      data,
                                                      data_offset,
                                                      byte_swap,
                                                      &getter,
                                                      result);
    case Primitive::kPrimFloat:
      // Treated as a bitwise representation. See javadoc comments for
      // java.lang.invoke.MethodHandles.byteArrayViewVarHandle().
      return ByteArrayViewAccessor<int32_t>::Dispatch(access_mode,
                                                      data,
                                                      data_offset,
                                                      byte_swap,
                                                      &getter,
                                                      result);
    case Primitive::kPrimLong:
      return ByteArrayViewAccessor<int64_t>::Dispatch(access_mode,
                                                      data,
                                                      data_offset,
                                                      byte_swap,
                                                      &getter,
                                                      result);
    case Primitive::kPrimDouble:
      // Treated as a bitwise representation. See javadoc comments for
      // java.lang.invoke.MethodHandles.byteArrayViewVarHandle().
      return ByteArrayViewAccessor<int64_t>::Dispatch(access_mode,
                                                      data,
                                                      data_offset,
                                                      byte_swap,
                                                      &getter,
                                                      result);
  }
  LOG(FATAL) << "Unreachable: Unexpected primitive " << primitive_type;
  UNREACHABLE();
}

Class* ByteBufferViewVarHandle::StaticClass() REQUIRES_SHARED(Locks::mutator_lock_) {
  return static_class_.Read();
}

void ByteBufferViewVarHandle::SetClass(Class* klass) {
  CHECK(static_class_.IsNull()) << static_class_.Read() << " " << klass;
  CHECK(klass != nullptr);
  static_class_ = GcRoot<Class>(klass);
}

void ByteBufferViewVarHandle::ResetClass() {
  CHECK(!static_class_.IsNull());
  static_class_ = GcRoot<Class>(nullptr);
}

void ByteBufferViewVarHandle::VisitRoots(RootVisitor* visitor) {
  static_class_.VisitRootIfNonNull(visitor, RootInfo(kRootStickyClass));
}

GcRoot<Class> ByteBufferViewVarHandle::static_class_;

}  // namespace mirror
}  // namespace art
