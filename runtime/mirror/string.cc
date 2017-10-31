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

#include "string-alloc-inl.h"

#include "arch/memcmp16.h"
#include "array-alloc-inl.h"
#include "base/array_ref.h"
#include "base/casts.h"
#include "base/stl_util.h"
#include "class-inl.h"
#include "dex/descriptors_names.h"
#include "dex/utf-inl.h"
#include "gc/accounting/card_table-inl.h"
#include "handle_scope-inl.h"
#include "intern_table.h"
#include "object-inl.h"
#include "runtime.h"
#include "string-inl.h"
#include "thread.h"

namespace art {
namespace mirror {

int32_t String::FastIndexOf(int32_t ch, int32_t start) {
  int32_t count = GetLength();
  if (start < 0) {
    start = 0;
  } else if (start > count) {
    start = count;
  }
  if (IsCompressed()) {
    return FastIndexOf<uint8_t>(GetValueCompressed(), ch, start);
  } else {
    return FastIndexOf<uint16_t>(GetValue(), ch, start);
  }
}

int String::ComputeHashCode() {
  int32_t hash_code = 0;
  if (IsCompressed()) {
    hash_code = ComputeUtf16Hash(GetValueCompressed(), GetLength());
  } else {
    hash_code = ComputeUtf16Hash(GetValue(), GetLength());
  }
  SetHashCode(hash_code);
  return hash_code;
}

int32_t String::GetUtfLength() {
  if (IsCompressed()) {
    return GetLength();
  } else {
    return CountUtf8Bytes(GetValue(), GetLength());
  }
}

inline bool String::AllASCIIExcept(const uint16_t* chars, int32_t length, uint16_t non_ascii) {
  DCHECK(!IsASCII(non_ascii));
  for (int32_t i = 0; i < length; ++i) {
    if (!IsASCII(chars[i]) && chars[i] != non_ascii) {
      return false;
    }
  }
  return true;
}

ObjPtr<String> String::DoReplace(Thread* self, Handle<String> src, uint16_t old_c, uint16_t new_c) {
  int32_t length = src->GetLength();
  DCHECK(src->IsCompressed()
             ? ContainsElement(ArrayRef<uint8_t>(src->value_compressed_, length), old_c)
             : ContainsElement(ArrayRef<uint16_t>(src->value_, length), old_c));
  bool compressible =
      kUseStringCompression &&
      IsASCII(new_c) &&
      (src->IsCompressed() || (!IsASCII(old_c) && AllASCIIExcept(src->value_, length, old_c)));
  gc::AllocatorType allocator_type = Runtime::Current()->GetHeap()->GetCurrentAllocator();
  const int32_t length_with_flag = String::GetFlaggedCount(length, compressible);
  SetStringCountVisitor visitor(length_with_flag);
  ObjPtr<String> string = Alloc<true>(self, length_with_flag, allocator_type, visitor);
  if (UNLIKELY(string == nullptr)) {
    return nullptr;
  }
  if (compressible) {
    auto replace = [old_c, new_c](uint16_t c) {
      return dchecked_integral_cast<uint8_t>((old_c != c) ? c : new_c);
    };
    uint8_t* out = string->value_compressed_;
    if (LIKELY(src->IsCompressed())) {  // LIKELY(compressible == src->IsCompressed())
      std::transform(src->value_compressed_, src->value_compressed_ + length, out, replace);
    } else {
      std::transform(src->value_, src->value_ + length, out, replace);
    }
    DCHECK(kUseStringCompression && AllASCII(out, length));
  } else {
    auto replace = [old_c, new_c](uint16_t c) {
      return (old_c != c) ? c : new_c;
    };
    uint16_t* out = string->value_;
    if (UNLIKELY(src->IsCompressed())) {  // LIKELY(compressible == src->IsCompressed())
      std::transform(src->value_compressed_, src->value_compressed_ + length, out, replace);
    } else {
      std::transform(src->value_, src->value_ + length, out, replace);
    }
    DCHECK(!kUseStringCompression || !AllASCII(out, length));
  }
  return string;
}

String* String::AllocFromStrings(Thread* self, Handle<String> string, Handle<String> string2) {
  int32_t length = string->GetLength();
  int32_t length2 = string2->GetLength();
  gc::AllocatorType allocator_type = Runtime::Current()->GetHeap()->GetCurrentAllocator();
  const bool compressible = kUseStringCompression &&
      (string->IsCompressed() && string2->IsCompressed());
  const int32_t length_with_flag = String::GetFlaggedCount(length + length2, compressible);

  SetStringCountVisitor visitor(length_with_flag);
  ObjPtr<String> new_string = Alloc<true>(self, length_with_flag, allocator_type, visitor);
  if (UNLIKELY(new_string == nullptr)) {
    return nullptr;
  }
  if (compressible) {
    uint8_t* new_value = new_string->GetValueCompressed();
    memcpy(new_value, string->GetValueCompressed(), length * sizeof(uint8_t));
    memcpy(new_value + length, string2->GetValueCompressed(), length2 * sizeof(uint8_t));
  } else {
    uint16_t* new_value = new_string->GetValue();
    if (string->IsCompressed()) {
      for (int i = 0; i < length; ++i) {
        new_value[i] = string->CharAt(i);
      }
    } else {
      memcpy(new_value, string->GetValue(), length * sizeof(uint16_t));
    }
    if (string2->IsCompressed()) {
      for (int i = 0; i < length2; ++i) {
        new_value[i+length] = string2->CharAt(i);
      }
    } else {
      memcpy(new_value + length, string2->GetValue(), length2 * sizeof(uint16_t));
    }
  }
  return new_string.Ptr();
}

String* String::AllocFromUtf16(Thread* self, int32_t utf16_length, const uint16_t* utf16_data_in) {
  CHECK(utf16_data_in != nullptr || utf16_length == 0);
  gc::AllocatorType allocator_type = Runtime::Current()->GetHeap()->GetCurrentAllocator();
  const bool compressible = kUseStringCompression &&
                            String::AllASCII<uint16_t>(utf16_data_in, utf16_length);
  int32_t length_with_flag = String::GetFlaggedCount(utf16_length, compressible);
  SetStringCountVisitor visitor(length_with_flag);
  ObjPtr<String> string = Alloc<true>(self, length_with_flag, allocator_type, visitor);
  if (UNLIKELY(string == nullptr)) {
    return nullptr;
  }
  if (compressible) {
    for (int i = 0; i < utf16_length; ++i) {
      string->GetValueCompressed()[i] = static_cast<uint8_t>(utf16_data_in[i]);
    }
  } else {
    uint16_t* array = string->GetValue();
    memcpy(array, utf16_data_in, utf16_length * sizeof(uint16_t));
  }
  return string.Ptr();
}

String* String::AllocFromModifiedUtf8(Thread* self, const char* utf) {
  DCHECK(utf != nullptr);
  size_t byte_count = strlen(utf);
  size_t char_count = CountModifiedUtf8Chars(utf, byte_count);
  return AllocFromModifiedUtf8(self, char_count, utf, byte_count);
}

String* String::AllocFromModifiedUtf8(Thread* self,
                                      int32_t utf16_length,
                                      const char* utf8_data_in) {
  return AllocFromModifiedUtf8(self, utf16_length, utf8_data_in, strlen(utf8_data_in));
}

String* String::AllocFromModifiedUtf8(Thread* self,
                                      int32_t utf16_length,
                                      const char* utf8_data_in,
                                      int32_t utf8_length) {
  gc::AllocatorType allocator_type = Runtime::Current()->GetHeap()->GetCurrentAllocator();
  const bool compressible = kUseStringCompression && (utf16_length == utf8_length);
  const int32_t utf16_length_with_flag = String::GetFlaggedCount(utf16_length, compressible);
  SetStringCountVisitor visitor(utf16_length_with_flag);
  ObjPtr<String> string = Alloc<true>(self, utf16_length_with_flag, allocator_type, visitor);
  if (UNLIKELY(string == nullptr)) {
    return nullptr;
  }
  if (compressible) {
    memcpy(string->GetValueCompressed(), utf8_data_in, utf16_length * sizeof(uint8_t));
  } else {
    uint16_t* utf16_data_out = string->GetValue();
    ConvertModifiedUtf8ToUtf16(utf16_data_out, utf16_length, utf8_data_in, utf8_length);
  }
  return string.Ptr();
}

bool String::Equals(ObjPtr<String> that) {
  if (this == that) {
    // Quick reference equality test
    return true;
  } else if (that == nullptr) {
    // Null isn't an instanceof anything
    return false;
  } else if (this->GetLength() != that->GetLength()) {
    // Quick length inequality test
    return false;
  } else {
    // Note: don't short circuit on hash code as we're presumably here as the
    // hash code was already equal
    for (int32_t i = 0; i < that->GetLength(); ++i) {
      if (this->CharAt(i) != that->CharAt(i)) {
        return false;
      }
    }
    return true;
  }
}

bool String::Equals(const char* modified_utf8) {
  const int32_t length = GetLength();
  int32_t i = 0;
  while (i < length) {
    const uint32_t ch = GetUtf16FromUtf8(&modified_utf8);
    if (ch == '\0') {
      return false;
    }

    if (GetLeadingUtf16Char(ch) != CharAt(i++)) {
      return false;
    }

    const uint16_t trailing = GetTrailingUtf16Char(ch);
    if (trailing != 0) {
      if (i == length) {
        return false;
      }

      if (CharAt(i++) != trailing) {
        return false;
      }
    }
  }
  return *modified_utf8 == '\0';
}

// Create a modified UTF-8 encoded std::string from a java/lang/String object.
std::string String::ToModifiedUtf8() {
  size_t byte_count = GetUtfLength();
  std::string result(byte_count, static_cast<char>(0));
  if (IsCompressed()) {
    for (size_t i = 0; i < byte_count; ++i) {
      result[i] = static_cast<char>(CharAt(i));
    }
  } else {
    const uint16_t* chars = GetValue();
    ConvertUtf16ToModifiedUtf8(&result[0], byte_count, chars, GetLength());
  }
  return result;
}

int32_t String::CompareTo(ObjPtr<String> rhs) {
  // Quick test for comparison of a string with itself.
  ObjPtr<String> lhs = this;
  if (lhs == rhs) {
    return 0;
  }
  int32_t lhs_count = lhs->GetLength();
  int32_t rhs_count = rhs->GetLength();
  int32_t count_diff = lhs_count - rhs_count;
  int32_t min_count = (count_diff < 0) ? lhs_count : rhs_count;
  if (lhs->IsCompressed() && rhs->IsCompressed()) {
    const uint8_t* lhs_chars = lhs->GetValueCompressed();
    const uint8_t* rhs_chars = rhs->GetValueCompressed();
    for (int32_t i = 0; i < min_count; ++i) {
      int32_t char_diff = static_cast<int32_t>(lhs_chars[i]) - static_cast<int32_t>(rhs_chars[i]);
      if (char_diff != 0) {
        return char_diff;
      }
    }
  } else if (lhs->IsCompressed() || rhs->IsCompressed()) {
    const uint8_t* compressed_chars =
        lhs->IsCompressed() ? lhs->GetValueCompressed() : rhs->GetValueCompressed();
    const uint16_t* uncompressed_chars = lhs->IsCompressed() ? rhs->GetValue() : lhs->GetValue();
    for (int32_t i = 0; i < min_count; ++i) {
      int32_t char_diff =
          static_cast<int32_t>(compressed_chars[i]) - static_cast<int32_t>(uncompressed_chars[i]);
      if (char_diff != 0) {
        return lhs->IsCompressed() ? char_diff : -char_diff;
      }
    }
  } else {
    const uint16_t* lhs_chars = lhs->GetValue();
    const uint16_t* rhs_chars = rhs->GetValue();
    // FIXME: The MemCmp16() name is misleading. It returns the char difference on mismatch
    // where memcmp() only guarantees that the returned value has the same sign.
    int32_t char_diff = MemCmp16(lhs_chars, rhs_chars, min_count);
    if (char_diff != 0) {
      return char_diff;
    }
  }
  return count_diff;
}

CharArray* String::ToCharArray(Thread* self) {
  StackHandleScope<1> hs(self);
  Handle<String> string(hs.NewHandle(this));
  ObjPtr<CharArray> result = CharArray::Alloc(self, GetLength());
  if (result != nullptr) {
    if (string->IsCompressed()) {
      int32_t length = string->GetLength();
      for (int i = 0; i < length; ++i) {
        result->GetData()[i] = string->CharAt(i);
      }
    } else {
      memcpy(result->GetData(), string->GetValue(), string->GetLength() * sizeof(uint16_t));
    }
  } else {
    self->AssertPendingOOMException();
  }
  return result.Ptr();
}

void String::GetChars(int32_t start, int32_t end, Handle<CharArray> array, int32_t index) {
  uint16_t* data = array->GetData() + index;
  if (IsCompressed()) {
    for (int i = start; i < end; ++i) {
      data[i-start] = CharAt(i);
    }
  } else {
    uint16_t* value = GetValue() + start;
    memcpy(data, value, (end - start) * sizeof(uint16_t));
  }
}

bool String::IsValueNull() {
  return (IsCompressed()) ? (GetValueCompressed() == nullptr) : (GetValue() == nullptr);
}

std::string String::PrettyStringDescriptor(ObjPtr<mirror::String> java_descriptor) {
  if (java_descriptor == nullptr) {
    return "null";
  }
  return java_descriptor->PrettyStringDescriptor();
}

std::string String::PrettyStringDescriptor() {
  return PrettyDescriptor(ToModifiedUtf8().c_str());
}

ObjPtr<String> String::Intern() {
  return Runtime::Current()->GetInternTable()->InternWeak(this);
}

class String::AppendBuilder {
 public:
  AppendBuilder(uint32_t format, const uint32_t* args, Thread* self)
      : format_(format),
        args_(args),
        hs_(self) {}

  int32_t CalculateLengthWithFlag() REQUIRES_SHARED(Locks::mutator_lock_);

  void operator()(ObjPtr<Object> obj, size_t usable_size) const
      REQUIRES_SHARED(Locks::mutator_lock_);

  bool HasConcurrentModification() const {
    return has_concurrent_modification_;
  }

 private:
  static size_t Uint64Length(uint64_t value);

  static size_t Int64Length(int64_t value) {
    uint64_t v = static_cast<uint64_t>(value);
    return (value >= 0) ? Uint64Length(v) : 1u + Uint64Length(-v);
  }

  static size_t RemainingSpace(ObjPtr<String> new_string, const uint8_t* data)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK(new_string->IsCompressed());
    DCHECK_GE(new_string->GetLength(), data - new_string->value_compressed_);
    return new_string->GetLength() - (data - new_string->value_compressed_);
  }

  static size_t RemainingSpace(ObjPtr<String> new_string, const uint16_t* data)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK(!new_string->IsCompressed());
    DCHECK_GE(new_string->GetLength(), data - new_string->value_);
    return new_string->GetLength() - (data - new_string->value_);
  }

  template <typename CharType, size_t size>
  static CharType* AppendLiteral(ObjPtr<String> new_string,
                                 CharType* data,
                                 const char (&literal)[size]) REQUIRES_SHARED(Locks::mutator_lock_);

  template <typename CharType>
  static CharType* AppendString(ObjPtr<String> new_string,
                                CharType* data,
                                ObjPtr<String> str) REQUIRES_SHARED(Locks::mutator_lock_);

  static uint16_t* AppendChars(ObjPtr<String> new_string,
                               uint16_t* data,
                               ObjPtr<CharArray> chars,
                               size_t length) REQUIRES_SHARED(Locks::mutator_lock_);
  static uint8_t* AppendChars(ObjPtr<String> new_string,
                              uint8_t* data,
                              ObjPtr<CharArray> chars,
                              size_t length) REQUIRES_SHARED(Locks::mutator_lock_);

  template <typename CharType>
  static CharType* AppendInt64(ObjPtr<String> new_string,
                               CharType* data,
                               int64_t value) REQUIRES_SHARED(Locks::mutator_lock_);

  template <typename CharType>
  void StoreData(ObjPtr<String> new_string, CharType* data) const
      REQUIRES_SHARED(Locks::mutator_lock_);

  static constexpr char kNull[] = "null";
  static constexpr size_t kNullLength = sizeof(kNull) - 1u;
  static constexpr char kTrue[] = "true";
  static constexpr size_t kTrueLength = sizeof(kTrue) - 1u;
  static constexpr char kFalse[] = "false";
  static constexpr size_t kFalseLength = sizeof(kFalse) - 1u;

  // The format and arguments to append.
  uint32_t format_;
  const uint32_t* args_;

  // References are moved to the handle scope during CalculateLengthWithFlag().
  StackHandleScope<kStringAppendMaxArgs> hs_;

  // For non-null StringBuilders, we store the CharArray in `hs_` and record
  // the length we see in CalculateLengthWithFlag(). This prevents buffer overflows
  // from racy code concurrently modifying the StringBuilder.
  uint32_t string_builder_lengths_[kStringAppendMaxArgs];
  size_t num_non_null_string_builders_ = 0u;

  // The length and flag to store when the AppendBuilder is used as a pre-fence visitor.
  int32_t length_with_flag_ = 0u;

  // Record whether we found concurrent modification of a char[]'s value between
  // CalculateLengthWithFlag() and copying the contents.
  bool has_concurrent_modification_ = false;
};

size_t String::AppendBuilder::Uint64Length(uint64_t value)  {
  if (value == 0u) {
    return 1u;
  }
  // Calculate floor(log2(value)).
  size_t log2_value = BitSizeOf<uint64_t>() - 1u - CLZ(value);
  // Calculate an estimate of floor(log10(value)).
  //   log10(2) = 0.301029996 > 0.296875 = 19/64
  //   floor(log10(v)) == floor(log2(v) * log10(2)) >= floor(log2(v) * 19/64)
  size_t log10_value_estimate = log2_value * 19u / 64u;
  static constexpr uint64_t bounds[] = {
      UINT64_C(9),
      UINT64_C(99),
      UINT64_C(999),
      UINT64_C(9999),
      UINT64_C(99999),
      UINT64_C(999999),
      UINT64_C(9999999),
      UINT64_C(99999999),
      UINT64_C(999999999),
      UINT64_C(9999999999),
      UINT64_C(99999999999),
      UINT64_C(999999999999),
      UINT64_C(9999999999999),
      UINT64_C(99999999999999),
      UINT64_C(999999999999999),
      UINT64_C(9999999999999999),
      UINT64_C(99999999999999999),
      UINT64_C(999999999999999999),
      UINT64_C(9999999999999999999),
      std::numeric_limits<uint64_t>::max(),  // Do we need this? Can we use 9/32 instead of 19/64?
  };
  // Add 1 for the lowest digit, add another 1 if the estimate was too low.
  size_t adjustment = (value > bounds[log10_value_estimate]) ? 2u : 1u;
  return log10_value_estimate + adjustment;
}

template <typename CharType, size_t size>
CharType* String::AppendBuilder::AppendLiteral(ObjPtr<String> new_string,
                                               CharType* data,
                                               const char (&literal)[size]) {
  static_assert(size >= 2, "We need something to append.");

  // Literals are zero-terminated.
  constexpr size_t length = size - 1u;
  DCHECK_EQ(literal[length], '\0');

  DCHECK_LE(length, RemainingSpace(new_string, data));
  for (size_t i = 0; i != length; ++i) {
    data[i] = literal[i];
  }
  return data + length;
}

template <typename CharType>
CharType* String::AppendBuilder::AppendString(ObjPtr<String> new_string,
                                              CharType* data,
                                              ObjPtr<String> str) {
  size_t length = dchecked_integral_cast<size_t>(str->GetLength());
  DCHECK_LE(length, RemainingSpace(new_string, data));
  if (str->IsCompressed()) {
    const uint8_t* value = str->value_compressed_;
    for (size_t i = 0; i != length; ++i) {
      data[i] = value[i];
    }
  } else {
    const uint16_t* value = str->value_;
    for (size_t i = 0; i != length; ++i) {
      data[i] = dchecked_integral_cast<CharType>(value[i]);
    }
  }
  return data + length;
}

uint16_t* String::AppendBuilder::AppendChars(ObjPtr<String> new_string,
                                             uint16_t* data,
                                             ObjPtr<CharArray> chars,
                                             size_t length) {
  DCHECK_LE(length, RemainingSpace(new_string, data));
  DCHECK_LE(dchecked_integral_cast<int32_t>(length), chars->GetLength());
  memcpy(data, chars->GetData(), length * sizeof(uint16_t));
  return data + length;
}

uint8_t* String::AppendBuilder::AppendChars(ObjPtr<String> new_string,
                                            uint8_t* data,
                                            ObjPtr<CharArray> chars,
                                            size_t length) {
  DCHECK_LE(length, RemainingSpace(new_string, data));
  DCHECK_LE(dchecked_integral_cast<int32_t>(length), chars->GetLength());
  for (size_t i = 0; i != length; ++i) {
    uint16_t value = chars->GetWithoutChecks(i);
    if (UNLIKELY(!IsASCII(value))) {
      // A character has changed from ASCII to non-ASCII between CalculateLengthWithFlag()
      // and copying the data. This can happen only with concurrent modification.
      return nullptr;
    }
    data[i] = value;
  }
  return data + length;
}

template <typename CharType>
CharType* String::AppendBuilder::AppendInt64(ObjPtr<String> new_string,
                                             CharType* data,
                                             int64_t value) {
  DCHECK_GE(RemainingSpace(new_string, data), Int64Length(value));
  uint64_t v = static_cast<uint64_t>(value);
  if (value < 0) {
    *data = '-';
    ++data;
    v = -v;
  }
  size_t length = Uint64Length(v);
  // Write the digits from the end, do not write the most significant digit
  // in the loop to avoid an unnecessary division.
  for (size_t i = 1; i != length; ++i) {
    uint64_t digit = v % UINT64_C(10);
    v /= UINT64_C(10);
    data[length - i] = '0' + static_cast<char>(digit);
  }
  DCHECK_LE(v, 10u);
  *data = '0' + static_cast<char>(v);
  return data + length;
}

int32_t String::AppendBuilder::CalculateLengthWithFlag() {
  static_assert(static_cast<size_t>(StringAppendArgument::kEnd) == 0u, "kEnd must be 0.");
  bool compressible = kUseStringCompression;
  uint64_t length = 0u;
  const uint32_t* current_arg = args_;
  for (uint32_t f = format_; f != 0u; f >>= kStringAppendBitsPerArg) {
    DCHECK_LE(f & kStringAppendArgMask, static_cast<uint32_t>(StringAppendArgument::kLast));
    switch (static_cast<StringAppendArgument>(f & kStringAppendArgMask)) {
      case StringAppendArgument::kStringBuilder: {
        ObjPtr<Object> sb = reinterpret_cast32<Object*>(*current_arg);
        if (sb != nullptr) {
          int32_t count = sb->GetField32(MemberOffset(/*FIXME do not hardcode*/ 12));
          if (count < 0) {
            // Message from AbstractStringBuilder.getChars() -> SIOOB.<init>(int).
            std::string message = "String index out of range: " + std::to_string(count);
            hs_.Self()->ThrowNewException("Ljava/lang/StringIndexOutOfBoundsException;",
                                          message.c_str());
            return -1;
          }
          Handle<CharArray> value = hs_.NewHandle(
              sb->GetFieldObject<CharArray>(MemberOffset(/*FIXME do not hardcode*/ 8)));
          if (value == nullptr) {
            // Message from AbstractStringBuilder.getChars() -> System.arraycopy().
            // Thrown even if `count == 0`.
            hs_.Self()->ThrowNewException("Ljava/lang/NullPointerException;", "src == null");
            return -1;
          }
          if (value->GetLength() < count) {
            hs_.Self()->ThrowNewExceptionF(
                "Ljava/lang/ArrayIndexOutOfBoundsException;",
                "Invalid AbstractStringBuilder, count = %d, value.length = %d",
                count,
                value->GetLength());
            return -1;
          }
          string_builder_lengths_[num_non_null_string_builders_] = static_cast<uint32_t>(count);
          length += count;
          compressible = compressible && AllASCII(value->GetData(), count);
          ++num_non_null_string_builders_;
        } else {
          hs_.NewHandle<CharArray>(nullptr);
          length += kNullLength;
        }
        break;
      }
      case StringAppendArgument::kString: {
        Handle<String> str = hs_.NewHandle(reinterpret_cast32<String*>(*current_arg));
        if (str != nullptr) {
          length += str->GetLength();
          compressible = compressible && str->IsCompressed();
        } else {
          length += kNullLength;
        }
        break;
      }
      case StringAppendArgument::kCharArray: {
        Handle<CharArray> array = hs_.NewHandle(reinterpret_cast32<CharArray*>(*current_arg));
        if (array != nullptr) {
          length += array->GetLength();
          compressible = compressible && AllASCII(array->GetData(), array->GetLength());
        } else {
          ThrowNullPointerException("Attempt to get length of null array");
          return -1;
        }
        break;
      }
      case StringAppendArgument::kBoolean: {
        length += (*current_arg != 0u) ? kTrueLength : kFalseLength;
        break;
      }
      case StringAppendArgument::kChar: {
        length += 1u;
        compressible = compressible && IsASCII(reinterpret_cast<const uint16_t*>(current_arg)[0]);
        break;
      }
      case StringAppendArgument::kInt: {
        length += Int64Length(static_cast<int32_t>(*current_arg));
        break;
      }
      case StringAppendArgument::kLong: {
        current_arg = AlignUp(current_arg, sizeof(int64_t));
        length += Int64Length(*reinterpret_cast<const int64_t*>(current_arg));
        ++current_arg;  // Skip the low word, let the common code skip the high word.
        break;
      }

      case StringAppendArgument::kObject:
      case StringAppendArgument::kFloat:
      case StringAppendArgument::kDouble:
        LOG(FATAL) << "Unimplemented arg format: 0x" << std::hex
            << (f & kStringAppendArgMask) << " full format: 0x" << std::hex << format_;
        UNREACHABLE();
      default:
        LOG(FATAL) << "Unexpected arg format: 0x" << std::hex
            << (f & kStringAppendArgMask) << " full format: 0x" << std::hex << format_;
        UNREACHABLE();
    }
    ++current_arg;
    DCHECK_LE(hs_.NumberOfReferences(), kStringAppendMaxArgs);
  }

  if (length > std::numeric_limits<int32_t>::max()) {
    // We cannot allocate memory for the entire result.
    hs_.Self()->ThrowNewException("Ljava/lang/OutOfMemoryError;",
                                  "Out of memory for String append.");
    return -1;
  }

  length_with_flag_ = String::GetFlaggedCount(length, compressible);
  return length_with_flag_;
}

template <typename CharType>
void String::AppendBuilder::StoreData(ObjPtr<String> new_string, CharType* data) const {
  size_t handle_index = 0u;
  size_t current_non_null_string_builder = 0u;
  const uint32_t* current_arg = args_;
  for (uint32_t f = format_; f != 0u; f >>= kStringAppendBitsPerArg) {
    DCHECK_LE(f & kStringAppendArgMask, static_cast<uint32_t>(StringAppendArgument::kLast));
    switch (static_cast<StringAppendArgument>(f & kStringAppendArgMask)) {
      case StringAppendArgument::kStringBuilder: {
        ObjPtr<CharArray> array =
            ObjPtr<CharArray>::DownCast(MakeObjPtr(hs_.GetReference(handle_index)));
        ++handle_index;
        if (array != nullptr) {
          DCHECK_LE(current_non_null_string_builder, num_non_null_string_builders_);
          size_t length = string_builder_lengths_[current_non_null_string_builder];
          ++current_non_null_string_builder;
          data = AppendChars(new_string, data, array, length);
          if (std::is_same<uint8_t, CharType>::value && data == nullptr) {
            const_cast<AppendBuilder*>(this)->has_concurrent_modification_ = true;
            return;
          }
          DCHECK(data != nullptr);
        } else {
          data = AppendLiteral(new_string, data, kNull);
        }
        break;
      }
      case StringAppendArgument::kString: {
        ObjPtr<String> str = ObjPtr<String>::DownCast(MakeObjPtr(hs_.GetReference(handle_index)));
        ++handle_index;
        if (str != nullptr) {
          data = AppendString(new_string, data, str);
        } else {
          data = AppendLiteral(new_string, data, kNull);
        }
        break;
      }
      case StringAppendArgument::kCharArray: {
        ObjPtr<CharArray> array =
            ObjPtr<CharArray>::DownCast(MakeObjPtr(hs_.GetReference(handle_index)));
        ++handle_index;
        if (array != nullptr) {
          data = AppendChars(new_string, data, array, array->GetLength());
          if (std::is_same<uint8_t, CharType>::value && data == nullptr) {
            const_cast<AppendBuilder*>(this)->has_concurrent_modification_ = true;
            return;
          }
          DCHECK(data != nullptr);
        } else {
          data = AppendLiteral(new_string, data, kNull);
        }
        break;
      }
      case StringAppendArgument::kBoolean: {
        if (*current_arg != 0u) {
          data = AppendLiteral(new_string, data, kTrue);
        } else {
          data = AppendLiteral(new_string, data, kFalse);
        }
        break;
      }
      case StringAppendArgument::kChar: {
        DCHECK_GE(RemainingSpace(new_string, data), 1u);
        *data = *reinterpret_cast<const CharType*>(current_arg);
        ++data;
        break;
      }
      case StringAppendArgument::kInt: {
        data = AppendInt64(new_string, data, static_cast<int32_t>(*current_arg));
        break;
      }
      case StringAppendArgument::kLong: {
        current_arg = AlignUp(current_arg, sizeof(int64_t));
        data = AppendInt64(new_string, data, *reinterpret_cast<const int64_t*>(current_arg));
        ++current_arg;  // Skip the low word, let the common code skip the high word.
        break;
      }

      case StringAppendArgument::kFloat:
      case StringAppendArgument::kDouble:
        LOG(FATAL) << "Unimplemented arg format: 0x" << std::hex
            << (f & kStringAppendArgMask) << " full format: 0x" << std::hex << format_;
        UNREACHABLE();
      default:
        LOG(FATAL) << "Unexpected arg format: 0x" << std::hex
            << (f & kStringAppendArgMask) << " full format: 0x" << std::hex << format_;
        UNREACHABLE();
    }
    ++current_arg;
    DCHECK_LE(handle_index, hs_.NumberOfReferences());
  }
  DCHECK_EQ(current_non_null_string_builder, num_non_null_string_builders_) << std::hex << format_;
  DCHECK_EQ(RemainingSpace(new_string, data), 0u) << std::hex << format_;
}

void String::AppendBuilder::operator()(ObjPtr<Object> obj,
                                       size_t usable_size ATTRIBUTE_UNUSED) const {
  ObjPtr<String> new_string = ObjPtr<String>::DownCast(obj);
  new_string->SetCount(length_with_flag_);
  if (IsCompressed(length_with_flag_)) {
    StoreData(new_string, new_string->value_compressed_);
  } else {
    StoreData(new_string, new_string->value_);
  }
}

String* String::AppendF(uint32_t format, const uint32_t* args, Thread* self) {
  AppendBuilder builder(format, args, self);
  self->AssertNoPendingException();
  int32_t length_with_flag = builder.CalculateLengthWithFlag();
  if (self->IsExceptionPending()) {
    return nullptr;
  }
  gc::AllocatorType allocator_type = Runtime::Current()->GetHeap()->GetCurrentAllocator();
  ObjPtr<String> result =
      Alloc</*kIsInstrumented=*/ false>(self, length_with_flag, allocator_type, builder);

  if (UNLIKELY(builder.HasConcurrentModification())) {
    if (!self->IsExceptionPending()) {
      self->ThrowNewException("Ljava/util/ConcurrentModificationException;",
                              "Concurrent modification during StringBuilder append.");
    }
    return nullptr;
  }
  return result.Ptr();
}

extern "C" void* artStringBuilderAppend(uint32_t format, const uint32_t* args, Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  return String::AppendF(format, args, self);
}

}  // namespace mirror
}  // namespace art
