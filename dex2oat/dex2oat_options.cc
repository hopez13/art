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

#include "dex2oat_options.h"

#include <memory>

namespace art {

// Specify storage for the Dex2oatOptions keys.

#define DEX2OAT_OPTIONS_KEY(Type, Name, ...) const Dex2oatArgumentMap::Key<Type> Dex2oatArgumentMap::Name {__VA_ARGS__};  // NOLINT [readability/braces] [4]
#include "dex2oat_options.def"

}  // namespace art
