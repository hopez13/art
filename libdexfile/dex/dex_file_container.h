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

#ifndef ART_LIBDEXFILE_DEX_DEX_FILE_CONTAINER_H_
#define ART_LIBDEXFILE_DEX_DEX_FILE_CONTAINER_H_

#include "base/macros.h"

namespace art {

// Some instances of DexFile own the storage referred to by DexFile.  Clients who create
// such management do so by subclassing Container.
class DexFileContainer {
 public:
  DexFileContainer() { }
  virtual ~DexFileContainer() { }
  virtual int GetPermissions() = 0;
  virtual bool IsReadOnly() = 0;
  virtual bool EnableWrite() = 0;
  virtual bool DisableWrite() = 0;
  virtual const uint8_t* Begin() = 0;
  virtual size_t Size() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(DexFileContainer);
};

class EmptyDexFileContainer FINAL : public DexFileContainer {
 public:
  EmptyDexFileContainer() { }
  ~EmptyDexFileContainer() { }

  int GetPermissions() OVERRIDE { return 0; }
  bool IsReadOnly() OVERRIDE { return true; }
  bool EnableWrite() OVERRIDE { return false; }
  bool DisableWrite() OVERRIDE { return false; }
  const uint8_t* Begin() OVERRIDE { return nullptr; }
  size_t Size() OVERRIDE { return 0U; }

 private:
  DISALLOW_COPY_AND_ASSIGN(EmptyDexFileContainer);
};

class NonOwningDexFileContainer FINAL : public DexFileContainer {
 public:
  NonOwningDexFileContainer(const uint8_t* begin, size_t size, bool is_read_only = true)
      : begin_(begin), size_(size), is_read_only_(is_read_only) { }
  ~NonOwningDexFileContainer() { }

  int GetPermissions() OVERRIDE { return 0; }
  bool IsReadOnly() OVERRIDE { return is_read_only_; }
  bool EnableWrite() OVERRIDE { return false; }
  bool DisableWrite() OVERRIDE { return false; }
  const uint8_t* Begin() OVERRIDE { return begin_; }
  size_t Size() OVERRIDE { return size_; }

 private:
  const uint8_t* begin_;
  size_t size_;
  bool is_read_only_;

  DISALLOW_COPY_AND_ASSIGN(NonOwningDexFileContainer);
};

}  // namespace art

#endif  // ART_LIBDEXFILE_DEX_DEX_FILE_CONTAINER_H_
