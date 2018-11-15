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

#ifndef ART_LIBDEXFILE_EXTERNAL_INCLUDE_ART_API_DEX_FILE_H_
#define ART_LIBDEXFILE_EXTERNAL_INCLUDE_ART_API_DEX_FILE_H_

// Dex file external API

#include <sys/types.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <android-base/macros.h>

extern "C" {

// This is the stable C ABI that backs art_api::dex below. Structs and functions
// may only be added here.
//
// Clients should use the C++ wrappers in art_api::dex instead.

// Opaque wrapper for a string allocated in libdexfile which must be freed using
// ExtDexFileFreeString.
typedef void ExtDexFileString;

const char* ExtDexFileGetString(const ExtDexFileString* ext_string, size_t* size);

void ExtDexFileFreeString(const ExtDexFileString* ext_string);

struct ExtDexFileMethodInfo {
  int32_t offset;
  int32_t len;
  const ExtDexFileString* name;
};

typedef void ExtDexFile;

bool ExtDexFileOpenFromMemory(ExtDexFile** ext_dex_file, const void* addr, size_t* size,
                              const char* location, const ExtDexFileString** error_msg);

bool ExtDexFileOpenFromFd(ExtDexFile** ext_dex_file, int fd, off_t offset, const char* location,
                          const ExtDexFileString** error_msg);

bool ExtDexFileGetMethodInfoForOffset(ExtDexFile* ext_dex_file, int64_t dex_offset,
                                      ExtDexFileMethodInfo* method_info);

typedef void ExtDexFileMethodInfoCb(const ExtDexFileMethodInfo* ext_method_info, void* ctx);

void ExtDexFileGetAllMethodInfos(ExtDexFile* ext_dex_file, bool with_signature,
                                 ExtDexFileMethodInfoCb* method_info_cb, void* ctx);

void ExtDexFileFree(ExtDexFile* ext_dex_file);

}  // extern "C"

namespace art_api {
namespace dex {

struct MethodInfo {
  int32_t offset;  // Offset relative to the start of the dex file header.
  int32_t len;
  std::string name;
};

inline bool operator==(const MethodInfo& s1, const MethodInfo& s2) {
  return s1.offset == s2.offset && s1.len == s2.len && s1.name == s2.name;
}

// External stable API to access ordinary dex files and CompactDex. This wraps
// the stable C ABI and handles instance ownerships. Thread-compatible but not
// thread-safe.
class DexFile {
 public:
  DexFile(DexFile&&) = default;
  virtual ~DexFile();

  // Interpretes a chunk of memory as a dex file. As long as *size is too small,
  // returns nullptr, sets *size to a new size to try again with, and sets
  // *error_msg to "". That might happen repeatedly. Also returns nullptr
  // on error in which case *error_msg is set to a nonempty string.
  //
  // location is a string that describes the dex file, and is preferably its
  // path. It is mostly used to make error messages better, and may be "".
  //
  // The caller must retain the memory.
  static std::unique_ptr<DexFile> OpenFromMemory(const void* addr, size_t* size,
                                                 const std::string& location,
                                                 std::string* error_msg) {
    ExtDexFile* ext_dex_file;
    const ExtDexFileString* ext_error_msg = nullptr;
    if (ExtDexFileOpenFromMemory(&ext_dex_file, addr, size, location.c_str(), &ext_error_msg)) {
      return std::unique_ptr<DexFile>(new DexFile(ext_dex_file));
    }
    if (ext_error_msg == nullptr) {
      *error_msg = "";
    } else {
      *error_msg = ConvertString(ext_error_msg);
      ExtDexFileFreeString(ext_error_msg);
    }
    return nullptr;
  }

  // mmaps the given file offset in the open fd and reads a dexfile from there.
  // Returns nullptr on error in which case *error_msg is set.
  //
  // location is a string that describes the dex file, and is preferably its
  // path. It is mostly used to make error messages better, and may be "".
  static std::unique_ptr<DexFile> OpenFromFd(int fd, off_t offset, const std::string& location,
                                             std::string* error_msg) {
    ExtDexFile* ext_dex_file;
    const ExtDexFileString* ext_error_msg = nullptr;
    if (ExtDexFileOpenFromFd(&ext_dex_file, fd, offset, location.c_str(), &ext_error_msg)) {
      return std::unique_ptr<DexFile>(new DexFile(ext_dex_file));
    }
    *error_msg = ConvertString(ext_error_msg);
    ExtDexFileFreeString(ext_error_msg);
    return nullptr;
  }

  // Given an offset relative to the start of the dex file header, if there is a
  // method whose instruction range includes that offset then returns info about
  // it, otherwise returns a struct with offset == 0.
  MethodInfo GetMethodInfoForOffset(int64_t dex_offset) {
    ExtDexFileMethodInfo ext_method_info;
    if (ExtDexFileGetMethodInfoForOffset(ext_dex_file_, dex_offset, &ext_method_info)) {
      return AbsorbMethodInfo(std::move(ext_method_info));
    }
    MethodInfo res;
    res.offset = 0;
    return res;
  }

  // Returns info structs about all methods in the dex file. MethodInfo.name
  // receives the full function signature if with_signature is set, otherwise it
  // gets the class and method name only.
  std::vector<MethodInfo> GetAllMethodInfos(bool with_signature = true) {
    MethodInfoVector res;
    ExtDexFileGetAllMethodInfos(ext_dex_file_, with_signature, AddMethodInfo,
                                static_cast<void*>(&res));
    return res;
  }

 private:
  explicit DexFile(ExtDexFile* ext_dex_file) : ext_dex_file_(ext_dex_file) {}
  ExtDexFile* ext_dex_file_;  // Owned instance.

  typedef std::vector<MethodInfo> MethodInfoVector;

  static std::string ConvertString(const ExtDexFileString* ext_string);
  static MethodInfo AbsorbMethodInfo(ExtDexFileMethodInfo&& ext_method_info);
  static void AddMethodInfo(const ExtDexFileMethodInfo* ext_method_info, void* ctx);

  DISALLOW_COPY_AND_ASSIGN(DexFile);
};

}  // namespace dex
}  // namespace art_api

#endif  // ART_LIBDEXFILE_EXTERNAL_INCLUDE_ART_API_DEX_FILE_H_
