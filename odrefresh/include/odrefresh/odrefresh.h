/*
 * Copyright (C) 2020 The Android Open Source Project
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

#ifndef ART_ODREFRESH_INCLUDE_ODREFRESH_ODREFRESH_H_
#define ART_ODREFRESH_INCLUDE_ODREFRESH_ODREFRESH_H_

#include <sysexits.h>

namespace art {
namespace odrefresh {

//
// Exit codes from the odrefresh process (in addtion to standard exit codes in sysexits.h).
//
// NB if odrefresh crashes the caller should not sign any artifacts on the filesystem and should
// remove unsigned artifacts it finds under /data/misc/apexdata/com.android.art/system/framework.
//
enum ExitCode {
  // No compilation required, all artifacts look good or there is insufficient space to compile.
  // For ART APEX in the system image, there may be no artifacts present under
  // /data/misc/apexdata/com.android.art/system/framework.
  kOkay = EX_OK,

  // Compilation required. Re-run program with --compile on the command-line to generate
  // new artifacts under /data/misc/apexdata/com.android.art/system/framework.
  kCompilationRequired = 1,

  // Compilation failed. Any artifacts in /data/misc/apexdata/com.android.art/system/framework
  // will be valid. This may happen if compilation of boot extensions succeeds, but system_server
  // jars fails.
  kCompilationFailed = 2,
};

static_assert(ExitCode::kOkay < EX__BASE);
static_assert(ExitCode::kCompilationFailed < EX__BASE);

}  // namespace odrefresh
}  // namespace art

#endif  // ART_ODREFRESH_INCLUDE_ODREFRESH_ODREFRESH_H_
