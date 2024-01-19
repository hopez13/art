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

#ifndef ART_RUNTIME_JNI_HASH_SET_H_
#define ART_RUNTIME_JNI_HASH_SET_H_

#include <string_view>
#include <android-base/logging.h>

#include "art_method.h"
#include "arch/arm64/jni_frame_arm64.h"
#include "arch/instruction_set.h"
#include "arch/riscv64/jni_frame_riscv64.h"
#include "arch/x86_64/jni_frame_x86_64.h"
#include "base/hash_set.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/mutex.h"

namespace art {

static char TranslateArgToJniShorty(char ch) {
  // Byte, char, int, short, boolean are treated the same(e.g., Wx registers for arm64) when
  // generating jni trampoline, so their jni shorty characters are same.
  //                                   ABCDEFGHIJKLMNOPQRSTUVWXYZ
  static const char kTranslations[] = ".PPD.F..PJ.L......P......P";
  DCHECK_GE(ch, 'A');
  DCHECK_LE(ch, 'Z');
  DCHECK_NE(kTranslations[ch - 'A'], '.');
  return kTranslations[ch - 'A'];
}

static char TranslateReturnTypeToJniShorty(char ch, InstructionSet isa) {
  // For all archs, reference type is treated differently than others(has a different shorty
  // character) as it needs to be decoded in jni trampoline.
  // For arm64, small return types need sign-/zero-extended.
  // For x86_64, small return types need sign-/zero-extended, and RAX needs preserve/restore when
  // transiting from native to runnable.
  // Other archs keeps untranslated for simplicity.
  // TODO: support riscv64.
  //                                         ABCDEFGHIJKLMNOPQRSTUVWXYZ
  static const char kArm64Translations[] =  ".BCP.P..PP.L......S..P...Z";
  static const char kX86_64Translations[] = ".BCP.P..RR.L......S..P...Z";
  static const char kOtherTranslations[] =  ".BCD.F..IJ.L......S..V...Z";
  DCHECK_GE(ch, 'A');
  DCHECK_LE(ch, 'Z');
  switch (isa) {
    case InstructionSet::kArm64:
      DCHECK_NE(kArm64Translations[ch - 'A'], '.');
      return kArm64Translations[ch - 'A'];
    case InstructionSet::kX86_64:
      DCHECK_NE(kX86_64Translations[ch - 'A'], '.');
      return kX86_64Translations[ch - 'A'];
    default:
      DCHECK_NE(kOtherTranslations[ch - 'A'], '.');
      return kOtherTranslations[ch - 'A'];
  }
}

static size_t GetMaxIntLikeRegisterArgs(InstructionSet isa) {
  switch (isa) {
    case InstructionSet::kArm64:
      return arm64::kMaxIntLikeRegisterArguments;
    case InstructionSet::kX86_64:
      return x86_64::kMaxIntLikeRegisterArguments;
    case InstructionSet::kRiscv64:
      return riscv64::kMaxIntLikeArgumentRegisters;
    default:
      UNREACHABLE();
  }
}

static size_t GetMaxFloatOrDoubleRegisterArgs(InstructionSet isa) {
  switch (isa) {
    case InstructionSet::kArm64:
      return arm64::kMaxFloatOrDoubleRegisterArguments;
    case InstructionSet::kX86_64:
      return x86_64::kMaxFloatOrDoubleRegisterArguments;
    case InstructionSet::kRiscv64:
      return riscv64::kMaxFloatOrDoubleArgumentRegisters;
    default:
      UNREACHABLE();
  }
}

static size_t StackOffset(char ch) {
  if (ch == 'J' || ch == 'D') {
    return 8;
  } else {
    return 4;
  }
}

static bool IsFloatOrDoubleArg(char ch) {
  return ch == 'F' || ch == 'D';
}

static bool IsIntegralArg(char ch) {
  return ch == 'B' || ch == 'C' || ch == 'I' || ch == 'J' || ch == 'S' || ch == 'Z';
}

static bool IsReferenceArg(char ch) {
  return ch == 'L';
}

class JniHashedKey {
  public:
    JniHashedKey(): flags_(0u), shorty_(), method_(nullptr) { }
    JniHashedKey(uint32_t flags, std::string_view shorty) : shorty_(shorty), method_(nullptr) {
      DCHECK(ArtMethod::IsNative(flags));
      flags_ = flags & (kAccStatic | kAccSynchronized | kAccFastNative | kAccCriticalNative);
    }
    JniHashedKey(ArtMethod* method) NO_THREAD_SAFETY_ANALYSIS {
      uint32_t flags = method->GetAccessFlags();
      DCHECK(ArtMethod::IsNative(flags));
      flags_ = flags & (kAccStatic | kAccSynchronized | kAccFastNative | kAccCriticalNative);
      shorty_ = method->GetShortyView();
      method_ = method;
    }

    uint32_t Flags() const {
      return flags_;
    }

    std::string_view Shorty() const {
      return shorty_;
    }

    ArtMethod* Method() const {
      return method_;
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
      ArtMethod* method_;
};

class JniShortyEmpty {
  public:
    bool IsEmpty(const JniHashedKey& key) const {
      return key.IsEmpty();
    }

    void MakeEmpty(JniHashedKey& key) {
      key.MakeEmpty();
    }
};

class JniShortyHash {
  public:
    explicit JniShortyHash(InstructionSet isa) : isa_(isa) {}

    size_t operator()(const JniHashedKey& key) const {
      bool is_static = key.Flags() & kAccStatic;
      std::string_view shorty = key.Shorty();
      size_t result = key.Flags();
      result ^= TranslateReturnTypeToJniShorty(shorty[0], isa_);
      if (isa_ == InstructionSet::kArm64 || isa_ == InstructionSet::kX86_64) {
        size_t max_float_or_double_register_args = GetMaxFloatOrDoubleRegisterArgs(isa_);
        size_t max_int_like_register_args = GetMaxIntLikeRegisterArgs(isa_);
        size_t float_or_double_args = 0;
        // ArtMethod* and 'Object* this' for non-static method.
        // ArtMethod* for static method.
        size_t int_like_args = is_static ? 1 : 2;
        size_t stack_offset = 0;
        for (char c : shorty.substr(1u)) {
          bool is_stack_offset_matters = false;
          stack_offset += StackOffset(c);
          if (IsFloatOrDoubleArg(c)) {
            ++float_or_double_args;
            if (float_or_double_args > max_float_or_double_register_args) {
              // Stack offset matters if we run out of fp argument regiters because the following
              // fp args should be passed on the stack.
              is_stack_offset_matters = true;
            } else {
              // Floating-point register arguments are not touched when generating jni trampoline,
              // so could be ignored when calculating hash value.
              continue;
            }
          } else {
            ++int_like_args;
            if (int_like_args > max_int_like_register_args || IsReferenceArg(c)) {
              // Stack offset matters if we run out of integral like argument registers because the
              // following integral like args should be passed on the stack. It also matters if
              // current arg is reference type because it needs to be spilled as raw data even
              // it's in register.
              is_stack_offset_matters = true;
            } else if (!is_static) {
              // For instance method, 2 managed arguments 'ArtMethod*' and 'Object* this' correspond
              // to 'JNIEnv*' and 'jobject'. So trailing integral arguments shall just remain in the
              // same registers, which do not need any generated codes.
              continue;
            }
          }
          // int_like_args is needed for reference type because it will determine from which
          // register we take the value to construct jobject.
          if (IsReferenceArg(c)) {
            result = result * 31u * int_like_args ^ TranslateArgToJniShorty(c);
          } else {
            result = result * 31u ^ TranslateArgToJniShorty(c);
          }
          if (is_stack_offset_matters) {
            result += stack_offset;
          }
        }
      } else {
        for (char c : shorty.substr(1u)) {
          result = result * 31u ^ TranslateArgToJniShorty(c);
        }
      }
      return result;
    }

  private:
    InstructionSet isa_;
};

class JniShortyEquals {
  public:
    explicit JniShortyEquals(InstructionSet isa) : isa_(isa) {}

    size_t operator()(const JniHashedKey& lhs, const JniHashedKey& rhs) const {
      if (lhs.Flags() != rhs.Flags()) {
        return false;
      }
      std::string_view shorty_lhs = lhs.Shorty();
      std::string_view shorty_rhs = rhs.Shorty();
      if (TranslateReturnTypeToJniShorty(shorty_lhs[0], isa_) !=
          TranslateReturnTypeToJniShorty(shorty_rhs[0], isa_)) {
        return false;
      }
      if (isa_ == InstructionSet::kArm64 || isa_ == InstructionSet::kX86_64) {
        bool is_static = lhs.Flags() & kAccStatic;
        size_t max_float_or_double_register_args = GetMaxFloatOrDoubleRegisterArgs(isa_);
        size_t max_int_like_register_args = GetMaxIntLikeRegisterArgs(isa_);
        size_t float_or_double_args_lhs = 0, float_or_double_args_rhs = 0;
        size_t int_like_args_lhs = is_static ? 1 : 2;
        size_t int_like_args_rhs = is_static ? 1 : 2;
        size_t stack_offset_lhs = 0, stack_offset_rhs = 0;
        size_t i = 1, j = 1;
        while (i < shorty_lhs.length() && j < shorty_rhs.length()) {
          bool should_skip = false;
          bool is_stack_offset_matters = false;
          char ch_lhs = shorty_lhs[i];
          char ch_rhs = shorty_rhs[j];

          if (IsFloatOrDoubleArg(ch_lhs) &&
              float_or_double_args_lhs < max_float_or_double_register_args) {
            // Skip floating-point register arguments.
            ++i;
            ++float_or_double_args_lhs;
            stack_offset_lhs += StackOffset(ch_lhs);
            should_skip = true;
          } else if (IsIntegralArg(ch_lhs) &&
              int_like_args_lhs < max_int_like_register_args) {
            if (!is_static) {
              // Skip integral register arguments for instance method.
              ++i;
              ++int_like_args_lhs;
              stack_offset_lhs += StackOffset(ch_lhs);
              should_skip = true;
            }
          } else {
            is_stack_offset_matters = true;
          }

          if (IsFloatOrDoubleArg(ch_rhs) &&
              float_or_double_args_rhs < max_float_or_double_register_args) {
            // Skip floating-point register arguments.
            ++j;
            ++float_or_double_args_rhs;
            stack_offset_rhs += StackOffset(ch_rhs);
            should_skip = true;
          } else if (IsIntegralArg(ch_rhs) &&
              int_like_args_rhs < max_int_like_register_args) {
            if (!is_static) {
              // Skip integral register arguments for instance method.
              ++j;
              ++int_like_args_rhs;
              stack_offset_rhs += StackOffset(ch_rhs);
              should_skip = true;
            }
          } else {
            is_stack_offset_matters = true;
          }

          if (should_skip) {
            continue;
          }
          if (TranslateArgToJniShorty(ch_lhs) != TranslateArgToJniShorty(ch_rhs)) {
            return false;
          }
          if (is_stack_offset_matters && stack_offset_lhs != stack_offset_rhs) {
            return false;
          }
          // int_like_args needs to be compared for reference type because it will determine from
          // which register we take the value to construct jobject.
          if (IsReferenceArg(ch_lhs) && int_like_args_lhs != int_like_args_rhs) {
            return false;
          }
          // Passed character comparison.
          ++i;
          ++j;
          stack_offset_lhs += StackOffset(ch_lhs);
          stack_offset_rhs += StackOffset(ch_rhs);
          if (IsFloatOrDoubleArg(ch_lhs)) {
            ++float_or_double_args_lhs;
            ++float_or_double_args_rhs;
          } else {
            ++int_like_args_lhs;
            ++int_like_args_rhs;
          }
        }
        auto remaining_shorty =
            i < shorty_lhs.length() ? shorty_lhs.substr(i) : shorty_rhs.substr(j);
        size_t float_or_double_args =
            i < shorty_lhs.length() ? float_or_double_args_lhs : float_or_double_args_rhs;
        size_t int_like_args = i < shorty_lhs.length() ? int_like_args_lhs : int_like_args_rhs;
        for (char c : remaining_shorty) {
          if (IsFloatOrDoubleArg(c) && float_or_double_args < max_float_or_double_register_args) {
            ++float_or_double_args;
            continue;
          }
          if (!is_static && IsIntegralArg(c) && int_like_args < max_int_like_register_args) {
            ++int_like_args;
            continue;
          }
          return false;
        }
      } else {
        if (shorty_lhs.length() != shorty_rhs.length()) {
          return false;
        }
        for (size_t i = 1; i < shorty_lhs.length(); ++i) {
          if (TranslateArgToJniShorty(shorty_lhs[i]) != TranslateArgToJniShorty(shorty_rhs[i])) {
            return false;
          }
        }
      }
      return true;
    }

  private:
    InstructionSet isa_;
};

using JniHashSet =
    HashSet<JniHashedKey, JniShortyEmpty, JniShortyHash, JniShortyEquals>;

} // namespace art

#endif // ART_RUNTIME_JNI_HASH_SET_H_
