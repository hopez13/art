/*
 * Copyright (C) 2019 The Android Open Source Project
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

#ifndef ART_LIBARTPALETTE_INCLUDE_PALETTE_PALETTE_METHOD_LIST_H_
#define ART_LIBARTPALETTE_INCLUDE_PALETTE_PALETTE_METHOD_LIST_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "jni.h"

#define PALETTE_METHOD_LIST(M)                                                                    \
  /* Methods in version 1 API, corresponding to SDK level 31. */                                  \
  M(31, PaletteSchedSetPriority, int32_t tid, int32_t java_priority)                              \
  M(31, PaletteSchedGetPriority, int32_t tid, /*out*/ int32_t* java_priority)                     \
  M(31, PaletteWriteCrashThreadStacks, const char* stacks, size_t stacks_len)                     \
  M(31, PaletteTraceEnabled, /*out*/ bool* enabled)                                               \
  M(31, PaletteTraceBegin, const char* name)                                                      \
  M(31, PaletteTraceEnd)                                                                          \
  M(31, PaletteTraceIntegerValue, const char* name, int32_t value)                                \
  M(31, PaletteAshmemCreateRegion, const char* name, size_t size, int* fd)                        \
  M(31, PaletteAshmemSetProtRegion, int, int)                                                     \
  /* Create the staging directory for on-device signing.           */                             \
  /* `staging_dir` is updated to point to a constant string in the */                             \
  /* palette implementation.                                       */                             \
  /* This method is not thread-safe.                               */                             \
  M(31, PaletteCreateOdrefreshStagingDirectory, /*out*/ const char** staging_dir)                 \
  M(31, PaletteShouldReportDex2oatCompilation, bool*)                                             \
  M(31, PaletteNotifyStartDex2oatCompilation, int source_fd, int art_fd, int oat_fd, int vdex_fd) \
  M(31, PaletteNotifyEndDex2oatCompilation, int source_fd, int art_fd, int oat_fd, int vdex_fd)   \
  M(31, PaletteNotifyDexFileLoaded, const char* path)                                             \
  M(31, PaletteNotifyOatFileLoaded, const char* path)                                             \
  M(31, PaletteShouldReportJniInvocations, bool*)                                                 \
  M(31, PaletteNotifyBeginJniInvocation, JNIEnv* env)                                             \
  M(31, PaletteNotifyEndJniInvocation, JNIEnv* env)                                               \
  /* Methods in version 2 API, corresponding to SDK level 33. */                                  \
  M(33,                                                                                           \
    PaletteReportLockContention,                                                                  \
    JNIEnv* env,                                                                                  \
    int32_t wait_ms,                                                                              \
    const char* filename,                                                                         \
    int32_t line_number,                                                                          \
    const char* method_name,                                                                      \
    const char* owner_filename,                                                                   \
    int32_t owner_line_number,                                                                    \
    const char* owner_method_name,                                                                \
    const char* proc_name,                                                                        \
    const char* thread_name)                                                                      \
  /* Methods in version 3 API, corresponding to SDK level 34. */                                  \
  M(34,                                                                                           \
    PaletteSetTaskProfiles,                                                                       \
    int32_t tid,                                                                                  \
    const char* profiles[],                                                                       \
    size_t profiles_len,                                                                          \
    bool use_fd_cache)

#endif  // ART_LIBARTPALETTE_INCLUDE_PALETTE_PALETTE_METHOD_LIST_H_
