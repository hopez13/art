#!/usr/bin/python
"""ART Run-Test TestRunner

The testrunner runs the ART run-tests by simply invoking the script.
It fetches the list of eligible tests from art/test directory, and list of
disabled tests from art/test/knownfailures.json. It runs the tests by
invoking art/test/run-test script and parses it output to check if the test
passed or failed.

Before invoking the script, first build all the tests dependencies by
building 'test-art-host-run-test' for host tests, 'test-art-target-run-test'
for target tests, and 'test-art-run-test' for building dependencies for both.
There are various options to invoke the script which are:
-t: Either the test name as in art/test or the test name including the variant
    information. Eg, "-t 001-HelloWorld",
    "-t test-art-host-run-test-debug-prebuild-optimizing-relocate-ntrace-cms-checkjni-picimage-npictest-ndebuggable-001-HelloWorld32"
-j: Number of thread workers to be used. Eg - "-j64"
--dry-run: Instead of running the test name, just print its name.
--verbose

To specify any specific variants for the test, use --<<variant-name>>.
For eg, for compiler type as optimizing, use --optimizing.


In the end, the script will print the failed and skipped tests if any.

"""
import fnmatch
import json
from optparse import OptionParser
import os
import re
import subprocess
import sys
import threading
import time

import env

TARGET_TYPES = set()
RUN_TYPES = set()
PREBUILD_TYPES = set()
COMPILER_TYPES = set()
RELOCATE_TYPES = set()
TRACE_TYPES = set()
GC_TYPES = set()
JNI_TYPES = set()
IMAGE_TYPES = set()
PICTEST_TYPES = set()
DEBUGGABLE_TYPES = set()
ADDRESS_SIZES = set()
OPTIMIZING_COMPILER_TYPES = set()
DISABLED_TEST_LIST = set()
ADDRESS_SIZES_TARGET = {'host': set(), 'target': set()}
VARIANT_TYPE = {}
COLOR_ERROR = '\033[91m'
COLOR_PASS = '\033[92m'
COLOR_SKIP = '\033[93m'
COLOR_NORMAL = '\033[0m'
TOTAL_VARIANTS = set()
test_count_mutex = threading.Lock()
RUN_TEST_LIST = set()
semaphore = threading.Semaphore(1)

# Flags
n_thread = 1
print_mutex = threading.Lock()
test_count = 0
total_test_count = 0
verbose = False
last_print_length = 0
dry_run = False
failed_tests = []
skipped_tests = []
build = False


def gather_test_info():
  """The method gathers test information about the test to be run which includes
  generating list of total tests from art/test and list of disabled test.
  It also maps various variant to types.
  """
  global TOTAL_VARIANTS
  VARIANT_TYPE['pictest'] = {'pictest', 'npictest'}
  VARIANT_TYPE['run'] = {'ndebug', 'debug'}
  VARIANT_TYPE['target'] = {'target', 'host'}
  VARIANT_TYPE['trace'] = {'trace', 'ntrace'}
  VARIANT_TYPE['image'] = {'image', 'picimage', 'no-image'}
  VARIANT_TYPE['debuggable'] = {'ndebuggable', 'debuggable'}
  VARIANT_TYPE['gc'] = {'gcstress', 'gcverify', 'cms'}
  VARIANT_TYPE['prebuild'] = {'no-prebuild', 'no-dex2oat', 'prebuild'}
  VARIANT_TYPE['relocate'] = {'relocate-npatchoat', 'relocate', 'nrelocate'}
  VARIANT_TYPE['jni'] = {'jni', 'forcecopy', 'checkjni'}
  VARIANT_TYPE['address_sizes'] = {'64', '32'}
  VARIANT_TYPE['compiler'] = {'interp-ac', 'interpreter', 'jit', 'optimizing'}

  for v_type in VARIANT_TYPE:
    TOTAL_VARIANTS = TOTAL_VARIANTS.union(VARIANT_TYPE.get(v_type))

  test_dir = env.ANDROID_BUILD_TOP + '/art/test'
  for f in os.listdir(test_dir):
    if fnmatch.fnmatch(f, '[0-9]*'):
      RUN_TEST_LIST.add(f)

  global DISABLED_TEST_LIST
  DISABLED_TEST_LIST = get_disabled_test_list()


def setup_test_env():
  """The method sets default value for the various variants of the tests if they
  are already not set.
  """
  if env.ART_TEST_BISECTION:
    # Need to keep rebuilding the test to bisection search it.
    env.ART_TEST_RUN_TEST_NO_PREBUILD = True
    env.ART_TEST_RUN_TEST_PREBUILD = False
    # Bisection search writes to standard output.
    env.ART_TEST_QUIET = False

  if not TARGET_TYPES:
    TARGET_TYPES.add('host')
    TARGET_TYPES.add('target')

  if not PREBUILD_TYPES:
    if env.ART_TEST_RUN_TEST_PREBUILD:
      PREBUILD_TYPES.add('prebuild')
    if env.ART_TEST_RUN_TEST_NO_PREBUILD:
      PREBUILD_TYPES.add('no-prebuild')
    if env.ART_TEST_RUN_TEST_NO_DEX2OAT:
      PREBUILD_TYPES.add('no-dex2oat')

  if not COMPILER_TYPES:
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

  if not RELOCATE_TYPES:
    RELOCATE_TYPES.add('relocate')
    if env.ART_TEST_RUN_TEST_NO_RELOCATE:
      RELOCATE_TYPES.add('no-relocate')
    if env.ART_TEST_RUN_TEST_RELOCATE_NO_PATCHOAT:
      RELOCATE_TYPES.add('relocate-npatchoat')

  if not TRACE_TYPES:
    TRACE_TYPES.add('ntrace')
    if env.ART_TEST_TRACE:
      TRACE_TYPES.add('trace')
    if env.ART_TEST_TRACE_STREAM:
      TRACE_TYPES.add('stream')

  if not GC_TYPES:
    GC_TYPES.add('cms')
    if env.ART_TEST_GC_STRESS:
      GC_TYPES.add('gcstress')
    if env.ART_TEST_GC_VERIFY:
      GC_TYPES.add('gcverify')

  if not JNI_TYPES:
    JNI_TYPES.add('checkjni')
    if env.ART_TEST_JNI_FORCECOPY:
      JNI_TYPES.add('forcecopy')

  if not IMAGE_TYPES:
    IMAGE_TYPES.add('picimage')
    if env.ART_TEST_RUN_TEST_NO_IMAGE:
      IMAGE_TYPES.add('no-image')
    if env.ART_TEST_RUN_TEST_MULTI_IMAGE:
      IMAGE_TYPES.add('multipicimage')
    if env.ART_TEST_NPIC_IMAGE:
      IMAGE_TYPES.add('npicimage')
    if env.ART_TEST_RUN_TEST_MULTI_IMAGE:
      IMAGE_TYPES.add('multinpicimage')

  if not PICTEST_TYPES:
    PICTEST_TYPES.add('npictest')
    if env.ART_TEST_PIC_TEST:
      PICTEST_TYPES.add('pictest')

  if not RUN_TYPES:
    if env.ART_TEST_RUN_TEST_DEBUG:
      RUN_TYPES.add('debug')
    if env.ART_TEST_RUN_TEST_NDEBUG:
      RUN_TYPES.add('ndebug')

  if not DEBUGGABLE_TYPES:
    DEBUGGABLE_TYPES.add('ndebuggable')

    if env.ART_TEST_RUN_TEST_DEBUGGABLE:
      DEBUGGABLE_TYPES.add('debuggable')

  ADDRESS_SIZES_TARGET['target'].add(env.ART_PHONY_TEST_TARGET_SUFFIX)
  ADDRESS_SIZES_TARGET['host'].add(env.ART_PHONY_TEST_HOST_SUFFIX)
  if not ADDRESS_SIZES:
    ADDRESS_SIZES_TARGET['target'].add(env.ART_PHONY_TEST_TARGET_SUFFIX)
    ADDRESS_SIZES_TARGET['host'].add(env.ART_PHONY_TEST_HOST_SUFFIX)
    if env.ART_TEST_RUN_TEST_2ND_ARCH:
      ADDRESS_SIZES_TARGET['host'].add(env._2ND_ART_PHONY_TEST_HOST_SUFFIX)
      ADDRESS_SIZES_TARGET['target'].add(env._2ND_ART_PHONY_TEST_TARGET_SUFFIX)
  else:
    ADDRESS_SIZES_TARGET['host'] = ADDRESS_SIZES_TARGET['host'].union(ADDRESS_SIZES)
    ADDRESS_SIZES_TARGET['target'] = ADDRESS_SIZES_TARGET['target'].union(ADDRESS_SIZES)

  global semaphore
  semaphore = threading.Semaphore(n_thread)


def run_tests(tests):
  """Creates thread workers to run the tests.

  The method generates command and thread worker to run the tests. Depending upon the
  user input for the number of threads to be used, the method uses a semaphore object to
  keep a count in control for the thread workers. As soon as a new worker is created,
  the semaphore gets acquired. When the worker ends, it releases the semaphore.

  Args:
    tests: The set of tests to be run.
  """
  options_all = ''
  global total_test_count
  total_test_count = len(tests)
  total_test_count *= len(RUN_TYPES)
  total_test_count *= len(PREBUILD_TYPES)
  total_test_count *= len(RELOCATE_TYPES)
  total_test_count *= len(TRACE_TYPES)
  total_test_count *= len(GC_TYPES)
  total_test_count *= len(JNI_TYPES)
  total_test_count *= len(IMAGE_TYPES)
  total_test_count *= len(PICTEST_TYPES)
  total_test_count *= len(DEBUGGABLE_TYPES)
  total_test_count *= len(COMPILER_TYPES)
  target_address_combinations = 0
  for target in TARGET_TYPES:
    for address_size in ADDRESS_SIZES_TARGET[target]:
      target_address_combinations += 1
  total_test_count *= target_address_combinations

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
                          for address_size in ADDRESS_SIZES_TARGET[target]:
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

                            variant_set = {target, run, prebuild, compiler,
                                           relocate, trace, gc, jni, image,
                                           pictest, debuggable, address_size}

                            output_dir = env.ART_HOST_TEST_DIR + '/run-test-output/' + test_name
                            options_test = ' --output-path ' + output_dir
                            options_test = options_all

                            if target == 'host':
                              options_test += ' --host'

                            if run == 'ndebug':
                              options_test += ' -O'

                            if prebuild == 'prebuild':
                              options_test += ' --prebuild'
                            elif prebuild == 'no-prebuild':
                              options_test += ' --no-prebuild'
                            elif prebuild == 'no-dex2oat':
                              options_test += ' --no-prebuild --no-dex2oat'

                            if compiler == 'optimizing':
                              options_test += ' --optimizing'
                            elif compiler == 'regalloc_gc':
                              options_test += ' --optimizing -Xcompiler-option --register-allocation-strategy=graph-color'
                            elif compiler == 'interpreter':
                              options_test += ' --interpreter'
                            elif compiler == 'interp-ac':
                              options_test += ' --interpreter --verify-soft-fail'
                            elif compiler == 'jit':
                              options_test += ' --jit'

                            if relocate == 'relocate':
                              options_test += ' --relocate'
                            elif relocate == 'no-relocate':
                              options_test += ' --no-relocate'
                            elif relocate == 'relocate-npatchoat':
                              options_test += ' --relocate --no-patchoat'

                            if trace == 'trace':
                              options_test += ' --trace'
                            elif trace == 'stream':
                              options_test += ' --trace --stream'

                            if gc == 'gcverify':
                              options_test += ' --gcverify'
                            elif gc == 'gcstress':
                              options_test += ' --gcstress'

                            if jni == 'forcecopy':
                              options_test += ' --runtime-option -Xjniopts:forcecopy'
                            elif jni == 'checkjni':
                              options_test += ' --runtime-option -Xcheck:jni'

                            if image == 'no-image':
                              options_test += ' --no-image'
                            elif image == 'image':
                              options_test += ' --image'
                            elif image == 'npicimage':
                              options_test += ' --npic-image'
                            elif image == 'multinpicimage':
                              options_test += ' --npic-image --multi-image'
                            elif image == 'multipicimage':
                              options_test += ' --multi-image'

                            if pictest == 'pictest':
                              options_test += ' --pic-test'

                            if debuggable == 'debuggable':
                              options_test += ' --debuggable'

                            if address_size == '64':
                              options_test += ' --64'

                              if env.DEX2OAT_HOST_INSTRUCTION_SET_FEATURES:
                                options_test += ' --instruction-set-features' + env.DEX2OAT_HOST_INSTRUCTION_SET_FEATURES

                            elif address_size == '32':
                              if env.HOST_2ND_ARCH_PREFIX_DEX2OAT_HOST_INSTRUCTION_SET_FEATURES:
                                test_options += ' --instruction-set-features ' + env.HOST_2ND_ARCH_PREFIX_DEX2OAT_HOST_INSTRUCTION_SET_FEATURES

                            run_test_sh = env.ANDROID_BUILD_TOP + '/art/test/run-test'
                            command = run_test_sh + ' ' + options_test + ' ' + test
                            #run_test(command, test, variant_set, test_name)
                            semaphore.acquire()
                            worker = threading.Thread(target=run_test, args=(command, test, variant_set, test_name))
                            worker.daemon = True
                            worker.start()
  while threading.active_count() > 2:
    time.sleep(0.1)


def run_test(command, test, test_variant, test_name):
  """Runs the test.

  It invokes art/test/run-test script to run the test. The output of the script is
  checked, and if it ends with "Succeeded!", it assumes that the tests passed, otherwise,
  put it in the list of failed test. Before actually running the test, it also checks if
  the tests is placed in the list of disabled tests, and if yes, it skips running it, and
  adds the test in the list of skipped tests. The method uses print_text method to actually
  print the output. After successfully running and capturing the output for the test, it
  releases the semaphore object.

  Args:
    command: The command to be used to invoke the script
    test: The name of the test without the variant information.
    test_variant: The set of variant for the test.
    test_name: The name of the test along with the variants.
  """
  global last_print_length
  global test_count
  if is_test_disabled(test, test_variant):
    result = ''
  else:
    proc = subprocess.Popen(command.split(), stderr=subprocess.PIPE)
    result = proc.stderr.read().strip()

  if not verbose:
    suffix = '\r'
    prefix = ' ' * last_print_length + '\r'
  else:
    suffix = '\n'
    prefix = ''
  test_count_mutex.acquire()
  test_count += 1
  percent = (test_count * 100) / total_test_count
  out = '[ ' + str(percent) + '% ' + str(test_count) + '/' + str(total_test_count) + ' ] '
  test_count_mutex.release()
  out += test_name + ' '
  if result:
    if result.endswith('succeeded!'):
      out += COLOR_PASS + 'PASS' + COLOR_NORMAL
    else:
      out += COLOR_ERROR + 'FAIL' + COLOR_NORMAL
      failed_tests.append(test_name)
      if verbose:
        out += '\n' + command + '\n' + result
  elif not dry_run:
    out += COLOR_SKIP + 'SKIP' + COLOR_NORMAL
    skipped_tests.append(test_name)
  last_print_length = len(out)
  print_mutex.acquire()
  print_text(prefix + out + suffix)
  print_mutex.release()
  semaphore.release()


def get_disabled_test_list():
  """Generate set of known failures.

  It parses art/test/knownfailures.json file to generate list of disabled tests.
  """
  known_failures_file = env.ANDROID_BUILD_TOP + '/art/test/knownfailures.json'
  with open(known_failures_file) as known_failures_json:
    known_failures_info = json.loads(known_failures_json.read())

  disabled_test_set = {}
  for failure in known_failures_info:
    tests = failure.get('test')
    if tests:
      tests = [tests]
    else:
      tests = failure.get('tests', [])
    variants = parse_variants(failure.get('variant'))
    env_vars = failure.get('env_vars')
    if check_env_vars(env_vars):
      for test in tests:
        if test in disabled_test_set:
          disabled_test_set[test] = disabled_test_set[test].union(variants)
        else:
          disabled_test_set[test] = variants
  return disabled_test_set


def check_env_vars(env_vars):
  """Checks if the env variables are set as required to run the test.

  Returns:
    True if all the env variables are set as required, otherwise False.
  """

  if not env_vars:
    return True
  for key in env_vars:
    if env.get_env(key) != env_vars.get(key):
      return False
  return True


def is_test_disabled(test, variant_set):
  """Checks if the test along with the variant_set is disabled.

  Args:
    test: The name of the test as in art/test directory.
    variant_set: Variants to be used for the test.
  Returns:
   True, if the test is disabled.
  """
  if dry_run:
    return True
  variants_list = DISABLED_TEST_LIST.get(test, {})
  for variants in variants_list:
    variants_present = True
    for variant in variants:
      if variant not in variant_set:
        variants_present = False
        break
    if variants_present:
      return True
  return False


def parse_variants(variants):
  """Parse variants fetched from art/test/knownfailures.
  """
  if not variants:
    variants = ''
    for variant in TOTAL_VARIANTS:
      variants += variant
      variants += '|'
    variants = variants[:-1]
  variant_list = set()
  or_variants = variants.split('|')
  for or_variant in or_variants:
    and_variants = or_variant.split('&')
    variant = set()
    for and_variant in and_variants:
      and_variant = and_variant.strip()
      variant.add(and_variant)
    variant_list.add(frozenset(variant))
  return variant_list


def print_text(output):
  sys.stdout.write(output)
  sys.stdout.flush()


def print_analysis():
  if not verbose:
    print_text(' ' * last_print_length + '\r')
  if skipped_tests:
    print_text(COLOR_SKIP + 'SKIPPED TESTS' + COLOR_NORMAL + '\n')
    for test in skipped_tests:
      print_text(test + '\n')
    print_text('\n')

  if failed_tests:
    print_text(COLOR_ERROR + 'FAILED TESTS' + COLOR_NORMAL + '\n')
    for test in failed_tests:
      print_text(test + '\n')


def parse_test_name(test_name):
  if test_name in RUN_TEST_LIST:
    return {test_name}

  regex = '^test-art-'
  regex += '(' + '|'.join(VARIANT_TYPE['target']) + ')-'
  regex += 'run-test-'
  regex += '(' + '|'.join(VARIANT_TYPE['run']) + ')-'
  regex += '(' + '|'.join(VARIANT_TYPE['prebuild']) + ')-'
  regex += '(' + '|'.join(VARIANT_TYPE['compiler']) + ')-'
  regex += '(' + '|'.join(VARIANT_TYPE['relocate']) + ')-'
  regex += '(' + '|'.join(VARIANT_TYPE['trace']) + ')-'
  regex += '(' + '|'.join(VARIANT_TYPE['gc']) + ')-'
  regex += '(' + '|'.join(VARIANT_TYPE['jni']) + ')-'
  regex += '(' + '|'.join(VARIANT_TYPE['image']) + ')-'
  regex += '(' + '|'.join(VARIANT_TYPE['pictest']) + ')-'
  regex += '(' + '|'.join(VARIANT_TYPE['debuggable']) + ')-'
  regex += '(' + '|'.join(RUN_TEST_LIST) + ')'
  regex += '(' + '|'.join(VARIANT_TYPE['address_sizes']) + ')$'
  match = re.match(regex, test_name)
  if match:
    TARGET_TYPES.add(match.group(1))
    RUN_TYPES.add(match.group(2))
    PREBUILD_TYPES.add(match.group(3))
    COMPILER_TYPES.add(match.group(4))
    RELOCATE_TYPES.add(match.group(5))
    TRACE_TYPES.add(match.group(6))
    GC_TYPES.add(match.group(7))
    JNI_TYPES.add(match.group(8))
    IMAGE_TYPES.add(match.group(9))
    PICTEST_TYPES.add(match.group(10))
    DEBUGGABLE_TYPES.add(match.group(11))
    ADDRESS_SIZES.add(match.group(13))
    return {match.group(12)}


def parse_option():
  global verbose
  global dry_run
  global n_thread
  global build

  parser = OptionParser()
  parser.add_option('-t', '--test', dest='test', help='name of the test')
  parser.add_option('-j', type='int', dest='n_thread')
  parser.add_option('--pictest', action='store_true', dest='pictest')
  parser.add_option('--ndebug', action='store_true', dest='ndebug')
  parser.add_option('--image', action='store_true', dest='image')
  parser.add_option('--interp-ac', action='store_true', dest='interp_ac')
  parser.add_option('--picimage', action='store_true', dest='picimage')
  parser.add_option('--64', action='store_true', dest='n64')
  parser.add_option('--interpreter', action='store_true', dest='interpreter')
  parser.add_option('--jni', action='store_true', dest='jni')
  parser.add_option('--relocate-npatchoat', action='store_true', dest='relocate_npatchoat')
  parser.add_option('--no-prebuild', action='store_true', dest='no_prebuild')
  parser.add_option('--npictest', action='store_true', dest='npictest')
  parser.add_option('--no-dex2oat', action='store_true', dest='no_dex2oat')
  parser.add_option('--jit', action='store_true', dest='jit')
  parser.add_option('--relocate', action='store_true', dest='relocate')
  parser.add_option('--ndebuggable', action='store_true', dest='ndebuggable')
  parser.add_option('--no-image', action='store_true', dest='no_image')
  parser.add_option('--optimizing', action='store_true', dest='optimizing')
  parser.add_option('--trace', action='store_true', dest='trace')
  parser.add_option('--gcstress', action='store_true', dest='gcstress')
  parser.add_option('--nrelocate', action='store_true', dest='nrelocate')
  parser.add_option('--target', action='store_true', dest='target')
  parser.add_option('--forcecopy', action='store_true', dest='forcecopy')
  parser.add_option('--32', action='store_true', dest='n32')
  parser.add_option('--host', action='store_true', dest='host')
  parser.add_option('--gcverify', action='store_true', dest='gcverify')
  parser.add_option('--debuggable', action='store_true', dest='debuggable')
  parser.add_option('--prebuild', action='store_true', dest='prebuild')
  parser.add_option('--debug', action='store_true', dest='debug')
  parser.add_option('--checkjni', action='store_true', dest='checkjni')
  parser.add_option('--ntrace', action='store_true', dest='ntrace')
  parser.add_option('--cms', action='store_true', dest='cms')
  parser.add_option('--verbose', '-v', action='store_true', dest='verbose')
  parser.add_option('--dry-run', action='store_true', dest='dry_run')
  parser.add_option('-b', '--build', action='store_true', dest='build')

  options = parser.parse_args()[0]
  test = ''
  if options.test:
    test = parse_test_name(options.test)
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
    ADDRESS_SIZES.add('64')
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
    ADDRESS_SIZES.add('32')
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
  if options.verbose:
    verbose = True
  if options.n_thread:
    n_thread = max(n_thread, options.n_thread)
  if options.dry_run:
    dry_run = True
    verbose = True
  if options.build:
    build = True

  return test

if __name__ == '__main__':
  gather_test_info()
  user_requested_test = parse_option()
  setup_test_env()
  if build:
    build_targets = ''
    if 'host' in TARGET_TYPES:
      build_targets += 'test-art-host-run-test-dependencies'
    if 'target' in TARGET_TYPES:
      build_targets += 'test-art-target-run-test-dependencies'
    build_command = 'make -j' + str(n_thread) + ' ' + build_targets
    if subprocess.call(build_command.split()):
      sys.exit(1)
  if user_requested_test:
    test_runner_thread = threading.Thread(target=run_tests, args=(user_requested_test,))
  else:
    test_runner_thread = threading.Thread(target=run_tests, args=(RUN_TEST_LIST,))
  test_runner_thread.daemon = True
  try:
    test_runner_thread.start()
    while threading.active_count() > 1:
      time.sleep(0.1)
    print_analysis()
    if failed_tests:
      sys.exit(1)
    sys.exit(0)
  except KeyboardInterrupt:
    print_analysis()
