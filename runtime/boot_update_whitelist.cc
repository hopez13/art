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

#ifndef ART_RUNTIME_AOT_CLASS_LINKER_H_
#define ART_RUNTIME_AOT_CLASS_LINKER_H_

#include <set>
#include <string>

#include "art_field-inl.h"
#include "boot_update_whitelist.h"
#include "class_linker.h"
#include "mirror/class-inl.h"
#include "runtime.h"

namespace art {

namespace transaction {

const std::set<std::string> whilte_list_ {
    "android.os.Build.SERIA",
    "java.lang.System.out",
    "java.lang.System.err",
    "android.icu.impl.TimeZoneNamesImpl$ZNames"
        ".-android-icu-text-TimeZoneNames$NameTypeSwitchesValues"
};

bool IsWhitelisted(const std::string& klass_name) {
  return whilte_list_.count(klass_name);
}

void BootChangeMonitor(ArtField* f) {
  if (f->IsFinal() &&
      f->IsStatic() &&
      !Runtime::Current()->GetClassLinker()->IsBootCompromised() &&
      f->GetDeclaringClass()->IsBootStrapClassLoaded() &&
      !IsWhitelisted(f->PrettyField(false))) {
    Runtime::Current()->GetClassLinker()->SetBootCompromised();
  }
}

}
}  // namespace art

#endif  // ART_RUNTIME_AOT_CLASS_LINKER_H_
