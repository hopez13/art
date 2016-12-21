#!/usr/bin/python3 -E
#
# Copyright (C) 2016 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""
Make an execution plan then execute a set of run-tests.
"""

import argparse
import collections
import concurrent.futures
import subprocess
import itertools
import sys
import xml.etree.ElementTree as ET

class SkipGroup(collections.namedtuple("SkipGroup", ['description', 'requirements', 'tests'])):
  __slots__ = ()
  def is_relevant(self, config):
    return self.requirements.matches(config)

class TestToSkip(collections.namedtuple("TestToSkip", ['name', 'reason'])):
  __slots__ = ()
  pass

class ConfigSet(collections.namedtuple("ConfigSet", ["dist",
                                                     "heap_poisoning",
                                                     "bisect",
                                                     "vixl",
                                                     "read_barrier",
                                                     "target_arch",
                                                     "test_quiet",
                                                     "strace",
                                                     "always_clean",
                                                     "android_root",
                                                     "host_jack_classpath",
                                                     "target_jack_classpath",
                                                     "host",
                                                     "run_type",
                                                     "prebuild",
                                                     "compiler",
                                                     "relocate",
                                                     "trace",
                                                     "gc",
                                                     "jni",
                                                     "image",
                                                     "pic",
                                                     "debuggable",
                                                     "address"])):
  """A single configuration we will add to the test-plan."""
  __slots__ = ()

  def get_arguments(self):
    return [] + (self.get_bisect_arguments() +
                 self.get_test_quiet_arguments() +
                 self.get_strace_arguments() +
                 self.get_always_clean_arguments() +
                 self.get_android_root_arguments() +
                 self.get_host_arguments() +
                 self.get_run_type_arguments() +
                 self.get_prebuild_arguments() +
                 self.get_compiler_arguments() +
                 self.get_relocate_arguments() +
                 self.get_trace_arguments() +
                 self.get_gc_arguments() +
                 self.get_jni_arguments() +
                 self.get_image_arguments() +
                 self.get_pic_arguments() +
                 self.get_debuggable_arguments() +
                 self.get_address_arguments())

  def get_bisect_arguments(self):
    if self.bisect == "True":
      return ['--bisect']
    elif self.bisect == "False":
      return []
    else:
      raise Exception("Unknown argument for 'bisect': {}".format(self.bisect))

  def get_test_quiet_arguments(self):
    if self.test_quiet == "True":
      return ['--quiet']
    elif self.test_quiet == "False":
      return []
    else:
      raise Exception("Unknown argument for 'test_quiet': {}".format(self.test_quiet))

  def get_strace_arguments(self):
    if self.strace == "True":
      return ['--strace']
    elif self.strace == "False":
      return []
    else:
      raise Exception("Unknown argument for 'strace': {}".format(self.strace))

  def get_always_clean_arguments(self):
    if self.always_clean == "True":
      return ['--always-clean']
    elif self.always_clean == "False":
      return []
    else:
      raise Exception("Unknown argument for 'always_clean': {}".format(self.always_clean))

  def get_android_root_arguments(self):
    if self.android_root is not None:
      return ['--android-root', self.android_root]
    else:
      return []

  def get_host_arguments(self):
    if self.host == 'host':
      if self.host_jack_classpath is None:
        jcp = []
      else:
        jcp = [self.host_jack_classpath]
      return ['--host'] + jcp
    elif self.host == 'target':
      if self.target_jack_classpath is None:
        jcp = []
      else:
        jcp = [self.target_jack_classpath]
      return ['--jack-classpath', self.target_jack_classpath]
    else:
      raise Exception("Unknown argument for 'host': {}".format(self.host))

  def get_run_type_arguments(self):
    if self.run_type == 'debug':
      return []
    elif self.run_type == 'ndebug':
      return ['-O']
    else:
      raise Exception("Unknown argument for 'run_type': {}".format(self.run_type))

  def get_prebuild_arguments(self):
    if self.prebuild == 'prebuild':
      return ['--prebuild']
    elif self.prebuild == 'no-prebuild':
      return ['--no-prebuild']
    elif self.prebuild == 'no-dex2oat':
      return ['--no-prebuild', '--no-dex2oat']
    else:
      raise Exception("Unknown argument for 'prebuild': {}".format(self.prebuild))

  def get_compiler_arguments(self):
    if self.compiler == 'optimizing':
      return ['--optimizing']
    elif self.compiler == 'regalloc_gc':
      return ['--optimizing', '-Xcompiler-option', '--register-allocation-strategy=graph-color']
    elif self.compiler == 'interpreter':
      return ['--interpreter']
    elif self.compiler == 'interp-ac':
      return ['--interpreter', '--verify-soft-fail']
    elif self.compiler == 'jit':
      return ['--jit']
    else:
      raise Exception("Unknown argument for 'compiler': {}".format(self.compiler))

  def get_relocate_arguments(self):
    if self.relocate == 'relocate':
      return ['--relocate']
    elif self.relocate == 'no-relocate':
      return ['--no-relocate']
    elif self.relocate == 'relocate-npatchoat':
      return ['--relocate', '--no-patchoat']
    else:
      raise Exception("Unknown argument for 'relocate': {}".format(self.relocate))

  def get_trace_arguments(self):
    if self.trace == 'trace':
      return ['--trace']
    elif self.trace == 'stream':
      return ['--trace', '--stream']
    elif self.trace == 'ntrace':
      return []
    else:
      raise Exception("Unknown argument for 'trace': {}".format(self.trace))

  def get_gc_arguments(self):
    if self.gc == 'gcverify':
      return ['--gcverify']
    elif self.gc == 'gcstress':
      return ['--gcstress']
    elif self.gc == 'cms':
      return []
    else:
      raise Exception("Unknown argument for 'gc': {}".format(self.gc))

  def get_jni_arguments(self):
    if self.jni == 'checkjni':
      return ['--runtime-option', '-Xcheck:jni']
    elif self.jni == 'forcecopy':
      return ['--runtime-option', '-Xjniopts:forcecopy']
    elif self.jni == 'jni':
      return []
    else:
      raise Exception("Unknown argument for 'jni': {}".format(self.jni))

  def get_image_arguments(self):
    if self.image == "no-image":
      return ['--no-image']
    elif self.image == "npicimage":
      return [ '--npic-image' ]
    elif self.image == 'picimage':
      return []
    elif self.image == 'multinpicimage':
      return ['--npic-image', '--multi-image']
    elif self.image == 'multipicimage':
      return ['--multi-image']
    else:
      raise Exception("Unknown argument for 'image': {}".format(self.image))

  def get_pic_arguments(self):
    if self.pic == 'pictest':
      return ['--pic-test']
    elif self.pic == 'npictest':
      return []
    else:
      raise Exception("Unknown argument for 'pic': {}".format(self.pic))

  def get_debuggable_arguments(self):
    if self.debuggable == 'debuggable':
      return ['--debuggable']
    elif self.debuggable == 'ndebuggable':
      return []
    else:
      raise Exception("Unknown argument for 'debuggable': {}".format(self.debuggable))

  # TODO Handle INSTRUCTION_SET_FEATURES
  def get_address_arguments(self):
    if self.address == '64':
      return ['--64']
    elif self.address == '32':
      return []
    else:
      raise Exception("Unknown argument for 'address': {}".format(self.address))

def make_config_list(options):
  """Dedups the options and makes all the ConfigSets that can be created from the given options."""
  res = []
  for permutation in itertools.product(frozenset(options.host),
                                       frozenset(options.run_type),
                                       frozenset(options.prebuild),
                                       frozenset(options.compiler),
                                       frozenset(options.relocate),
                                       frozenset(options.trace),
                                       frozenset(options.gc),
                                       frozenset(options.jni),
                                       frozenset(options.image),
                                       frozenset(options.pic),
                                       frozenset(options.debuggable),
                                       frozenset(options.address)):
    res.append(ConfigSet(options.dist,
                         options.heap_poisoning,
                         options.bisect,
                         options.vixl,
                         options.read_barrier,
                         options.target_arch,
                         options.test_quiet,
                         options.strace,
                         options.always_clean,
                         options.android_root,
                         options.host_jack_classpath,
                         options.target_jack_classpath,
                         *permutation))
  return res

class Requirements(collections.namedtuple("Requirements", ConfigSet._fields)):
  """A holder for the requirements of a skip group. It has the same layout as a ConfigSet."""
  def matches(self, config):
    # Guaranteed to be in the same order since we constructed this type with the same fields as
    # ConfigSet
    for req_field, config_field in zip(self, config):
      if req_field is not None and req_field != config_field:
        return False
    return True


EMPTY_REQUIREMENT = Requirements._make(None for i in range(len(Requirements._fields)))

def create_requirement(element):
  """Makes a requirments object"""
  reqs = dict(element.attrib)
  # Remove the description tag since that isn't in the Requirments structure.
  del reqs['description']
  return EMPTY_REQUIREMENT._replace(**reqs)


def make_skip_group(skip):
  tests = [TestToSkip(child.attrib['name'], child.attrib['reason']) for child in skip]
  return SkipGroup(skip.attrib['description'], create_requirement(skip), tests)

def make_skip_list(tree):
  return [make_skip_group(skip) for skip in tree]

class TestGroup(collections.namedtuple("TestGroup", ['config', 'tests'])):
  pass

class SkippedTest(collections.namedtuple("SkippedTest", ['skip_group',
                                                         'config',
                                                         'args',
                                                         'test_to_skip'])):
  pass

class ExecutedTest(collections.namedtuple("ExecutedTest", ['test_group',
                                                           'args',
                                                           'name',
                                                           'output',
                                                           'success'])):
  pass

def make_test_groups(configs, skip_groups, tests):
  skips = []
  groups = []
  for config in configs:
    config_tests = set(tests)
    for skip_group in skip_groups:
      if skip_group.is_relevant(config):
        for to_skip in skip_group.tests:
          if to_skip.name in config_tests:
            skips.append(SkippedTest(skip_group, config, config.get_arguments(), to_skip))
          config_tests.discard(to_skip.name)
    groups.append(TestGroup(config, config_tests))
  return groups, skips

class Test(collections.namedtuple("Test", ['test_group', 'args', 'name'])):
  pass

def get_all_tests(test_groups):
  for group in test_groups:
    args = group.config.get_arguments()
    for test in group.tests:
      yield Test(group, args, test)

def run_one_test(test):
  with subprocess.Popen(['art/test/run-test'] + test.args + [test.name],
                        stderr=subprocess.STDOUT,
                        stdout=subprocess.PIPE,
                        universal_newlines=True) as rt:
    # Read output until it is finished.
    output = rt.stdout.read()
    # wait for process to exit fully
    rt.wait()
    return ExecutedTest(test.test_group, test.args, test.name, output, rt.returncode == 0)

def run_test_groups(args, test_groups):
  with concurrent.futures.ProcessPoolExecutor(args.jobs) as executor:
    results = list(executor.map(run_one_test, get_all_tests(test_groups)))
  successes = list(filter(lambda a: a.success, results))
  failures = list(filter(lambda a: not a.success, results))
  return successes, failures

def report_outcome(success, skip, fail):
  # TODO options to control output.
  for test in success:
    print("succeeded: {}\t{}".format(test.name, test.test_group.config))
  for test in skip:
    print("skipped: {}\t{}".format(test.test_to_skip, test.config))
  for test in fail:
    print("failed: {}\t{}\n{}".format(test.name, test.test_group.config, test.output))
  return 0

def run_tests(args):
  configs = make_config_list(args)
  test_groups, skipped_tests = make_test_groups(configs, make_skip_list(args.skips), args.tests)
  successes, failures = run_test_groups(args, test_groups)
  return report_outcome(successes, skipped_tests, failures)

def main():
  parser = argparse.ArgumentParser(
      description="Program to create and run a collection of run-tests.")
  parser.add_argument("-j", '--jobs', required=False,
                      type=int,
                      default=1,
                      help="Number of processes to run the tests with.")
  parser.add_argument("--skips",
                      required=True,
                      type=lambda f: ET.parse(f).getroot(),
                      help="XML file containing skips")
  # parser.add_argument("--output", required=True, type=argparse.FileType("w"),
  #                     help="xml file to write test plan to.")
  parser.add_argument("--dist", required=False, action="store_const", default="False", const="True",
                      help="Generate a plan knowing this is a dist build")
  parser.add_argument("--heap_poisoning", required=False, action="store_const", default="False",
                      const="True",
                      help="The runtime is using heap poisoning.")
  parser.add_argument("--bisect", required=False, action="store_const", default="False",
                      const="True",
                      help="Tell the tester to run bisection test on failures")
  parser.add_argument("--vixl", required=False, action="store_const", default="False", const="True",
                      help="The test is using a VIXL backend")
  parser.add_argument("--read_barrier",
                      choices=["None", "Baker", "Non-Baker"],
                      required=False,
                      default="None",
                      help="The runtime uses read-barriers")
  parser.add_argument("--target_arch",
                      choices=["arm","mips","intel", "None"],
                      default="None",
                      required=False,
                      help="The target arch type")
  parser.add_argument("--target_jack_classpath",
                      required=False,
                      action="store",
                      help="Set the jack-class-path argument to run-test on target")
  parser.add_argument("--host_jack_classpath",
                      required=False,
                      action="store",
                      help="Set the jack-class-path argument to run-test on host")
  parser.add_argument("--android_root",
                      required=False,
                      action="store",
                      default=None,
                      help="Set the android-root argument to run-test")
  parser.add_argument("--always_clean",
                      required=False,
                      action="store_const",
                      const="True",
                      default="False",
                      help="Run the tests with the --always-clean run-test option")
  parser.add_argument("--strace",
                      required=False,
                      action="store_const",
                      const="True",
                      default="False",
                      help="Run the tests with the --strace run-test option")
  parser.add_argument("--test_quiet",
                      required=False,
                      action="store_const",
                      const="True",
                      default="False",
                      help="Run the tests with the --quiet run-test option")
  parser.add_argument("--host", choices=["host","target"], action="append", required=True)
  parser.add_argument("--run_type", choices=["debug", "ndebug"], action="append", required="True")
  parser.add_argument("--prebuild",
                      choices=["prebuild", "no-prebuild", "no-dex2oat"],
                      action="append",
                      required="True")
  parser.add_argument("--compiler",
                      choices=["optimizing", "regalloc_gc", "interpreter", "interp-ac", "jit"],
                      action="append",
                      required="True")
  parser.add_argument("--relocate",
                      choices=["relocate", "no-relocate", "relocate-npatchoat"],
                      action="append",
                      required="True")
  parser.add_argument("--trace",
                      choices=["trace", "stream", "ntrace"],
                      action="append",
                      required="True")
  parser.add_argument("--gc",
                      choices=["gcverify", "gcstress", "cms"],
                      action="append",
                      required="True")
  parser.add_argument("--jni",
                      choices=["checkjni", "forcecopy", "jni"],
                      action="append",
                      required="True")
  parser.add_argument("--image",
                      choices=["no-image",
                               "npic-image",
                               "picimage",
                               "multinpicimage",
                               "multipicimage"],
                      action="append",
                      required="True")
  parser.add_argument("--pic",
                      choices=["pictest", "npictest"],
                      action="append",
                      required="True")
  parser.add_argument("--debuggable",
                      choices=["debuggable", "ndebuggable"],
                      action="append",
                      required="True")
  parser.add_argument("--address",
                      choices=["64", "32"],
                      action="append",
                      required="True")
  parser.add_argument("tests", nargs="+", help="The tests this plan should include if possible")
  return run_tests(parser.parse_args())

if __name__ == '__main__':
  sys.exit(main())
