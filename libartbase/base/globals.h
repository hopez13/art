/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_LIBARTBASE_BASE_GLOBALS_H_
#define ART_LIBARTBASE_BASE_GLOBALS_H_

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"

namespace art {

static constexpr size_t KB = 1024;
static constexpr size_t MB = KB * KB;
static constexpr size_t GB = KB * KB * KB;

// Runtime sizes.
static constexpr size_t kBitsPerByte = 8;
static constexpr size_t kBitsPerByteLog2 = 3;
static constexpr int kBitsPerIntPtrT = sizeof(intptr_t) * kBitsPerByte;

// Required stack alignment
static constexpr size_t kStackAlignment = 16;

// Minimum supported page size.
static constexpr size_t kMinPageSize = 4096;

#if defined(ART_PAGE_SIZE_AGNOSTIC)
static constexpr bool kPageSizeAgnostic = true;
// Maximum supported page size.
static constexpr size_t kMaxPageSize = 16384;
#else
static constexpr bool kPageSizeAgnostic = false;
// Maximum supported page size.
static constexpr size_t kMaxPageSize = kMinPageSize;
#endif

// Targets can have different page size (eg. 4kB or 16kB). Because Art can crosscompile, it needs
// to be able to generate OAT (ELF) and other image files with alignment other than the host page
// size. kElfSegmentAlignment needs to be equal to the largest page size supported. Effectively,
// this is the value to be used in images files for aligning contents to page size.
static constexpr size_t kElfSegmentAlignment = kMaxPageSize;

// Clion, clang analyzer, etc can falsely believe that "if (kIsDebugBuild)" always
// returns the same value. By wrapping into a call to another constexpr function, we force it
// to realize that is not actually always evaluating to the same value.
static constexpr bool GlobalsReturnSelf(bool self) { return self; }

// Whether or not this is a debug build. Useful in conditionals where NDEBUG isn't.
// TODO: Use only __clang_analyzer__ here. b/64455231
#if defined(NDEBUG) && !defined(__CLION_IDE__)
static constexpr bool kIsDebugBuild = GlobalsReturnSelf(false);
#else
static constexpr bool kIsDebugBuild = GlobalsReturnSelf(true);
#endif

#if defined(ART_PGO_INSTRUMENTATION)
static constexpr bool kIsPGOInstrumentation = true;
#else
static constexpr bool kIsPGOInstrumentation = false;
#endif

// ART_TARGET - Defined for target builds of ART.
// ART_TARGET_LINUX - Defined for target Linux builds of ART.
// ART_TARGET_ANDROID - Defined for target Android builds of ART.
// ART_TARGET_FUCHSIA - Defined for Fuchsia builds of ART.
// Note: Either ART_TARGET_LINUX, ART_TARGET_ANDROID or ART_TARGET_FUCHSIA
//       need to be set when ART_TARGET is set.
// Note: When ART_TARGET_LINUX is defined mem_map.h will not be using Ashmem for memory mappings
// (usually only available on Android kernels).
#if defined(ART_TARGET)
// Useful in conditionals where ART_TARGET isn't.
static constexpr bool kIsTargetBuild = true;
# if defined(ART_TARGET_LINUX)
static constexpr bool kIsTargetLinux = true;
static constexpr bool kIsTargetFuchsia = false;
static constexpr bool kIsTargetAndroid = false;
# elif defined(ART_TARGET_ANDROID)
static constexpr bool kIsTargetLinux = false;
static constexpr bool kIsTargetFuchsia = false;
static constexpr bool kIsTargetAndroid = true;
# elif defined(ART_TARGET_FUCHSIA)
static constexpr bool kIsTargetLinux = false;
static constexpr bool kIsTargetFuchsia = true;
static constexpr bool kIsTargetAndroid = false;
# else
# error "Either ART_TARGET_LINUX, ART_TARGET_ANDROID or ART_TARGET_FUCHSIA " \
        "needs to be defined for target builds."
# endif
#else
static constexpr bool kIsTargetBuild = false;
# if defined(ART_TARGET_LINUX)
# error "ART_TARGET_LINUX defined for host build."
# elif defined(ART_TARGET_ANDROID)
# error "ART_TARGET_ANDROID defined for host build."
# elif defined(ART_TARGET_FUCHSIA)
# error "ART_TARGET_FUCHSIA defined for host build."
# else
static constexpr bool kIsTargetLinux = false;
static constexpr bool kIsTargetFuchsia = false;
static constexpr bool kIsTargetAndroid = false;
# endif
#endif

// Additional statically-linked ART binaries (dex2oats, oatdumps, etc.) are
// always available on the host
#if !defined(ART_TARGET)
static constexpr bool kHostStaticBuildEnabled = true;
#else
static constexpr bool kHostStaticBuildEnabled = false;
#endif

<<<<<<< PATCH SET (c5cc72 Optimize access to gPageSize)
// System property for phenotype flag to test disabling compact dex and in
// particular dexlayout.
// TODO(b/256664509): Clean this up.
static constexpr char kPhDisableCompactDex[] =
    "persist.device_config.runtime_native_boot.disable_compact_dex";

// Within libart, gPageSize should be used to get the page size value once Runtime is initialized.
// For most other cases MemMap::GetPageSize() should be used instead. However, where MemMap is
// unavailable e.g. during static initialization or another stage when MemMap isn't yet initialized,
// or in a component which might operate without MemMap being initialized, the GetPageSizeSlow()
// would be generally suitable. For performance-sensitive code, GetPageSizeSlow() shouldn't be used
// without caching the value to remove repeated calls of the function.
=======
// Helper class that acts as a global constant which can be initialized with
// a dynamically computed value while not being subject to static initialization
// order issues via gating access to the value through a function which ensures
// the value is initialized before being accessed.
//
// The Initialize function should return T type. It shouldn't have side effects
// and should always return the same value.
template<typename T, auto Initialize>
struct GlobalConst {
  operator T() const {
    static T data = Initialize();
    return data;
  }
};

// Helper macros for declaring and defining page size agnostic global values
// which are constants in page size agnostic configuration and constexpr
// in non page size agnostic configuration.
//
// For the former case, this uses the GlobalConst class initializing it with given expression
// which might be the same as for the non page size agnostic configuration (then
// ART_PAGE_SIZE_AGNOSTIC_DECLARE is most suitable to avoid duplication) or might be different
// (in which case ART_PAGE_SIZE_AGNOSTIC_DECLARE_ALT should be used).
//
// The motivation behind these helpers is mainly to provide a way to declare / define / initialize
// the global constants protected from static initialization order issues.
//
// Adding a new value e.g. `const uint32_t gNewVal = function(gPageSize);` can be done,
// for example, via:
//  - declaring it using ART_PAGE_SIZE_AGNOSTIC_DECLARE in this header;
//  - and defining it with ART_PAGE_SIZE_AGNOSTIC_DEFINE in the globals_unix.cc
//    or another suitable module.
// The statements might look as follows:
//  ART_PAGE_SIZE_AGNOSTIC_DECLARE(uint32_t, gNewVal, function(gPageSize));
//  ART_PAGE_SIZE_AGNOSTIC_DEFINE(uint32_t, gNewVal);
//
// NOTE:
//      The initializer expressions shouldn't have side effects
//      and should always return the same value.

>>>>>>> BASE      (34023f Use runtime page size for large object alignment)
#ifdef ART_PAGE_SIZE_AGNOSTIC
inline ALWAYS_INLINE size_t GetPageSizeSlow() {
  static_assert(kPageSizeAgnostic, "The dynamic version is only for page size agnostic build");
  static const size_t page_size = sysconf(_SC_PAGE_SIZE);
  return page_size;
}
#else
constexpr size_t GetPageSizeSlow() {
  static_assert(!kPageSizeAgnostic, "The constexpr version is only for page size agnostic build");
  return kMinPageSize;
}
#endif

}  // namespace art

#endif  // ART_LIBARTBASE_BASE_GLOBALS_H_
