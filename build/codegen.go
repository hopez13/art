
			Target struct {				Host    *CodegenSourceArchPropertie		}
		if host {

	addCodegenArchProperties := func(host bool, archName string) {
		type commonProps struct {
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

		type sharedLibraryProps struct {
			Target struct {
				Android *CodegenLibraryArchSharedProperties
				Host    *CodegenLibraryArchSharedProperties
			}
		}

		type staticLibraryProps struct {
			Target struct {
				Android *CodegenLibraryArchStaticProperties
				Host    *CodegenLibraryArchStaticProperties
			}
		}

		arch := getCodegenArchProperties(archName)

		cp := &commonProps{}
		lp := &libraryProps{}
		sharedLP := &sharedLibraryProps{}
		staticLP := &staticLibraryProps{}
		if host {
			cp.Target.Host = &arch.CodegenCommonArchProperties
			lp.Target.Host = &arch.CodegenLibraryArchProperties
			sharedLP.Target.Host = &arch.CodegenLibraryArchSharedProperties
			staticLP.Target.Host = &arch.CodegenLibraryArchStaticProperties
		} else {
			cp.Target.Android = &arch.CodegenCommonArchProperties
			lp.Target.Android = &arch.CodegenLibraryArchProperties
			sharedLP.Target.Android = &arch.CodegenLibraryArchSharedProperties
			staticLP.Target.Android = &arch.CodegenLibraryArchStaticProperties
		}

		ctx.AppendProperties(cp)
		if t.library {
			ctx.AppendProperties(lp)
			if t.static {
				ctx.AppendProperties(staticLP)
			}
			if t.shared {
				ctx.AppendProperties(sharedLP)
			}
		}
	}

	addCodegenProperties := func(host bool, arches []string) {
		sourceProps := &CodegenSourceArchProperties{}
		for _, arch := range arches {
			appendCodegenSourceArchProperties(sourceProps, arch)
			addCodegenArchProperties(host, arch)
		}
		sourceProps.Srcs = android.FirstUniqueStrings(sourceProps.Srcs)
		addCodegenSourceArchProperties(host, sourceProps)
	}

	addCodegenProperties(false /* host */, deviceArches)
	addCodegenProperties(true /* host */, hostArches)
}

// These properties are allowed to contain the same source file name in different architectures.
// They we will be deduplicated automatically.
type CodegenSourceArchProperties struct {
	Srcs []string
}

type CodegenCommonArchProperties struct {
	Cflags   []string
	Cppflags []string
}

type CodegenLibraryArchProperties struct {
	Static_libs               []string
	Export_static_lib_headers []string
}

type CodegenLibraryArchStaticProperties struct {
	Static struct {
		Whole_static_libs []string
	}
}
type CodegenLibraryArchSharedProperties struct {
	Shared struct {
		Shared_libs               []string
		Export_shared_lib_headers []string
	}
}

type codegenArchProperties struct {
	CodegenSourceArchProperties
	CodegenCommonArchProperties
	CodegenLibraryArchProperties
	CodegenLibraryArchStaticProperties
	CodegenLibraryArchSharedProperties
}

type codegenProperties struct {
	Codegen struct {
		Arm, Arm64, Riscv64, X86, X86_64 codegenArchProperties
	}
}

func defaultDeviceCodegenArches(ctx android.LoadHookContext) []string {
	arches := make(map[string]bool)
	for _, a := range ctx.DeviceConfig().Arches() {
		s := a.ArchType.String()
		arches[s] = true
		if s == "arm64" {
			arches["arm"] = true
		} else if s == "riscv64" {
			arches["riscv64"] = true
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

func installCodegenCustomizer(module android.Module, t moduleType) {
	c := &codegenProperties{}
	android.AddLoadHook(module, func(ctx android.LoadHookContext) { codegen(ctx, c, t) })
	module.AddProperties(c)
}
