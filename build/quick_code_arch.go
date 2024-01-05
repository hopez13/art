// Copyright (C) 2023 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package art

//
// This file implements the 'quick_code_arch' property which is used to add architecture specific
// sources to their respective arch field. This acts as a wrapper for the "arch" property which can
// include additional (e.g: arm64_on_x86_64) fields with special setups without needing to add
// additional architectures in Soong itself.
//
// This property copies across all properties it contains, into its respective arch property, e.g:
//
// quick_code_arch: {
//    arm64: {
//      srcs: [
//        "arch/arm64/context_arm64.cc",
//      ],
//    },
// },
//
// Gets copied to:
//
// arch: {
//    arm64: {
//      srcs: [
//        "arch/arm64/context_arm64.cc"
//      ]
//    }
// }
//
// Note that additional arches (e.g: arm64_on_x86_64) supported by this property which don't have
// respective fields in "arch" to copy to, may perform their own logic to decide which arches to
// copy their sources to.
//
// At this stage Soong is unaware of which architecture we are building for so we always copy
// across all supported quick_code_arch architectures into the "arch" property and let Soong handle
// which architectures should actually be built.
//

import (
  "android/soong/android"
)

type archProperty struct {
  Arch struct {
    Arm *quickCodeArchArchProperties
    Arm64 *quickCodeArchArchProperties
    Riscv64 *quickCodeArchArchProperties
    X86 *quickCodeArchArchProperties
    X86_64 *quickCodeArchArchProperties
  }
}

// Make an 'arches' property from 'quick_code_arch' and a mapping which directs how the properties
// should be copied across.
func MakeArchesFromQuickCodeArch(
    ctx android.LoadHookContext,
    c *quickCodeArchProperties,
    arch_mapping map[string]*quickCodeArchArchProperties) *archProperty {
  archProps := &archProperty{}

  // Go through each architecture and copy its properties into the corresponding arch field.
  for to, from := range arch_mapping {
    switch to {
    case "arm":
      archProps.Arch.Arm = from
    case "arm64":
      archProps.Arch.Arm64 = from
    case "riscv64":
      archProps.Arch.Riscv64 = from
    case "x86":
      archProps.Arch.X86 = from
    case "x86_64":
      archProps.Arch.X86_64 = from
    default:
      ctx.ModuleErrorf("Unknown 'arch' architecture %q", to)
    }
  }

  return archProps
}

// Implements the quick_code_arch property by copying its contained properties to the 'arch'
// property.
func quick_code_arch(ctx android.LoadHookContext, c *quickCodeArchProperties, t moduleType) {
  type sourceProps struct {
    Arch struct {
      Arm *quickCodeArchArchProperties
      Arm64 *quickCodeArchArchProperties
      Riscv64 *quickCodeArchArchProperties
      X86 *quickCodeArchArchProperties
      X86_64 *quickCodeArchArchProperties
    }
  }

  arches := map[string]*quickCodeArchArchProperties{}

  if (ctx.Config().IsEnvTrue("ART_USE_SIMULATOR")) {
    arches = map[string]*quickCodeArchArchProperties{"arm": &c.Quick_code_arch.Arm,
                                                   "arm64": &c.Quick_code_arch.Arm64,
                                                   "riscv64": &c.Quick_code_arch.Riscv64,
                                                   "x86": &c.Quick_code_arch.X86,
                                                   "x86_64": &c.Quick_code_arch.Arm64_on_x86_64}
  } else {
    arches = map[string]*quickCodeArchArchProperties{"arm": &c.Quick_code_arch.Arm,
                                                   "arm64": &c.Quick_code_arch.Arm64,
                                                   "riscv64": &c.Quick_code_arch.Riscv64,
                                                   "x86": &c.Quick_code_arch.X86,
                                                   "x86_64": &c.Quick_code_arch.X86_64}
  }

  archProps := MakeArchesFromQuickCodeArch(ctx, c, arches)
  ctx.AppendProperties(archProps)
}

// The properties to copy from the architecture specific quick_code_arch property.
type quickCodeArchArchProperties struct {
  Srcs []string
}

// Defines the quick_code_arch property.
type quickCodeArchProperties struct {
  Quick_code_arch struct {
    Arm, Arm64, Riscv64, X86, X86_64, Arm64_on_x86_64 quickCodeArchArchProperties
  }
}

// Install the quick_code_arch property to a module.
func installQuickCodeArchCustomizer(module android.Module, t moduleType) {
  c := &quickCodeArchProperties{}
  android.AddLoadHook(module, func(ctx android.LoadHookContext) { quick_code_arch(ctx, c, t) })
  module.AddProperties(c)
}
