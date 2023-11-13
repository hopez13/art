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

#include <android-base/logging.h>
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

// System property for phenotype flag to test disabling compact dex and in
// particular dexlayout.
// TODO(b/256664509): Clean this up.
static constexpr char kPhDisableCompactDex[] =
    "persist.device_config.runtime_native_boot.disable_compact_dex";

// This is a helper class intended to support declaring dynamically initialized global constants
// in a way that ensures no static initialization order issues while not introducing a dynamic
// check on each access to the constant.
//
// An alternative implementation would have been implementing access to the constants via
// a function that would check if the initialization was complete, and if it wasn't then the
// initialization would be performed before the function returns.
//
// However, with the implemented approach, the initialization happens early when the DSO (or static
// binary) is loaded into the process - via the init_array. This way the conditional can be removed
// everywhere the constants are accessed.
//
// Potential issue with the approach is connected with static initialization order being
// unspecified. Where a global constant depends on another one for initialization, it is important
// to ensure that dependent global constant is initialized after the constant it needs. However, by
// default the order isn't guaranteed if the values are defined in different compilation units. The
// `init_priority` compiler attribute allows to specify particular priority of initialization for
// each value, therefore ensuring the values are initialized in the correct order. This attribute
// is used in these changes to prevent the static initialization order issues.
//
// However, the `init_priority` attribute only allows to specify priorities within a DSO (or a
// static binary) but not across libraries. To ensure there is no initialization order issues
// between libraries, we duplicate the GlobalConst values into each library requiring it.
// Specifically, for gPageSize, gPMDSize, gPUDSize that's accomplished via introducing
// libartbase_constglobals helper static library which is linked into every library or binary
// requiring these values. This is a valid approach because the values are constants and are always
// the same for each of the libraries, so duplicating them isn't going to cause any issue unless
// some component would attempt relying on a particular address of the constant values being always
// the same.
//
// It is worth noting that the initialiation with this approach is accomplished in a
// single-threaded mode so there is no need to handle multiple threads attempting to initialize
// values at the same time.
//
// The GlobalConstManager isn't strictly speaking required as its only function is to help
// asserting that the requested order of initialization is followed at run time.
class GlobalConstManager {
public:
  friend class GlobalConst;
  friend class InitializeStart;
  friend class InitializeFinal;

  // Helper class that acts as a global constant which can be initialized with
  // a dynamically computed value while not being subject to static initialization
  // order issues via gating access to the value through a function which ensures
  // the value is initialized before being accessed.
  //
  // The Initialize function should return T type. It shouldn't have side effects
  // and should always return the same value.
  //
  // It can also implement the value as constexpr if the Initialize function is constexpr.
  // This can be requested via IsConstExpr.
  template<typename T, auto Initialize, int Priority, bool IsConstExpr>
  class GlobalConst {};

  // The dynamic version works with the GlobalConstManager to ensure it is
  // initialized when expected according to the priority requested.
  //
  // The Priority should match the "init_priority" attribute's value at the
  // definition of the corresponding global constant value.
  template<typename T, auto Initialize, int Priority>
  class GlobalConst<T, Initialize, Priority, false> {
    // Wrapper over the Initialize function which communicates to GlobalConstManager that
    // initialization is happening, from which compilation unit and with which priority.
    static const T InitializeInternal(const char *compilation_unit) {
      GlobalConstManager::SetModuleAndPriority(Priority, compilation_unit);
      T value = Initialize();
      GlobalConstManager::ResetModuleAndPriority();

      return value;
    }

    public:
      // Constructor of the constant object.
      // Initializes the object with the expected value as per the Initialize function and saves
      // the name of the compilation unit where it is defined for subsequent checks.
      GlobalConst(const char *compilation_unit)
          : data_(InitializeInternal(compilation_unit)), compilation_unit_(compilation_unit) {}

      // Normal accessor of the value of the constant - available once the
      // overall constants initialization is complete as decided by the
      // GlobalConstManager.
      operator T() const {
        GlobalConstManager::AssertInitializationComplete();
        DCHECK(IsInitialized());

        return data_;
      }

      // "Early" accessor of the value of the constant - available during the
      // constants initialization process as decided by the GlobalConstManager.
      T getEarly() const {
        GlobalConstManager::EnsureAllowedOrder(Priority, compilation_unit_);
        CHECK(IsInitialized());

        return data_;
      }

    private:
      // If the value is initialized, the compilation unit name is set.
      bool IsInitialized() const {
        return compilation_unit_ != nullptr;
      }

      const T data_; // value of the constant
      const char *compilation_unit_; // name of the compilation unit defining it
  };

  // The constexpr version reflects interfaces of the dynamic version, however
  // doesn't do any checks as it is supposed to declare constexpr versions only
  // where the initialization is happening at compile-time.
  template<typename T, auto Initialize, int Priority>
  class GlobalConst<T, Initialize, Priority, true> {
    public:
      constexpr GlobalConst() : data_(Initialize()) {}

      constexpr operator T() const {
        return data_;
      }

      constexpr T getEarly() const {
        return data_;
      }

    private:
      const T data_;
  };

  // init_priority attribute allows to choose priority for initialization each global within a DSO
  // or static binary. The lower the value the higher the priority is. Range of values available
  // for general software is 101 to 65535, where 101 corresponds to the highest priority and 65535
  // corresponds to the lowest priority which is also default for any variables defined without
  // using the init_priority attribute.
  //
  // kInitStartPriority is the priority chosen for the GlobalConstManager to declare that the
  // constants initialization is started.
  //
  // kPriority1 is the first priority available for assigning to GlobalConst objects.
  //
  // Subsequent kPriority2 etc. can be used for constants which should be initialized later. Choice
  // of the priorities should be based on dependencies between the values of the constants and
  // generally speaking priority chosen for a constant should be the maximum of priority values
  // among constants it depends on for initialization.
  //
  // However, within the same compilation unit, the priority can be the same as the order will be
  // defined according to the order of definition of the constants.
  //
  // kInitFinalPriority is the priority chosen for the GlobalConstManager to
  // declare that the constants initialization is finished.
  static constexpr int kInitStartPriority = 101;
  enum {
    kPriority1 = kInitStartPriority + 1,
    kPriority2,
    kInitFinalPriority,
  };
  static constexpr int kLowestPriority = 65535;
  static_assert(kInitFinalPriority < kLowestPriority);

  // Helper class declaring the initialization is started - defined with
  // kInitStartPriority.
  class InitializeStart {
    public:
      InitializeStart() {
        GlobalConstManager::current_priority = kLowestPriority;
        GlobalConstManager::current_compilation_unit = nullptr;
        GlobalConstManager::initialization_complete = false;
      }
  };

  // Helper class declaring the initialization is finished - defined with
  // kInitFinalPriority.
  class InitializeFinal {
    public:
      InitializeFinal() {
        DCHECK(GlobalConstManager::current_priority == kLowestPriority);
        DCHECK(GlobalConstManager::current_compilation_unit == nullptr);
        GlobalConstManager::initialization_complete = true;
      }
  };

private:
  // Helper to check that no other GlobalConst is currently being initialized,
  // and then to declare initialization of a GlobalConst from specified compilation unit
  // with specified priority.
  static void SetModuleAndPriority(int priority, const char *compilation_unit) {
    CHECK(!initialization_complete) << compilation_unit;
    // No nested initialization is supported (and it isn't required).
    CHECK(current_priority == kLowestPriority);
    CHECK(current_compilation_unit == nullptr);

    current_priority = priority;
    current_compilation_unit = compilation_unit;
  }

  // Helper to reset the state declaring that initialization of a GlobalConst is finished.
  static void ResetModuleAndPriority() {
    CHECK(!initialization_complete);
    CHECK(current_priority < kLowestPriority);
    CHECK(current_compilation_unit != nullptr);

    current_priority = kLowestPriority;
    current_compilation_unit = nullptr;
  }

  // Helper to ensure that accesses to global values are accomplished according to declared
  // priorities i.e. either priority of accessed global is higher than of the currently initialized
  // one or they're defined in the same compilation unit.
  static void EnsureAllowedOrder(int accessed_priority, const char *accessed_compilation_unit) {
    CHECK(!initialization_complete);
    CHECK(current_compilation_unit != nullptr);
    CHECK(accessed_priority < current_priority
          || (accessed_priority == current_priority
              && !strcmp(accessed_compilation_unit, current_compilation_unit)))
            << accessed_compilation_unit << current_compilation_unit
            << accessed_priority << current_priority;
  }

  // Assert that overall constants initialization is complete according to GlobalConstManager.
  static void AssertInitializationComplete() {
    DCHECK(initialization_complete);
  }

  static int current_priority;
  static const char* current_compilation_unit;
  static bool initialization_complete;
};

// Helper macros for declaring and defining page size agnostic global values which are constants
// in page size agnostic configuration and constexpr in non page size agnostic configuration.
//
// For both cases, this uses the GlobalConst class initializing it with given expression which
// should be the same as for both configurations. kPageSizeAgnostic can be used as a conditional
// in the expression to distinguish between the two modes.
//
// The motivation behind these helpers is mainly to provide a way to declare / define / initialize
// the global constants protected from static initialization order issues.
//
// Adding a new value e.g. `const uint32_t gNewVal = function(gPageSize);` can be done,
// for example, via:
//  - declaring it using ART_PAGE_SIZE_AGNOSTIC_DECLARE in this header;
//  - and defining it with ART_PAGE_SIZE_AGNOSTIC_DEFINE in the const_globals.cc
//    or another suitable compilation unit.
// The statements might look as follows:
//  ART_PAGE_SIZE_AGNOSTIC_DECLARE(uint32_t, gNewVal, function(gPageSize));
//  ART_PAGE_SIZE_AGNOSTIC_DEFINE(uint32_t, gNewVal);
//
// NOTE:
//      The initializer expressions shouldn't have side effects
//      and should always return the same value.

#ifdef ART_PAGE_SIZE_AGNOSTIC
// Declaration (page size agnostic version).
#define ART_PAGE_SIZE_AGNOSTIC_DECLARE(type, name, expr, initialization_order) \
  inline type HIDDEN name ## _Initializer(void) { return (expr); } \
  static constexpr int name ## _Priority = initialization_order; \
  static_assert(name ## _Priority < GlobalConstManager::kInitFinalPriority, ""); \
  extern const \
    GlobalConstManager::GlobalConst<type, name ## _Initializer, name ## _Priority, false> name
// Definition (page size agnostic version).
#define ART_PAGE_SIZE_AGNOSTIC_DEFINE(type, name) \
    const GlobalConstManager::GlobalConst<type, name ## _Initializer, name ## _Priority, false> \
    name HIDDEN INIT_PRIORITY(name ## _Priority) (__builtin_FILE())
#else
// Declaration (non page size agnostic version).
#define ART_PAGE_SIZE_AGNOSTIC_DECLARE(type, name, expr, initialization_order) \
  constexpr type name ## _Initializer(void) { return (expr); } \
  static constexpr GlobalConstManager::GlobalConst<type, name ## _Initializer, 0, true> name
// Definition (non page size agnostic version).
#define ART_PAGE_SIZE_AGNOSTIC_DEFINE(type, name)
#endif  // ART_PAGE_SIZE_AGNOSTIC

// Declaration and definition combined.
#define ART_PAGE_SIZE_AGNOSTIC_DECLARE_AND_DEFINE(type, name, expr, initialization_order) \
  ART_PAGE_SIZE_AGNOSTIC_DECLARE(type, name, expr, initialization_order); \
  ART_PAGE_SIZE_AGNOSTIC_DEFINE(type, name)

// System page size. We check this against sysconf(_SC_PAGE_SIZE) at runtime,
// but for non page size agnostic configuration we use a simple compile-time
// constant so the compiler can generate better code.
ART_PAGE_SIZE_AGNOSTIC_DECLARE(size_t, gPageSize,
                               kPageSizeAgnostic ? sysconf(_SC_PAGE_SIZE) : kMinPageSize,
                               GlobalConstManager::kPriority1);

// TODO: Kernels for arm and x86 in both, 32-bit and 64-bit modes use 512 entries per page-table
// page. Find a way to confirm that in userspace.
// Address range covered by 1 Page Middle Directory (PMD) entry in the page table
ART_PAGE_SIZE_AGNOSTIC_DECLARE(size_t, gPMDSize,
                               (gPageSize.getEarly() / sizeof(uint64_t)) * gPageSize.getEarly(),
                               GlobalConstManager::kPriority1);
// Address range covered by 1 Page Upper Directory (PUD) entry in the page table
ART_PAGE_SIZE_AGNOSTIC_DECLARE(size_t, gPUDSize,
                               (gPageSize.getEarly() / sizeof(uint64_t)) * gPMDSize.getEarly(),
                               GlobalConstManager::kPriority1);

// Returns the ideal alignment corresponding to page-table levels for the
// given size.
static inline size_t BestPageTableAlignment(size_t size) {
  return size < gPUDSize ? gPMDSize : gPUDSize;
}

}  // namespace art

#endif  // ART_LIBARTBASE_BASE_GLOBALS_H_
