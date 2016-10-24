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

#include "emulated_stack_frame.h"

#include "class-inl.h"
#include "gc_root-inl.h"
#include "jvalue-inl.h"
#include "method_handles.h"
#include "method_handles-inl.h"
#include "reflection-inl.h"

namespace art {
namespace mirror {

GcRoot<mirror::Class> EmulatedStackFrame::static_class_;

REQUIRES_SHARED(Locks::mutator_lock_)
static void CalculateFrameAndReferencesSize(ObjPtr<mirror::ObjectArray<mirror::Class>> p_types,
                                            ObjPtr<mirror::Class> r_type,
                                            size_t* frame_size_out,
                                            size_t* references_size_out) {
  const size_t length = p_types->GetLength();
  size_t frame_size = 0;
  size_t references_size = 0;
  for (size_t i = 0; i < length; ++i) {
    ObjPtr<mirror::Class> type = p_types->GetWithoutChecks(i);
    const Primitive::Type primitive_type = type->GetPrimitiveType();
    if (primitive_type == Primitive::kPrimNot) {
      references_size++;
    } else if (Primitive::Is64BitType(primitive_type)) {
      frame_size += 8;
    } else {
      frame_size += 4;
    }
  }

  const Primitive::Type return_type = r_type->GetPrimitiveType();
  if (return_type == Primitive::kPrimNot) {
    references_size++;
  } else if (Primitive::Is64BitType(return_type)) {
    frame_size += 8;
  } else {
    frame_size += 4;
  }

  (*frame_size_out) = frame_size;
  (*references_size_out) = references_size;
}

// 
class EmulatedStackFrameAccessor {
 public:
  EmulatedStackFrameAccessor(Handle<mirror::ObjectArray<mirror::Object>> references,
                             Handle<mirror::ByteArray> stack_frame,
                             size_t stack_frame_size) :
    references_(references),
    stack_frame_(stack_frame),
    stack_frame_size_(stack_frame_size),
    reference_idx_(0u),
    stack_frame_idx_(0u) {
  }

  REQUIRES_SHARED(Locks::mutator_lock_)
  inline void SetReference(ObjPtr<mirror::Object> reference) ALWAYS_INLINE {
    references_->Set(reference_idx_++, reference);
  }

  REQUIRES_SHARED(Locks::mutator_lock_)
  inline void Set(const uint32_t value) ALWAYS_INLINE {
    int8_t* array = stack_frame_->GetData();

    CHECK_LT((stack_frame_idx_ + 4u), stack_frame_size_);
    memcpy(array + stack_frame_idx_, &value, sizeof(uint32_t));
    stack_frame_idx_ += 4u;
  }

  REQUIRES_SHARED(Locks::mutator_lock_)
  inline void SetLong(const int64_t value) ALWAYS_INLINE {
    int8_t* array = stack_frame_->GetData();

    CHECK_LT((stack_frame_idx_ + 8u), stack_frame_size_);
    memcpy(array + stack_frame_idx_, &value, sizeof(int64_t));
    stack_frame_idx_ += 8u;
  }

  REQUIRES_SHARED(Locks::mutator_lock_)
  inline ObjPtr<mirror::Object> GetReference() ALWAYS_INLINE {
    return ObjPtr<mirror::Object>(references_->Get(reference_idx_++));
  }

  REQUIRES_SHARED(Locks::mutator_lock_)
  inline uint32_t Get() ALWAYS_INLINE {
    const int8_t* array = stack_frame_->GetData();

    CHECK_LT((stack_frame_idx_ + 4u), stack_frame_size_);
    uint32_t val = 0;

    memcpy(&val, array + stack_frame_idx_, sizeof(uint32_t));
    stack_frame_idx_ += 4u;
    return val;
  }

  REQUIRES_SHARED(Locks::mutator_lock_)
  inline int64_t GetLong() ALWAYS_INLINE {
    const int8_t* array = stack_frame_->GetData();

    CHECK_LT((stack_frame_idx_ + 8u), stack_frame_size_);
    int64_t val = 0;

    memcpy(&val, array + stack_frame_idx_, sizeof(int64_t));
    stack_frame_idx_ += 8u;
    return val;
  }

 private:
  Handle<mirror::ObjectArray<mirror::Object>> references_;
  Handle<mirror::ByteArray> stack_frame_;
  const size_t stack_frame_size_;

  size_t reference_idx_;
  size_t stack_frame_idx_;

  DISALLOW_COPY_AND_ASSIGN(EmulatedStackFrameAccessor);
};

template <bool is_range>
mirror::EmulatedStackFrame* EmulatedStackFrame::CreateFromShadowFrameAndArgs(
    Thread* self,
    Handle<mirror::MethodType> caller_type,
    Handle<mirror::MethodType> callee_type,
    const ShadowFrame& caller_frame,
    const uint32_t first_src_reg,
    const uint32_t (&arg)[Instruction::kMaxVarArgRegs]) {

  StackHandleScope<8> hs(self);
  Handle<mirror::ObjectArray<mirror::Class>> from_types(hs.NewHandle(caller_type->GetPTypes()));
  Handle<mirror::ObjectArray<mirror::Class>> to_types(hs.NewHandle(callee_type->GetPTypes()));

  Handle<mirror::Class> r_type(hs.NewHandle(callee_type->GetRType()));

  const int32_t num_method_params = from_types->GetLength();
  if (to_types->GetLength() != num_method_params) {
    ThrowWrongMethodTypeException(callee_type.Get(), caller_type.Get());
    return nullptr;
  }

  size_t frame_size = 0;
  size_t refs_size = 0;
  CalculateFrameAndReferencesSize(to_types.Get(), r_type.Get(), &frame_size, &refs_size);

  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  ObjPtr<mirror::Class> array_class(class_linker->GetClassRoot(ClassLinker::kObjectArrayClass));

  Handle<mirror::ObjectArray<mirror::Object>> references(hs.NewHandle(
      mirror::ObjectArray<mirror::Object>::Alloc(self, array_class, refs_size)));
  Handle<ByteArray> stack_frame(hs.NewHandle(ByteArray::Alloc(self, frame_size)));

  ShadowFrameGetter<is_range> getter(first_src_reg, arg, caller_frame);
  EmulatedStackFrameAccessor setter(references, stack_frame, stack_frame->GetLength());

  if (!PerformConversions<ShadowFrameGetter<is_range>, EmulatedStackFrameAccessor>(
      self, from_types, to_types, &getter, &setter, num_method_params)) {
    return nullptr;
  }

  Handle<EmulatedStackFrame> sf(hs.NewHandle(
      ObjPtr<EmulatedStackFrame>::DownCast(StaticClass()->AllocObject(self))));
  sf->SetFieldObject<false>(TypeOffset(), callee_type.Get());
  sf->SetFieldObject<false>(ReferencesOffset(), references.Get());
  sf->SetFieldObject<false>(StackFrameOffset(), stack_frame.Get());

  return sf.Get();
}

bool EmulatedStackFrame::WriteToShadowFrame(Thread* self,
                                            Handle<mirror::MethodType> callee_type,
                                            const uint32_t first_dest_reg,
                                            ShadowFrame* callee_frame) {
  StackHandleScope<6> hs(self);
  Handle<mirror::ObjectArray<mirror::Class>> from_types(hs.NewHandle(GetType()->GetPTypes()));
  Handle<mirror::ObjectArray<mirror::Class>> to_types(hs.NewHandle(callee_type->GetPTypes()));

  const int32_t num_method_params = from_types->GetLength();
  if (to_types->GetLength() != num_method_params) {
    ThrowWrongMethodTypeException(callee_type.Get(), GetType());
    return false;
  }

  Handle<mirror::ObjectArray<mirror::Object>> references(hs.NewHandle(GetReferences()));
  Handle<ByteArray> stack_frame(hs.NewHandle(GetStackFrame()));

  EmulatedStackFrameAccessor getter(references, stack_frame, stack_frame->GetLength());
  ShadowFrameSetter setter(callee_frame, first_dest_reg);

  return PerformConversions<EmulatedStackFrameAccessor, ShadowFrameSetter>(
      self, from_types, to_types, &getter, &setter, num_method_params);
}

void EmulatedStackFrame::SetClass(Class* klass) {
  CHECK(static_class_.IsNull()) << static_class_.Read() << " " << klass;
  CHECK(klass != nullptr);
  static_class_ = GcRoot<Class>(klass);
}

void EmulatedStackFrame::ResetClass() {
  CHECK(!static_class_.IsNull());
  static_class_ = GcRoot<Class>(nullptr);
}

void EmulatedStackFrame::VisitRoots(RootVisitor* visitor) {
  static_class_.VisitRootIfNonNull(visitor, RootInfo(kRootStickyClass));
}

// Explicit DoInvokePolymorphic template function declarations.
#define EXPLICIT_CREATE_FROM_SHADOW_FRAME_AND_ARGS_DECL(_is_range)                         \
  template REQUIRES_SHARED(Locks::mutator_lock_)                                           \
  mirror::EmulatedStackFrame* EmulatedStackFrame::CreateFromShadowFrameAndArgs<_is_range>( \
    Thread* self,                                                                          \
    Handle<mirror::MethodType> caller_type,                                                \
    Handle<mirror::MethodType> callee_type,                                                \
    const ShadowFrame& caller_frame,                                                       \
    const uint32_t first_src_reg,                                                          \
    const uint32_t (&arg)[Instruction::kMaxVarArgRegs])                                    \

EXPLICIT_CREATE_FROM_SHADOW_FRAME_AND_ARGS_DECL(true);
EXPLICIT_CREATE_FROM_SHADOW_FRAME_AND_ARGS_DECL(false);
#undef EXPLICIT_CREATE_FROM_SHADOW_FRAME_AND_ARGS_DECL


}  // namespace mirror
}  // namespace art
