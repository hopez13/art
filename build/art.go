			}		DynamicLinker string		p.DynamicLinker = linker	return config.Once(testMapKey, func() interface{} 	return config.Once(testcasesContentKey, func() interface{} {
		return make(map[string]string)
// running tests on host.  This method adds module to the list of needed files.
// The 'key' is the file in testcases and 'value' is the path to copy it from.
// The actual copy will be done in make since soong does not do installations.
func addTestcasesFile(ctx android.InstallHookContext) {
	if ctx.Os() != ctx.Config().BuildOS || ctx.Target().HostCross || ctx.Module().IsSkipInstall() {
		return
	}

	testcasesContent := testcasesContent(ctx.Config())

	artTestMutex.Lock()
	defer artTestMutex.Unlock()

	src := ctx.SrcPath().String()
	path := strings.Split(ctx.Path().String(), "/")
	// Keep last two parts of the install path (e.g. bin/dex2oat).
	dst := strings.Join(path[len(path)-2:], "/")
	if oldSrc, ok := testcasesContent[dst]; ok {
		ctx.ModuleErrorf("Conflicting sources for %s: %s and %s", dst, oldSrc, src)
	}
	testcasesContent[dst] = src
}

var artTestMutex sync.Mutex

func init() {
	artModuleTypes := []string{
		"art_cc_library",
		"art_cc_library_static",
		"art_cc_binary",
		"art_cc_test",
		"art_cc_test_library",
		"art_cc_defaults",
		"art_global_defaults",
		"art_apex_test_host",
	}
	android.AddNeverAllowRules(
		android.NeverAllow().
			NotIn("art", "external/vixl").
			ModuleType(artModuleTypes...))

	android.RegisterModuleType("art_cc_library", artLibrary)
	android.RegisterModuleType("art_cc_library_static", artStaticLibrary)
	android.RegisterModuleType("art_cc_binary", artBinary)
	android.RegisterModuleType("art_cc_test", artTest)
	android.RegisterModuleType("art_cc_test_library", artTestLibrary)
	android.RegisterModuleType("art_cc_defaults", artDefaultsFactory)
	android.RegisterModuleType("art_global_defaults", artGlobalDefaultsFactory)

	// TODO: This makes the module disable itself for host if HOST_PREFER_32_BIT is
	// set. We need this because the multilib types of binaries listed in the apex
	// rule must match the declared type. This is normally not difficult but HOST_PREFER_32_BIT
	// changes this to 'prefer32' on all host binaries. Since HOST_PREFER_32_BIT is
	// only used for testing we can just disable the module.
	// See b/120617876 for more information.
	android.RegisterModuleType("art_apex_test_host", artHostTestApexBundleFactory)
}

func artHostTestApexBundleFactory() android.Module {
	module := apex.ApexBundleFactory(true)
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

	return module
}

func artGlobalDefaultsFactory() android.Module {
	module := artDefaultsFactory()
	android.AddLoadHook(module, addImplicitFlags)
	android.AddLoadHook(module, globalDefaults)

	return module
}

func artDefaultsFactory() android.Module {
	c := &codegenProperties{}
	module := cc.DefaultsFactory(c)
	android.AddLoadHook(module, func(ctx android.LoadHookContext) { codegen(ctx, c, staticAndSharedLibrary) })

	return module
}

func artLibrary() android.Module {
	module := cc.LibraryFactory()

	installCodegenCustomizer(module, staticAndSharedLibrary)

	android.AddLoadHook(module, addImplicitFlags)
	android.AddInstallHook(module, addTestcasesFile)
	return module
}

func artStaticLibrary() android.Module {
	module := cc.LibraryStaticFactory()

	installCodegenCustomizer(module, staticLibrary)

	android.AddLoadHook(module, addImplicitFlags)
	return module
}

func artBinary() android.Module {
	module := cc.BinaryFactory()

	android.AddLoadHook(module, addImplicitFlags)
	android.AddLoadHook(module, customLinker)
	android.AddLoadHook(module, prefer32Bit)
	android.AddInstallHook(module, addTestcasesFile)
	return module
}

func artTest() android.Module {
	// Disable bp2build.
	module := cc.NewTest(android.HostAndDeviceSupported, false /* bazelable */).Init()

	installCodegenCustomizer(module, binary)

	android.AddLoadHook(module, addImplicitFlags)
	android.AddLoadHook(module, customLinker)
	android.AddLoadHook(module, prefer32Bit)
	android.AddInstallHook(module, testInstall)
	return module
}

func artTestLibrary() android.Module {
	module := cc.TestLibraryFactory()

	installCodegenCustomizer(module, staticAndSharedLibrary)

	android.AddLoadHook(module, addImplicitFlags)
	android.AddLoadHook(module, prefer32Bit)
	android.AddInstallHook(module, testInstall)
	return module
}
