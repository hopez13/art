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

#include "base/bit_memory_region.h"
#include "base/memory_region.h"
#include "base/scoped_arena_containers.h"
#include "base/stl_util.h"

namespace art {

constexpr uint32_t kVarintHeaderBits = 4;
constexpr uint32_t kVarintSmallValue = 11;  // Maximum value which is stored as-is.

// Load variable-length bit-packed integer from `data` starting at `bit_offset`.
// The first four bits determine the variable length of the encoded integer:
//   Values 0..11 represent the result as-is, with no further following bits.
//   Values 12..15 mean the result is in the next 8/16/24/32-bits respectively.
ALWAYS_INLINE static inline uint32_t DecodeVarintBits(BitMemoryRegion region, size_t* bit_offset) {
  uint32_t x = region.LoadBitsAndAdvance(bit_offset, kVarintHeaderBits);
  if (x > kVarintSmallValue) {
    x = region.LoadBitsAndAdvance(bit_offset, (x - kVarintSmallValue) * kBitsPerByte);
  }
  return x;
}

// Store variable-length bit-packed integer from `data` starting at `bit_offset`.
template<typename Vector>
ALWAYS_INLINE static inline void EncodeVarintBits(Vector* out, size_t* bit_offset, uint32_t value) {
  if (value <= kVarintSmallValue) {
    out->resize(BitsToBytesRoundUp(*bit_offset + kVarintHeaderBits));
    BitMemoryRegion region(MemoryRegion(out->data(), out->size()));
    region.StoreBitsAndAdvance(bit_offset, value, kVarintHeaderBits);
  } else {
    uint32_t num_bits = RoundUp(MinimumBitsToStore(value), kBitsPerByte);
    out->resize(BitsToBytesRoundUp(*bit_offset + kVarintHeaderBits + num_bits));
    BitMemoryRegion region(MemoryRegion(out->data(), out->size()));
    uint32_t header = kVarintSmallValue + num_bits / kBitsPerByte;
    region.StoreBitsAndAdvance(bit_offset, header, kVarintHeaderBits);
    region.StoreBitsAndAdvance(bit_offset, value, num_bits);
  }
}

template<uint32_t kNumColumns>
class BitTable {
 public:
  class Accessor {
   public:
    static constexpr uint32_t kNoValue = std::numeric_limits<uint32_t>::max();

    Accessor(const BitTable* table, uint32_t row) : table_(table), row_(row) {}

    ALWAYS_INLINE uint32_t Row() const { return row_; }

    ALWAYS_INLINE bool IsValid() const { return table_ != nullptr && row_ < table_->NumRows(); }

    template<uint32_t Column>
    ALWAYS_INLINE uint32_t Get() const {
      static_assert(Column < kNumColumns, "Column out of bounds");
      return table_->Get(row_, Column);
    }

    ALWAYS_INLINE bool Equals(const Accessor& other) {
      return this->table_ == other.table_ && this->row_ == other.row_;
    }

    Accessor& operator++() {
      row_++;
      return *this;
    }

   protected:
    const BitTable* table_;
    uint32_t row_;
  };

  static constexpr uint32_t kValueBias = -1;

  BitTable() {}
  BitTable(void* data, size_t size, size_t* bit_offset = 0) {
    Decode(BitMemoryRegion(MemoryRegion(data, size)), bit_offset);
  }

  ALWAYS_INLINE void Decode(BitMemoryRegion region, size_t* bit_offset) {
    // Decode row count and column sizes from the table header.
    num_rows_ = DecodeVarintBits(region, bit_offset);
    if (num_rows_ != 0) {
      column_offset_[0] = 0;
      for (uint32_t i = 0; i < kNumColumns; i++) {
        size_t column_end = column_offset_[i] + DecodeVarintBits(region, bit_offset);
        column_offset_[i + 1] = dchecked_integral_cast<uint16_t>(column_end);
      }
    }

    // Record the region which contains the table data and skip past it.
    table_data_ = region.Subregion(*bit_offset, num_rows_ * NumRowBits());
    *bit_offset += table_data_.size_in_bits();
  }

  ALWAYS_INLINE uint32_t Get(uint32_t row, uint32_t column = 0) const {
    DCHECK_LT(row, num_rows_);
    DCHECK_LT(column, kNumColumns);
    size_t offset = row * NumRowBits() + column_offset_[column];
    return table_data_.LoadBits(offset, NumColumnBits(column)) + kValueBias;
  }

  ALWAYS_INLINE BitMemoryRegion GetBitMemoryRegion(uint32_t row, uint32_t column = 0) const {
    DCHECK_LT(row, num_rows_);
    DCHECK_LT(column, kNumColumns);
    size_t offset = row * NumRowBits() + column_offset_[column];
    return table_data_.Subregion(offset, NumColumnBits(column));
  }

  size_t NumRows() const { return num_rows_; }

  uint32_t NumRowBits() const { return column_offset_[kNumColumns]; }

  constexpr size_t NumColumns() const { return kNumColumns; }

  uint32_t NumColumnBits(uint32_t column) const {
    return column_offset_[column + 1] - column_offset_[column];
  }

  size_t DataBitSize() const { return num_rows_ * column_offset_[kNumColumns]; }

 protected:
  BitMemoryRegion table_data_;
  size_t num_rows_ = 0;

  uint16_t column_offset_[kNumColumns + 1] = {};
};

template<uint32_t kNumColumns>
constexpr uint32_t BitTable<kNumColumns>::Accessor::kNoValue;

template<uint32_t kNumColumns>
constexpr uint32_t BitTable<kNumColumns>::kValueBias;

// Helper class for encoding BitTable. It can optionally de-duplicate the inputs.
// Type 'T' must be POD type consisting of uint32_t fields (one for each column).
template<typename T>
class BitTableBuilder : public ScopedArenaVector<T*> {
 public:
  explicit BitTableBuilder(ScopedArenaAllocator* const allocator)
    : ScopedArenaVector<T*>(allocator->Adapter(kArenaAllocBitTableBuilder)),
      allocator_(allocator),
      dedup_(0, allocator_->Adapter(kArenaAllocBitTableBuilder)) {
  }

  // Append given value to the vector without de-duplication, and return its index.
  // This will not add the element to the dedup map to avoid its associated costs.
  uint32_t Add(T value) {
    uint32_t index = this->size();
    this->push_back(allocator_->Alloc<T>(kArenaAllocBitTableBuilder));
    *this->back() = value;
    return index;
  }

  // Append given list of values to the vector and return the index of the first value.
  // If the exactly same set of values was already added, return the old index.
  uint32_t Dedup(T* values, size_t count = 1) {
    MemoryRegion storage(values, sizeof(T) * count);
    auto it = dedup_.find(storage);
    if (it == dedup_.end()) {
      T* copy = allocator_->AllocArray<T>(count, kArenaAllocBitTableBuilder);
      std::copy(values, values + count, copy);
      it = dedup_.emplace(MemoryRegion(copy, storage.size()), this->size()).first;
      for (size_t i = 0; i < count; i++) {
        this->push_back(&copy[i]);
      }
    }
    return it->second;
  }

  ALWAYS_INLINE uint32_t Get(uint32_t row, uint32_t column) const {
    const uint32_t* data = reinterpret_cast<const uint32_t*>((*this)[row]);
    return data[column];
  }

  // Encode the stored data into a BitTable.
  // Type 'T' must be POD type consisting of uint32_t fields.
  template<typename Vector>
  void Encode(Vector* out, size_t* bit_offset) const {
    constexpr size_t kNumColumns = sizeof(T)/sizeof(uint32_t);
    constexpr uint32_t bias = BitTable<kNumColumns>::kValueBias;
    size_t initial_bit_offset = *bit_offset;

    // Measure data size.
    uint32_t max_column_value[kNumColumns] = {};
    for (uint32_t r = 0; r < this->size(); r++) {
      for (uint32_t c = 0; c < kNumColumns; c++) {
        max_column_value[c] |= Get(r, c) - bias;
      }
    }

    // Write table header.
    uint32_t table_data_bits = 0;
    uint32_t column_bits[kNumColumns] = {};
    EncodeVarintBits(out, bit_offset, this->size());
    if (this->size() != 0) {
      for (uint32_t c = 0; c < kNumColumns; c++) {
        column_bits[c] = MinimumBitsToStore(max_column_value[c]);
        EncodeVarintBits(out, bit_offset, column_bits[c]);
        table_data_bits += this->size() * column_bits[c];
      }
    }

    // Write table data.
    out->resize(BitsToBytesRoundUp(*bit_offset + table_data_bits));
    BitMemoryRegion region(MemoryRegion(out->data(), out->size()));
    for (uint32_t r = 0; r < this->size(); r++) {
      for (uint32_t c = 0; c < kNumColumns; c++) {
        region.StoreBitsAndAdvance(bit_offset, Get(r, c) - bias, column_bits[c]);
      }
    }

    // Verify the written data.
    if (kIsDebugBuild) {
      BitTable<kNumColumns> table;
      table.Decode(region, &initial_bit_offset);
      DCHECK_EQ(this->size(), table.NumRows());
      for (uint32_t c = 0; c < kNumColumns; c++) {
        DCHECK_EQ(column_bits[c], table.NumColumnBits(c));
      }
      for (uint32_t r = 0; r < this->size(); r++) {
        for (uint32_t c = 0; c < kNumColumns; c++) {
          DCHECK_EQ(Get(r, c), table.Get(r, c)) << " (" << r << ", " << c << ")";
        }
      }
    }
  }

 protected:
  ScopedArenaAllocator* const allocator_;
  ScopedArenaUnorderedMap<MemoryRegion,
                          size_t,
                          FNVHash<MemoryRegion>,
                          MemoryRegion::ContentEquals> dedup_;
};

// Helper class for encoding single-column BitTable of bitmaps (allows more than 32 bits).
class BitmapTableBuilder : public ScopedArenaVector<MemoryRegion> {
 public:
  explicit BitmapTableBuilder(ScopedArenaAllocator* const allocator)
    : ScopedArenaVector<MemoryRegion>(allocator->Adapter(kArenaAllocBitTableBuilder)),
      allocator_(allocator),
      dedup_(0, allocator_->Adapter(kArenaAllocBitTableBuilder)) {
  }

  uint32_t Dedup(void* bitmap, size_t num_bits) {
    MemoryRegion storage(bitmap, BitsToBytesRoundUp(num_bits));
    auto it = dedup_.find(storage);
    if (it == dedup_.end()) {
      void* copy = allocator_->Alloc(storage.size(), kArenaAllocBitTableBuilder);
      memcpy(copy, storage.pointer(), storage.size());
      it = dedup_.emplace(MemoryRegion(copy, storage.size()), dedup_.size()).first;
      this->push_back(it->first);
      max_num_bits_ = std::max(max_num_bits_, num_bits);
    }
    return it->second;
  }

  template<typename Vector>
  void Encode(Vector* out, size_t* bit_offset) const {
    size_t initial_bit_offset = *bit_offset;

    // Write table header.
    EncodeVarintBits(out, bit_offset, dedup_.size());
    if (dedup_.size() != 0) {
      EncodeVarintBits(out, bit_offset, max_num_bits_);
    }

    // Write table data.
    out->resize(BitsToBytesRoundUp(*bit_offset + max_num_bits_ * dedup_.size()));
    BitMemoryRegion region(MemoryRegion(out->data(), out->size()));
    BitMemoryRegion data_region = region.Subregion(*bit_offset, max_num_bits_ * dedup_.size());
    for (auto row : dedup_) {
      BitMemoryRegion src(row.first);
      BitMemoryRegion dst = data_region.Subregion(row.second * max_num_bits_, max_num_bits_);
      size_t copy_bits = std::min(max_num_bits_, src.size_in_bits());
      for (size_t bit = 0; bit < copy_bits; bit += BitSizeOf<uint32_t>()) {
        size_t num_bits = std::min<size_t>(copy_bits - bit, BitSizeOf<uint32_t>());
        dst.StoreBits(bit, src.LoadBits(bit, num_bits), num_bits);
      }
    }
    *bit_offset += data_region.size_in_bits();

    // Verify the written data.
    if (kIsDebugBuild) {
      BitTable<1> table;
      table.Decode(region, &initial_bit_offset);
      DCHECK_EQ(this->size(), table.NumRows());
      DCHECK_EQ(max_num_bits_, table.NumColumnBits(0));
      for (uint32_t r = 0; r < this->size(); r++) {
        BitMemoryRegion expected((*this)[r]);
        BitMemoryRegion seen = table.GetBitMemoryRegion(r);
        size_t num_bits = std::max(expected.size_in_bits(), seen.size_in_bits());
        for (size_t b = 0; b < num_bits; b++) {
          bool e = b < expected.size_in_bits() && expected.LoadBit(b);
          bool s = b < seen.size_in_bits() && seen.LoadBit(b);
          DCHECK_EQ(e, s) << " (" << r << ")[" << b << "]";
        }
      }
    }
  }

 private:
  ScopedArenaAllocator* const allocator_;
  ScopedArenaUnorderedMap<MemoryRegion,
                          size_t,
                          FNVHash<MemoryRegion>,
                          MemoryRegion::ContentEquals> dedup_;
  size_t max_num_bits_ = 0;
};

}  // namespace art

#endif  // ART_LIBARTBASE_BASE_BIT_TABLE_H_
