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

#ifndef ART_RUNTIME_CDEX_EXPERIMENTS_H_
#define ART_RUNTIME_CDEX_EXPERIMENTS_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "dex_file.h"
#include "method_reference.h"

namespace art {

class DexFile;

class CDexExperiments {
 public:
  void RunAll(const std::vector<const DexFile*>& dex_files);

 private:
  void OutliningExperiments(const std::map<const DexFile::CodeItem*, MethodReference>& code_items);
  void LebEncodeCodeItems(const std::map<const DexFile::CodeItem*, MethodReference>& code_items);
  void LebEncodeDebugInfos(const std::map<const DexFile::CodeItem*, MethodReference>& code_items);
  void DedupeNoDebugOffset(const std::map<const DexFile::CodeItem*, MethodReference>& code_items);
  void FuseInvoke(const std::map<const DexFile::CodeItem*, MethodReference>& code_items);

  size_t total_dex_size_ = 0;

  std::string Size(size_t sz) const;
};

}  // namespace art

#endif  // ART_RUNTIME_CDEX_EXPERIMENTS_H_
