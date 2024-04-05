/*
 * Copyright (C) 2024 The Android Open Source Project
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

// Check that the current executable or shared library only links known exported
// libraries dynamically. Intended to be statically linked into standalone
// tests.

#include <dlfcn.h>
#include <fcntl.h>
#include <gelf.h>
#include <libelf.h>

#include <algorithm>
#include <string>
#include <vector>

#include "android-base/result-gmock.h"
#include "android-base/result.h"
#include "android-base/strings.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using android::base::ErrnoError;
using android::base::Error;
using android::base::Result;
using android::base::testing::Ok;
using testing::IsEmpty;

const std::vector<std::string> kAllowedDynamicLibDeps = {
    // Bionic
    "libc.so",
    "libdl.so",
    "libdl_android.so",
    "libm.so",
    // Platform
    "heapprofd_client_api.so",
    "libbinder_ndk.so",
    "liblog.so",
    "libselinux.so",
    "libz.so",
    // Other modules
    "libstatspull.so",
    "libstatssocket.so",
    // ART exported
    "libdexfile.so",
    "libnativebridge.so",
    "libnativehelper.so",
    "libnativeloader.so",
    // TODO(b/333438055): Remove this when we can link libc++.so statically everywhere.
    "libc++.so",
};

Result<std::string> GetCurrentElfObjectPath() {
  Dl_info info;

  if (dladdr(reinterpret_cast<void*>(GetCurrentElfObjectPath), &info) == 0) {
    return Error() << "dladdr failed to map own address to a shared object.";
  }

  char resolvedPath[PATH_MAX];
  realpath(info.dli_fname, resolvedPath);
  return resolvedPath;
}

Result<std::vector<std::string>> GetDynamicLibDeps(const std::string& filename) {
  if (elf_version(EV_CURRENT) == EV_NONE) {
    return Error() << "libelf initialization failed: " << elf_errmsg(-1);
  }

  int fd = open(filename.c_str(), O_RDONLY);
  if (fd == -1) {
    return ErrnoError() << "Error opening " << filename;
  }

  Elf* elf = elf_begin(fd, ELF_C_READ, nullptr);
  if (elf == nullptr) {
    return Error() << "Error creating ELF object for " << filename << ": " << elf_errmsg(-1);
  }

  std::vector<std::string> libs;

  // Get the dynamic section.
  for (Elf_Scn* dyn_scn = nullptr; (dyn_scn = elf_nextscn(elf, dyn_scn)) != nullptr;) {
    GElf_Shdr scn_hdr;
    if (gelf_getshdr(dyn_scn, &scn_hdr) != &scn_hdr) {
      return Error() << "Failed to retrieve ELF section header in " << filename << ": "
                     << elf_errmsg(-1);
    }

    if (scn_hdr.sh_type == SHT_DYNAMIC) {
      Elf_Data* data = elf_getdata(dyn_scn, nullptr);

      // Iterate through dynamic section entries.
      for (int i = 0; i < scn_hdr.sh_size / scn_hdr.sh_entsize; i++) {
        GElf_Dyn dyn_entry;
        if (gelf_getdyn(data, i, &dyn_entry) != &dyn_entry) {
          return Error() << "Failed to get entry " << i << " in ELF dynamic section of " << filename
                         << ": " << elf_errmsg(-1);
        }

        if (dyn_entry.d_tag == DT_NEEDED) {
          const char* lib_name = elf_strptr(elf, scn_hdr.sh_link, dyn_entry.d_un.d_val);
          if (lib_name == nullptr) {
            return Error() << "Failed to get string from entry " << i
                           << " in ELF dynamic section of " << filename << ": " << elf_errmsg(-1);
          }
          libs.push_back(lib_name);
        }
      }
      break;  // Found the dynamic section, no need to continue.
    }
  }

  elf_end(elf);
  close(fd);
  return libs;
}

}  // namespace

TEST(StandaloneTestAllowedLibDeps, test) {
  Result<std::string> path_to_self = GetCurrentElfObjectPath();
  EXPECT_THAT(path_to_self, Ok());
  Result<std::vector<std::string>> dyn_lib_deps = GetDynamicLibDeps(path_to_self.value());
  EXPECT_THAT(dyn_lib_deps, Ok());

  std::vector<std::string> unallowed_libs;
  for (const std::string& dyn_lib_dep : dyn_lib_deps.value()) {
    if (std::find(kAllowedDynamicLibDeps.begin(), kAllowedDynamicLibDeps.end(), dyn_lib_dep) ==
        kAllowedDynamicLibDeps.end()) {
      unallowed_libs.push_back(dyn_lib_dep);
    }
  }

  EXPECT_THAT(unallowed_libs, IsEmpty())
      << path_to_self.value() << " has unallowed shared library dependencies.";
}
