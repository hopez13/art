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

#ifndef ART_RUNTIME_OAT_FILE_INL_H_
#define ART_RUNTIME_OAT_FILE_INL_H_

#include "oat_file.h"
#include "oat_quick_method_header.h"

namespace art {

template<>
inline bool OatFile::CanUseCode<ArtMethodType::kNativeMethod>() const {
  // Since jni-bridges don't have any debuggable data attached to them we can always use it as long
  // as we can execute it.
  return IsExecutable();
}

template<>
inline bool OatFile::CanUseCode<ArtMethodType::kDexMethod>() const {
  // We must be executable and either the runtime must not be debuggable or we must be.
  return IsExecutable() && (!Runtime::Current()->IsJavaDebuggable() || IsDebuggable());
}

template<>
inline bool OatFile::CanUseCode<ArtMethodType::kAll>() const {
  // If we are in oatdump or dex2oat it is always fine to use the compiled code (since it won't
  // actually be run).
  if (Runtime::Current() == nullptr || Runtime::Current()->IsAotCompiler()) {
    return true;
  }
  return CanUseCode<ArtMethodType::kDexMethod>() && CanUseCode<ArtMethodType::kNativeMethod>();
}

inline const OatQuickMethodHeader* OatFile::OatMethod::GetOatQuickMethodHeader() const {
  const void* code = EntryPointToCodePointer(GetOatPointer<const void*>(code_offset_));
  if (code == nullptr) {
    return nullptr;
  }
  // Return a pointer to the packed struct before the code.
  return reinterpret_cast<const OatQuickMethodHeader*>(code) - 1;
}

inline uint32_t OatFile::OatMethod::GetOatQuickMethodHeaderOffset() const {
  const OatQuickMethodHeader* method_header = GetOatQuickMethodHeader();
  if (method_header == nullptr) {
    return 0u;
  }
  return reinterpret_cast<const uint8_t*>(method_header) - begin_;
}

inline uint32_t OatFile::OatMethod::GetQuickCodeSizeOffset() const {
  const OatQuickMethodHeader* method_header = GetOatQuickMethodHeader();
  if (method_header == nullptr) {
    return 0u;
  }
  return reinterpret_cast<const uint8_t*>(method_header->GetCodeSizeAddr()) - begin_;
}

inline size_t OatFile::OatMethod::GetFrameSizeInBytes() const {
  const void* code = EntryPointToCodePointer(GetQuickCode());
  if (code == nullptr) {
    return 0u;
  }
  return reinterpret_cast<const OatQuickMethodHeader*>(code)[-1].GetFrameInfo().FrameSizeInBytes();
}

inline uint32_t OatFile::OatMethod::GetCoreSpillMask() const {
  const void* code = EntryPointToCodePointer(GetQuickCode());
  if (code == nullptr) {
    return 0u;
  }
  return reinterpret_cast<const OatQuickMethodHeader*>(code)[-1].GetFrameInfo().CoreSpillMask();
}

inline uint32_t OatFile::OatMethod::GetFpSpillMask() const {
  const void* code = EntryPointToCodePointer(GetQuickCode());
  if (code == nullptr) {
    return 0u;
  }
  return reinterpret_cast<const OatQuickMethodHeader*>(code)[-1].GetFrameInfo().FpSpillMask();
}

inline uint32_t OatFile::OatMethod::GetVmapTableOffset() const {
  const uint8_t* vmap_table = GetVmapTable();
  return static_cast<uint32_t>(vmap_table != nullptr ? vmap_table - begin_ : 0u);
}

inline uint32_t OatFile::OatMethod::GetVmapTableOffsetOffset() const {
  const OatQuickMethodHeader* method_header = GetOatQuickMethodHeader();
  if (method_header == nullptr) {
    return 0u;
  }
  return reinterpret_cast<const uint8_t*>(method_header->GetVmapTableOffsetAddr()) - begin_;
}

inline const uint8_t* OatFile::OatMethod::GetVmapTable() const {
  const void* code = EntryPointToCodePointer(GetOatPointer<const void*>(code_offset_));
  if (code == nullptr) {
    return nullptr;
  }
  uint32_t offset = reinterpret_cast<const OatQuickMethodHeader*>(code)[-1].GetVmapTableOffset();
  if (UNLIKELY(offset == 0u)) {
    return nullptr;
  }
  return reinterpret_cast<const uint8_t*>(code) - offset;
}

inline uint32_t OatFile::OatMethod::GetQuickCodeSize() const {
  const void* code = EntryPointToCodePointer(GetOatPointer<const void*>(code_offset_));
  if (code == nullptr) {
    return 0u;
  }
  return reinterpret_cast<const OatQuickMethodHeader*>(code)[-1].GetCodeSize();
}

inline uint32_t OatFile::OatMethod::GetCodeOffset() const {
  return (GetQuickCodeSize() == 0) ? 0 : code_offset_;
}

inline const void* OatFile::OatMethod::GetQuickCode() const {
  return GetOatPointer<const void*>(GetCodeOffset());
}

template <ArtMethodType kArtMethodType>
inline const OatFile::OatMethod OatFile::OatClass::GetOatMethod(uint32_t method_index) const {
  const OatMethodOffsets* oat_method_offsets = GetOatMethodOffsets(method_index);
  if (oat_method_offsets == nullptr) {
    return OatMethod(nullptr, 0);
  }
  if (oat_file_->CanUseCode<kArtMethodType>()) {
    return OatMethod(oat_file_->Begin(), oat_method_offsets->code_offset_);
  }
  // We aren't allowed to use the compiled code. We just force it down the interpreted / jit
  // version.
  return OatMethod(oat_file_->Begin(), 0);
}

}  // namespace art

#endif  // ART_RUNTIME_OAT_FILE_INL_H_
