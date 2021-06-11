/*
 * Copyright (C) 2021 The Android Open Source Project
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

#pragma once

#include <sys/types.h>

namespace art {
namespace tools {

// This is used as a string literal, can't be constants. TODO: std::string...
#define DALVIK_CACHE "dalvik-cache"
constexpr const char* DALVIK_CACHE_POSTFIX = "@classes.dex";

constexpr size_t PKG_NAME_MAX = 128u;  /* largest allowed package name */
constexpr size_t PKG_PATH_MAX = 1024u; /* max size of any path we use */

// NOTE: keep in sync with StorageManager
constexpr int FLAG_STORAGE_DE = 1 << 0;
constexpr int FLAG_STORAGE_CE = 1 << 1;

}  // namespace tools
}  // namespace art