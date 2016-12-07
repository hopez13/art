import os
import tempfile
import subprocess

env = dict(os.environ)

def getEnvBoolean(var, default):
	val = env.get(var)
	if val:
		if val == "True" or val == "true":
			return True
		if val == "False" or val == "false":
			return False
	return default

def get_build_var(var_name):
  command = ("CALLED_FROM_SETUP=true BUILD_SYSTEM=build/core "
             "make --no-print-directory -C \"%s\" -f build/core/config.mk "
             "dumpvar-%s") % (ANDROID_BUILD_TOP, var_name)
  config = subprocess.Popen(command, stdout=subprocess.PIPE,
                            shell=True).communicate()[0]
  return config.strip()

# Directory used for temporary test files on the host.
ART_HOST_TEST_DIR = tempfile.mkstemp(prefix = 'test-art-')[1]

# Keep going after encountering a test failure?
ART_TEST_KEEP_GOING = getEnvBoolean('ART_TEST_KEEP_GOING', True)

# Do you want all tests, even those that are time consuming?
ART_TEST_FULL = getEnvBoolean('ART_TEST_FULL', False)

# Do you want run-test to be quieter? run-tests will only show output if they fail.
ART_TEST_QUIET = getEnvBoolean('ART_TEST_QUIET', True)

# Do you want interpreter tests run?
ART_TEST_INTERPRETER = getEnvBoolean('ART_TEST_INTERPRETER', ART_TEST_FULL)
ART_TEST_INTERPRETER_ACCESS_CHECKS = getEnvBoolean('ART_TEST_INTERPRETER_ACCESS_CHECKS', ART_TEST_FULL)

# Do you want JIT tests run?
ART_TEST_JIT = getEnvBoolean('ART_TEST_JIT', ART_TEST_FULL)

# Do you want optimizing compiler tests run?
ART_TEST_OPTIMIZING = getEnvBoolean('ART_TEST_OPTIMIZING', True)

# Do you want to test the optimizing compiler with graph coloring register allocation?
ART_TEST_OPTIMIZING_GRAPH_COLOR = getEnvBoolean('ART_TEST_OPTIMIZING_GRAPH_COLOR', ART_TEST_FULL)

# Do we want to test a non-PIC-compiled core image?
ART_TEST_NPIC_IMAGE = getEnvBoolean('ART_TEST_NPIC_IMAGE', ART_TEST_FULL)

# Do we want to test PIC-compiled tests ("apps")?
ART_TEST_PIC_TEST = getEnvBoolean('ART_TEST_PIC_TEST', ART_TEST_FULL)
# Do you want tracing tests run?
ART_TEST_TRACE = getEnvBoolean('ART_TEST_TRACE', ART_TEST_FULL)

# Do you want tracing tests (streaming mode) run?
ART_TEST_TRACE_STREAM = getEnvBoolean('ART_TEST_TRACE_STREAM', ART_TEST_FULL)

# Do you want tests with GC verification enabled run?
ART_TEST_GC_VERIFY = getEnvBoolean('ART_TEST_GC_VERIFY', ART_TEST_FULL)

# Do you want tests with the GC stress mode enabled run?
ART_TEST_GC_STRESS = getEnvBoolean('ART_TEST_GC_STRESS', ART_TEST_FULL)

# Do you want tests with the JNI forcecopy mode enabled run?
ART_TEST_JNI_FORCECOPY = getEnvBoolean('ART_TEST_JNI_FORCECOPY', ART_TEST_FULL)

# Do you want run-tests with relocation disabled run?
ART_TEST_RUN_TEST_NO_RELOCATE = getEnvBoolean('ART_TEST_RUN_TEST_NO_RELOCATE', ART_TEST_FULL)

# Do you want run-tests with prebuilding?
ART_TEST_RUN_TEST_PREBUILD = getEnvBoolean('ART_TEST_RUN_TEST_PREBUILD', True)

# Do you want run-tests with no prebuilding enabled run?
ART_TEST_RUN_TEST_NO_PREBUILD = getEnvBoolean('ART_TEST_RUN_TEST_NO_PREBUILD', ART_TEST_FULL)

# Do you want run-tests without a pregenerated core.art?

ART_TEST_RUN_TEST_NO_IMAGE = getEnvBoolean('ART_TEST_RUN_TEST_NO_IMAGE', ART_TEST_FULL)

# Do you want run-tests with relocation enabled but patchoat failing?
ART_TEST_RUN_TEST_RELOCATE_NO_PATCHOAT = getEnvBoolean('ART_TEST_RUN_TEST_RELOCATE_NO_PATCHOAT', ART_TEST_FULL)

# Do you want run-tests without a dex2oat?
ART_TEST_RUN_TEST_NO_DEX2OAT = getEnvBoolean('ART_TEST_RUN_TEST_NO_DEX2OAT', ART_TEST_FULL)

# Do you want run-tests with libartd.so?
ART_TEST_RUN_TEST_DEBUG = getEnvBoolean('ART_TEST_RUN_TEST_DEBUG', True)

# Do you want run-tests with libart.so?
ART_TEST_RUN_TEST_NDEBUG = getEnvBoolean('ART_TEST_RUN_TEST_NDEBUG', ART_TEST_FULL)

# Do you want run-tests with the host/target's second arch?
ART_TEST_RUN_TEST_2ND_ARCH = getEnvBoolean('ART_TEST_RUN_TEST_2ND_ARCH', True)

# Do you want failed tests to have their artifacts cleaned up?
ART_TEST_RUN_TEST_ALWAYS_CLEAN = getEnvBoolean('ART_TEST_RUN_TEST_ALWAYS_CLEAN', True)

# Do you want run-tests with the --debuggable flag
ART_TEST_RUN_TEST_DEBUGGABLE = getEnvBoolean('ART_TEST_RUN_TEST_DEBUGGABLE', ART_TEST_FULL)

# Do you want to test multi-part boot-image functionality?
ART_TEST_RUN_TEST_MULTI_IMAGE = getEnvBoolean('ART_TEST_RUN_TEST_MULTI_IMAGE', ART_TEST_FULL)

ART_TEST_DEBUG_GC = getEnvBoolean('ART_TEST_DEBUG_GC', False)

ANDROID_BUILD_TOP = env.get("ANDROID_BUILD_TOP")

JACK = get_build_var("JACK")
DX = get_build_var("DX")
HOST_OUT_EXECUTABLES = get_build_var("HOST_OUT_EXECUTABLES")
SMALI = HOST_OUT_EXECUTABLES + '/smali'
JASMIN = HOST_OUT_EXECUTABLES + '/jasmin'
DEXMERGER = HOST_OUT_EXECUTABLES + '/dexmerger'

ART_TEST_BISECTION = getEnvBoolean('ART_TEST_BISECTION', False)

ART_HOST_EXECUTABLES = ('dex2oat-host ',
                        'imgdiag-host',
                        'oatdump-host',
                        'patchoat-host',
                        'profman-host',
                        'dalvikvm-host',
                        'dexlist-hsot')

OUT_DIR = get_build_var('OUT_DIR')

HOST_PREFER_32_BIT = getEnvBoolean('HOST_PREFER_32_BIT', False)

ART_HOST_ARCH = ''
if HOST_PREFER_32_BIT:
  ART_HOST_ARCH = 'x86'
else:
  ART_HOST_ARCH = 'x86_64'
