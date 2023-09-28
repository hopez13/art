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

#include <unistd.h>
#include <stdint.h>

#include "base/global_const.h"
#include "base/macros.h"

#ifndef ART_LIBARTBASE_BASE_PAGE_SIZE_AGNOSTIC_GLOBALS_H_
#define ART_LIBARTBASE_BASE_PAGE_SIZE_AGNOSTIC_GLOBALS_H_

namespace art {

// Helper macros for declaring and defining page size agnostic global values
// which are constants in page size agnostic configuration and constexpr
// in non page size agnostic configuration.
//
// For the former case, this uses the GlobalConst class initializing it with given expression
// which might be the same as for the non page size agnostic configuration (then
// ART_PAGE_SIZE_AGNOSTIC_DECLARE is most suitable to avoid duplication) or might be different
// (in which case ART_PAGE_SIZE_AGNOSTIC_DECLARE_ALT should be used).
//
// The motivation behind these helpers is mainly to provide a away to declare / define / initialize
// the global constants in a way protected from static initialization order issues.
//
// Adding a new value e.g. `const uint32_t gNewVal = function(gPageSize);` can be done via:
//  - declaring it using ART_PAGE_SIZE_AGNOSTIC_DECLARE in this header;
//  - and defining it with ART_PAGE_SIZE_AGNOSTIC_DEFINE in the page_size_agnostic_globals.cpp.
// The statements might look as follows:
//  ART_PAGE_SIZE_AGNOSTIC_DECLARE(uint32_t, gNewVal, function(gPageSize));
//  ART_PAGE_SIZE_AGNOSTIC_DEFINE(uint32_t, gNewVal);
//
// NOTE:
//      The initializer expressions shouldn't have side effects
//      and should always return the same value.

#ifdef ART_PAGE_SIZE_AGNOSTIC
// Declaration (page size agnostic version).
#define ART_PAGE_SIZE_AGNOSTIC_DECLARE_ALT(type, name, page_size_agnostic_expr, const_expr) \
  inline type EXPORT \
  name ## _Initializer(void) { \
    return (page_size_agnostic_expr); \
  } \
  extern GlobalConst<type, name ## _Initializer> name
// Definition (page size agnostic version).
#define ART_PAGE_SIZE_AGNOSTIC_DEFINE(type, name) GlobalConst<type, name ## _Initializer> name
#else
// Declaration (non page size agnostic version).
#define ART_PAGE_SIZE_AGNOSTIC_DECLARE_ALT(type, name, page_size_agnostic_expr, const_expr) \
  static constexpr type name = (const_expr)
// Definition (non page size agnostic version).
#define ART_PAGE_SIZE_AGNOSTIC_DEFINE(type, name)
#endif  // ART_PAGE_SIZE_AGNOSTIC

// ART_PAGE_SIZE_AGNOSTIC_DECLARE is same as ART_PAGE_SIZE_AGNOSTIC_DECLARE_ALT
// for the case when the initializer expressions are the same.
#define ART_PAGE_SIZE_AGNOSTIC_DECLARE(type, name, expr) \
    ART_PAGE_SIZE_AGNOSTIC_DECLARE_ALT(type, name, expr, expr)

// Declaration and definition combined.
#define ART_PAGE_SIZE_AGNOSTIC_DECLARE_AND_DEFINE(type, name, expr) \
  ART_PAGE_SIZE_AGNOSTIC_DECLARE(type, name, expr); \
  ART_PAGE_SIZE_AGNOSTIC_DEFINE(type, name)

// System page size. We check this against sysconf(_SC_PAGE_SIZE) at runtime,
// but for non page size agnostic configuration we use a simple compile-time
// constant so the compiler can generate better code.
ART_PAGE_SIZE_AGNOSTIC_DECLARE_ALT(size_t, gPageSize, sysconf(_SC_PAGE_SIZE), 4096);

// TODO: Kernels for arm and x86 in both, 32-bit and 64-bit modes use 512 entries per page-table
// page. Find a way to confirm that in userspace.
// Address range covered by 1 Page Middle Directory (PMD) entry in the page table
ART_PAGE_SIZE_AGNOSTIC_DECLARE(size_t, gPMDSize, (gPageSize / sizeof(uint64_t)) * gPageSize);
// Address range covered by 1 Page Upper Directory (PUD) entry in the page table
ART_PAGE_SIZE_AGNOSTIC_DECLARE(size_t, gPUDSize, (gPageSize / sizeof(uint64_t)) * gPMDSize);

// Returns the ideal alignment corresponding to page-table levels for the
// given size.
static inline size_t BestPageTableAlignment(size_t size) {
  return size < gPUDSize ? gPMDSize : gPUDSize;
}

}  // namespace art

#endif  // ART_LIBARTBASE_BASE_PAGE_SIZE_AGNOSTIC_GLOBALS_H_
