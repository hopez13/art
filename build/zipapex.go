// Copyright (C) 2018 The Android Open Source Project
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

import (
	"android/soong/android"
	"android/soong/cc"
	"log"
	"sort"
	"strings"

	// "fmt"

	// "sync"
	"path/filepath"

	"github.com/google/blueprint"
	// "github.com/google/blueprint/proptools"
)

var (
	zipApex = pctx.AndroidStaticRule("zipApex",
		blueprint.RuleParams{
			Command: `rm -rf ${apex_dir} && mkdir -p ${apex_dir} && ` +
				`(${copy_commands}) && ` +
				`${soong_zip} -o ${out} -C ${apex_dir} -D ${apex_dir} `,
			CommandDeps: []string{"${soong_zip}"},
			Description: "ZIPAPEX ${apex_dir} => ${out}",
		},
		"apex_dir", "copy_commands")
)

type dependencyTag struct {
	blueprint.BaseDependencyTag
	name string
}

var apexSuffix = ".zip"

var (
	sharedLibTag  = dependencyTag{name: "sharedLib"}
	executableTag = dependencyTag{name: "executable"}
)

func initZipApex() {
	pctx.Import("android/soong/common")
	pctx.HostBinToolVariable("soong_zip", "soong_zip")
}

type artZipApexProperties struct {
	// List of native executables to include
	Binaries []string

	// List of native shared libraries to include
	Native_shared_libs []string

	Multilib struct {
		First struct {
			// List of native libraries whose compile_multilib is "first"
			Native_shared_libs []string
			// List of native executables whose compile_multilib is "first"
			Binaries []string
		}
		Both struct {
			// List of native libraries whose compile_multilib is "both"
			Native_shared_libs []string
			// List of native executables whose compile_multilib is "both"
			Binaries []string
		}
		Prefer32 struct {
			// List of native libraries whose compile_multilib is "prefer32"
			Native_shared_libs []string
			// List of native executables whose compile_multilib is "prefer32"
			Binaries []string
		}
		Lib32 struct {
			// List of native libraries whose compile_multilib is "32"
			Native_shared_libs []string
			// List of native executables whose compile_multilib is "32"
			Binaries []string
		}
		Lib64 struct {
			// List of native libraries whose compile_multilib is "64"
			Native_shared_libs []string
			// List of native executables whose compile_multilib is "64"
			Binaries []string
		}
	}
}

type zipApexFile struct {
	builtFile  android.Path
	moduleName string
	archType   android.ArchType
	installDir string
	binary     bool
}
type artZipApex struct {
	android.ModuleBase
	android.DefaultableModuleBase

	properties artZipApexProperties
}

func artZipApexFactory() android.Module {
	module := &artZipApex{}
	module.AddProperties(&module.properties)
	module.Prefer32(func(ctx android.BaseModuleContext, base *android.ModuleBase,
		class android.OsClass) bool {
		return class == android.Device && ctx.Config().DevicePrefer32BitExecutables()
	})
	// Mark as multilib first
	android.InitAndroidMultiTargetsArchModule(module, android.HostAndDeviceSupported, android.MultilibFirst)
	android.InitDefaultableModule(module)
	return module
}
func addDependenciesForNativeModules(ctx android.BottomUpMutatorContext,
	native_shared_libs []string, binaries []string, arch string) {
	// Use *FarVariation* to be able to depend on modules having
	// conflicting variations with this module. This is required since
	// arch variant of an APEX bundle is 'common' but it is 'arm' or 'arm64'
	// for native shared libs.
	ctx.AddFarVariationDependencies([]blueprint.Variation{
		{Mutator: "arch", Variation: arch},
		{Mutator: "image", Variation: "core"},
		{Mutator: "link", Variation: "shared"},
	}, sharedLibTag, native_shared_libs...)

	ctx.AddFarVariationDependencies([]blueprint.Variation{
		{Mutator: "arch", Variation: arch},
		{Mutator: "image", Variation: "core"},
	}, executableTag, binaries...)
}

func (a *artZipApex) DepsMutator(ctx android.BottomUpMutatorContext) {
	targets := ctx.MultiTargets()
	has32Bit := false
	for _, target := range targets {
		if target.Arch.ArchType.Multilib == "lib32" {
			has32Bit = true
		}
	}
	use32BitTarget := func(target android.Target) bool {
		return has32Bit
		// return target.Os.Class == android.Device
		// if target.Arch.ArchType.Multilib == "lib32" {
		// 	return target.Os.Class == android.Device
		// }
		// return false
	}
	for i, target := range targets {
		var has32 string
		if use32BitTarget(target) {
			has32 = "true"
		} else {
			has32 = "false"
		}
		log.Print("Target is " + target.String() + " use 32: " + has32)
		// When multilib.* is omitted for native_shared_libs, it implies
		// multilib.both.
		ctx.AddFarVariationDependencies([]blueprint.Variation{
			{Mutator: "arch", Variation: target.String()},
			{Mutator: "image", Variation: "core"},
			{Mutator: "link", Variation: "shared"},
		}, sharedLibTag, a.properties.Native_shared_libs...)

		// Add native modules targetting both ABIs
		addDependenciesForNativeModules(ctx,
			a.properties.Multilib.Both.Native_shared_libs,
			a.properties.Multilib.Both.Binaries, target.String())

		if i == 0 {
			// When multilib.* is omitted for binaries, it implies
			// multilib.first.
			ctx.AddFarVariationDependencies([]blueprint.Variation{
				{Mutator: "arch", Variation: target.String()},
				{Mutator: "image", Variation: "core"},
			}, executableTag, a.properties.Binaries...)

			// Add native modules targetting the first ABI
			addDependenciesForNativeModules(ctx,
				a.properties.Multilib.First.Native_shared_libs,
				a.properties.Multilib.First.Binaries, target.String())
		}

		switch target.Arch.ArchType.Multilib {
		case "lib32":
			// Add native modules targetting 32-bit ABI
			addDependenciesForNativeModules(ctx,
				a.properties.Multilib.Lib32.Native_shared_libs,
				a.properties.Multilib.Lib32.Binaries, target.String())

			if use32BitTarget(target) {
				addDependenciesForNativeModules(ctx,
					a.properties.Multilib.Prefer32.Native_shared_libs,
					a.properties.Multilib.Prefer32.Binaries, target.String())
			}
		case "lib64":
			// Add native modules targetting 64-bit ABI
			addDependenciesForNativeModules(ctx,
				a.properties.Multilib.Lib64.Native_shared_libs,
				a.properties.Multilib.Lib64.Binaries, target.String())

			if !use32BitTarget(target) {
				addDependenciesForNativeModules(ctx,
					a.properties.Multilib.Prefer32.Native_shared_libs,
					a.properties.Multilib.Prefer32.Binaries, target.String())
			}
		}
	}
	// ctx.AddVariationDependencies([]blueprint.Variation{
	// 	{Mutator: "link", Variation: "shared"},
	// }, sharedLibTag, a.properties.Native_shared_libs...)
	// ctx.AddVariationDependencies([]blueprint.Variation{
	// 	{Mutator: "image", Variation: "core"},
	// 	{Mutator: "version", Variation: ""},
	// }, executableTag, a.properties.Binaries...)
}

func getCopyManifestForExecutable(cc *cc.Module) (fileToCopy android.Path, dirInApex string) {
	dirInApex = "bin"
	fileToCopy = cc.OutputFile().Path()
	return
}

func getCopyManifestForNativeLibrary(cc *cc.Module) (fileToCopy android.Path, dirInApex string) {
	// Decide the APEX-local directory by the multilib of the library
	// In the future, we may query this to the module.
	switch cc.Arch().ArchType.Multilib {
	case "lib32":
		dirInApex = "lib"
	case "lib64":
		dirInApex = "lib64"
	}
	if !cc.Arch().Native {
		dirInApex = filepath.Join(dirInApex, cc.Arch().ArchType.String())
	}

	fileToCopy = cc.OutputFile().Path()
	return
}

func (a *artZipApex) GenerateAndroidBuildActions(ctx android.ModuleContext) {
	filesInfo := []zipApexFile{}
	ctx.WalkDeps(func(child, parent android.Module) bool {
		if _, ok := parent.(*artZipApex); ok {
			// direct deps
			depName := ctx.OtherModuleName(child)
			depTag := ctx.OtherModuleDependencyTag(child)
			switch depTag {
			case sharedLibTag:
				if cc, ok := child.(*cc.Module); ok {
					fileToCopy, dirInApex := getCopyManifestForNativeLibrary(cc)
					filesInfo = append(filesInfo, zipApexFile{fileToCopy, depName, cc.Arch().ArchType, dirInApex, false})
					return true
				} else {
					ctx.PropertyErrorf("native_shared_libs", "%q is not a cc_library or cc_library_shared module", depName)
				}
			case executableTag:
				if cc, ok := child.(*cc.Module); ok {
					fileToCopy, dirInApex := getCopyManifestForExecutable(cc)
					filesInfo = append(filesInfo, zipApexFile{fileToCopy, depName, cc.Arch().ArchType, dirInApex, true})
					return true
				} else {
					ctx.PropertyErrorf("binaries", "%q is not a cc_binary module", depName)
				}
			}
		} else {
			if am, ok := child.(android.ApexModule); ok && am.CanHaveApexVariants() && am.IsInstallableToApex() {
				if cc, ok := child.(*cc.Module); ok {
					depName := ctx.OtherModuleName(child)
					fileToCopy, dirInApex := getCopyManifestForNativeLibrary(cc)
					filesInfo = append(filesInfo, zipApexFile{fileToCopy, depName, cc.Arch().ArchType, dirInApex, false})
					return true
				}
			}
		}
		return false
	})

	// remove duplicates in filesInfo
	removeDup := func(filesInfo []zipApexFile) []zipApexFile {
		encountered := make(map[android.Path]bool)
		result := []zipApexFile{}
		for _, f := range filesInfo {
			if !encountered[f.builtFile] {
				encountered[f.builtFile] = true
				result = append(result, f)
			}
		}
		return result
	}
	filesInfo = removeDup(filesInfo)

	// to have consistent build rules
	sort.Slice(filesInfo, func(i, j int) bool {
		return filesInfo[i].builtFile.String() < filesInfo[j].builtFile.String()
	})

	// prepend the name of this APEX to the module names. These names will be the names of
	// modules that will be defined if the APEX is flattened.
	for i := range filesInfo {
		filesInfo[i].moduleName = ctx.ModuleName() + "." + filesInfo[i].moduleName
	}

	filesToCopy := []android.Path{}
	for _, f := range filesInfo {
		filesToCopy = append(filesToCopy, f.builtFile)
	}
	copyCommands := []string{}
	for i, src := range filesToCopy {
		dest := filepath.Join(filesInfo[i].installDir, src.Base())
		dest_path := filepath.Join(android.PathForModuleOut(ctx, "apex_dir").String(), dest)
		copyCommands = append(copyCommands, "mkdir -p "+filepath.Dir(dest_path))
		copyCommands = append(copyCommands, "cp "+src.String()+" "+dest_path)
	}
	implicitInputs := append(android.Paths(nil), filesToCopy...)
	OutputFile := android.PathForModuleOut(ctx, ctx.ModuleName()+apexSuffix)

	ctx.Build(pctx, android.BuildParams{
		Rule:        zipApex,
		Implicits:   implicitInputs,
		Output:      OutputFile,
		Description: "zip-apex",
		Args: map[string]string{
			"apex_dir":      android.PathForModuleOut(ctx, "apex_dir").String(),
			"copy_commands": strings.Join(copyCommands, " && "),
		},
	})

	ctx.InstallFile(android.PathForModuleInstall(ctx, "apex_zip"), ctx.ModuleName()+apexSuffix, OutputFile, OutputFile)
}
