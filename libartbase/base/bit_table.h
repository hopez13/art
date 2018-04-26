/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef ART_LIBARTBASE_BASE_BIT_TABLE_H_
#define ART_LIBARTBASE_BASE_BIT_TABLE_H_

#include <vector>

#include "base/memory_region.h"

namespace art {

// Load variable-length bit-packed integer from `data` starting at `bit_offset`.
// The first four bits determine the variable length of the encoded integer:
//   Values 0..11 represent the result as-is, with no further following bits.
//   Values 12..15 mean the result is in the next 8/16/24/32-bits respectively.
ALWAYS_INLINE static uint32_t DecodeVarintBits(MemoryRegion region, size_t* bit_offset) {
  uint32_t x = region.LoadBits(*bit_offset, 4);
  *bit_offset += 4;
  if (x > 11) {
    size_t num_bits = (x - 11) * 8;
    x = region.LoadBits(*bit_offset, num_bits);
    *bit_offset += num_bits;
  }
  return x;
}

template<typename Vector>
ALWAYS_INLINE void EncodeVarintBits(Vector* out, size_t* bit_offset, uint32_t value) {
  constexpr uint32_t kHeaderBits = 4;
  if (value < 12) {
    out->resize(BitsToBytesRoundUp(*bit_offset + kHeaderBits));
    MemoryRegion region(out->data(), out->size());
    region.StoreBits(*bit_offset, value, kHeaderBits);
    *bit_offset += kHeaderBits;
  } else {
    uint32_t num_bits = RoundUp(MinimumBitsToStore(value), 8);
    out->resize(BitsToBytesRoundUp(*bit_offset + kHeaderBits + num_bits));
    MemoryRegion region(out->data(), out->size());
    region.StoreBits(*bit_offset, 11 + num_bits / 8, kHeaderBits);
    *bit_offset += kHeaderBits;
    region.StoreBits(*bit_offset, value, num_bits);
    *bit_offset += num_bits;
  }
}

template<uint32_t NumColumns>
class BitTable {
 public:
  class Iterator {
   public:
    static constexpr uint32_t kNoValue = -1;

    Iterator(BitTable* table, uint32_t row) : table_(table), row_(row) {}

    ALWAYS_INLINE uint32_t Row() const { return row_; }

    ALWAYS_INLINE bool IsValid() const { return row_ < table_->NumRows(); }

    template<uint32_t Column>
    ALWAYS_INLINE uint32_t Get() const {
      static_assert(Column < NumColumns, "Column out of bounds");
      return table_->Get(row_, Column);
    }

    ALWAYS_INLINE bool Equals(const Iterator& other) {
      return this->table_ == other.table_ && this->row_ == other.row_;
    }

   protected:
    BitTable* table_;
    uint32_t row_;
  };

  static constexpr uint32_t kValueBias = -1;

  BitTable() {}
  BitTable(void* data, size_t size, size_t* bit_offset = 0) {
    Decode(MemoryRegion(data, size), bit_offset);
  }

  ALWAYS_INLINE void Decode(MemoryRegion region, size_t* bit_offset) {
    region_ = region;

    // Decode row count and column sizes from the table header.
    num_rows_ = DecodeVarintBits(region, bit_offset);
    if (num_rows_ != 0) {
      column_offset_[0] = 0;
      for (uint32_t i = 0; i < NumColumns; i++) {
        column_offset_[i + 1] = column_offset_[i] + DecodeVarintBits(region, bit_offset);
      }
    }

    // Record the offset the start of the table data, and skip past it.
    data_bit_offset_ = *bit_offset;
    *bit_offset += num_rows_ * column_offset_[NumColumns];
  }

  ALWAYS_INLINE uint32_t Get(uint32_t row, uint32_t column = 0) const {
    DCHECK_LT(row, num_rows_);
    DCHECK_LT(column, NumColumns);
    size_t offset = data_bit_offset_ + row * column_offset_[NumColumns] + column_offset_[column];
    return region_.LoadBits(offset, NumColumnBits(column)) + kValueBias;
  }

  template<typename I>
  ALWAYS_INLINE const I ConstIterator(uint32_t row) const {
    return I(const_cast<BitTable<NumColumns>*>(this), row);
  }

  size_t NumRows() const { return num_rows_; }

  uint32_t NumColumnBits(uint32_t column) const {
    return column_offset_[column + 1] - column_offset_[column];
  }

  size_t DataBitSize() const { return num_rows_ * column_offset_[NumColumns]; }

 protected:
  MemoryRegion region_;
  size_t data_bit_offset_ = 0;
  size_t num_rows_ = 0;

  uint16_t column_offset_[NumColumns + 1] = { 0 };
};

template<uint32_t NumColumns>
constexpr uint32_t BitTable<NumColumns>::Iterator::kNoValue;

template<uint32_t NumColumns>
constexpr uint32_t BitTable<NumColumns>::kValueBias;

template<uint32_t NumColumns, typename Alloc = std::allocator<uint32_t>>
class BitTableBuilder {
 public:
  BitTableBuilder(Alloc alloc = Alloc()) : buffer_(alloc) {}

  template<typename ... T>
  uint32_t AddRow(T ... values) {
    constexpr size_t count = sizeof...(values);
    static_assert(count == NumColumns, "Incorrect argument count");
    uint32_t data[count] = { values... };
    buffer_.insert(buffer_.end(), data, data + count);
    return num_rows_++;
  }

  ALWAYS_INLINE uint32_t Get(uint32_t row, uint32_t column) const {
    return buffer_[row * NumColumns + column];
  }

  template<typename Vector>
  void Encode(Vector* out, size_t* bit_offset) {
    constexpr uint32_t bias = BitTable<NumColumns>::kValueBias;
    size_t initial_bit_offset = *bit_offset;
    // Measure data size.
    uint32_t max_column_value[NumColumns] = {0};
    for (uint32_t r = 0; r < num_rows_; r++) {
      for (uint32_t c = 0; c < NumColumns; c++) {
        max_column_value[c] |= Get(r, c) - bias;
      }
    }
    // Write table header.
    uint32_t table_data_bits = 0;
    uint32_t column_bits[NumColumns] = {0};
    EncodeVarintBits(out, bit_offset, num_rows_);
    if (num_rows_ != 0) {
      for (uint32_t c = 0; c < NumColumns; c++) {
        column_bits[c] = MinimumBitsToStore(max_column_value[c]);
        EncodeVarintBits(out, bit_offset, column_bits[c]);
        table_data_bits += num_rows_ * column_bits[c];
      }
    }
    // Write table data.
    out->resize(BitsToBytesRoundUp(*bit_offset + table_data_bits));
    MemoryRegion region(out->data(), out->size());
    for (uint32_t r = 0; r < num_rows_; r++) {
      for (uint32_t c = 0; c < NumColumns; c++) {
        region.StoreBits(*bit_offset, Get(r, c) - bias, column_bits[c]);
        *bit_offset += column_bits[c];
      }
    }
    // Verify the written data.
    if (kIsDebugBuild) {
      BitTable<NumColumns> table;
      table.Decode(region, &initial_bit_offset);
      DCHECK_EQ(this->num_rows_, table.NumRows());
      for (uint32_t c = 0; c < NumColumns; c++) {
        DCHECK_EQ(column_bits[c], table.NumColumnBits(c));
      }
      for (uint32_t r = 0; r < num_rows_; r++) {
        for (uint32_t c = 0; c < NumColumns; c++) {
          DCHECK_EQ(this->Get(r, c), table.Get(r, c)) << " (" << r << ", " << c << ")";
        }
      }
    }
  }

 protected:
  std::vector<uint32_t, Alloc> buffer_;
  uint32_t num_rows_ = 0;
};

}  // namespace art

#endif  // ART_LIBARTBASE_BASE_BIT_TABLE_H_
