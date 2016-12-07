import json
from sets import Set
import os
import env_param
import fnmatch

def initialize_env():
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

  if env_param.ART_TEST_BISECTION:
    # Need to keep rebuilding the test to bisection search it.
    env_param.ART_TEST_RUN_TEST_NO_PREBUILD = True
    env_param.ART_TEST_RUN_TEST_PREBUILD := False
    # Bisection search writes to standard output.
    env_param.ART_TEST_QUIET = False

  TARGET_TYPES.add('host')
  TARGET_TYPES.add('target')

  if env_param.ART_TEST_RUN_TEST_PREBUILD:
    PREBUILD_TYPES.add('prebuild')
  if env_param.ART_TEST_RUN_TEST_NO_PREBUILD:
    PREBUILD_TYPES.add('no-prebuild')
  if env_param.ART_TEST_RUN_TEST_NO_DEX2OAT:
    PREBUILD_TYPES.add('no-dex2oat')

  if env_param.ART_TEST_INTERPRETER_ACCESS_CHECKS:
    COMPILER_TYPES.add('interp-ac')
  if env_param.ART_TEST_INTERPRETER:
		COMPILER_TYPES.add('interpreter')
  if env_param.ART_TEST_JIT:
    COMPILER_TYPES.add('jit')

  if env_param.ART_TEST_OPTIMIZING:
    COMPILER_TYPES.add('optimizing')
    OPTIMIZING_COMPILER_TYPES.add('optimizing')
  if env_param.ART_TEST_OPTIMIZING_GRAPH_COLOR:
    COMPILER_TYPES.add('regalloc_gc')
    OPTIMIZING_COMPILER_TYPES.add('regalloc_gc')

  RELOCATE_TYPES.add('relocate')
  if env_param.ART_TEST_RUN_TEST_NO_RELOCATE:
    RELOCATE_TYPES.add('no-relocate')
  if env_param.ART_TEST_RUN_TEST_RELOCATE_NO_PATCHOAT:
    RELOCATE_TYPES.add('relocate-npatchoat')

  TRACE_TYPES.add('ntrace')
  if env_param.ART_TEST_TRACE:
    TRACE_TYPES.add('trace')
  if env_param.ART_TEST_TRACE_STREAM:
    TRACE_TYPES.add('stream')

  GC_TYPES.add('cms')
  if env_param.ART_TEST_GC_STRESS:
    GC_TYPES.add('gcstress')
  if env_param.ART_TEST_GC_VERIFY:
    GC_TYPES.add('gcverify')

  JNI_TYPES.add('checkjni')
  if env_param.ART_TEST_JNI_FORCECOPY:
    JNI_TYPES.add('forcecopy')

  IMAGE_TYPES.add('picimage')
  if env_param.ART_TEST_RUN_TEST_NO_IMAGE:
    IMAGE_TYPES.add('no-image')
  if env_param.ART_TEST_RUN_TEST_MULTI_IMAGE:
    IMAGE_TYPES.add('multipicimage')
  if env_param.ART_TEST_NPIC_IMAGE:
		IMAGE_TYPES.add('npicimage')
  if env_param.ART_TEST_RUN_TEST_MULTI_IMAGE:
    IMAGE_TYPES.add('multinpicimage')

  PICTEST_TYPES.add('npictest')
  if env_param.ART_TEST_PIC_TEST:
    PICTEST_TYPES.add('pictest')

  if env_param.ART_TEST_RUN_TEST_DEBUG:
    RUN_TYPES.add('debug')
  if env_param.ART_TEST_RUN_TEST_NDEBUG:
    RUN_TYPES.add('ndebug')

  DEBUGGABLE_TYPES.add('ndebuggable')
  if env_param.ART_TEST_RUN_TEST_DEBUGGABLE:
    DEBUGGABLE_TYPES.add('debuggable')

  #    ADDRESS_SIZES_TARGET.add(env_param.ART_PHONY_TEST_TARGET_SUFFIX)
	#    ADDRESS_SIZES_HOST.add(env.ART_PHONY_TEST_HOST_SUFFIX)
	#    if env.get('ART_TEST_RUN_TEST_2ND_ARCH'):
  #        ADDRESS_SIZES_TARGET.add(env_param.2ND_ART_PHONY_TEST_TARGET_SUFFIX)
  #        ADDRESS_SIZES_HOST.add(env_param.2ND_ART_PHONY_TEST_HOST_SUFFIX)

  ADDRESS_SIZES.add('64')
  ADDRESS_SIZES.add('32')

  ART_TEST_WITH_STRACE = env_param.ART_TEST_DEBUG_GC

def run_test(tests=None):
  test_dependencies = find_dependencies_for_tests(tests)
  if not tests:
    tests = []
    test_dir = env_param.ANDROID_BUILD_TOP + '/art/test'
    for file in os.listdir(test_dir):
      if fnmatch.fnmatch(file, '[0-9]*'):
        tests.append(file)



  test_art_run_test_dependencies = env_param.DX + ' '
  test_art_run_test_dependencies += env_param.JASMIN + ' '
  test_art_run_test_dependencies += env_param.SMALI + ' '
  test_art_run_test_dependencies += env_param.DEXMERGER + ' '
  test_art_run_test_dependencies += env_param.DX

  run_test_options = ''

  if ART_TEST_WITH_STRACE:
    run_test_options += ' --strace'

  if env_param.ART_TEST_RUN_TEST_ALWAYS_CLEAN:
    run_test_options += ' --always-clean'

  if env.ART_TEST_BISECTION:
    run_test_options += ' --bisection-search'

  for target in TARGET_TYPES:
    if target == 'host':
      run_test_options += ' --host'

def find_dependencies_for_tests(tests):
  variants = []
  if not tests:
    variants = variant_to_type.keys()
  else:
    tests

def get_disabled_test_list():
  known_failures_file = env_param.ANDROID_BUILD_TOP + '/art/test/knownfailures.json'
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
