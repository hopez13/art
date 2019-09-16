/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef ART_RUNTIME_REFLECTIVE_HANDLE_SCOPE_H_
#define ART_RUNTIME_REFLECTIVE_HANDLE_SCOPE_H_

#include <android-base/logging.h>

#include <array>
#include <compare>
#include <functional>
#include <stack>

#include "android-base/macros.h"
#include "base/enums.h"
#include "base/globals.h"
#include "base/locks.h"
#include "base/macros.h"
#include "base/value_object.h"
#include "reflective_handle.h"
#include "reflective_reference.h"

namespace art {

class ArtField;
class ArtMethod;
class BaseReflectiveHandleScope;
class Thread;

enum ReflectionSourceType {
  kSourceUnknown = 0,
  kSourceJavaLangReflectExecutable,
  kSourceJavaLangReflectField,
  kSourceJavaLangInvokeMethodHandle,
  kSourceJavaLangInvokeFieldVarHandle,
  kSourceThreadHandleScope,
  kSourceMiscInternal,
};
std::ostream& operator<<(std::ostream& os, const ReflectionSourceType& type);

class ReflectionSourceInfo : public ValueObject {
 public:
  virtual ~ReflectionSourceInfo() {}
  // Thread id 0 is for non thread roots.
  explicit ReflectionSourceInfo(ReflectionSourceType type) : type_(type) {}
  virtual void Describe(std::ostream& os) const {
    os << "Type=" << type_;
  }

 private:
  const ReflectionSourceType type_;

  DISALLOW_COPY_AND_ASSIGN(ReflectionSourceInfo);
};
inline std::ostream& operator<<(std::ostream& os, const ReflectionSourceInfo& info) {
  info.Describe(os);
  return os;
}

class ReflectiveHandleScopeSourceInfo : public ReflectionSourceInfo {
 public:
  ReflectiveHandleScopeSourceInfo(BaseReflectiveHandleScope* source)
      : ReflectionSourceInfo(kSourceThreadHandleScope), source_(source) {}

  void Describe(std::ostream& os) const override {
    ReflectionSourceInfo::Describe(os);
    os << " source=" << source_;
  }

 private:
  BaseReflectiveHandleScope* source_;
};

// TODO Maybe make this public and give this the ability to retrieve the type and ref, if it's
// useful.
class HeapReflectiveSourceInfo : public ReflectionSourceInfo {
public:
  explicit HeapReflectiveSourceInfo(ReflectionSourceType t, mirror::Object* src) : ReflectionSourceInfo(t), src_(src) {}
  void Describe(std::ostream& os) const override;

private:
  ObjPtr<mirror::Object> src_;
};

class ReflectiveValueVisitor : public ValueObject {
 public:
  virtual ~ReflectiveValueVisitor() {}

  virtual ArtMethod* VisitMethod(ArtMethod* in, const ReflectionSourceInfo& info)
      REQUIRES_SHARED(Locks::mutator_lock_) = 0;
  virtual ArtField* VisitField(ArtField* in, const ReflectionSourceInfo& info)
      REQUIRES_SHARED(Locks::mutator_lock_) = 0;

  template <typename T,
            typename = typename std::enable_if<std::is_same_v<T, ArtField> ||
                                               std::is_same_v<T, ArtMethod>>>
  T* operator()(T* t, const ReflectionSourceInfo& info) REQUIRES_SHARED(Locks::mutator_lock_) {
    if constexpr (std::is_same_v<T, ArtField>) {
      return VisitField(t, info);
    } else {
      return VisitMethod(t, info);
    }
  }
};

template <typename T>
class FunctionReflectiveValueVisitor : public ReflectiveValueVisitor {
 public:
  FunctionReflectiveValueVisitor(T t) : t_(t) {}
  ArtField* VisitField(ArtField* in, const ReflectionSourceInfo& info) override
      REQUIRES_SHARED(Locks::mutator_lock_) {
    return t_(in, info);
  }
  ArtMethod* VisitMethod(ArtMethod* in, const ReflectionSourceInfo& info) override
      REQUIRES_SHARED(Locks::mutator_lock_) {
    return t_(in, info);
  }

 private:
  T t_;
};

class BaseReflectiveHandleScope {
 public:
  template <typename Visitor>
  ALWAYS_INLINE void VisitTargets(Visitor& visitor) REQUIRES_SHARED(Locks::mutator_lock_) {
    FunctionReflectiveValueVisitor v(&visitor);
    VisitTargets(&v);
  }

  ALWAYS_INLINE virtual ~BaseReflectiveHandleScope() {
    DCHECK(link_ == nullptr);
  }

  virtual void VisitTargets(ReflectiveValueVisitor* visitor)
      REQUIRES_SHARED(Locks::mutator_lock_) = 0;

  BaseReflectiveHandleScope* GetLink() {
    return link_;
  }

  Thread* GetThread() {
    return self_;
  }

 protected:
  ALWAYS_INLINE BaseReflectiveHandleScope() : self_(nullptr), link_(nullptr) {}

  ALWAYS_INLINE inline void PushScope(Thread* self) REQUIRES_SHARED(Locks::mutator_lock_);
  ALWAYS_INLINE inline void PopScope() REQUIRES_SHARED(Locks::mutator_lock_);

  // Thread this node is rooted in.
  Thread* self_;
  // Next node in the handle-scope linked list. Root is held by Thread.
  BaseReflectiveHandleScope* link_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BaseReflectiveHandleScope);
};

template <size_t kNumFields, size_t kNumMethods>
class StackReflectiveHandleScope : public BaseReflectiveHandleScope {
 private:
  static constexpr bool kHasFields = kNumFields > 0;
  static constexpr bool kHasMethods = kNumMethods > 0;

 public:
  ALWAYS_INLINE explicit StackReflectiveHandleScope(Thread* self)
      REQUIRES_SHARED(Locks::mutator_lock_);
  ALWAYS_INLINE ~StackReflectiveHandleScope() REQUIRES_SHARED(Locks::mutator_lock_);

  void VisitTargets(ReflectiveValueVisitor* visitor) override REQUIRES_SHARED(Locks::mutator_lock_);

  template <typename = typename std::enable_if_t<kHasFields>>
  ALWAYS_INLINE MutableReflectiveHandle<ArtField> NewFieldHandle(ArtField* f)
      REQUIRES_SHARED(art::Locks::mutator_lock_) {
    DCHECK_LT(field_pos_, kNumFields);
    ++field_pos_;
    MutableReflectiveHandle<ArtField> fh(GetMutableMethodHandle(field_pos_));
    fh.Assign(f);
    return fh;
  }

  template <typename = typename std::enable_if_t<kHasFields>>
  ALWAYS_INLINE ArtField* GetField(size_t i) {
    return GetFieldReference(i)->Ptr();
  }
  template <typename = typename std::enable_if_t<kHasFields>>
  ALWAYS_INLINE ReflectiveHandle<ArtField> GetFieldHandle(size_t i) {
    return ReflectiveHandle<ArtField>(GetFieldReference(i));
  }
  template <typename = typename std::enable_if_t<kHasFields>>
  ALWAYS_INLINE MutableReflectiveHandle<ArtField> GetMutableFieldHandle(size_t i) {
    return MutableReflectiveHandle<ArtField>(GetFieldReference(i));
  }

  template <typename = typename std::enable_if_t<kHasMethods>>
  ALWAYS_INLINE MutableReflectiveHandle<ArtMethod> NewMethodHandle(ArtMethod* m)
      REQUIRES_SHARED(art::Locks::mutator_lock_) {
    DCHECK_LT(method_pos_, kNumMethods);
    ++method_pos_;
    MutableReflectiveHandle<ArtMethod> mh(GetMutableMethodHandle(method_pos_));
    mh.Assign(m);
    return mh;
  }

  template <typename = typename std::enable_if_t<kHasMethods>>
  ALWAYS_INLINE ArtMethod* GetMethod(size_t i) {
    return GetMethodReference(i)->Ptr();
  }
  template <typename = typename std::enable_if_t<kHasMethods>>
  ALWAYS_INLINE ReflectiveHandle<ArtMethod> GetMethodHandle(size_t i) {
    return ReflectiveHandle<ArtMethod>(GetMethodReference(i));
  }
  template <typename = typename std::enable_if_t<kHasMethods>>
  ALWAYS_INLINE MutableReflectiveHandle<ArtMethod> GetMutableMethodHandle(size_t i) {
    return MutableReflectiveHandle<ArtMethod>(GetMethodReference(i));
  }

  size_t RemainingFieldSlots() const {
    return kNumFields - field_pos_;
  }

  size_t RemainingMethodSlots() const {
    return kNumMethods - method_pos_;
  }

 private:
  ReflectiveReference<ArtMethod>* GetMethodReference(size_t i) {
    DCHECK_LT(i, method_pos_);
    return &methods_[i];
  }

  ReflectiveReference<ArtField>* GetFieldReference(size_t i) {
    DCHECK_LT(i, field_pos_);
    return &fields_[i];
  }

  size_t field_pos_;
  size_t method_pos_;
  std::array<ReflectiveReference<ArtField>, kNumFields> fields_;
  std::array<ReflectiveReference<ArtMethod>, kNumMethods> methods_;
};

template <size_t kNumMethods>
using StackArtMethodHandleScope = StackReflectiveHandleScope</*kNumFields=*/0, kNumMethods>;

template <size_t kNumFields>
using StackArtFieldHandleScope = StackReflectiveHandleScope<kNumFields, /*kNumMethods=*/0>;

}  // namespace art

#endif  // ART_RUNTIME_REFLECTIVE_HANDLE_SCOPE_H_
