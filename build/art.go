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

import (
	"fmt"
	"log"
	"path/filepath"
	"reflect"
	"strings"
	"sync"

	"github.com/google/blueprint"
	"github.com/google/blueprint/proptools"

	"android/soong/android"
	"android/soong/apex"
	"android/soong/cc"
	"android/soong/cc/config"
)

var supportedArches = []string{"arm", "arm64", "x86", "x86_64"}

type artVariantProperties struct {
	Enabled  *bool
	Cflags   []string
	Cppflags []string
	Asflags  []string
}

// Properties added to all art_* and libart_* module types.
type artProperties struct {
	Art struct {
		// Split the module into non-debug and debug variants, passing the flags
		// specified in the nodebug and debug clauses to each. If this is disabled
		// the same module will be used for both.
		Debug_split *bool

		// Flags for the non-debug variant, only applicable if debug_split is true.
		// See artVariantProperties in art/build/art.go for details about the
		// fields.
		Nodebug artVariantProperties

		// Flags for the debug variant, only applicable if debug_split is true. This
		// variant appends "d" to the output file suffix for libraries. See
		// artVariantProperties in art/build/art.go for details about the fields.
		Debug artVariantProperties

		// Set this if the current module is not split into non-debug and debug
		// variants but it has dependencies that are and it should depend on both.
		// Otherwise it will depend only on the non-debug variant if that is
		// enabled, otherwise the debug one (following standard Blueprint logic).
		// Not applicable if debug_split is true.
		Depend_on_all *bool
	}
}

type artModule struct {
}

func globalFlags(ctx android.LoadHookContext) ([]string, []string) {
	var cflags []string
	var asflags []string

	opt := ctx.Config().GetenvWithDefault("ART_NDEBUG_OPT_FLAG", "-O3")
	cflags = append(cflags, opt)

	tlab := false

	gcType := ctx.Config().GetenvWithDefault("ART_DEFAULT_GC_TYPE", "CMS")

	if ctx.Config().IsEnvTrue("ART_TEST_DEBUG_GC") {
		gcType = "SS"
		tlab = true
	}

	cflags = append(cflags, "-DART_DEFAULT_GC_TYPE_IS_"+gcType)
	if tlab {
		cflags = append(cflags, "-DART_USE_TLAB=1")
	}

	if ctx.Config().IsEnvTrue("ART_HEAP_POISONING") {
		cflags = append(cflags, "-DART_HEAP_POISONING=1")
		asflags = append(asflags, "-DART_HEAP_POISONING=1")
	}
	if ctx.Config().IsEnvTrue("ART_USE_CXX_INTERPRETER") {
		cflags = append(cflags, "-DART_USE_CXX_INTERPRETER=1")
	}

	if !ctx.Config().IsEnvFalse("ART_USE_READ_BARRIER") && ctx.Config().ArtUseReadBarrier() {
		// Used to change the read barrier type. Valid values are BAKER, BROOKS,
		// TABLELOOKUP. The default is BAKER.
		barrierType := ctx.Config().GetenvWithDefault("ART_READ_BARRIER_TYPE", "BAKER")
		cflags = append(cflags,
			"-DART_USE_READ_BARRIER=1",
			"-DART_READ_BARRIER_TYPE_IS_"+barrierType+"=1")
		asflags = append(asflags,
			"-DART_USE_READ_BARRIER=1",
			"-DART_READ_BARRIER_TYPE_IS_"+barrierType+"=1")
	}

	if !ctx.Config().IsEnvFalse("ART_USE_GENERATIONAL_CC") {
		cflags = append(cflags, "-DART_USE_GENERATIONAL_CC=1")
	}

	cdexLevel := ctx.Config().GetenvWithDefault("ART_DEFAULT_COMPACT_DEX_LEVEL", "fast")
	cflags = append(cflags, "-DART_DEFAULT_COMPACT_DEX_LEVEL="+cdexLevel)

	// We need larger stack overflow guards for ASAN, as the compiled code will have
	// larger frame sizes. For simplicity, just use global not-target-specific cflags.
	// Note: We increase this for both debug and non-debug, as the overflow gap will
	//       be compiled into managed code. We always preopt (and build core images) with
	//       the debug version. So make the gap consistent (and adjust for the worst).
	if len(ctx.Config().SanitizeDevice()) > 0 || len(ctx.Config().SanitizeHost()) > 0 {
		cflags = append(cflags,
			"-DART_STACK_OVERFLOW_GAP_arm=8192",
			"-DART_STACK_OVERFLOW_GAP_arm64=16384",
			"-DART_STACK_OVERFLOW_GAP_x86=16384",
			"-DART_STACK_OVERFLOW_GAP_x86_64=20480")
	} else {
		cflags = append(cflags,
			"-DART_STACK_OVERFLOW_GAP_arm=8192",
			"-DART_STACK_OVERFLOW_GAP_arm64=8192",
			"-DART_STACK_OVERFLOW_GAP_x86=8192",
			"-DART_STACK_OVERFLOW_GAP_x86_64=8192")
	}

	if ctx.Config().IsEnvTrue("ART_ENABLE_ADDRESS_SANITIZER") {
		// Used to enable full sanitization, i.e., user poisoning, under ASAN.
		cflags = append(cflags, "-DART_ENABLE_ADDRESS_SANITIZER=1")
		asflags = append(asflags, "-DART_ENABLE_ADDRESS_SANITIZER=1")
	}

	if !ctx.Config().IsEnvFalse("USE_D8_DESUGAR") {
		cflags = append(cflags, "-DUSE_D8_DESUGAR=1")
	}

	return cflags, asflags
}

func debugFlags(ctx android.LoadHookContext) []string {
	var cflags []string

	opt := ctx.Config().GetenvWithDefault("ART_DEBUG_OPT_FLAG", "-O2")
	cflags = append(cflags, opt)

	return cflags
}

func deviceFlags(ctx android.LoadHookContext) []string {
	var cflags []string
	deviceFrameSizeLimit := 1736
	if len(ctx.Config().SanitizeDevice()) > 0 {
		deviceFrameSizeLimit = 7400
	}
	cflags = append(cflags,
		fmt.Sprintf("-Wframe-larger-than=%d", deviceFrameSizeLimit),
		fmt.Sprintf("-DART_FRAME_SIZE_LIMIT=%d", deviceFrameSizeLimit),
	)

	cflags = append(cflags, "-DART_BASE_ADDRESS="+ctx.Config().LibartImgDeviceBaseAddress())
	minDelta := ctx.Config().GetenvWithDefault("LIBART_IMG_TARGET_MIN_BASE_ADDRESS_DELTA", "-0x1000000")
	maxDelta := ctx.Config().GetenvWithDefault("LIBART_IMG_TARGET_MAX_BASE_ADDRESS_DELTA", "0x1000000")
	cflags = append(cflags, "-DART_BASE_ADDRESS_MIN_DELTA="+minDelta)
	cflags = append(cflags, "-DART_BASE_ADDRESS_MAX_DELTA="+maxDelta)

	return cflags
}

func hostFlags(ctx android.LoadHookContext) []string {
	var cflags []string
	hostFrameSizeLimit := 1736
	if len(ctx.Config().SanitizeHost()) > 0 {
		// art/test/137-cfi/cfi.cc
		// error: stack frame size of 1944 bytes in function 'Java_Main_unwindInProcess'
		hostFrameSizeLimit = 6400
	}
	cflags = append(cflags,
		fmt.Sprintf("-Wframe-larger-than=%d", hostFrameSizeLimit),
		fmt.Sprintf("-DART_FRAME_SIZE_LIMIT=%d", hostFrameSizeLimit),
	)

	cflags = append(cflags, "-DART_BASE_ADDRESS="+ctx.Config().LibartImgHostBaseAddress())
	minDelta := ctx.Config().GetenvWithDefault("LIBART_IMG_HOST_MIN_BASE_ADDRESS_DELTA", "-0x1000000")
	maxDelta := ctx.Config().GetenvWithDefault("LIBART_IMG_HOST_MAX_BASE_ADDRESS_DELTA", "0x1000000")
	cflags = append(cflags, "-DART_BASE_ADDRESS_MIN_DELTA="+minDelta)
	cflags = append(cflags, "-DART_BASE_ADDRESS_MAX_DELTA="+maxDelta)

	if len(ctx.Config().SanitizeHost()) > 0 && !ctx.Config().IsEnvFalse("ART_ENABLE_ADDRESS_SANITIZER") {
		// We enable full sanitization on the host by default.
		cflags = append(cflags, "-DART_ENABLE_ADDRESS_SANITIZER=1")
	}

	clang_path := filepath.Join(config.ClangDefaultBase, ctx.Config().PrebuiltOS(), config.ClangDefaultVersion)
	cflags = append(cflags, "-DART_CLANG_PATH=\""+clang_path+"\"")

	return cflags
}

func globalDefaults(ctx android.LoadHookContext) {
	type props struct {
		Target struct {
			Android struct {
				Cflags []string
			}
			Host struct {
				Cflags []string
			}
		}
		Cflags   []string
		Asflags  []string
		Sanitize struct {
			Recover []string
		}
	}

	p := &props{}
	p.Cflags, p.Asflags = globalFlags(ctx)
	p.Target.Android.Cflags = deviceFlags(ctx)
	p.Target.Host.Cflags = hostFlags(ctx)

	if ctx.Config().IsEnvTrue("ART_DEX_FILE_ACCESS_TRACKING") {
		p.Cflags = append(p.Cflags, "-DART_DEX_FILE_ACCESS_TRACKING")
		p.Sanitize.Recover = []string{
			"address",
		}
	}

	ctx.AppendProperties(p)
}

// Hook that adds flags that are implicit for all cc_art_* modules.
func addImplicitFlags(ctx android.LoadHookContext) {
	type props struct {
		Target struct {
			Android struct {
				Cflags []string
			}
		}
	}

	p := &props{}
	if ctx.Config().IsEnvTrue("ART_TARGET_LINUX") {
		p.Target.Android.Cflags = []string{"-DART_TARGET", "-DART_TARGET_LINUX"}
	} else {
		p.Target.Android.Cflags = []string{"-DART_TARGET", "-DART_TARGET_ANDROID"}
	}

	ctx.AppendProperties(p)
}

func debugDefaults(ctx android.LoadHookContext) {
	type props struct {
		Cflags []string
	}

	p := &props{}
	p.Cflags = debugFlags(ctx)
	ctx.AppendProperties(p)
}

func customLinker(ctx android.LoadHookContext) {
	linker := ctx.Config().Getenv("CUSTOM_TARGET_LINKER")
	type props struct {
		DynamicLinker string
	}

	p := &props{}
	if linker != "" {
		p.DynamicLinker = linker
	}

	ctx.AppendProperties(p)
}

func prefer32Bit(ctx android.LoadHookContext) {
	type props struct {
		Target struct {
			Host struct {
				Compile_multilib *string
			}
		}
	}

	p := &props{}
	if ctx.Config().IsEnvTrue("HOST_PREFER_32_BIT") {
		p.Target.Host.Compile_multilib = proptools.StringPtr("prefer32")
	}

	// Prepend to make it overridable in the blueprints. Note that it doesn't work
	// to override the property in a cc_defaults module.
	ctx.PrependProperties(p)
}

func findPropertyStruct(mod android.Module, structType reflect.Type) interface{} {
	for _, props := range mod.GetProperties() {
		if reflect.TypeOf(props) == structType {
			return props
		}
	}
	return reflect.Zero(structType).Interface() // nil with the appropriate type
}

// Creates the debug and nodebug variants.
func artVariantMutator(mctx android.BottomUpMutatorContext) {
	// Our modules come from the cc package, which isn't constructed to allow us
	// to mix in a struct of our own that we can use to recognise them, so we have
	// to match the module type instead, and get the properties the hard way.
	if artModuleFactories[mctx.ModuleType()] == nil {
		return
	}

	artProps := findPropertyStruct(mctx.Module(), reflect.TypeOf((*artProperties)(nil))).(*artProperties)
	if artProps == nil {
		artProps = &artProperties{}
	}

	if !proptools.BoolDefault(artProps.Art.Debug_split, false) {
		return
	}

	nodebugVariant, debugVariant := -1, -1
	variantNames := []string{}
	variantProps := []*artVariantProperties{}
	if proptools.BoolDefault(artProps.Art.Nodebug.Enabled, true) {
		nodebugVariant = len(variantNames)
		variantNames = append(variantNames, "nodebug")
		variantProps = append(variantProps, &artProps.Art.Nodebug)
	}
	if proptools.BoolDefault(artProps.Art.Debug.Enabled, true) {
		debugVariant = len(variantNames)
		variantNames = append(variantNames, "debug")
		variantProps = append(variantProps, &artProps.Art.Debug)
	}

	modules := mctx.CreateVariations(variantNames...)

	for i, m := range modules {
		if p := findPropertyStruct(m, reflect.TypeOf((*cc.BaseCompilerProperties)(nil))).(*cc.BaseCompilerProperties); p != nil {
			artProps := variantProps[i]
			p.Cflags = append(p.Cflags, artProps.Cflags...)
			p.Cppflags = append(p.Cppflags, artProps.Cppflags...)
			p.Asflags = append(p.Asflags, artProps.Asflags...)
		}
	}

	_ = nodebugVariant
	// if nodebugVariant >= 0 {
	// 	mctx.AliasVariation("nodebug")
	// } else {
	// 	mctx.AliasVariation("debug")
	// }

	if debugVariant >= 0 {
		m := modules[debugVariant]
		if p := findPropertyStruct(m, reflect.TypeOf((*cc.LibraryProperties)(nil))).(*cc.LibraryProperties); p != nil {
			p.Suffix = proptools.StringPtr(proptools.String(p.Suffix) + "d")
		}

		if len(modules) > 1 {
			if ccMod, ok := m.(*cc.Module); ok && ccMod.Static() {
				// Necessary to avoid duplicate AndroidMk stanzas, since they are identified
				// by the module basename and not the stem.
				m.SkipInstall()
			}
		}
	}
}

// Adds the extra dependencies for art.depend_on_all modules.
func artDependencyMutator(mctx android.BottomUpMutatorContext) {
	if artModuleFactories[mctx.ModuleType()] == nil {
		return
	}

	artProps := findPropertyStruct(mctx.Module(), reflect.TypeOf((*artProperties)(nil))).(*artProperties)
	if artProps == nil {
		artProps = &artProperties{}
	}
	if !proptools.BoolDefault(artProps.Art.Depend_on_all, false) {
		return
	}

	type depInfo struct {
		module string
		tag    blueprint.DependencyTag
	}
	artMultiVariantDeps := []depInfo{}
	mctx.VisitDirectDeps(func(child android.Module) {
		//log.Printf("%s -> %s | %v", mctx.Module(), child, mctx.OtherModuleDependencyTag(child))
		if artModuleFactories[mctx.OtherModuleType(child)] == nil {
			return
		}
		artProps := findPropertyStruct(child, reflect.TypeOf((*artProperties)(nil))).(*artProperties)
		if proptools.BoolDefault(artProps.Art.Debug_split, false) &&
			proptools.BoolDefault(artProps.Art.Nodebug.Enabled, true) &&
			proptools.BoolDefault(artProps.Art.Debug.Enabled, true) {
			artMultiVariantDeps = append(artMultiVariantDeps, depInfo{
				module: mctx.OtherModuleName(child),
				tag:    mctx.OtherModuleDependencyTag(child),
			})
		}
	})

	// FIXME

	if len(artMultiVariantDeps) > 0 {
		log.Printf("%s: x %v", mctx.Module(), artMultiVariantDeps)
	}
}

var testMapKey = android.NewOnceKey("artTests")

func testMap(config android.Config) map[string][]string {
	return config.Once(testMapKey, func() interface{} {
		return make(map[string][]string)
	}).(map[string][]string)
}

func testInstall(ctx android.InstallHookContext) {
	testMap := testMap(ctx.Config())

	var name string
	if ctx.Host() {
		name = "host_"
	} else {
		name = "device_"
	}
	name += ctx.Arch().ArchType.String() + "_" + ctx.ModuleName()

	artTestMutex.Lock()
	defer artTestMutex.Unlock()

	tests := testMap[name]
	tests = append(tests, ctx.Path().ToMakePath().String())
	testMap[name] = tests
}

var testcasesContentKey = android.NewOnceKey("artTestcasesContent")

func testcasesContent(config android.Config) map[string]string {
	return config.Once(testcasesContentKey, func() interface{} {
		return make(map[string]string)
	}).(map[string]string)
}

// Binaries and libraries also need to be copied in the testcases directory for
// running tests on host.  This method adds module to the list of needed files.
// The 'key' is the file in testcases and 'value' is the path to copy it from.
// The actual copy will be done in make since soong does not do installations.
func addTestcasesFile(ctx android.InstallHookContext) {
	testcasesContent := testcasesContent(ctx.Config())

	artTestMutex.Lock()
	defer artTestMutex.Unlock()

	if ctx.Os().Class == android.Host {
		src := ctx.SrcPath().String()
		path := strings.Split(ctx.Path().ToMakePath().String(), "/")
		// Keep last two parts of the install path (e.g. bin/dex2oat).
		dst := strings.Join(path[len(path)-2:], "/")
		testcasesContent[dst] = src
	}
}

var artTestMutex sync.Mutex

var artModuleFactories = map[string]func() android.Module{
	"art_cc_library":            artLibrary,
	"art_cc_library_static":     artStaticLibrary,
	"art_cc_binary":             artBinary,
	"art_cc_test":               artTest,
	"art_cc_test_library":       artTestLibrary,
	"art_cc_defaults":           artDefaultsFactory,
	"libart_cc_defaults":        libartDefaultsFactory,
	"libart_static_cc_defaults": libartStaticDefaultsFactory,
	"art_global_defaults":       artGlobalDefaultsFactory,
	"art_debug_defaults":        artDebugDefaultsFactory,
	"art_apex":                  artApexBundleFactory,
	"art_apex_test":             artTestApexBundleFactory,
	"art_apex_test_host":        artHostTestApexBundleFactory,
}

func init() {
	moduleTypeList := []string{}
	for name, _ := range artModuleFactories {
		moduleTypeList = append(moduleTypeList, name)
	}
	android.AddNeverAllowRules(
		android.NeverAllow().
			NotIn("art", "libcore", "external/vixl").
			ModuleType(moduleTypeList...))

	for name, factory := range artModuleFactories {
		if factory != nil {
			android.RegisterModuleType(name, factory)
		}
	}

	ctx := android.InitRegistrationContext
	ctx.PostDepsMutators(func(ctx android.RegisterMutatorsContext) {
		ctx.BottomUp("art", artVariantMutator).Parallel()
		//ctx.BottomUp("art_dependency", artDependencyMutator).Parallel()
	})
}

func initArtModule(m android.Module) {
	m.AddProperties(&artProperties{})
	// The module factories in the cc package have already called this, but we
	// need to call it again to register our own property struct as well.
	android.InitDefaultableModule(m.(android.DefaultableModule))
}

func artApexBundleFactory() android.Module {
	// ART apex is special because it must include dexpreopt files for bootclasspath jars.
	module := apex.ApexBundleFactory(false /*testApex*/, true /*artApex*/)
	initArtModule(module)
	return module
}

func artTestApexBundleFactory() android.Module {
	// ART apex is special because it must include dexpreopt files for bootclasspath jars.
	module := apex.ApexBundleFactory(true /*testApex*/, true /*artApex*/)
	initArtModule(module)
	return module
}

func artHostTestApexBundleFactory() android.Module {
	// ART apex is special because it must include dexpreopt files for bootclasspath jars.
	module := apex.ApexBundleFactory(true /*testApex*/, true /*artApex*/)

	// TODO: This makes the module disable itself for host if HOST_PREFER_32_BIT is
	// set. We need this because the multilib types of binaries listed in the apex
	// rule must match the declared type. This is normally not difficult but HOST_PREFER_32_BIT
	// changes this to 'prefer32' on all host binaries. Since HOST_PREFER_32_BIT is
	// only used for testing we can just disable the module.
	// See b/120617876 for more information.
	android.AddLoadHook(module, func(ctx android.LoadHookContext) {
		if ctx.Config().IsEnvTrue("HOST_PREFER_32_BIT") {
			type props struct {
				Target struct {
					Host struct {
						Enabled *bool
					}
				}
			}

			p := &props{}
			p.Target.Host.Enabled = proptools.BoolPtr(false)
			ctx.AppendProperties(p)
			log.Print("Disabling host build of " + ctx.ModuleName() + " for HOST_PREFER_32_BIT=true")
		}
	})

	initArtModule(module)
	return module
}

func artGlobalDefaultsFactory() android.Module {
	module := artDefaultsFactory()
	android.AddLoadHook(module, addImplicitFlags)
	android.AddLoadHook(module, globalDefaults)

	return module
}

func artDebugDefaultsFactory() android.Module {
	module := artDefaultsFactory()
	android.AddLoadHook(module, debugDefaults)

	return module
}

func artDefaultsFactory() android.Module {
	c := &codegenProperties{}
	module := cc.DefaultsFactory(c)
	android.AddLoadHook(module, func(ctx android.LoadHookContext) { codegen(ctx, c, staticAndSharedLibrary) })

	initArtModule(module)
	return module
}

func libartDefaultsFactory() android.Module {
	c := &codegenProperties{}
	module := cc.DefaultsFactory(c)
	android.AddLoadHook(module, func(ctx android.LoadHookContext) { codegen(ctx, c, staticAndSharedLibrary) })

	initArtModule(module)
	return module
}

func libartStaticDefaultsFactory() android.Module {
	c := &codegenProperties{}
	module := cc.DefaultsFactory(c)
	android.AddLoadHook(module, func(ctx android.LoadHookContext) { codegen(ctx, c, staticLibrary) })

	initArtModule(module)
	return module
}

func artLibrary() android.Module {
	module := cc.LibraryFactory()
	initArtModule(module)

	installCodegenCustomizer(module, staticAndSharedLibrary)

	android.AddLoadHook(module, addImplicitFlags)
	android.AddInstallHook(module, addTestcasesFile)
	return module
}

func artStaticLibrary() android.Module {
	module := cc.LibraryStaticFactory()

	installCodegenCustomizer(module, staticLibrary)

	android.AddLoadHook(module, addImplicitFlags)
	initArtModule(module)
	return module
}

func artBinary() android.Module {
	module := cc.BinaryFactory()

	android.AddLoadHook(module, addImplicitFlags)
	android.AddLoadHook(module, customLinker)
	android.AddLoadHook(module, prefer32Bit)
	android.AddInstallHook(module, addTestcasesFile)
	initArtModule(module)
	return module
}

func artTest() android.Module {
	module := cc.TestFactory()

	installCodegenCustomizer(module, binary)

	android.AddLoadHook(module, addImplicitFlags)
	android.AddLoadHook(module, customLinker)
	android.AddLoadHook(module, prefer32Bit)
	android.AddInstallHook(module, testInstall)
	initArtModule(module)
	return module
}

func artTestLibrary() android.Module {
	module := cc.TestLibraryFactory()

	installCodegenCustomizer(module, staticAndSharedLibrary)

	android.AddLoadHook(module, addImplicitFlags)
	android.AddLoadHook(module, prefer32Bit)
	android.AddInstallHook(module, testInstall)
	initArtModule(module)
	return module
}
