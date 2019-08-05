// Copyright (C) 2016 The Android Open Source Project
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

// This file implements the "codegen" property to apply different properties based on the currently
// selected codegen arches, which defaults to all arches on the host and the primary and secondary
// arches on the device.

import (
	"android/soong/android"
	"sort"
	"strings"
)

func codegen(ctx android.LoadHookContext, c *codegenProperties, library bool) {
	var hostArches, deviceArches []string

	e := envDefault(ctx, "ART_HOST_CODEGEN_ARCHS", "")
	if e == "" {
		hostArches = supportedArches
	} else {
		hostArches = strings.Split(e, " ")
	}

	e = envDefault(ctx, "ART_TARGET_CODEGEN_ARCHS", "")
	if e == "" {
		deviceArches = defaultDeviceCodegenArches(ctx)
	} else {
		deviceArches = strings.Split(e, " ")
	}

	addCodegenArchProperties := func(host bool, archName string, seenSrcs map[string]string) {
		type props struct {
			Target struct {
				Android *CodegenCommonArchProperties
				Host    *CodegenCommonArchProperties
			}
		}

		type libraryProps struct {
			Target struct {
				Android *CodegenLibraryArchProperties
				Host    *CodegenLibraryArchProperties
			}
		}

		var arch *codegenArchProperties
		switch archName {
		case "arm":
			arch = &c.Codegen.Arm
		case "arm64":
			arch = &c.Codegen.Arm64
		case "mips":
			arch = &c.Codegen.Mips
		case "mips64":
			arch = &c.Codegen.Mips64
		case "x86":
			arch = &c.Codegen.X86
		case "x86_64":
			arch = &c.Codegen.X86_64
		default:
			ctx.ModuleErrorf("Unknown codegen architecture %q", archName)
			return
		}

		p := &props{}
		l := &libraryProps{}
		if host {
			p.Target.Host = filterSrcs(&arch.CodegenCommonArchProperties, seenSrcs)
			l.Target.Host = &arch.CodegenLibraryArchProperties
		} else {
			p.Target.Android = filterSrcs(&arch.CodegenCommonArchProperties, seenSrcs)
			l.Target.Android = &arch.CodegenLibraryArchProperties
		}

		ctx.AppendProperties(p)
		if library {
			ctx.AppendProperties(l)
		}
	}

	seenTargetSrcs := make(map[string]string)
	for _, arch := range deviceArches {
		addCodegenArchProperties(false /* host */, arch, seenTargetSrcs)
		if ctx.Failed() {
			return
		}
	}

	seenHostSrcs := make(map[string]string)
	for _, arch := range hostArches {
		addCodegenArchProperties(true /* host */, arch, seenHostSrcs)
		if ctx.Failed() {
			return
		}
	}
}

// Filter file names in property `Srcs` from `p` using `seenSrcs` and return the filtered result.
// Update `seenSrcs` with newly seen `Srcs` file names.
func filterSrcs(p *CodegenCommonArchProperties, seenSrcs map[string]string) *CodegenCommonArchProperties {
	var srcs []string
	for _, src := range p.Srcs {
		if _, found := seenSrcs[src]; !found {
			srcs = append(srcs, src)
			seenSrcs[src] = src
		}
	}
	return &CodegenCommonArchProperties{srcs, p.Cflags, p.Cppflags}
}

type CodegenCommonArchProperties struct {
	Srcs     []string
	Cflags   []string
	Cppflags []string
}

type CodegenLibraryArchProperties struct {
	Static struct {
		Whole_static_libs []string
	}
	Shared struct {
		Shared_libs               []string
		Export_shared_lib_headers []string
	}
}

type codegenArchProperties struct {
	CodegenCommonArchProperties
	CodegenLibraryArchProperties
}

type codegenProperties struct {
	Codegen struct {
		Arm, Arm64, Mips, Mips64, X86, X86_64 codegenArchProperties
	}
}

type codegenCustomizer struct {
	library           bool
	codegenProperties codegenProperties
}

func defaultDeviceCodegenArches(ctx android.LoadHookContext) []string {
	arches := make(map[string]bool)
	for _, a := range ctx.DeviceConfig().Arches() {
		s := a.ArchType.String()
		arches[s] = true
		if s == "arm64" {
			arches["arm"] = true
		} else if s == "mips64" {
			arches["mips"] = true
		} else if s == "x86_64" {
			arches["x86"] = true
		}
	}
	ret := make([]string, 0, len(arches))
	for a := range arches {
		ret = append(ret, a)
	}
	sort.Strings(ret)
	return ret
}

func installCodegenCustomizer(module android.Module, library bool) {
	c := &codegenProperties{}
	android.AddLoadHook(module, func(ctx android.LoadHookContext) { codegen(ctx, c, library) })
	module.AddProperties(c)
}
