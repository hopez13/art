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

#ifndef ART_RUNTIME_ENTRYPOINTS_ENTRYPOINT_ASM_CONSTANTS_H_
#define ART_RUNTIME_ENTRYPOINTS_ENTRYPOINT_ASM_CONSTANTS_H_

// Reserved area on stack for art_quick_generic_jni_trampoline:
//      4    local state ref
//      4    padding
//   4196    4k scratch space, enough for 2x 256 8-byte parameters (TODO: handle scope overhead?)
//     16    handle scope member fields?
// +  112    14x 8-byte stack-2-register space
// ------
//   4332
// 16-byte aligned: 4336
// Note: 14x8 = 7*16, so the stack stays aligned for the native call...
//       Also means: the padding is somewhere in the middle
//
// Round up to 5k = 5120
#define GENERIC_JNI_TRAMPOLINE_RESERVED_AREA 5120

#endif  // ART_RUNTIME_ENTRYPOINTS_ENTRYPOINT_ASM_CONSTANTS_H_
