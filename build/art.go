lll	android.RegisterModuleType("art_apex_test_host", artHostTestApexBundleFactory)
}

l	android.AddLoadHook(module, func(ctx android.LoadHookContext) {
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
