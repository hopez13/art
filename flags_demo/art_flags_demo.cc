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

#include <stdio.h>

#include "base/macros.h"
#include "com_android_art_flags.h"

namespace art_flags = com::android::art::flags;

NO_INLINE void foo() { printf("foo\n"); }

NO_INLINE void bar() { printf("bar\n"); }

int main() {
  if (art_flags::enable_foo()) {
    foo();
  } else {
    bar();
  }
  return 0;
}
