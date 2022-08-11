/*
 * Copyright (C) 2022 The Android Open Source Project
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

#ifndef ART_COMPILER_COMMON_CODEGEN_TEST_H_
#define ART_COMPILER_COMMON_CODEGEN_TEST_H_

// `ART_ENABLE_CODEGEN_TEST_<arch>` macros are used to enable/disable tests for
// code generator (back end) targeting architecture `<arch>`.
//
// When building a code generator related test for target (device), only enable
// an architecture-specific test case when the test is built specifically for
// that architecture (i.e. do not cross-compile). This is in order to prevent
// ART tests built for Android dual-architecture test suites (which contain
// e.g. both Arm and Arm64 test binaries having *both* the Arm and Arm64 code
// generators enabled) from failing to link with ART artifacts built for only
// one of these architectures (e.g. Arm) on the tested platform, because they
// would not have the code generator for the other architecture (e.g. Arm64).
//
// On host, rely on the `ART_ENABLE_CODEGEN_TEST_<arch>` macros, as we normally
// build code generators for all architectures supported by ART.

#ifdef ART_TARGET
# ifdef __arm__
#  define ART_ENABLE_CODEGEN_TEST_arm
#  ifndef ART_ENABLE_CODEGEN_arm
#   error "Arm code generator not enabled for test built for Arm target build"
#  endif
# endif
# ifdef __aarch64__
#  define ART_ENABLE_CODEGEN_TEST_arm64
#  ifndef ART_ENABLE_CODEGEN_arm64
#   error "Arm64 code generator not enabled for test built for Arm64 target build"
#  endif
# endif
# ifdef __i386__
#  define ART_ENABLE_CODEGEN_TEST_x86
#  ifndef ART_ENABLE_CODEGEN_x86
#   error "x86 code generator not enabled for test built for x86 target build"
#  endif
# endif
# ifdef __x86_64__
#  define ART_ENABLE_CODEGEN_TEST_x86_64
#  ifndef ART_ENABLE_CODEGEN_x86_64
#   error "x86-64 code generator not enabled for test built for x86-64 target build"
#  endif
# endif
#else
# ifdef ART_ENABLE_CODEGEN_arm
#  define ART_ENABLE_CODEGEN_TEST_arm
# endif
# ifdef ART_ENABLE_CODEGEN_arm64
#  define ART_ENABLE_CODEGEN_TEST_arm64
# endif
# ifdef ART_ENABLE_CODEGEN_x86
#  define ART_ENABLE_CODEGEN_TEST_x86
# endif
# ifdef ART_ENABLE_CODEGEN_x86_64
#  define ART_ENABLE_CODEGEN_TEST_x86_64
# endif
#endif

#endif  // ART_COMPILER_COMMON_CODEGEN_TEST_H_
