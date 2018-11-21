/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_RUNTIME_INTERPRETER_SHADOW_FRAME_H_
#define ART_RUNTIME_INTERPRETER_SHADOW_FRAME_H_

#include <cstdint>
#include <cstring>
#include <string>

#include "base/casts.h"
#include "base/macros.h"
#include "base/mutex.h"
#include "dex/dex_file.h"
#include "lock_count_data.h"
#include "read_barrier.h"
#include "stack_reference.h"
#include "verify_object.h"

namespace art {

namespace mirror {
class Object;
}  // namespace mirror

class ArtMethod;
class ShadowFrame;
template<class MirrorType> class ObjPtr;
class Thread;
union JValue;

// Forward declaration. Just calls the destructor.
struct ShadowFrameDeleter;
using ShadowFrameAllocaUniquePtr = std::unique_ptr<ShadowFrame, ShadowFrameDeleter>;

// Register values are stored twice in the shadow frame to support garbage collection.
// Note that long and double values therefore are not trivially consecutive in memory.
//
// Note that this works almost unmodified for 64-bit references - if the heap is above
// the low 4GB, we can still use the high 32-bits to determine the value is an object.
class alignas(uint64_t) ShadowVreg {
 public:
  uint32_t value;
  uint32_t object;  // Stores copy of the value if it is an object, otherwise it is 0.
};

class ShadowFrame {
 private:
  // Used to keep track of extra state the shadowframe has.
  enum class FrameFlags : uint32_t {
    // We have been requested to notify when this frame gets popped.
    kNotifyFramePop = 1 << 0,
    // We have been asked to pop this frame off the stack as soon as possible.
    kForcePopFrame  = 1 << 1,
    // We have been asked to re-execute the last instruction.
    kForceRetryInst = 1 << 2,
  };

 public:
  // Compute size of ShadowFrame in bytes.
  static size_t ComputeSize(uint32_t num_vregs) {
    return sizeof(ShadowFrame) + (sizeof(ShadowVreg) * num_vregs);
  }

  // Create ShadowFrame in heap for deoptimization.
  static ShadowFrame* CreateDeoptimizedFrame(uint32_t num_vregs, ShadowFrame* link,
                                             ArtMethod* method, uint32_t dex_pc) {
    uint8_t* memory = new uint8_t[ComputeSize(num_vregs)];
    return CreateShadowFrameImpl(num_vregs, link, method, dex_pc, memory);
  }

  // Delete a ShadowFrame allocated on the heap for deoptimization.
  static void DeleteDeoptimizedFrame(ShadowFrame* sf) {
    sf->~ShadowFrame();  // Explicitly destruct.
    uint8_t* memory = reinterpret_cast<uint8_t*>(sf);
    delete[] memory;
  }

  // Create a shadow frame in a fresh alloca. This needs to be in the context of the caller.
  // Inlining doesn't work, the compiler will still undo the alloca. So this needs to be a macro.
#define CREATE_SHADOW_FRAME(num_vregs, link, method, dex_pc) ({                              \
    size_t frame_size = ShadowFrame::ComputeSize(num_vregs);                                 \
    void* alloca_mem = alloca(frame_size);                                                   \
    ShadowFrameAllocaUniquePtr(                                                              \
        ShadowFrame::CreateShadowFrameImpl((num_vregs), (link), (method), (dex_pc),          \
                                           (alloca_mem)));                                   \
    })

  ~ShadowFrame() {}

  uint32_t NumberOfVRegs() const {
    return number_of_vregs_;
  }

  uint32_t GetDexPC() const {
    return (dex_pc_ptr_ == nullptr) ? dex_pc_ : dex_pc_ptr_ - dex_instructions_;
  }

  int16_t GetCachedHotnessCountdown() const {
    return cached_hotness_countdown_;
  }

  void SetCachedHotnessCountdown(int16_t cached_hotness_countdown) {
    cached_hotness_countdown_ = cached_hotness_countdown;
  }

  int16_t GetHotnessCountdown() const {
    return hotness_countdown_;
  }

  void SetHotnessCountdown(int16_t hotness_countdown) {
    hotness_countdown_ = hotness_countdown;
  }

  void SetDexPC(uint32_t dex_pc) {
    dex_pc_ = dex_pc;
    dex_pc_ptr_ = nullptr;
  }

  ShadowFrame* GetLink() const {
    return link_;
  }

  void SetLink(ShadowFrame* frame) {
    DCHECK_NE(this, frame);
    link_ = frame;
  }

  int32_t GetVReg(size_t idx) const {
    DCHECK_LT(idx, NumberOfVRegs());
    return vregs_[idx].value;
  }

  // Shorts are extended to Ints in VRegs.  Interpreter intrinsics needs them as shorts.
  int16_t GetVRegShort(size_t i) const {
    return static_cast<int16_t>(GetVReg(i));
  }

  ALWAYS_INLINE void CopyArgsTo(uint32_t* dst, size_t first_idx, size_t last_idx) {
    DCHECK_LE(first_idx, last_idx);
    DCHECK_LE(last_idx, NumberOfVRegs());
    for (; first_idx < last_idx; ++dst, ++first_idx) {
      *dst = GetVReg(first_idx);
    }
  }

  const uint16_t* GetDexInstructions() const {
    return dex_instructions_;
  }

  float GetVRegFloat(size_t idx) const {
    return bit_cast<float>(GetVReg(idx));
  }

  int64_t GetVRegLong(size_t idx) const {
    uint32_t lo = GetVReg(idx);
    uint32_t hi = GetVReg(idx + 1);
    return lo | (static_cast<uint64_t>(hi) << 32);
  }

  double GetVRegDouble(size_t idx) const {
    return bit_cast<double>(GetVRegLong(idx));
  }

  // Returns the object reference or nullptr if the register holds primitive value.
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  mirror::Object* GetVRegReference(size_t i) const REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK_LT(i, NumberOfVRegs());
    static_assert(sizeof(StackReference<mirror::Object>) == sizeof(vregs_[i].object), "size");
    mirror::Object* ref =
        reinterpret_cast<const StackReference<mirror::Object>*>(&vregs_[i].object)->AsMirrorPtr();
    ReadBarrier::MaybeAssertToSpaceInvariant(ref);
    if (kVerifyFlags & kVerifyReads) {
      VerifyObject(ref);
    }
    return ref;
  }

  ALWAYS_INLINE void SetVReg(size_t idx, int32_t val) {
    DCHECK_LT(idx, NumberOfVRegs());
    vregs_[idx].value = val;
    vregs_[idx].object = 0;
  }

  ALWAYS_INLINE void SetVRegFloat(size_t idx, float val) {
    SetVReg(idx, bit_cast<uint32_t>(val));
  }

  ALWAYS_INLINE void SetVRegLong(size_t idx, int64_t val) {
    SetVReg(idx, static_cast<int32_t>(val));
    SetVReg(idx + 1, static_cast<int32_t>(val >> 32));
  }

  ALWAYS_INLINE void SetVRegDouble(size_t idx, double val) {
    SetVRegLong(idx, bit_cast<int64_t>(val));
  }

  ALWAYS_INLINE void SetVRegFrom(size_t idx, ShadowFrame& other, size_t other_idx) {
    DCHECK_LT(idx, NumberOfVRegs());
    DCHECK_LT(other_idx, other.NumberOfVRegs());
    vregs_[idx] = other.vregs_[other_idx];
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  void SetVRegReference(size_t i, ObjPtr<mirror::Object> val)
      REQUIRES_SHARED(Locks::mutator_lock_);

  void SetMethod(ArtMethod* method) REQUIRES(Locks::mutator_lock_) {
    DCHECK(method != nullptr);
    DCHECK(method_ != nullptr);
    method_ = method;
  }

  ArtMethod* GetMethod() const REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK(method_ != nullptr);
    return method_;
  }

  mirror::Object* GetThisObject() const REQUIRES_SHARED(Locks::mutator_lock_);

  mirror::Object* GetThisObject(uint16_t num_ins) const REQUIRES_SHARED(Locks::mutator_lock_);

  bool Contains(StackReference<mirror::Object>* shadow_frame_entry_obj) const {
    ShadowVreg* ptr = reinterpret_cast<ShadowVreg*>(shadow_frame_entry_obj);
    return &vregs_[0] <= ptr && ptr < &vregs_[number_of_vregs_];
  }

  LockCountData& GetLockCountData() {
    return lock_count_data_;
  }

  static constexpr size_t LockCountDataOffset() {
    return OFFSETOF_MEMBER(ShadowFrame, lock_count_data_);
  }

  static constexpr size_t LinkOffset() {
    return OFFSETOF_MEMBER(ShadowFrame, link_);
  }

  static constexpr size_t MethodOffset() {
    return OFFSETOF_MEMBER(ShadowFrame, method_);
  }

  static constexpr size_t DexPCOffset() {
    return OFFSETOF_MEMBER(ShadowFrame, dex_pc_);
  }

  static constexpr size_t NumberOfVRegsOffset() {
    return OFFSETOF_MEMBER(ShadowFrame, number_of_vregs_);
  }

  static constexpr size_t VRegsOffset() {
    return OFFSETOF_MEMBER(ShadowFrame, vregs_);
  }

  static constexpr size_t ResultRegisterOffset() {
    return OFFSETOF_MEMBER(ShadowFrame, result_register_);
  }

  static constexpr size_t DexPCPtrOffset() {
    return OFFSETOF_MEMBER(ShadowFrame, dex_pc_ptr_);
  }

  static constexpr size_t DexInstructionsOffset() {
    return OFFSETOF_MEMBER(ShadowFrame, dex_instructions_);
  }

  static constexpr size_t CachedHotnessCountdownOffset() {
    return OFFSETOF_MEMBER(ShadowFrame, cached_hotness_countdown_);
  }

  static constexpr size_t HotnessCountdownOffset() {
    return OFFSETOF_MEMBER(ShadowFrame, hotness_countdown_);
  }

  // Create ShadowFrame for interpreter using provided memory.
  static ShadowFrame* CreateShadowFrameImpl(uint32_t num_vregs,
                                            ShadowFrame* link,
                                            ArtMethod* method,
                                            uint32_t dex_pc,
                                            void* memory) {
    return new (memory) ShadowFrame(num_vregs, link, method, dex_pc);
  }

  const uint16_t* GetDexPCPtr() {
    return dex_pc_ptr_;
  }

  void SetDexPCPtr(uint16_t* dex_pc_ptr) {
    dex_pc_ptr_ = dex_pc_ptr;
  }

  JValue* GetResultRegister() {
    return result_register_;
  }

  bool NeedsNotifyPop() const {
    return GetFrameFlag(FrameFlags::kNotifyFramePop);
  }

  void SetNotifyPop(bool notify) {
    UpdateFrameFlag(notify, FrameFlags::kNotifyFramePop);
  }

  bool GetForcePopFrame() const {
    return GetFrameFlag(FrameFlags::kForcePopFrame);
  }

  void SetForcePopFrame(bool enable) {
    UpdateFrameFlag(enable, FrameFlags::kForcePopFrame);
  }

  bool GetForceRetryInstruction() const {
    return GetFrameFlag(FrameFlags::kForceRetryInst);
  }

  void SetForceRetryInstruction(bool enable) {
    UpdateFrameFlag(enable, FrameFlags::kForceRetryInst);
  }

  void CheckConsistentVRegs() const {
    if (kIsDebugBuild) {
      // A shadow frame visible to GC requires the following rule: for a given vreg,
      // its vreg reference equivalent should be the same, or null.
      for (uint32_t i = 0; i < NumberOfVRegs(); ++i) {
        ShadowVreg vreg = vregs_[i];
        CHECK((vreg.value == vreg.object) || (vreg.object == 0));
      }
    }
  }

 private:
  ShadowFrame(uint32_t num_vregs, ShadowFrame* link, ArtMethod* method, uint32_t dex_pc)
      : link_(link),
        method_(method),
        result_register_(nullptr),
        dex_pc_ptr_(nullptr),
        dex_instructions_(nullptr),
        number_of_vregs_(num_vregs),
        dex_pc_(dex_pc),
        cached_hotness_countdown_(0),
        hotness_countdown_(0),
        frame_flags_(0) {
    memset(vregs_, 0, num_vregs * sizeof(*vregs_));
  }

  void UpdateFrameFlag(bool enable, FrameFlags flag) {
    if (enable) {
      frame_flags_ |= static_cast<uint32_t>(flag);
    } else {
      frame_flags_ &= ~static_cast<uint32_t>(flag);
    }
  }

  bool GetFrameFlag(FrameFlags flag) const {
    return (frame_flags_ & static_cast<uint32_t>(flag)) != 0;
  }

  // Link to previous shadow frame or null.
  ShadowFrame* link_;
  ArtMethod* method_;
  JValue* result_register_;
  const uint16_t* dex_pc_ptr_;
  // Dex instruction base of the code item.
  const uint16_t* dex_instructions_;
  LockCountData lock_count_data_;  // This may contain GC roots when lock counting is active.
  const uint32_t number_of_vregs_;
  uint32_t dex_pc_;
  int16_t cached_hotness_countdown_;
  int16_t hotness_countdown_;

  // This is a set of ShadowFrame::FrameFlags which denote special states this frame is in.
  // NB alignment requires that this field takes 4 bytes no matter its size. Only 3 bits are
  // currently used.
  uint32_t frame_flags_;

  ShadowVreg vregs_[0];

  DISALLOW_IMPLICIT_CONSTRUCTORS(ShadowFrame);
};

struct ShadowFrameDeleter {
  inline void operator()(ShadowFrame* frame) {
    if (frame != nullptr) {
      frame->~ShadowFrame();
    }
  }
};

}  // namespace art

#endif  // ART_RUNTIME_INTERPRETER_SHADOW_FRAME_H_
