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

static bool g_show_key = false;
static bool g_verbose = false;
static bool g_show_statistics = false;

struct DexSectionInfo {
 public:
  std::string name_;
  char letter_;
};

static const std::map<uint16_t, DexSectionInfo> kDexSectionInfoMap = {
   { art::DexFile::kDexTypeHeaderItem, { "Header", 'H' } },
   { art::DexFile::kDexTypeStringIdItem, { "StringId", 'S' } },
   { art::DexFile::kDexTypeTypeIdItem, { "TypeId", 'T' } },
   { art::DexFile::kDexTypeProtoIdItem, { "ProtoId", 'P' } },
   { art::DexFile::kDexTypeFieldIdItem, { "FieldId", 'F' } },
   { art::DexFile::kDexTypeMethodIdItem, { "MethodId", 'M' } },
   { art::DexFile::kDexTypeClassDefItem, { "ClassDef", 'C' } },
   { art::DexFile::kDexTypeCallSiteIdItem, { "CallSiteId", 'z' } },
   { art::DexFile::kDexTypeMethodHandleItem, { "MethodHandle", 'Z' } },
   { art::DexFile::kDexTypeMapList, { "TypeMap", 'L' } },
   { art::DexFile::kDexTypeTypeList, { "TypeList", 't' } },
   { art::DexFile::kDexTypeAnnotationSetRefList, { "AnnotationSetReferenceItem", '1' } },
   { art::DexFile::kDexTypeAnnotationSetItem, { "AnnotationSetItem", '2' } },
   { art::DexFile::kDexTypeClassDataItem, { "ClassData", 'c' } },
   { art::DexFile::kDexTypeCodeItem, { "CodeItem", 'X' } },
   { art::DexFile::kDexTypeStringDataItem, { "StringData", 's' } },
   { art::DexFile::kDexTypeDebugInfoItem, { "DebugInfo", 'D' } },
   { art::DexFile::kDexTypeAnnotationItem, { "AnnotationItem", '3' } },
   { art::DexFile::kDexTypeEncodedArrayItem, { "EncodedArrayItem", 'E' } },
   { art::DexFile::kDexTypeAnnotationsDirectoryItem, { "AnnotationsDirectoryItem", '4' } }
};

class PageCount {
 public:
  PageCount() {
    for (auto i = kDexSectionInfoMap.begin(); i != kDexSectionInfoMap.end(); ++i) {
      map_[i->first] = 0;
    }
  }
  void Increment(uint16_t type) {
    map_[type]++;
  }
  size_t Get(uint16_t type) const {
    return map_.at(type);
  }
 private:
  std::map<uint16_t, size_t> map_;
  DISALLOW_COPY_AND_ASSIGN(PageCount);
};

static void PrintLetterKey() {
  printf("letter section_type\n");
  for (const auto& p : kDexSectionInfoMap) {
    const DexSectionInfo& section_info = p.second;
    printf("%c      %s\n", section_info.letter_, section_info.name_.c_str());
  }
}

static void Usage(const char *cmd) {
  fprintf(stderr, "Usage: %s [-s] [-v] pid\n"
                  "    -s Shows a key to verbose display characters.\n"
                  "    -s Shows section statistics for individual dex files.\n"
                  "    -v Verbosely displays resident pages for dex files.\n",
          cmd);
}

static char PageTypeChar(uint16_t type) {
  if (kDexSectionInfoMap.find(type) == kDexSectionInfoMap.end()) {
    return '-';
  }
  return kDexSectionInfoMap.find(type)->second.letter_;
}

static uint16_t FindSectionTypeForPage(size_t page,
                                       const std::vector<art::dex_ir::DexFileSection>& sections) {
  for (const auto& section : sections) {
    size_t first_page_of_section = section.offset_ / art::kPageSize;
    // Only consider non-empty sections.
    if (section.size_ == 0) {
      continue;
    }
    // Attribute the page to the highest-offset section that starts before the page.
    if (first_page_of_section <= page) {
      return section.type_;
    }
  }
  // If there's no non-zero sized section with an offset below offset we're looking for, it
  // must be the header.
  return art::DexFile::kDexTypeHeaderItem;
}

static void ProcessPageMap(uint64_t* pagemap, size_t start, size_t end,
                           const std::vector<art::dex_ir::DexFileSection>& sections,
                           PageCount* page_counts) {
  for (size_t page = start; page < end; ++page) {
    char type_char = '.';
    if (PM_PAGEMAP_PRESENT(pagemap[page])) {
      uint16_t type = FindSectionTypeForPage(page, sections);
      page_counts->Increment(type);
      type_char = PageTypeChar(type);
    }
    if (g_verbose) {
      printf("%c", type_char);
      if ((page - start) % kLineLength == kLineLength - 1) {
        printf("\n");
      }
    }
  }
  if (g_verbose) {
    if ((end - start) % kLineLength != 0) {
      printf("\n");
    }
  }
}

static void DisplayDexStatistics(size_t start, size_t end, const PageCount& resident_pages,
                                 const std::vector<art::dex_ir::DexFileSection>& sections) {
  // Compute the total possible sizes for sections.
  PageCount mapped_pages;
  for (size_t page = start; page < end; ++page) {
    mapped_pages.Increment(FindSectionTypeForPage(page, sections));
  }
  size_t total_file_pages = 0;
  size_t total_resident_pages = 0;
  // Display the sections.
  printf("Section name               offset   resident total    pct.\n");
  for (size_t i = sections.size(); i > 0; --i) {
    const art::dex_ir::DexFileSection& section = sections[i - 1];
    const uint16_t type = section.type_;
    const DexSectionInfo& section_info = kDexSectionInfoMap.find(type)->second;
    size_t pages_mapped = mapped_pages.Get(type);
    size_t pages_resident = resident_pages.Get(type);
    double percent_resident = 0;
    if (mapped_pages.Get(type) > 0) {
      percent_resident = 100.0 * pages_resident / mapped_pages.Get(type);
    }
    printf("%-26s %08x %08zx %08zx %6.2f\n",
           section_info.name_.c_str(),
           section.offset_,
           pages_resident,
           mapped_pages.Get(type),
           percent_resident);
    total_file_pages += pages_mapped;
    total_resident_pages += pages_resident;
  }
  double total_percent_resident = 0;
  if (total_file_pages > 0) {
    total_percent_resident = 100.0 * total_resident_pages / total_file_pages;
  }
  printf("GRAND TOTAL                         %08zx %08zx %6.2f\n",
         total_resident_pages, total_file_pages, total_percent_resident);
  printf("\n");
}

static void ProcessOneDexMapping(uint64_t *pagemap, uint64_t map_start,
                                 const art::DexFile* dex_file, uint64_t vdex_start) {
  uint64_t dex_file_start = reinterpret_cast<uint64_t>(dex_file->Begin());
  size_t dex_file_size = dex_file->Size();
  if (dex_file_start < vdex_start) {
    fprintf(stderr, "Dex file start offset for %s is incorrect: map start %zx > dex start %zx\n",
            dex_file->GetLocation().c_str(), map_start, dex_file_start);
    return;
  }
  uint64_t start = (dex_file_start - vdex_start) / art::kPageSize;
  uint64_t end = (start + dex_file_size + art::kPageSize - 1) / art::kPageSize;
  printf("DEX %s: %zx-%zx\n", dex_file->GetLocation().c_str(), map_start + start * art::kPageSize,
         map_start + end * art::kPageSize);
  // Build a list of the dex file section types, sorted from highest offset to lowest.
  std::vector<art::dex_ir::DexFileSection> sections;
  {
    std::unique_ptr<art::dex_ir::Header> header(art::dex_ir::DexIrBuilder(*dex_file));
    sections = art::dex_ir::GetSortedDexFileSections(header.get(), art::dex_ir::kSortDescending);
  }
  PageCount section_resident_pages;
  ProcessPageMap(pagemap, start, end, sections, &section_resident_pages);
  if (g_show_statistics) {
    DisplayDexStatistics(start, end, section_resident_pages, sections);
  }
}

static void DisplayMappingIfFromVdexFile(pm_map_t *map) {
  // Confirm that the map is from a vdex file.
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
  // Extract all the dex files from the vdex file.
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
  // Open the page mapping (one uint64_t per page) for the entire vdex mapping.
  uint64_t *pagemap;
  size_t len;
  if (pm_map_pagemap(map, &pagemap, &len) != 0) {
    fprintf(stderr, "error creating pagemap\n");
    exit(EXIT_FAILURE);
  }
  // Process the dex files.
  printf("MAPPING %s: %zx-%zx\n", pm_map_name(map), pm_map_start(map), pm_map_end(map));
  for (const auto& dex_file : dex_files) {
    ProcessOneDexMapping(pagemap, pm_map_start(map), dex_file.get(),
                         reinterpret_cast<uint64_t>(vdex->Begin()));
  }
  free(pagemap);
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    Usage(argv[0]);
    exit(EXIT_FAILURE);
  }

  for (int i = 1; i < argc - 1; ++i) {
    if (strcmp(argv[i], "-k") == 0) {
      g_show_key = true;
    } else if (strcmp(argv[i], "-s") == 0) {
      g_show_statistics = true;
    } else if (strcmp(argv[i], "-v") == 0) {
      g_verbose = true;
    } else {
      Usage(argv[0]);
    }
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

  if (g_show_key) {
    PrintLetterKey();
  }
  return 0;
}

