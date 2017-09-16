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

#include "experiments.h"

#include <iostream>
#include <set>
#include <map>

#include "dex_instruction.h"
#include "dex_file-inl.h"
#include "experimental_flags.h"
#include "leb128.h"
#include "method_reference.h"
#include "safe_map.h"
#include "utf-inl.h"
#include "utils.h"

namespace art {

struct InstBlob {
  const uint16_t* start;
  const uint16_t* end;
  MethodReference ref;
  uint32_t dex_pc;

  size_t Size() const {
    return end - start;
  }
};

bool operator<(const InstBlob& a, const InstBlob& b) {
  return memcmp(a.start, b.start, std::min(a.Size(), b.Size()) * 2) < 0;
}

size_t SharedLen(const InstBlob& a, const InstBlob& b) {
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

void CDexExperiments::RunAll(const std::vector<std::unique_ptr<const DexFile>>& dex_files) {
  std::map<const DexFile::CodeItem*, MethodReference> code_items;
  for (const std::unique_ptr<const DexFile>& dex : dex_files) {
    for (size_t i = 0; i < dex->NumClassDefs(); ++i) {
      const DexFile::ClassDef& def = dex->GetClassDef(i);
      const uint8_t* data = dex->GetClassData(def);
      if (data == nullptr) {
        continue;
      }
      ClassDataItemIterator it(*dex, data);
      it.SkipAllFields();
      while (it.HasNextDirectMethod() || it.HasNextVirtualMethod()) {
        auto* item = it.GetMethodCodeItem();
        if (item != nullptr) {
          code_items.insert(std::make_pair(item, MethodReference(dex.get(), it.GetMemberIndex())));
        }
        it.Next();
      }
    }
  }
  std::cout << "Code items " << code_items.size() << "\n";
  std::vector<InstBlob> blobs;
  size_t insns = 0;
  for (auto&& pair : code_items) {
    const DexFile::CodeItem* code_item = pair.first;
    const uint16_t* code_start = code_item->insns_;
    const uint16_t* code_end = code_item->insns_ + code_item->insns_size_in_code_units_;
    for (auto* ptr = code_start; ptr != code_end; ptr += Instruction::At(ptr)->SizeInCodeUnits()) {
      blobs.push_back(InstBlob {ptr, code_end, pair.second, static_cast<uint32_t>(ptr - code_start)});
    }
    insns += code_item->insns_size_in_code_units_;
  }
  std::sort(blobs.begin(), blobs.end());
  size_t saved = 0;
  static const size_t kMinLen = 5;
  for (size_t i = kMinLen; i < blobs.size(); ++i) {
    size_t len = 0x1000000;
    for (size_t j = i - kMinLen; j < i; ++j) {
      len = std::min(len, SharedLen(blobs[j], blobs[i]));
    }

    if (len > 6) {
      saved += len - 1;
      std::cout << "Shared code blob (count>= " << kMinLen << " len= " << len << "): "
                << blobs[i].ref.PrettyMethod() << "@" << blobs[i].dex_pc << "\n";
      for (size_t pos = 0; pos < len; ) {
        auto* inst = Instruction::At(blobs[i].start + pos);
        std::cout << inst->DumpString(nullptr) << "\n";
        pos += inst->SizeInCodeUnits();
      }
      std::cout << "\n";
    }
  }
  std::cout << "Saved " << saved << " / " << insns << "\n";
  std::cout << "Blob count " << blobs.size() << "\n";
}

}  // namespace art
