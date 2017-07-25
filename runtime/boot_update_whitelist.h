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

#ifndef ART_RUNTIME_BOOT_UPDATE_WHITELIST_H_
#define ART_RUNTIME_BOOT_UPDATE_WHITELIST_H_

#include <string>

#include "art_field.h"
#include "base/mutex.h"

namespace art {

namespace transaction {

ALWAYS_INLINE void BootChangeMonitor(ArtField* f) REQUIRES_SHARED(Locks::mutator_lock_);
ALWAYS_INLINE bool IsWhitelisted(const std::string& klass_name);

}
}  // namespace art

#endif  // ART_RUNTIME_BOOT_UPDATE_WHITELIST_H_
