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
#include <sstream>

#include "cdex/outline_dictionary.h"
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

std::string CDexExperiments::Size(size_t sz) const {
  std::ostringstream oss;
  oss << sz << "(" << static_cast<double>(sz) / total_dex_size_ << ")";
  return oss.str();
}

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

void CDexExperiments::RunAll(const std::vector<const DexFile*>& dex_files) {
  std::map<const DexFile::CodeItem*, MethodReference> code_items;
  size_t num_type_ids = 0;
  for (const DexFile* dex : dex_files) {
    total_dex_size_ += dex->Size();
    num_type_ids += dex->NumTypeIds();
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
          code_items.insert(std::make_pair(item, MethodReference(dex, it.GetMemberIndex())));
        }
        it.Next();
      }
    }
  }
  std::cout << "Code items " << code_items.size() << "\n";
  std::cout << "TypeId bytes " << Size(num_type_ids * sizeof(DexFile::TypeId)) << "\n";
  // OutliningExperiments(code_items);
  LebEncodeCodeItems(code_items);
  LebEncodeDebugInfos(code_items);
  DedupeNoDebugOffset(code_items);
  FuseInvoke(code_items);
  // OutlineDictionary oo;
  // oo.Build(code_items);
}

void CDexExperiments::FuseInvoke(
    const std::map<const DexFile::CodeItem*, MethodReference>& code_items) {
  size_t save_v = 0, save_d = 0, save_s = 0, save_i = 0;
  size_t save_nf = 0;
  // Measure savings form fusing invokes with
  for (auto&& pair : code_items) {
    auto insns = pair.first->Instructions();
    for (auto it = insns.begin(); it != insns.end(); ++it) {
      bool v = it->Opcode() == Instruction::INVOKE_VIRTUAL;
      bool d = it->Opcode() == Instruction::INVOKE_DIRECT;
      bool s = it->Opcode() == Instruction::INVOKE_STATIC;
      bool i = it->Opcode() == Instruction::INVOKE_INTERFACE;
      if (v || d || s || i) {
        auto next = it;
        ++next;
        if (next != insns.end() &&
            (it->Fetch16(0) >> 12) <= 5) {
          const size_t save = 4;
          if (next->Opcode() == Instruction::MOVE_RESULT_OBJECT) {
            save_v += v * save;
            save_d += d * save;
            save_s += s * save;
            save_i += i * save;
          } else {
            save_nf += save;
          }
        }
      }
    }
  }
  std::cout
      << "Fuse invoke"
      << " virtual=" << Size(save_v)
      << " direct=" << Size(save_d)
      << " static=" << Size(save_s)
      << " interface=" << Size(save_i)
      << " non-move-result=" << Size(save_nf)
      << " total=" << Size(save_v + save_d + save_s + save_i + save_nf)
      << std::endl;
}

void CDexExperiments::LebEncodeCodeItems(
    const std::map<const DexFile::CodeItem*, MethodReference>& code_items) {
  size_t normal_size = 0;
  size_t leb_size = 0;
  size_t leb_debug_size = 0;
  size_t align_pad = 0;
  for (auto&& pair : code_items) {
    const DexFile::CodeItem* item = pair.first;
    normal_size += 12;  // sizeof(DexFile::CodeItem) - 8?
    leb_debug_size += UnsignedLeb128Size(item->debug_info_off_);
    leb_size += UnsignedLeb128Size(item->ins_size_);
    leb_size += UnsignedLeb128Size(item->insns_size_in_code_units_);
    leb_size += UnsignedLeb128Size(item->outs_size_);
    leb_size += UnsignedLeb128Size(item->registers_size_);
    leb_size += UnsignedLeb128Size(item->tries_size_);
    const size_t sz = DexFile::GetCodeItemSize(*item);
    align_pad += RoundUp(sz, 4) - RoundUp(sz, 2);
  }
  normal_size += align_pad;  // 4 byte alignment padding.
  // Assuming 2 bytes for debug:
  std::cout << "Base size=" << Size(normal_size)
            << " savings=" << Size(normal_size - leb_size)
            << std::endl;
}

static uint32_t GetDebugInfoStreamSize(const uint8_t* debug_info_stream) {
  const uint8_t* stream = debug_info_stream;
  DecodeUnsignedLeb128(&stream);  // line_start
  uint32_t parameters_size = DecodeUnsignedLeb128(&stream);
  for (uint32_t i = 0; i < parameters_size; ++i) {
    DecodeUnsignedLeb128P1(&stream);  // Parameter name.
  }

  for (;;)  {
    uint8_t opcode = *stream++;
    switch (opcode) {
      case DexFile::DBG_END_SEQUENCE:
        return stream - debug_info_stream;  // end of stream.
      case DexFile::DBG_ADVANCE_PC:
        DecodeUnsignedLeb128(&stream);  // addr_diff
        break;
      case DexFile::DBG_ADVANCE_LINE:
        DecodeSignedLeb128(&stream);  // line_diff
        break;
      case DexFile::DBG_START_LOCAL:
        DecodeUnsignedLeb128(&stream);  // register_num
        DecodeUnsignedLeb128P1(&stream);  // name_idx
        DecodeUnsignedLeb128P1(&stream);  // type_idx
        break;
      case DexFile::DBG_START_LOCAL_EXTENDED:
        DecodeUnsignedLeb128(&stream);  // register_num
        DecodeUnsignedLeb128P1(&stream);  // name_idx
        DecodeUnsignedLeb128P1(&stream);  // type_idx
        DecodeUnsignedLeb128P1(&stream);  // sig_idx
        break;
      case DexFile::DBG_END_LOCAL:
      case DexFile::DBG_RESTART_LOCAL:
        DecodeUnsignedLeb128(&stream);  // register_num
        break;
      case DexFile::DBG_SET_PROLOGUE_END:
      case DexFile::DBG_SET_EPILOGUE_BEGIN:
        break;
      case DexFile::DBG_SET_FILE: {
        DecodeUnsignedLeb128P1(&stream);  // name_idx
        break;
      }
      default: {
        break;
      }
    }
  }
  LOG(FATAL) << "MISSING END SEQUENCE";
}

void CDexExperiments::LebEncodeDebugInfos(
    const std::map<const DexFile::CodeItem*, MethodReference>& code_items) {
  size_t normal_size = 0;
  size_t leb_size = 0;
  for (auto&& pair : code_items) {
    const DexFile::CodeItem* item = pair.first;
    const uint8_t* stream = pair.second.dex_file->GetDebugInfoStream(item);
    normal_size += sizeof(uint32_t);
    size_t debug_size = 0;
    if (stream != nullptr) {
      debug_size = GetDebugInfoStreamSize(stream);
    }
    leb_size += UnsignedLeb128Size(debug_size);
  }
  const size_t table_max = sizeof(uint32_t) * 2 * code_items.size();
  std::cout << "Base size=" << Size(normal_size)
            << " savings(4)=" << Size(normal_size - leb_size - table_max / 4)
            << " savings(16)=" << Size(normal_size - leb_size - table_max / 8)
            << " savings(32)=" << Size(normal_size - leb_size - table_max / 16)
            << std::endl;
}

class CompareCodeItems {
 public:
  bool operator()(const DexFile::CodeItem* a, const DexFile::CodeItem* b) const {
    const size_t sz1 = DexFile::GetCodeItemSize(*a);
    const size_t sz2 = DexFile::GetCodeItemSize(*b);
    if (sz1 != sz2) {
      return sz1 < sz2;
    }
    const uint8_t* a_ptr = reinterpret_cast<const uint8_t*>(a);
    const uint8_t* b_ptr = reinterpret_cast<const uint8_t*>(b);
    // Compare everything other than code item.
    size_t offset = reinterpret_cast<const uint8_t*>(&a->debug_info_off_) - a_ptr;
    int c = memcmp(a_ptr, b_ptr, offset);
    if (c != 0) {
      return c < 0;
    }
    offset += 4;
    return memcmp(a_ptr + offset, b_ptr + offset, sz1 - offset) < 0;
  }
};

void CDexExperiments::DedupeNoDebugOffset(
    const std::map<const DexFile::CodeItem*, MethodReference>& code_items) {
  size_t total_size = 0;
  size_t deduped_size = 0;
  std::set<const DexFile::CodeItem*, CompareCodeItems> deduped;
  for (auto&& pair : code_items) {
    total_size += DexFile::GetCodeItemSize(*pair.first);
    deduped.insert(pair.first);
  }
  for (const DexFile::CodeItem* item : deduped) {
    deduped_size += DexFile::GetCodeItemSize(*item);
  }
  std::cout << "Dedupe code items: " << total_size << "->" << deduped_size
            << " " << Size(total_size - deduped_size) << "\n";
}

void CDexExperiments::OutliningExperiments(
    const std::map<const DexFile::CodeItem*, MethodReference>& code_items) {
  std::vector<InstBlob> blobs;
  size_t insns = 0;
  for (auto&& pair : code_items) {
    const DexFile::CodeItem* code_item = pair.first;
    const uint16_t* code_start = code_item->insns_;
    const uint16_t* code_end = code_item->insns_ + code_item->insns_size_in_code_units_;
    for (auto* ptr = code_start; ptr != code_end; ptr += Instruction::At(ptr)->SizeInCodeUnits()) {
      blobs.push_back(
          InstBlob {ptr, code_end, pair.second, static_cast<uint32_t>(ptr - code_start)});
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
