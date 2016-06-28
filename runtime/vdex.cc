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

#include "vdex.h"

namespace art {
  constexpr uint8_t VdexFile::Header::kVdexMagic[4];
  constexpr uint8_t VdexFile::Header::kVdexVersion[4];

  bool VdexFile::Header::IsMagicValid() const {
    return (memcmp(magic_, kVdexMagic, sizeof(kVdexMagic)) == 0);
  }

  bool VdexFile::Header::IsVersionValid() const {
    const uint8_t* version = &magic_[sizeof(kVdexMagic)];
    return (memcmp(version, kVdexVersion, sizeof(kVdexVersion)) == 0);
  }

  VdexFile::Header::Header()
    : dex_files_size_(0),
      verifier_metadata_size_(0) {
    memcpy(magic_, kVdexMagic, sizeof(kVdexMagic));
    memcpy(&magic_[sizeof(kVdexMagic)], kVdexVersion, sizeof(kVdexVersion));
    DCHECK(IsMagicValid());
    DCHECK(IsVersionValid());
  }
}  // namespace art
