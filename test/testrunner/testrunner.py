import json
from sets import Set
import os
import env
import fnmatch
import subprocess

def setup():
  initializeGlobalVariables()
  variant_to_type['host'] = 'target'
  variant_to_type['target'] = 'target'
  variant_to_type['debug'] = 'run'
  variant_to_type['ndebug'] = 'run'
  variant_to_type['prebuild'] = 'prebuild'
  variant_to_type['no-prebuild'] = 'prebuild'
  variant_to_type['no-dex2oat'] = 'prebuild'
  variant_to_type['interpreter'] = 'compiler'
  variant_to_type['optimizing'] = 'compiler'
  variant_to_type['jit'] = 'compiler'
  variant_to_type['interp-ac'] = 'compiler'
  variant_to_type['relocate'] = 'relocate'
  variant_to_type['nrelocate'] = 'relocate'
  variant_to_type['relocate-npatchoat'] = 'relocate'
  variant_to_type['trace'] = 'trace'
  variant_to_type['ntrace'] = 'trace'
  variant_to_type['gcstress'] = 'gc'
  variant_to_type['gcverify'] = 'gc'
  variant_to_type['cms'] = 'gc'
  variant_to_type['forcecopy'] = 'jni'
  variant_to_type['checkjni'] = 'jni'
  variant_to_type['jni'] = 'jni'
  variant_to_type['no-image'] = 'image'
  variant_to_type['image'] = 'image'
  variant_to_type['picimage'] = 'image'
  variant_to_type['pictest'] = 'pictest'
  variant_to_type['npictest'] = 'pictest'
  variant_to_type['ndebuggable'] = 'debuggable'
  variant_to_type['debuggable'] = 'debuggable'
  variant_to_type['32'] = 'address_sizes'
  variant_to_type['64'] = 'address_sizes'

  if env.ART_TEST_BISECTION:
    # Need to keep rebuilding the test to bisection search it.
    env.ART_TEST_RUN_TEST_NO_PREBUILD = True
    env.ART_TEST_RUN_TEST_PREBUILD = False
    # Bisection search writes to standard output.
    env.ART_TEST_QUIET = False

  TARGET_TYPES.add('host')
 # TARGET_TYPES.add('target')

  if env.ART_TEST_RUN_TEST_PREBUILD:
    PREBUILD_TYPES.add('prebuild')
  if env.ART_TEST_RUN_TEST_NO_PREBUILD:
    PREBUILD_TYPES.add('no-prebuild')
  if env.ART_TEST_RUN_TEST_NO_DEX2OAT:
    PREBUILD_TYPES.add('no-dex2oat')

  if env.ART_TEST_INTERPRETER_ACCESS_CHECKS:
   COMPILER_TYPES.add('interp-ac')
  if env.ART_TEST_INTERPRETER:
		COMPILER_TYPES.add('interpreter')
  if env.ART_TEST_JIT:
    COMPILER_TYPES.add('jit')

  if env.ART_TEST_OPTIMIZING:
    COMPILER_TYPES.add('optimizing')
    OPTIMIZING_COMPILER_TYPES.add('optimizing')
  if env.ART_TEST_OPTIMIZING_GRAPH_COLOR:
    COMPILER_TYPES.add('regalloc_gc')
    OPTIMIZING_COMPILER_TYPES.add('regalloc_gc')

  RELOCATE_TYPES.add('relocate')
  if env.ART_TEST_RUN_TEST_NO_RELOCATE:
    RELOCATE_TYPES.add('no-relocate')
  if env.ART_TEST_RUN_TEST_RELOCATE_NO_PATCHOAT:
    RELOCATE_TYPES.add('relocate-npatchoat')

  TRACE_TYPES.add('ntrace')
  if env.ART_TEST_TRACE:
    TRACE_TYPES.add('trace')
  if env.ART_TEST_TRACE_STREAM:
    TRACE_TYPES.add('stream')

  GC_TYPES.add('cms')
  if env.ART_TEST_GC_STRESS:
    GC_TYPES.add('gcstress')
  if env.ART_TEST_GC_VERIFY:
    GC_TYPES.add('gcverify')

  JNI_TYPES.add('checkjni')
  if env.ART_TEST_JNI_FORCECOPY:
    JNI_TYPES.add('forcecopy')

  IMAGE_TYPES.add('picimage')
  if env.ART_TEST_RUN_TEST_NO_IMAGE:
    IMAGE_TYPES.add('no-image')
  if env.ART_TEST_RUN_TEST_MULTI_IMAGE:
    IMAGE_TYPES.add('multipicimage')
  if env.ART_TEST_NPIC_IMAGE:
		IMAGE_TYPES.add('npicimage')
  if env.ART_TEST_RUN_TEST_MULTI_IMAGE:
    IMAGE_TYPES.add('multinpicimage')

  PICTEST_TYPES.add('npictest')
  if env.ART_TEST_PIC_TEST:
    PICTEST_TYPES.add('pictest')

  if env.ART_TEST_RUN_TEST_DEBUG:
    RUN_TYPES.add('debug')
  if env.ART_TEST_RUN_TEST_NDEBUG:
    RUN_TYPES.add('ndebug')

  DEBUGGABLE_TYPES.add('ndebuggable')
  if env.ART_TEST_RUN_TEST_DEBUGGABLE:
    DEBUGGABLE_TYPES.add('debuggable')

  #    ADDRESS_SIZES_TARGET.add(env.ART_PHONY_TEST_TARGET_SUFFIX)
	#    ADDRESS_SIZES_HOST.add(env.ART_PHONY_TEST_HOST_SUFFIX)
	#    if env.get('ART_TEST_RUN_TEST_2ND_ARCH'):
  #        ADDRESS_SIZES_TARGET.add(env.2ND_ART_PHONY_TEST_TARGET_SUFFIX)
  #        ADDRESS_SIZES_HOST.add(env.2ND_ART_PHONY_TEST_HOST_SUFFIX)

  ADDRESS_SIZES.add('64')
  ADDRESS_SIZES.add('32')

  ART_TEST_WITH_STRACE = env.ART_TEST_DEBUG_GC

def run_tests(tests=None):

  if not tests:
    tests = []
    test_dir = env.ANDROID_BUILD_TOP + '/art/test'
    for file in os.listdir(test_dir):
      if fnmatch.fnmatch(file, '[0-9]*'):
        tests.append(file)

  options_all = ''

  if env.ART_TEST_WITH_STRACE:
    options_all += ' --strace'

  if env.ART_TEST_RUN_TEST_ALWAYS_CLEAN:
    options_all += ' --always-clean'

  if env.ART_TEST_BISECTION:
    options_all += ' --bisection-search'

  if env.ART_TEST_ANDROID_ROOT:
    options_all += ' --android-root ' + env.ART_TEST_ARDROID_ROOT

  if env.ART_TEST_QUIET:
    options_all += ' --quiet'

  for test in tests:
    options_test = options_all
    for target in TARGET_TYPES:
      for run in RUN_TYPES:
        for prebuild in PREBUILD_TYPES:
          for compiler in COMPILER_TYPES:
            for relocate in RELOCATE_TYPES:
              for trace in TRACE_TYPES:
                for gc in GC_TYPES:
                  for jni in JNI_TYPES:
                    for image in IMAGE_TYPES:
                      for pictest in PICTEST_TYPES:
                        for debuggable in DEBUGGABLE_TYPES:
                          for address_size in ADDRESS_SIZES:
                            test_name = 'test-art-'
                            test_name += target + '-run-test-'
                            test_name += run + '-'
                            test_name += prebuild + '-'
                            test_name += compiler + '-'
                            test_name += relocate + '-'
                            test_name += trace + '-'
                            test_name += gc + '-'
                            test_name += jni + '-'
                            test_name += image + '-'
                            test_name += pictest + '-'
                            test_name += debuggable + '-'
                            test_name += test
                            test_name += address_size

                            options_test = options_all

                            if target is 'host':
                              options_test += ' --host'

                            if run is 'ndebug':
                              options_test += ' --O'

                            if prebuild is 'prebuild':
                              options_test += ' --prebuild'
                            elif prebuild is 'no-prebuild':
                              options_test += ' --no-prebuild'
                            elif prebuild is 'no-dex2oat':
                              options_test += ' --no-prebuild --no-dex2oat'

                            if compiler is 'optimizing':
                              options_test += ' --optimizing'
                            elif compiler is 'regalloc_gc':
                              options_test += ' --optimizing -Xcompiler-option --register-allocation-strategy=graph-color'
                            elif compiler is 'interpreter':
                              options_test += ' --interpreter'
                            elif compiler is 'interp-ac':
                              options_test += ' --interpreter --verify-soft-fail'
                            elif compiler is 'jit':
                              options_test += ' --jit'

                            if relocate is 'relocate':
                              options_test += ' --relocate'
                            elif relocate is 'no-relocate':
                              options_test += ' --no-relocate'
                            elif relocate is 'relocate-npatchoat':
                              options_test += ' --relocate --no-patchoat'

                            if trace is 'trace':
                              options_test += ' --trace'
                            elif trace is 'stream':
                              options_test += ' --trace --stream'

                            if gc is 'gcverify':
                              options_test += ' --gcverify'
                            elif gc is 'gcstress':
                              options_test += ' --gcstress'

                            if jni is 'forcecopy':
                              options_test += ' --runtime-option -Xjniopts:forcecopy'
                            elif jni is 'checkjni':
                              options_test += ' --runtime-option -Xcheck:jni'

                            if image is 'no-image':
                              options_test += ' --no-image'
                            elif image is 'image':
                              options_test += ' --image'
                            elif image is 'npicimage':
                              options_test += ' --npic-image'
                            elif image is 'multinpicimage':
                              options_test += ' --npic-image --multi-image'
                            elif image is 'multipicimage':
                              options_test += ' --multi-image'


                            if pictest is 'pictest':
                              options_test += ' --pictest'

                            if debuggable is 'debuggable':
                              options_test += ' --debuggable'

                            if address_size is '64':
                              options_test += ' --64'

                              if env.DEX2OAT_HOST_INSTRUCTION_SET_FEATURES:
                                options_test += ' --instruction-set-features' + env.DEX2OAT_HOST_INSTRUCTION_SET_FEATURES

                            elif address_size is '32':
                              if env.HOST_2ND_ARCH_PREFIX_DEX2OAT_HOST_INSTRUCTION_SET_FEATURES:
                                test_options += ' --instruction-set-features ' + env.HOST_2ND_ARCH_PREFIX_DEX2OAT_HOST_INSTRUCTION_SET_FEATURES


                            #options_test += ' --output-path ' + env.ART_HOST_TEST_DIR + '/run-test-output/' + test_name
                            run_test_sh = env.ANDROID_BUILD_TOP + '/art/test/run-test'
                            command = run_test_sh + ' ' + options_test + ' ' + test
                            #print options_test
                            print '###'
                            print command
                            subprocess.call(command.split())


def build_dependencies():
  prereq = env.DX + ' '
  prereq += env.JASMIN + ' '
  prereq += env.SMALI + ' '
  prereq += env.DEXMERGER + ' '
  prereq += env.DX

  if 'host' in TARGET_TYPES:
    prereq += env.ART_HOST_EXECUTABLES + ' '

def get_disabled_test_list():
  known_failures_file = env.ANDROID_BUILD_TOP + '/art/test/knownfailures.json'
  with open(known_failures_file) as known_failures_json:
    known_failures_info = json.loads(known_failures_json.read())

  disabled_test_set = {}
  for failure in known_failures_info:
    tests = failure.get("test")
    if tests:
      tests = [tests]
    else:
      tests = failure.get("tests", [])
    variants = parse_variants(failure.get('variant'))
    for test in tests:
      if test in disabled_test_set:
        disabled_test_set[test].union(variants)
      else:
        disabled_test_set[test] = variants
  return disabled_test_set

def parse_variants(variants):
  if not variants:
    variants = ""
    for variant in variant_to_type:
      variants += variant
      variants += "|"
    variants = variants[:-1]
  variant_list = Set()
  or_variants = variants.split('|')
  for or_variant in or_variants:
    and_variants = or_variant.split('&')
    variant = Set()
    for and_variant in and_variants:
      and_variant = and_variant.strip()
      variant.add(and_variant)
      variant_list.add(variant)
  return variant_list

def initializeGlobalVariables():
  global TARGET_TYPES
  global RUN_TYPES
  global PREBUILD_TYPES
  global COMPILER_TYPES
  global  RELOCATE_TYPES
  global  TRACE_TYPES
  global  GC_TYPES
  global  JNI_TYPES
  global  IMAGE_TYPES
  global  PICTEST_TYPES
  global  DEBUGGABLE_TYPES
  global  ADDRESS_SIZES
  global  OPTIMIZING_COMPILER_TYPES
  global ART_TEST_WITH_STRACE
  global  variant_to_type
  TARGET_TYPES = Set()
  RUN_TYPES = Set()
  PREBUILD_TYPES = Set()
  COMPILER_TYPES = Set()
  RELOCATE_TYPES = Set()
  TRACE_TYPES = Set()
  GC_TYPES = Set()
  JNI_TYPES = Set()
  IMAGE_TYPES = Set()
  PICTEST_TYPES = Set()
  DEBUGGABLE_TYPES = Set()
  ADDRESS_SIZES = Set()
  OPTIMIZING_COMPILER_TYPES = Set()
  variant_to_type = {}
