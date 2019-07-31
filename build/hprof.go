// Copyright 2019 Google Inc. All rights reserved.
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
	"fmt"
	"runtime"

	"android/soong/android"

	"github.com/google/blueprint"
	"github.com/google/blueprint/proptools"
)

func init() {
	android.RegisterModuleType("ahat_hprof", ahatHprofFactory)
}

const hprofCoreImage = "core-image-optimizing"

type ahatHprof struct {
	android.ModuleBase
	properties hprofProperties

	outputFile android.Path
}

var _ android.OutputFileProducer = (*ahatHprof)(nil)

type hprofProperties struct {
	Test_dump *string
	Flags     []string
	Out       *string
}

func ahatHprofFactory() android.Module {
	module := &ahatHprof{}

	module.AddProperties(&module.properties)

	// The ahat tests rely on running ART to generate a heap dump for test, but ART
	// doesn't run on darwin. Only build and run the tests for linux.
	android.AddLoadHook(module, func(ctx android.LoadHookContext) {
		if runtime.GOOS == "darwin" {
			disable := struct {
				Enabled *bool
			}{}
			disable.Enabled = proptools.BoolPtr(false)
			ctx.AppendProperties(&disable)
		}
	})

	android.AddLoadHook(module, prefer32Bit)

	android.InitAndroidModule(module)

	return module
}

func (h *ahatHprof) DepsMutator(ctx android.BottomUpMutatorContext) {
	if td := proptools.String(h.properties.Test_dump); td != "" {
		ctx.AddFarVariationDependencies([]blueprint.Variation{
			{Mutator: "arch", Variation: "android_common"},
		}, testDumpJarDepTag, td)
	} else {
		ctx.ModuleErrorf("test_dump property is required")
	}

	var coreImageTarget android.Target
	if ctx.Config().IsEnvTrue("HOST_PREFER_32_BIT") {
		coreImageTarget = ctx.Config().Targets[android.BuildOs][1]
	} else {
		coreImageTarget = ctx.Config().Targets[android.BuildOs][0]
	}

	ctx.AddFarVariationDependencies([]blueprint.Variation{
		{Mutator: "arch", Variation: coreImageTarget.String()},
	}, coreImageDepTag, hprofCoreImage)
}

func (h *ahatHprof) GenerateAndroidBuildActions(ctx android.ModuleContext) {
	rule := android.NewRuleBuilder()

	var dalvikvm android.Paths
	var dalvikvmFlag string
	var dalvikvmLibDir string
	if ctx.Config().IsEnvTrue("HOST_PREFER_32_BIT") {
		dalvikvm = append(dalvikvm, ctx.Config().HostToolPath(ctx, "dalvikvm32"))
		dalvikvmFlag = "--32"
		dalvikvmLibDir = "lib"
	} else {
		dalvikvm = append(dalvikvm, ctx.Config().HostToolPath(ctx, "dalvikvm64"))
		dalvikvmFlag = "--64"
		dalvikvmLibDir = "lib64"
	}

	// dalvikvm depends on the non-debug runtime libraries, but this passes "-d" which will make it load the
	// debug runtime libraries.  Manually add dependencies on the installed locations.
	for _, lib := range debugSharedLibraries {
		dalvikvm = append(dalvikvm,
			android.PathForOutput(ctx, "host", ctx.Config().PrebuiltOS(), dalvikvmLibDir, lib+".so"))
	}

	testDumpJar := getTestDumpJar(ctx, proptools.String(h.properties.Test_dump))
	if ctx.Failed() {
		return
	}

	coreImage := getCoreImage(ctx, hprofCoreImage)

	outputDir := android.PathForModuleGen(ctx, "hprof")
	filename := proptools.StringDefault(h.properties.Out, ctx.ModuleName()+".hprof")
	outputFile := outputDir.Join(ctx, filename)

	rule.Sbox(outputDir)

	rule.Command().Text("mkdir -p __SBOX_OUT_DIR__/android-data")
	rule.Command().
		Text("ANDROID_DATA=__SBOX_OUT_DIR__/android-data").
		BuiltTool(ctx, "art").
		Implicit(coreImage).
		Flag("--no-compile"). // avoid dependency on dex2oat
		Flag("-d").
		Flag(dalvikvmFlag).
		Implicits(dalvikvm).
		Implicit(coreImage).
		FlagWithInput("-cp ", testDumpJar).
		Text("Main").
		Output(outputFile).
		Flags(h.properties.Flags)

	rule.Build(pctx, ctx, "hprof", "hprof")

	h.outputFile = outputFile
}

func (h *ahatHprof) OutputFiles(tag string) (android.Paths, error) {
	switch tag {
	case "":
		return android.Paths{h.outputFile}, nil
	default:
		return nil, fmt.Errorf("unsupported module reference tag %q", tag)
	}
}

func getTestDumpJar(ctx android.ModuleContext, testDump string) android.Path {
	module := ctx.GetDirectDepWithTag(testDump, testDumpJarDepTag)
	if module == nil {
		ctx.PropertyErrorf("test_dump", "failed to get test_dump module %q", testDump)
		return nil
	}
	return android.OutputFileForModule(ctx, module, "")
}

func getCoreImage(ctx android.ModuleContext, coreImage string) android.Path {
	module := ctx.GetDirectDepWithTag(coreImage, coreImageDepTag)
	if module == nil {
		ctx.ModuleErrorf("failed to get core image module")
		return nil
	}
	if image, ok := module.(*image); ok {
		return image.installPath
	} else {
		ctx.ModuleErrorf("core image module is not a *image")
		return nil
	}
}

type hprofDepTag struct {
	blueprint.DependencyTag
	name string
}

var testDumpJarDepTag = hprofDepTag{name: "test dump jar"}
var coreImageDepTag = hprofDepTag{name: "core image"}
