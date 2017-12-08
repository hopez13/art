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

#include "art_field-inl.h"
#include "class-inl.h"
#include "class_linker.h"
#include "gc_root-inl.h"
#include "method_handles.h"  // For type conversion helpers.
#include "method_type.h"

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

// Map of VarHandle accessor method names to access mode values. The
// list is alpha-sorted to support binary search.
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

size_t GetNumberOfVarTypeParameters(VarHandle::AccessMode access_mode) {
  AccessModeTemplate access_mode_template = GetAccessModeTemplate(access_mode);
  return GetNumberOfVarTypeParameters(access_mode_template);
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
// Unsupported access modes
//

#define UNSUPPORTED_ACCESS_MODE(ACCESS_MODE, TYPE)                                    \
template<> void ACCESS_MODE ## Accessor<TYPE>::Access(TYPE*) {                        \
  LOG(FATAL) << "Unsupported access mode :" << #ACCESS_MODE << " for type " << #TYPE; \
  UNREACHABLE();                                                                      \
}

UNSUPPORTED_ACCESS_MODE(GetAndAdd, float);
UNSUPPORTED_ACCESS_MODE(GetAndAddAcquire, float);
UNSUPPORTED_ACCESS_MODE(GetAndAddRelease, float);
UNSUPPORTED_ACCESS_MODE(GetAndBitwiseOr, float);
UNSUPPORTED_ACCESS_MODE(GetAndBitwiseOrAcquire, float);
UNSUPPORTED_ACCESS_MODE(GetAndBitwiseOrRelease, float);
UNSUPPORTED_ACCESS_MODE(GetAndBitwiseAnd, float);
UNSUPPORTED_ACCESS_MODE(GetAndBitwiseAndAcquire, float);
UNSUPPORTED_ACCESS_MODE(GetAndBitwiseAndRelease, float);
UNSUPPORTED_ACCESS_MODE(GetAndBitwiseXor, float);
UNSUPPORTED_ACCESS_MODE(GetAndBitwiseXorAcquire, float);
UNSUPPORTED_ACCESS_MODE(GetAndBitwiseXorRelease, float);

UNSUPPORTED_ACCESS_MODE(GetAndAdd, double);
UNSUPPORTED_ACCESS_MODE(GetAndAddAcquire, double);
UNSUPPORTED_ACCESS_MODE(GetAndAddRelease, double);
UNSUPPORTED_ACCESS_MODE(GetAndBitwiseOr, double);
UNSUPPORTED_ACCESS_MODE(GetAndBitwiseOrAcquire, double);
UNSUPPORTED_ACCESS_MODE(GetAndBitwiseOrRelease, double);
UNSUPPORTED_ACCESS_MODE(GetAndBitwiseAnd, double);
UNSUPPORTED_ACCESS_MODE(GetAndBitwiseAndAcquire, double);
UNSUPPORTED_ACCESS_MODE(GetAndBitwiseAndRelease, double);
UNSUPPORTED_ACCESS_MODE(GetAndBitwiseXor, double);
UNSUPPORTED_ACCESS_MODE(GetAndBitwiseXorAcquire, double);
UNSUPPORTED_ACCESS_MODE(GetAndBitwiseXorRelease, double);

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
    obj->UpdateFieldBooleanViaAccessor<true>(field_offset, accessor);
  } else {
    obj->UpdateFieldBooleanViaAccessor<false>(field_offset, accessor);
  }
}

template <>
void FieldAccessViaAccessor<int8_t>::Update(ObjPtr<Object> obj,
                                            MemberOffset field_offset,
                                            Accessor* accessor)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (Runtime::Current()->IsActiveTransaction()) {
    obj->UpdateFieldByteViaAccessor<true>(field_offset, accessor);
  } else {
    obj->UpdateFieldByteViaAccessor<false>(field_offset, accessor);
  }
}

template <>
void FieldAccessViaAccessor<uint16_t>::Update(ObjPtr<Object> obj,
                                              MemberOffset field_offset,
                                              Accessor* accessor)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (Runtime::Current()->IsActiveTransaction()) {
    obj->UpdateFieldCharViaAccessor<true>(field_offset, accessor);
  } else {
    obj->UpdateFieldCharViaAccessor<false>(field_offset, accessor);
  }
}

template <>
void FieldAccessViaAccessor<int16_t>::Update(ObjPtr<Object> obj,
                                              MemberOffset field_offset,
                                              Accessor* accessor)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (Runtime::Current()->IsActiveTransaction()) {
    obj->UpdateFieldShortViaAccessor<true>(field_offset, accessor);
  } else {
    obj->UpdateFieldShortViaAccessor<false>(field_offset, accessor);
  }
}

template <>
void FieldAccessViaAccessor<int32_t>::Update(ObjPtr<Object> obj,
                                             MemberOffset field_offset,
                                             Accessor* accessor)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (Runtime::Current()->IsActiveTransaction()) {
    obj->UpdateField32ViaAccessor<true>(field_offset, accessor);
  } else {
    obj->UpdateField32ViaAccessor<false>(field_offset, accessor);
  }
}

template <>
void FieldAccessViaAccessor<int64_t>::Update(ObjPtr<Object> obj,
                                             MemberOffset field_offset,
                                             Accessor* accessor)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (Runtime::Current()->IsActiveTransaction()) {
    obj->UpdateField64ViaAccessor<true>(field_offset, accessor);
  } else {
    obj->UpdateField64ViaAccessor<false>(field_offset, accessor);
  }
}

template <>
void FieldAccessViaAccessor<float>::Update(ObjPtr<Object> obj,
                                           MemberOffset field_offset,
                                           Accessor* accessor)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  TypeAdaptorAccessor<int32_t, float> float_to_int_accessor(accessor);
  if (Runtime::Current()->IsActiveTransaction()) {
    obj->UpdateField32ViaAccessor<true>(field_offset, &float_to_int_accessor);
  } else {
    obj->UpdateField32ViaAccessor<false>(field_offset, &float_to_int_accessor);
  }
}

template <>
void FieldAccessViaAccessor<double>::Update(ObjPtr<Object> obj,
                                            MemberOffset field_offset,
                                            Accessor* accessor)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  TypeAdaptorAccessor<int64_t, double> double_to_int_accessor(accessor);
  if (Runtime::Current()->IsActiveTransaction()) {
    obj->UpdateField64ViaAccessor<true>(field_offset, &double_to_int_accessor);
  } else {
    obj->UpdateField64ViaAccessor<false>(field_offset, &double_to_int_accessor);
  }
}

template <typename T>
class ObjectAccessor {
 public:
  static T GetValue(void* values, size_t index) {
    if (sizeof(T) == 8) {
      int64_t* values64 = reinterpret_cast<int64_t*>(values);
      return *reinterpret_cast<T*>(&values64[index]);
    } else {
      int32_t* values32 = reinterpret_cast<int32_t*>(values);
      return *reinterpret_cast<T*>(&values32[index]);
    }
  }

  static bool Dispatch(VarHandle::AccessMode access_mode,
                       ObjPtr<Object> obj,
                       MemberOffset field_offset,
                       void* values,
                       size_t values_count,
                       JValue* result)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    switch (access_mode) {
      case VarHandle::AccessMode::kGet: {
        DCHECK_EQ(0u, values_count);
        GetAccessor<T> accessor(result);
        FieldAccessViaAccessor<T>::Get(obj, field_offset, &accessor);
        break;
      }
      case VarHandle::AccessMode::kSet: {
        DCHECK_EQ(1u, values_count);
        T new_value = GetValue(values, 0);
        SetAccessor<T> accessor(new_value);
        FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
        break;
      }
      case VarHandle::AccessMode::kGetVolatile: {
        DCHECK_EQ(0u, values_count);
        GetVolatileAccessor<T> accessor(result);
        FieldAccessViaAccessor<T>::Get(obj, field_offset, &accessor);
        break;
      }
      case VarHandle::AccessMode::kSetVolatile: {
        DCHECK_EQ(1u, values_count);
        T new_value = GetValue(values, 0);
        SetVolatileAccessor<T> accessor(new_value);
        FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
        break;
      }
      case VarHandle::AccessMode::kGetAcquire: {
        DCHECK_EQ(0u, values_count);
        GetAcquireAccessor<T> accessor(result);
        FieldAccessViaAccessor<T>::Get(obj, field_offset, &accessor);
        break;
      }
      case VarHandle::AccessMode::kSetRelease: {
        DCHECK_EQ(1u, values_count);
        T new_value = GetValue(values, 0);
        SetReleaseAccessor<T> accessor(new_value);
        FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
        break;
      }
      case VarHandle::AccessMode::kGetOpaque: {
        DCHECK_EQ(0u, values_count);
        GetOpaqueAccessor<T> accessor(result);
        FieldAccessViaAccessor<T>::Get(obj, field_offset, &accessor);
        break;
      }
      case VarHandle::AccessMode::kSetOpaque: {
        DCHECK_EQ(1u, values_count);
        T new_value = GetValue(values, 0);
        SetOpaqueAccessor<T> accessor(new_value);
        FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
        break;
      }
      case VarHandle::AccessMode::kCompareAndSet: {
        DCHECK_EQ(2u, values_count);
        T expected_value = GetValue(values, 0);
        T desired_value = GetValue(values, 1);
        CompareAndSetAccessor<T> accessor(expected_value, desired_value, result);
        FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
        break;
      }
      case VarHandle::AccessMode::kCompareAndExchange: {
        DCHECK_EQ(2u, values_count);
        T expected_value = GetValue(values, 0);
        T desired_value = GetValue(values, 1);
        CompareAndExchangeAccessor<T> accessor(expected_value, desired_value, result);
        FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
        break;
      }
      case VarHandle::AccessMode::kCompareAndExchangeAcquire: {
        DCHECK_EQ(2u, values_count);
        T expected_value = GetValue(values, 0);
        T desired_value = GetValue(values, 1);
        CompareAndExchangeAcquireAccessor<T> accessor(expected_value, desired_value, result);
        FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
        break;
      }
      case VarHandle::AccessMode::kCompareAndExchangeRelease: {
        DCHECK_EQ(2u, values_count);
        T expected_value = GetValue(values, 0);
        T desired_value = GetValue(values, 1);
        CompareAndExchangeReleaseAccessor<T> accessor(expected_value, desired_value, result);
        FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
        break;
      }
      case VarHandle::AccessMode::kWeakCompareAndSetPlain: {
        DCHECK_EQ(2u, values_count);
        T expected_value = GetValue(values, 0);
        T desired_value = GetValue(values, 1);
        WeakCompareAndSetPlainAccessor<T> accessor(expected_value, desired_value, result);
        FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
        break;
      }
      case VarHandle::AccessMode::kWeakCompareAndSet: {
        DCHECK_EQ(2u, values_count);
        T expected_value = GetValue(values, 0);
        T desired_value = GetValue(values, 1);
        WeakCompareAndSetAccessor<T> accessor(expected_value, desired_value, result);
        FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
        break;
      }
      case VarHandle::AccessMode::kWeakCompareAndSetAcquire: {
        DCHECK_EQ(2u, values_count);
        T expected_value = GetValue(values, 0);
        T desired_value = GetValue(values, 1);
        WeakCompareAndSetAcquireAccessor<T> accessor(expected_value, desired_value, result);
        FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
        break;
      }
      case VarHandle::AccessMode::kWeakCompareAndSetRelease: {
        DCHECK_EQ(2u, values_count);
        T expected_value = GetValue(values, 0);
        T desired_value = GetValue(values, 1);
        WeakCompareAndSetReleaseAccessor<T> accessor(expected_value, desired_value, result);
        FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
        break;
      }
      case VarHandle::AccessMode::kGetAndSet: {
        DCHECK_EQ(1u, values_count);
        T new_value = GetValue(values, 0);
        GetAndSetAccessor<T> accessor(new_value, result);
        FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
        break;
      }
      case VarHandle::AccessMode::kGetAndSetAcquire: {
        DCHECK_EQ(1u, values_count);
        T new_value = GetValue(values, 0);
        GetAndSetAcquireAccessor<T> accessor(new_value, result);
        FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
        break;
      }
      case VarHandle::AccessMode::kGetAndSetRelease: {
        DCHECK_EQ(1u, values_count);
        T new_value = GetValue(values, 0);
        GetAndSetReleaseAccessor<T> accessor(new_value, result);
        FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
        break;
      }
      case VarHandle::AccessMode::kGetAndAdd: {
        DCHECK_EQ(1u, values_count);
        T value = GetValue(values, 0);
        GetAndAddAccessor<T> accessor(value, result);
        FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
        break;
      }
      case VarHandle::AccessMode::kGetAndAddAcquire: {
        DCHECK_EQ(1u, values_count);
        T value = GetValue(values, 0);
        GetAndAddAcquireAccessor<T> accessor(value, result);
        FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
        break;
      }
      case VarHandle::AccessMode::kGetAndAddRelease: {
        DCHECK_EQ(1u, values_count);
        T value = GetValue(values, 0);
        GetAndAddReleaseAccessor<T> accessor(value, result);
        FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
        break;
      }
      case VarHandle::AccessMode::kGetAndBitwiseOr: {
        DCHECK_EQ(1u, values_count);
        T value = GetValue(values, 0);
        GetAndBitwiseOrAccessor<T> accessor(value, result);
        FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
        break;
      }
      case VarHandle::AccessMode::kGetAndBitwiseOrAcquire: {
        DCHECK_EQ(1u, values_count);
        T value = GetValue(values, 0);
        GetAndBitwiseOrAcquireAccessor<T> accessor(value, result);
        FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
        break;
      }
      case VarHandle::AccessMode::kGetAndBitwiseOrRelease: {
        DCHECK_EQ(1u, values_count);
        T value = GetValue(values, 0);
        GetAndBitwiseOrReleaseAccessor<T> accessor(value, result);
        FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
        break;
      }
      case VarHandle::AccessMode::kGetAndBitwiseAnd: {
        DCHECK_EQ(1u, values_count);
        T value = GetValue(values, 0);
        GetAndBitwiseAndAccessor<T> accessor(value, result);
        FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
        break;
      }
      case VarHandle::AccessMode::kGetAndBitwiseAndAcquire: {
        DCHECK_EQ(1u, values_count);
        T value = GetValue(values, 0);
        GetAndBitwiseAndAcquireAccessor<T> accessor(value, result);
        FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
        break;
      }
      case VarHandle::AccessMode::kGetAndBitwiseAndRelease: {
        DCHECK_EQ(1u, values_count);
        T value = GetValue(values, 0);
        GetAndBitwiseAndReleaseAccessor<T> accessor(value, result);
        FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
        break;
      }
      case VarHandle::AccessMode::kGetAndBitwiseXor: {
        DCHECK_EQ(1u, values_count);
        T value = GetValue(values, 0);
        GetAndBitwiseXorAccessor<T> accessor(value, result);
        FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
        break;
      }
      case VarHandle::AccessMode::kGetAndBitwiseXorAcquire: {
        DCHECK_EQ(1u, values_count);
        T value = GetValue(values, 0);
        GetAndBitwiseXorAcquireAccessor<T> accessor(value, result);
        FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
        break;
      }
      case VarHandle::AccessMode::kGetAndBitwiseXorRelease: {
        DCHECK_EQ(1u, values_count);
        T value = GetValue(values, 0);
        GetAndBitwiseXorReleaseAccessor<T> accessor(value, result);
        FieldAccessViaAccessor<T>::Update(obj, field_offset, &accessor);
        break;
      }
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
  ObjPtr<Object> ct0;
  ArtField* field = GetField();
  size_t index = 0;
  if (field->IsStatic()) {
    DCHECK_LE(operands->GetNumberOfOperands(), 2u);
    ct0 = field->GetDeclaringClass();
  } else {
    DCHECK_GE(operands->GetNumberOfOperands(), 1u);
    DCHECK_LE(operands->GetNumberOfOperands(), 3u);
    ct0 = shadow_frame->GetVRegReference(operands->GetOperand(index++));
  }
  DCHECK(!ct0.IsNull());

  MemberOffset offset = field->GetOffset();
  Primitive::Type primitive_type = GetVarType()->GetPrimitiveType();
  if (primitive_type == Primitive::Type::kPrimNot) {
    UNIMPLEMENTED(FATAL) << "TODO(oth): b/65872996";
    return false;
  } else if (Primitive::Is64BitType(primitive_type)) {
    size_t values_count = operands->GetNumberOfOperands() - index;
    DCHECK_EQ(GetNumberOfVarTypeParameters(access_mode), values_count);
    int64_t values[2];
    for (size_t i = 0; i < values_count; ++i) {
      values[i] = shadow_frame->GetVRegLong(operands->GetOperand(index++));
    }
    switch (primitive_type) {
      case Primitive::kPrimLong:
        return ObjectAccessor<int64_t>::Dispatch(access_mode,
                                                 ct0,
                                                 offset,
                                                 values,
                                                 values_count,
                                                 result);
      case Primitive::kPrimDouble:
        return ObjectAccessor<double>::Dispatch(access_mode,
                                                ct0,
                                                offset,
                                                values,
                                                values_count,
                                                result);
      default:
        break;
    }
  } else {
    size_t values_count = operands->GetNumberOfOperands() - index;
    DCHECK_EQ(GetNumberOfVarTypeParameters(access_mode), values_count);
    int32_t values[2];
    for (size_t i = 0; i < values_count; ++i) {
      values[i] = shadow_frame->GetVReg(operands->GetOperand(index++));
    }
    switch (primitive_type) {
    case Primitive::kPrimBoolean:
      return ObjectAccessor<uint8_t>::Dispatch(access_mode,
                                               ct0,
                                               offset,
                                               values,
                                               values_count,
                                               result);
    case Primitive::kPrimByte:
      return ObjectAccessor<int8_t>::Dispatch(access_mode,
                                              ct0,
                                              offset,
                                              values,
                                              values_count,
                                              result);
    case Primitive::kPrimChar:
      return ObjectAccessor<uint16_t>::Dispatch(access_mode,
                                                ct0,
                                                offset,
                                                values,
                                                values_count,
                                                result);
    case Primitive::kPrimShort:
      return ObjectAccessor<int16_t>::Dispatch(access_mode,
                                               ct0,
                                               offset,
                                               values,
                                               values_count,
                                               result);
    case Primitive::kPrimInt:
      return ObjectAccessor<int32_t>::Dispatch(access_mode,
                                               ct0,
                                               offset,
                                               values,
                                               values_count,
                                               result);
    case Primitive::kPrimFloat:
      return ObjectAccessor<float>::Dispatch(access_mode,
                                             ct0,
                                             offset,
                                             values,
                                             values_count,
                                             result);
    default:
      break;
    }
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

bool ArrayElementVarHandle::Access(AccessMode access_mode ATTRIBUTE_UNUSED,
                                   ShadowFrame* shadow_frame ATTRIBUTE_UNUSED,
                                   InstructionOperands* operands ATTRIBUTE_UNUSED,
                                   JValue* result ATTRIBUTE_UNUSED) {
  UNIMPLEMENTED(FATAL) << "TODO(oth): b/65872996";
  return false;
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

bool ByteArrayViewVarHandle::Access(AccessMode access_mode ATTRIBUTE_UNUSED,
                                    ShadowFrame* shadow_frame ATTRIBUTE_UNUSED,
                                    InstructionOperands* operands ATTRIBUTE_UNUSED,
                                    JValue* result ATTRIBUTE_UNUSED) {
  UNIMPLEMENTED(FATAL) << "TODO(oth): b/65872996";
  return false;
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

bool ByteBufferViewVarHandle::Access(AccessMode access_mode ATTRIBUTE_UNUSED,
                                     ShadowFrame* shadow_frame ATTRIBUTE_UNUSED,
                                     InstructionOperands* operands ATTRIBUTE_UNUSED,
                                     JValue* result ATTRIBUTE_UNUSED) {
  UNIMPLEMENTED(FATAL) << "TODO(oth): b/65872996";
  return false;
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
