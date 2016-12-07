import json
from sets import Set
import os
import env
import fnmatch
import subprocess
import threading
from optparse import OptionParser

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
DISABLED_TEST_LIST = Set()
variant_to_type = {}
n_thread = 1

def setup():
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

  if len(TARGET_TYPES) == 0:
    TARGET_TYPES.add('host')
    #TARGET_TYPES.add('target')

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

  if len(RELOCATE_TYPES) == 0:
    RELOCATE_TYPES.add('relocate')
  if env.ART_TEST_RUN_TEST_NO_RELOCATE:
    RELOCATE_TYPES.add('no-relocate')
  if env.ART_TEST_RUN_TEST_RELOCATE_NO_PATCHOAT:
    RELOCATE_TYPES.add('relocate-npatchoat')

  if len(TRACE_TYPES) == 0:
    TRACE_TYPES.add('ntrace')
  if env.ART_TEST_TRACE:
    TRACE_TYPES.add('trace')
  if env.ART_TEST_TRACE_STREAM:
    TRACE_TYPES.add('stream')

  if len(GC_TYPES) == 0:
    GC_TYPES.add('cms')
  if env.ART_TEST_GC_STRESS:
    GC_TYPES.add('gcstress')
  if env.ART_TEST_GC_VERIFY:
    GC_TYPES.add('gcverify')

  if len(JNI_TYPES) == 0:
    JNI_TYPES.add('checkjni')
  if env.ART_TEST_JNI_FORCECOPY:
    JNI_TYPES.add('forcecopy')

  if len(IMAGE_TYPES) == 0:
    IMAGE_TYPES.add('picimage')
  if env.ART_TEST_RUN_TEST_NO_IMAGE:
    IMAGE_TYPES.add('no-image')
  if env.ART_TEST_RUN_TEST_MULTI_IMAGE:
    IMAGE_TYPES.add('multipicimage')
  if env.ART_TEST_NPIC_IMAGE:
		IMAGE_TYPES.add('npicimage')
  if env.ART_TEST_RUN_TEST_MULTI_IMAGE:
    IMAGE_TYPES.add('multinpicimage')

  if len(PICTEST_TYPES) == 0:
    PICTEST_TYPES.add('npictest')
  if env.ART_TEST_PIC_TEST:
    PICTEST_TYPES.add('pictest')

  if env.ART_TEST_RUN_TEST_DEBUG:
    RUN_TYPES.add('debug')
  if env.ART_TEST_RUN_TEST_NDEBUG:
    RUN_TYPES.add('ndebug')

  if len(DEBUGGABLE_TYPES) == 0:
    DEBUGGABLE_TYPES.add('ndebuggable')

  if env.ART_TEST_RUN_TEST_DEBUGGABLE:
    DEBUGGABLE_TYPES.add('debuggable')

  #    ADDRESS_SIZES_TARGET.add(env.ART_PHONY_TEST_TARGET_SUFFIX)
	#    ADDRESS_SIZES_HOST.add(env.ART_PHONY_TEST_HOST_SUFFIX)
	#    if env.get('ART_TEST_RUN_TEST_2ND_ARCH'):
  #        ADDRESS_SIZES_TARGET.add(env.2ND_ART_PHONY_TEST_TARGET_SUFFIX)
  #        ADDRESS_SIZES_HOST.add(env.2ND_ART_PHONY_TEST_HOST_SUFFIX)

  if len(ADDRESS_SIZES) == 0:
    ADDRESS_SIZES.add('64')
    ADDRESS_SIZES.add('32')

  ART_TEST_WITH_STRACE = env.ART_TEST_DEBUG_GC

  global semaphore
  semaphore = threading.Semaphore(n_thread)
  global DISABLED_TEST_LIST
  DISABLED_TEST_LIST = get_disabled_test_list()


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

                            variant_set = {target, run, prebuild, compiler, relocate, trace, gc, jni, image, pictest, debuggable, address_size}

                            options_test = ' --output-path ' + env.ART_HOST_TEST_DIR + '/run-test-output/' + test_name
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

                            run_test_sh = env.ANDROID_BUILD_TOP + '/art/test/run-test'
                            command = run_test_sh + ' ' + options_test + ' ' + test
                            semaphore.acquire()
                            worker = threading.Thread(target = run_test, args = (command, test, variant_set, test_name))
                            worker.start()


def run_test(command, test, test_variant, test_name):
  if is_test_disabled(test, test_variant):
    print test_name + ": Skipped"
    return
  proc = subprocess.Popen(command.split(), stderr=subprocess.PIPE)
  if proc.stderr.read().strip().endswith('succeeded!'):
    print test_name + ': Passed'
  else:
    print test_name + ': Failed'
  semaphore.release()

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

def is_test_disabled(test, variant_set):
  variants_list = DISABLED_TEST_LIST.get(test, {})
  for variants in variants_list:
    variants_present = True
    for variant in variants:
      if not variant in variant_set:
        variants_present = False
        break
    if variants_present:
      return True
  return False

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

def parse_option():
  parser = OptionParser()
  parser.add_option("-t", "--test", dest="test", help="name of the test")
  parser.add_option("-j", type="int", dest="n_thread")
  parser.add_option("--pictest", action="store_true", dest="pictest")
  parser.add_option("--ndebug", action="store_true", dest="ndebug")
  parser.add_option("--image", action="store_true", dest="image")
  parser.add_option("--interp-ac", action="store_true", dest="interp_ac")
  parser.add_option("--picimage", action="store_true", dest="picimage")
  parser.add_option("--64", action="store_true", dest="n64")
  parser.add_option("--interpreter", action="store_true", dest="interpreter")
  parser.add_option("--jni", action="store_true", dest="jni")
  parser.add_option("--relocate-npatchoat", action="store_true", dest="relocate_npatchoat")
  parser.add_option("--no-prebuild", action="store_true", dest="no_prebuild")
  parser.add_option("--npictest", action="store_true", dest="npictest")
  parser.add_option("--no-dex2oat", action="store_true", dest="no_dex2oat")
  parser.add_option("--jit", action="store_true", dest="jit")
  parser.add_option("--relocate", action="store_true", dest="relocate")
  parser.add_option("--ndebuggable", action="store_true", dest="ndebuggable")
  parser.add_option("--no-image", action="store_true", dest="no_image")
  parser.add_option("--optimizing", action="store_true", dest="optimizing")
  parser.add_option("--trace", action="store_true", dest="trace")
  parser.add_option("--gcstress", action="store_true", dest="gcstress")
  parser.add_option("--nrelocate", action="store_true", dest="nrelocate")
  parser.add_option("--target", action="store_true", dest="target")
  parser.add_option("--forcecopy", action="store_true", dest="forcecopy")
  parser.add_option("--32", action="store_true", dest="n32")
  parser.add_option("--host", action="store_true", dest="host")
  parser.add_option("--gcverify", action="store_true", dest="gcverify")
  parser.add_option("--debuggable", action="store_true", dest="debuggable")
  parser.add_option("--prebuild", action="store_true", dest="prebuild")
  parser.add_option("--debug", action="store_true", dest="debug")
  parser.add_option("--checkjni", action="store_true", dest="checkjni")
  parser.add_option("--ntrace", action="store_true", dest="ntrace")
  parser.add_option("--cms", action="store_true", dest="cms")

  (options, args) =  parser.parse_args()

  if options.pictest:
    PICTEST_TYPES.add('pictest')
  if options.ndebug:
    RUN_TYPES.add('ndebug')
  if options.image:
    IMAGE_TYPES.add('image')
  if options.interp_ac:
    COMPILER_TYPES.add('interp-ac')
  if options.picimage:
    IMAGE_TYPES.add('picimage')
  if options.n64:
    ADDRESS_SIZES_TYPES.add('64')
  if options.interpreter:
    COMPILER_TYPES.add('interpreter')
  if options.jni:
    JNI_TYPES.add('jni')
  if options.relocate_npatchoat:
    RELOCATE_TYPES.add('relocate-npatchoat')
  if options.no_prebuild:
    PREBUILD_TYPES.add('no-prebuild')
  if options.npictest:
    PICTEST_TYPES.add('npictest')
  if options.no_dex2oat:
    PREBUILD_TYPES.add('no-dex2oat')
  if options.jit:
    COMPILER_TYPES.add('jit')
  if options.relocate:
    RELOCATE_TYPES.add('relocate')
  if options.ndebuggable:
    DEBUGGABLE_TYPES.add('ndebuggable')
  if options.no_image:
    IMAGE_TYPES.add('no-image')
  if options.optimizing:
    COMPILER_TYPES.add('optimizing')
  if options.trace:
    TRACE_TYPES.add('trace')
  if options.gcstress:
    GC_TYPES.add('gcstress')
  if options.nrelocate:
    RELOCATE_TYPES.add('nrelocate')
  if options.target:
    TARGET_TYPES.add('target')
  if options.forcecopy:
    JNI_TYPES.add('forcecopy')
  if options.n32:
    ADDRESS_SIZES_TYPES.add('32')
  if options.host:
    TARGET_TYPES.add('host')
  if options.gcverify:
    GC_TYPES.add('gcverify')
  if options.debuggable:
    DEBUGGABLE_TYPES.add('debuggable')
  if options.prebuild:
    PREBUILD_TYPES.add('prebuild')
  if options.debug:
    RUN_TYPES.add('debug')
  if options.checkjni:
    JNI_TYPES.add('checkjni')
  if options.ntrace:
    TRACE_TYPES.add('ntrace')
  if options.cms:
    GC_TYPES.add('cms')
  global n_thread
  if options.n_thread:
    n_thread = max(n_thread, options.n_thread)

  return options.test

if __name__ == "__main__":
  test = parse_option()
  setup()
  if test:
    run_tests([test])
  else:
    run_tests()
