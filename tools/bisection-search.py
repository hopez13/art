#!/usr/bin/env python
#
# Copyright (C) 2016 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Performs bisection bug search on methods and optimizations.

Attempts to find a method which when compiled causes incorrect program behavior.
If succesful, attempts to further narrow down bug location and find an
optimization pass which when ran for the method causes incorrect program
behavior.

Example usage:
./art/tools/bisection-search.py -cp test_float/classes.dex --correct_output test_float/out_comp Test19742
"""

from __future__ import division
from __future__ import print_function
import abc
import argparse
import os
import re
from subprocess import CalledProcessError
from subprocess import check_call
from subprocess import PIPE
from subprocess import Popen
import sys
from tempfile import mkdtemp
from tempfile import NamedTemporaryFile

MANDATORY_PASSES = ["x86_memory_operand_generation",
                    "dex_cache_array_fixups_arm",
                    "dex_cache_array_fixups_mips",
                    "pc_relative_fixups_x86",
                    "pc_relative_fixups_mips"]
NON_PASSES = ["builder", "prepare_for_register_allocation",
              "liveness", "register"]
DEVICE_TMP_PATH = "/data/local/tmp"


class BisectionException(Exception):
  pass


class Dex2OatWrapperTestable(object):
  """Class representing a testable compilation.

  Accepts filters on compiled methods and optimization passes.
  """

  def __init__(self, base_cmd, test_env, class_name, correct_output=None):
    self._base_cmd = base_cmd
    self._test_env = test_env
    self._class_name = class_name
    self._correct_output = correct_output
    self._compiled_methods_path = self._test_env.CreateTempFile()
    self._passes_to_run_path = self._test_env.CreateTempFile()

  def Test(self, compiled_methods, passes_to_run=None):
    """Tests compilation with compiled_methods and run_passes switches active.

    If compiled_methods is None then compiles all methods.
    If passes_to_run is None then runs default passes.

    Args:
      compiled_methods: List of strings representing methods to compile or None.
      passes_to_run: List of strings representing passes to run or None.

    Returns:
      True if test passes with given settings. False otherwise.
    """
    cmd = self._PrepareCmd(compiled_methods=compiled_methods,
                           passes_to_run=passes_to_run,
                           dump_passes=True)
    try:
      output = self._test_env.RunForOutput(cmd)
    except CalledProcessError:
      return False
    return output == self._correct_output

  def GetAllMethods(self):
    """Get methods compiled during the test.

    Returns:
      List of strings representing methods compiled during the test.

    Raises:
      BisectionException: An error occurred when retrieving methods list.
    """
    cmd = self._PrepareCmd(dump_passes=True)
    try:
      output = self._test_env.RunForErrOutput(cmd)
    except CalledProcessError as ex:
      output = ex.output
    match_methods = re.findall(r"TIMINGS ([^\n]+)\n", output)
    if not match_methods:
      raise BisectionException("Not recognized --dump-passes output format.")
    return match_methods

  def GetAllPassesForMethod(self, compiled_method):
    """Get all optimization passes ran for a method during the test.

    Args:
      compiled_method: String representing method to compile.

    Returns:
      List of strings representing passes ran for compiled_method during test.

    Raises:
      BisectionException: An error occurred when retrieving passes list.
    """
    cmd = self._PrepareCmd(compiled_methods=[compiled_method], dump_passes=True)
    try:
      output = self._test_env.RunForErrOutput(cmd)
    except CalledProcessError as ex:
      output = ex.output
    lines = output.split("\n")
    start = end = -1
    i = 0
    while i < len(lines):
      if start == -1 and "TIMINGS" in lines[i]:
        start = i
      if "end," in lines[i]:
        end = i
        break
      i += 1
    if start == -1 or end == -1:
      raise BisectionException("Not recognized --dump-passes format.")
    passes = [line.split()[-1] for line in lines[start+2:end]]
    return [p for p in passes if p not in NON_PASSES]

  def _PrepareCmd(self, compiled_methods=None, passes_to_run=None,
                  dump_passes=False):
    """Prepare command to run."""
    cmd = self._base_cmd + " -Xcompiler-option -j1"
    if compiled_methods is not None:
      self._test_env.WriteLines(self._compiled_methods_path, compiled_methods)
      cmd += " -Xcompiler-option --compiled-methods={0}".format(
          self._compiled_methods_path)
    if passes_to_run is not None:
      self._test_env.WriteLines(self._passes_to_run_path, passes_to_run)
      cmd += " -Xcompiler-option --run-passes={0}".format(
          self._passes_to_run_path)
    if dump_passes:
      cmd += " -Xcompiler-option --dump-passes"
    cmd += " -classpath {0} {1}".format(self._test_env.classpath,
                                        self._class_name)
    return cmd


class ITestEnv(object):
  """Test environment abstraction."""
  __meta_class__ = abc.ABCMeta

  @abc.abstractmethod
  def CreateTempFile(self):
    """Creates temporary file in test directory.

    Returns:
      Environment specific path to file.
    """
    pass

  @abc.abstractmethod
  def WriteLines(self, file_path, lines):
    """Writes lines to file in test directory.

    If file exists it gets overwrited.

    Args:
      file_path: environment specific path to file.
      lines: list of strings to write.
    """
    pass

  @abc.abstractmethod
  def RunForErrOutput(self, cmd):
    pass

  @abc.abstractmethod
  def RunForOutput(self, cmd):
    pass

  @abc.abstractproperty
  def classpath(self):
    """Classpath with test class."""
    pass

  @abc.abstractproperty
  def logfile(self):
    pass


def GetDexArchCachePaths(android_data_path):
  return ["{0}/dalvik-cache/{1}".format(android_data_path, arch)
          for arch in ["arm", "arm64", "x86", "x86_64"]]


def CheckAndLogOutput(cmd, env, stdout_logfile):
  program = Popen(cmd.split(), stderr=PIPE, stdout=PIPE, env=env)
  (output, err_output) = program.communicate()
  stdout_logfile.write("Command:\n{0}\nSTDERR:\n{1}STDOUT:\n{2}\n".format(
      cmd, err_output, output))
  if program.returncode != 0:
    raise CalledProcessError(program.returncode, cmd)
  return (output, err_output)


class HostTestEnv(ITestEnv):
  """Host test environment."""

  def __init__(self, classpath):
    self._classpath = classpath
    self._env_path = mkdtemp(dir="/tmp/")
    self._logfile = open("{0}/log".format(self._env_path), "w+")
    os.mkdir("{0}/dalvik-cache".format(self._env_path))
    for arch_cache_path in GetDexArchCachePaths(self._env_path):
      os.mkdir(arch_cache_path)
    self._shell_env = os.environ.copy()
    self._shell_env["ANDROID_DATA"] = self._env_path
    cwd = os.getcwd()
    self._shell_env["ANDROID_ROOT"] = "{0}/out/host/linux-x86".format(cwd)
    self._shell_env["LD_LIBRARY_PATH"] = "{0}/out/host/linux-x86/lib64".format(
        cwd)
    self._shell_env["PATH"] = ("{0}/out/host/linux-x86/bin:".format(cwd)
                               + self._shell_env["PATH"])
    self._shell_env["LD_USE_LOAD_BIAS"] = "1"

  def CreateTempFile(self):
    return NamedTemporaryFile(dir=self._env_path, delete=False).name

  def WriteLines(self, file_path, lines):
    f = open(file_path, "w")
    f.writelines("{0}\n".format(line) for line in lines)
    return

  def RunForErrOutput(self, cmd):
    self._EmptyDexCache()
    return CheckAndLogOutput(cmd, self._shell_env, self._logfile)[1]

  def RunForOutput(self, cmd):
    self._EmptyDexCache()
    return CheckAndLogOutput(cmd, self._shell_env, self._logfile)[0]

  @property
  def classpath(self):
    return self._classpath

  @property
  def logfile(self):
    return self._logfile

  @property
  def base_path(self):
    return self._env_path

  def _EmptyDexCache(self):
    for arch_cache_path in GetDexArchCachePaths(self._env_path):
      for file_path in os.listdir(arch_cache_path):
        file_path = os.path.join(arch_cache_path, file_path)
        if os.path.isfile(file_path):
          os.unlink(file_path)


class DeviceTestEnv(ITestEnv):
  """Device test environment."""

  def __init__(self, classpath):
    self._host_test_env = HostTestEnv(classpath)
    self._host_env_path = self._host_test_env.base_path
    self._logfile = self._host_test_env.logfile
    self._device_env_path = os.path.join(
        DEVICE_TMP_PATH, os.path.basename(self._host_env_path))
    self._classpath = os.path.join(
        self._device_env_path, os.path.basename(classpath))
    self._shell_env = os.environ.copy()

    self._AdbPush(self._host_env_path, DEVICE_TMP_PATH)
    # push doesn't work for empty directories, need to use mkdir
    self._AdbMkdir("{0}/dalvik-cache".format(self._device_env_path))
    for arch_cache_path in GetDexArchCachePaths(self._device_env_path):
      self._AdbMkdir(arch_cache_path)
    self._AdbPush(classpath, self._device_env_path)

  def CreateTempFile(self):
    host_path = self._host_test_env.CreateTempFile()
    device_path = self._ToDevicePath(host_path)
    self._AdbPush(host_path, device_path)
    return device_path

  def WriteLines(self, file_path, lines):
    host_path = self._ToHostPath(file_path)
    self._host_test_env.WriteLines(host_path, lines)
    self._AdbPush(host_path, file_path)
    return

  def RunForErrOutput(self, cmd):
    self._EmptyDexCache()
    # Hack until a mechanism to redirect error output is implemented
    cmd = ("adb shell ANDROID_DATA={0} logcat -c && {1}"
           "&& logcat -d dex2oat:* *:S").format(self._device_env_path, cmd)
    return CheckAndLogOutput(cmd, self._shell_env, self._logfile)[0]

  def RunForOutput(self, cmd):
    self._EmptyDexCache()
    cmd = "adb shell ANDROID_DATA={0} {1}".format(self._device_env_path, cmd)
    return CheckAndLogOutput(cmd, self._shell_env, self._logfile)[0]

  @property
  def classpath(self):
    return self._classpath

  @property
  def logfile(self):
    return self._logfile

  def _ToDevicePath(self, host_path):
    assert host_path.startswith(self._host_env_path), "Not a host path."
    return self._device_env_path + host_path[len(self._host_env_path):]

  def _ToHostPath(self, device_path):
    assert device_path.startswith(self._device_env_path), "Not a device path."
    return self._host_env_path + device_path[len(self._device_env_path):]

  def _AdbPush(self, what, where):
    check_call("adb push {0} {1}".format(what, where).split(),
               stdout=self._logfile, stderr=self._logfile)

  def _AdbMkdir(self, path):
    check_call("adb shell mkdir {0}".format(path).split(), stdout=self._logfile,
               stderr=self._logfile)

  def _EmptyDexCache(self):
    for arch_cache_path in GetDexArchCachePaths(self._device_env_path):
      check_call("adb shell rm {0}/* -f".format(arch_cache_path).split(),
                 stdout=self._logfile, stderr=self._logfile)


def BinarySearch(start, end, test):
  """Binary search integers using test function to guide the process."""
  while start < end:
    mid = (start + end) // 2
    if test(mid):
      start = mid + 1
    else:
      end = mid
  return start


def FilterPasses(passes, cutoff_idx):
  return [opt_pass for idx, opt_pass in enumerate(passes)
          if opt_pass in MANDATORY_PASSES or idx < cutoff_idx]


def BugSearch(testable):
  """Find buggy (method, optimization pass) pair for a given testable.

  Args:
    testable: Dex2OatWrapperTestable.

  Returns:
    (string, string) tuple. First element is name of method which when compiled
    exposes test failure. Second element is name of optimization pass such that
    for aforementioned method running all passes up to and excluding the pass
    results in test passing but running all passes up to and including the pass
    results in test failing.

    (None, None) if test passes when compiling all methods.
    (string, None) if a method is found which exposes the failure, but the
      failure happens even when running just mandatory passes.
  """
  all_methods = testable.GetAllMethods()
  faulty_method_idx = BinarySearch(
      0,
      len(all_methods),
      lambda mid: testable.Test(all_methods[0:mid]))
  if faulty_method_idx == len(all_methods):
    return (None, None)
  assert faulty_method_idx != 0, "Testable fails with no methods compiled."
  faulty_method = all_methods[faulty_method_idx - 1]
  all_passes = testable.GetAllPassesForMethod(faulty_method)
  faulty_pass_idx = BinarySearch(
      0,
      len(all_passes),
      lambda mid: testable.Test([faulty_method],
                                FilterPasses(all_passes, mid)))
  if faulty_pass_idx == 0:
    return (faulty_method, None)
  assert faulty_pass_idx != len(all_passes), "Test passes for faulty method."
  faulty_pass = all_passes[faulty_pass_idx - 1]
  return (faulty_method, faulty_pass)


def PrepareParser():
  """Prepares argument parser."""
  parser = argparse.ArgumentParser()
  parser.add_argument(
      "-cp", "--classpath", required=True, type=str, help="classpath")
  parser.add_argument("--correct_output",
                      type=str,
                      help="file containing correct output for the program")
  parser.add_argument(
      "--host", action="store_true", default=False, help="run on host")
  parser.add_argument("classname", help="name of class to run")
  return parser


def main():
  parser = PrepareParser()
  args = parser.parse_args()
  cwd = os.getcwd()
  if args.correct_output is not None:
    with open(args.correct_output, "r") as myfile:
      correct_output = myfile.read()
  else:
    correct_output = None
  if args.host:
    run_cmd = (
        "{0}/out/host/linux-x86/bin/dalvikvm -XXlib:libart.so -Xnorelocate "
        "-Ximage:{0}/out/host/linux-x86/framework/core-optimizing-pic.art"
    ).format(cwd)
    test_env = HostTestEnv(args.classpath)
  else:
    run_cmd = "dalvikvm"
    test_env = DeviceTestEnv(args.classpath)
  testable = Dex2OatWrapperTestable(run_cmd, test_env, args.classname,
                                    correct_output)
  (method, opt_pass) = BugSearch(testable)
  if method is None:
    print("Couldn't find any bugs.")
  elif opt_pass is None:
    print("Faulty method: {0}. Fails with just mandatory passes.".format(
        method))
  else:
    print("Faulty method and pass: {0}, {1}.".format(method, opt_pass))
  print("Logfile: {0}".format(test_env.logfile.name))
  sys.exit(0)


if __name__ == "__main__":
  main()
