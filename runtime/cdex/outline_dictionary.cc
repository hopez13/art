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

#include "cdex/outline_dictionary.h"

#include <iostream>

#include "dex_instruction.h"

namespace art {

struct InstructionBlob {
  const uint16_t* start;
  const uint16_t* end;

  size_t Size() const {
    return end - start;
  }
};

static inline bool operator<(const InstructionBlob& a, const InstructionBlob& b) {
  return memcmp(a.start, b.start, std::min(a.Size(), b.Size()) * 2) < 0;
}

static inline size_t SharedLength(const InstructionBlob& a, const InstructionBlob& b) {
  size_t len = 0;
  while (len < a.Size() && len < b.Size()) {
    size_t len1 = Instruction::At(a.start + len)->SizeInCodeUnits();
    size_t len2 = Instruction::At(b.start + len)->SizeInCodeUnits();
    if (len1 != len2) break;
    for (size_t i = 0; i < len1; ++i) {
      if (a.start[len + i] != b.start[len + i]) return len;
    }
    len += len1;
  }
  return len;
}

class Builder {
 public:
  void AddInstructions(const uint16_t* start, const uint16_t* end);
  void Generate();

 private:
  // Build entries need to contain pointers to the actual code.
  class Entry {
   public:
    const uint16_t* data_;
    uint32_t len_;
  };

  std::vector<InstructionBlob> blobs;
};

void Builder::AddInstructions(const uint16_t* start, const uint16_t* end) {
  for (const uint16_t* ptr = start; ptr != end; ptr += Instruction::At(ptr)->SizeInCodeUnits()) {
    // TODO: Check for cross block jumps.
    blobs.push_back(InstructionBlob {ptr, end});
  }
}

static inline uint32_t RectArea(const std::vector<uint32_t>& lens, uint32_t start, uint32_t end) {
  DCHECK_LE(start, end);
  DCHECK_GT(end, 0u);
  const uint32_t height = std::min(lens[start], lens[end - 1]);
  for (size_t i = start; i < end; ++i) {
    DCHECK_GE(lens[i], height);
  }
  return height * (end - start);
}

static inline std::pair<uint32_t, uint32_t> MaxArea(const std::vector<uint32_t>& lens) {
  if (lens.empty()) {
    return std::pair<uint32_t, uint32_t>();
  }
  std::vector<uint32_t> open(1, lens.front());
  std::pair<uint32_t, uint32_t> best;
  uint32_t best_area = 0;
  for (size_t i = 0; i < lens.size(); ++i) {
    if (lens.empty() || lens[i] > open.back()) {
      open.push_back(i);
      continue;
    }
    while (!open.empty() && lens[i] < open.back()) {
      uint32_t start_idx = open.back();
      open.pop_back();
      const uint32_t area = RectArea(lens, start_idx, i);
      if (area > best_area) {
        best_area = area;
        best.first = start_idx;
        best.second = i;
      }
    }
    open.push_back(i);
  }
  return best;
}

void Builder::Generate() {
  // Group common blobs together by sorting the suffixes.
  std::sort(blobs.begin(), blobs.end());

  // Calculate matching length with previous items.
  std::vector<uint32_t> matching_lengths(blobs.size(), 0);
  for (size_t i = 1; i < blobs.size(); ++i) {
    matching_lengths[i] = SharedLength(blobs[i], blobs[i - 1]);
    if (matching_lengths[i] > 0) {
      // Remove one instruction for outline jump overhead.
      --matching_lengths[i];
    }
  }

  // At this point we have a set of values of the following form:
  // -
  // --
  // ----
  // ----
  // ------
  // ---
  // The idea is to pick the set of disjoint rectangles with the largest area. These are the
  // outlines that will provide the most savings. Be careful about overlapping sets though!
  //
  // For a single rectangle it is possible to use the "largest rectangle area in a histogram"
  // solution with a stack.
  //
  size_t savings_1b = 0;
  for (size_t i = 0; i < 256; ++i) {
    auto pair = MaxArea(matching_lengths);
    const size_t area = RectArea(matching_lengths, pair.first, pair.second);
    savings_1b += area * 2;
    std::cout << "SQUARE1B " << i << " = " << area << std::endl;
    matching_lengths.erase(matching_lengths.begin() + pair.first,
                           matching_lengths.begin() + pair.second);
  }
  // 2b opcodes have
  for (auto& len : matching_lengths) {
    if (len > 0) {
      --len;
    }
  }
  size_t savings_2b = 0;
  for (size_t i = 0; ; ++i) {
    auto pair = MaxArea(matching_lengths);
    const size_t area = RectArea(matching_lengths, pair.first, pair.second);
    savings_2b += area * 2;
    if (area == 0) {
      break;
    }
    std::cout << "SQUARE2B " << i << " = " << area << std::endl;
    matching_lengths.erase(matching_lengths.begin() + pair.first,
                           matching_lengths.begin() + pair.second);
  }
  std::cout << "1b=" << savings_1b << " 2b=" << savings_2b << std::endl;
}

void OutlineDictionary::Build(
    const std::map<const DexFile::CodeItem*, MethodReference>& code_items) {
  Builder builder;
  for (auto&& pair : code_items) {
    const DexFile::CodeItem* code_item = pair.first;
    builder.AddInstructions(code_item->insns_,
                            code_item->insns_ + code_item->insns_size_in_code_units_);
  }
  builder.Generate();
}

}  // namespace art
