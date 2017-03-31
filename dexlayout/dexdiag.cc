/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <memory>

#include "dex_file.h"
#include "dex_ir.h"
#include "dex_ir_builder.h"
#include "pagemap/pagemap.h"
#include "runtime.h"
#include "vdex_file.h"

static constexpr size_t kLineLength = 32;

static void Usage(const char *cmd) {
  fprintf(stderr, "Usage: %s pid\n"
                  "    Displays resident pages for DEX files.\n",
          cmd);
}

static char LetterForType(uint16_t type) {
  switch (type) {
   case art::DexFile::kDexTypeHeaderItem: return 'H';
   case art::DexFile::kDexTypeStringIdItem: return 'S';
   case art::DexFile::kDexTypeTypeIdItem: return 'T';
   case art::DexFile::kDexTypeProtoIdItem: return 'P';
   case art::DexFile::kDexTypeFieldIdItem: return 'F';
   case art::DexFile::kDexTypeMethodIdItem: return 'M';
   case art::DexFile::kDexTypeClassDefItem: return 'C';
   case art::DexFile::kDexTypeCallSiteIdItem: return 'z';
   case art::DexFile::kDexTypeMethodHandleItem: return 'Z';
   case art::DexFile::kDexTypeMapList: return 'L';
   case art::DexFile::kDexTypeTypeList: return 't';
   case art::DexFile::kDexTypeAnnotationSetRefList: return '1';
   case art::DexFile::kDexTypeAnnotationSetItem: return '2';
   case art::DexFile::kDexTypeClassDataItem: return 'c';
   case art::DexFile::kDexTypeCodeItem: return 'X';
   case art::DexFile::kDexTypeStringDataItem: return 's';
   case art::DexFile::kDexTypeDebugInfoItem: return 'D';
   case art::DexFile::kDexTypeAnnotationItem: return '3';
   case art::DexFile::kDexTypeEncodedArrayItem: return 'E';
   case art::DexFile::kDexTypeAnnotationsDirectoryItem: return '4';
   default: return '-';
  }
}

static char PageTypeChar(size_t offset, uint64_t* pagemap,
                         const std::vector<art::dex_ir::DexFileSection>& sections) {
  // If there's no non-zero sized section with an offset below offset we're looking for, it
  // must be the header.
  int letter = 'H';
  for (const auto& section : sections) {
    if (section.size_ > 0 && section.offset_ / 0x1000 < offset) {
      letter = LetterForType(section.type_);
      break;
    }
  }
  if (PM_PAGEMAP_PRESENT(pagemap[offset])) {
    return letter;
  } else if (PM_PAGEMAP_SWAPPED(pagemap[offset])) {
    return '*';
  } else {
    return '.';
  }
}

static void ProcessPageMap(uint64_t* pagemap, size_t start, size_t end,
                           const std::vector<art::dex_ir::DexFileSection>& sections) {
  for (size_t i = start; i < end; ++i) {
    printf("%c", PageTypeChar(i, pagemap, sections));
    if ((i - start) % kLineLength == kLineLength - 1) {
      printf("\n");
    }
  }
  if ((end - start) % kLineLength != 0) {
    printf("\n");
  }
}

static void ProcessDexMapping(uint64_t *pagemap, uint64_t map_start, const art::DexFile* dex_file,
                              uint64_t vdex_start) {
  uint64_t dex_file_start = reinterpret_cast<uint64_t>(dex_file->Begin());
  size_t dex_file_size = dex_file->Size();
  if (dex_file_start < vdex_start) {
    fprintf(stderr, "Something's wrong with DEX %s: %zx > %zx\n", dex_file->GetLocation().c_str(),
            map_start, dex_file_start);
    return;
  }
  uint64_t start = (dex_file_start - vdex_start) / 0x1000;
  uint64_t end = (start + dex_file_size + 0xfff) / 0x1000;
  printf("DEX %s: %zx-%zx\n", dex_file->GetLocation().c_str(), map_start + start * 0x1000,
         map_start + end * 0x1000);
  std::vector<art::dex_ir::DexFileSection> sections;
  {
    std::unique_ptr<art::dex_ir::Header> header(art::dex_ir::DexIrBuilder(*dex_file));
    sections = art::dex_ir::GetSortedDexFileSections(header.get(), art::dex_ir::kSortDescending);
  }
  ProcessPageMap(pagemap, start, end, sections);
}

static void DisplayMappingIfFromVdexFile(pm_map_t *map) {
  static const char* suffixes[] = { ".vdex" };
  std::string vdex_name;
  bool found = false;
  for (size_t j = 0; j < sizeof(suffixes) / sizeof(suffixes[0]); ++j) {
    if (strstr(pm_map_name(map), suffixes[j]) != NULL) {
      vdex_name = pm_map_name(map);
      found = true;
      break;
    }
  }
  if (!found) {
    return;
  }
  std::string error_string;
  std::unique_ptr<art::VdexFile> vdex(art::VdexFile::Open(vdex_name,
                                                          false /*writeable*/,
                                                          false /*low_4gb*/,
                                                          &error_string));
  std::vector<std::unique_ptr<const art::DexFile>> dex_files;
  std::string error_msg;
  if (!vdex->OpenAllDexFiles(&dex_files, &error_msg)) {
    fprintf(stderr, "Dex files could not be opened for %s: error %s\n",
            vdex_name.c_str(), error_msg.c_str());
  }
  printf("MAPPING %s: %zx-%zx\n", pm_map_name(map), pm_map_start(map), pm_map_end(map));
  uint64_t *pagemap;
  size_t len;
  if (pm_map_pagemap(map, &pagemap, &len) != 0) {
    fprintf(stderr, "error creating pagemap\n");
    exit(EXIT_FAILURE);
  }
  for (const auto& dex_file : dex_files) {
    ProcessDexMapping(pagemap, pm_map_start(map), dex_file.get(),
                      reinterpret_cast<uint64_t>(vdex->Begin()));
  }
  free(pagemap);
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    Usage(argv[0]);
    exit(EXIT_FAILURE);
  }

  // Art specific set up.
  art::InitLogging(argv, art::Runtime::Aborter);
  art::MemMap::Init();

  pid_t pid;
  char *endptr;
  pid = (pid_t)strtol(argv[argc - 1], &endptr, 10);
  if (*endptr != '\0') {
    fprintf(stderr, "Invalid PID \"%s\".\n", argv[argc - 1]);
    exit(EXIT_FAILURE);
  }

  // get libpagemap kernel information.
  pm_kernel_t *ker;
  if (pm_kernel_create(&ker) != 0) {
    fprintf(stderr, "error creating kernel interface -- "
                    "does this kernel have pagemap?\n");
    exit(EXIT_FAILURE);
  }

  // get libpagemap process information.
  pm_process_t *proc;
  if (pm_process_create(ker, pid, &proc) != 0) {
    fprintf(stderr, "error creating process interface -- "
                    "does process %d really exist?\n", pid);
    exit(EXIT_FAILURE);
  }

  // Get the set of mappings by the specified process.
  pm_map_t **maps;
  size_t num_maps;
  if (pm_process_maps(proc, &maps, &num_maps) != 0) {
    fprintf(stderr, "error listing maps.\n");
    exit(EXIT_FAILURE);
  }

  // Process the mappings that are due to DEX files.
  for (size_t i = 0; i < num_maps; ++i) {
    DisplayMappingIfFromVdexFile(maps[i]);
  }

  return 0;
}

