/*
 * Copyright (C) 2023 The Android Open Source Project
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

#ifndef ART_LIBARTBASE_BASE_DEBUGSTORE_H_
#define ART_LIBARTBASE_BASE_DEBUGSTORE_H_

#include <sstream>
#include <string>

#include "palette/palette.h"

#define STORE_MAX_SIZE 1024

namespace art {

inline bool DebugStoreEnabled() {
  bool enabled = false;
  return PaletteDebugStoreIsDebugStoreEnabled(&enabled) == PALETTE_STATUS_OK && enabled;
}

inline std::string DebugStoreGetString() {
  char result[STORE_MAX_SIZE];
  PaletteDebugStoreGetString(result, STORE_MAX_SIZE);
  return std::string(result);
}

}  // namespace art

#endif  // ART_LIBARTBASE_BASE_DEBUGSTORE_H_
