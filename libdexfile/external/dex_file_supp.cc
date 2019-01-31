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

#include "art_api/dex_file_support.h"

#include <dlfcn.h>

#include <log/log.h>

namespace art_api {
namespace dex {

constexpr char kLibdexfileExternalLib[] = "libdexfile_external.so";

decltype(ExtDexFileMakeString)* DexString::g_ExtDexFileMakeString = nullptr;
decltype(ExtDexFileGetString)* DexString::g_ExtDexFileGetString = nullptr;
decltype(ExtDexFileFreeString)* DexString::g_ExtDexFileFreeString = nullptr;

decltype(ExtDexFileOpenFromMemory)* DexFile::g_ExtDexFileOpenFromMemory = nullptr;
decltype(ExtDexFileOpenFromFd)* DexFile::g_ExtDexFileOpenFromFd = nullptr;
decltype(ExtDexFileGetMethodInfoForOffset)* DexFile::g_ExtDexFileGetMethodInfoForOffset = nullptr;
decltype(ExtDexFileGetAllMethodInfos)* DexFile::g_ExtDexFileGetAllMethodInfos = nullptr;
decltype(ExtDexFileFree)* DexFile::g_ExtDexFileFree = nullptr;

void LoadLibdexfileExternal() {
  static void* handle = dlopen(kLibdexfileExternalLib, RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE);
  LOG_ALWAYS_FATAL_IF(handle == nullptr, "Failed to load %s: %s", kLibdexfileExternalLib, dlerror());

#define INSTALL_SYMBOL(CLASS, SYMBOL)                                                \
  do {                                                                               \
    CLASS::g_##SYMBOL = reinterpret_cast<decltype(SYMBOL)*>(dlsym(handle, #SYMBOL)); \
    LOG_ALWAYS_FATAL_IF(CLASS::g_##SYMBOL == nullptr,                                \
                        "Failed to find %s in %s: %s",                               \
                        #SYMBOL,                                                     \
                        kLibdexfileExternalLib,                                      \
                        dlerror());                                                  \
  } while (0)
  INSTALL_SYMBOL(DexString, ExtDexFileMakeString);
  INSTALL_SYMBOL(DexString, ExtDexFileGetString);
  INSTALL_SYMBOL(DexString, ExtDexFileFreeString);
  INSTALL_SYMBOL(DexFile, ExtDexFileOpenFromMemory);
  INSTALL_SYMBOL(DexFile, ExtDexFileOpenFromFd);
  INSTALL_SYMBOL(DexFile, ExtDexFileGetMethodInfoForOffset);
  INSTALL_SYMBOL(DexFile, ExtDexFileGetAllMethodInfos);
  INSTALL_SYMBOL(DexFile, ExtDexFileFree);
#undef INSTALL_SYMBOL
}

DexFile::~DexFile() { g_ExtDexFileFree(ext_dex_file_); }

MethodInfo DexFile::AbsorbMethodInfo(const ExtDexFileMethodInfo& ext_method_info) {
  return {ext_method_info.offset, ext_method_info.len, DexString(ext_method_info.name)};
}

void DexFile::AddMethodInfoCallback(const ExtDexFileMethodInfo* ext_method_info, void* ctx) {
  auto vect = static_cast<MethodInfoVector*>(ctx);
  vect->emplace_back(AbsorbMethodInfo(*ext_method_info));
}

}  // namespace dex
}  // namespace art_api
