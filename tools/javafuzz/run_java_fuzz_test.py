#!/usr/bin/env python3.4
#
# Copyright (C) 2016 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import abc
import argparse
import filecmp

from glob import glob

import os
import shlex
import shutil
import subprocess
import sys

from tempfile import mkdtemp


sys.path.append(os.path.dirname(os.path.dirname(__file__)))

from bisection_search.common import RetCode
from bisection_search.common import CommandListToCommandString
from bisection_search.common import FatalError
from bisection_search.common import GetEnvVariableOrError
from bisection_search.common import RunCommandForOutput
from bisection_search.common import DeviceTestEnv


# execution modes which support bisection bug search.
BISECTABLE_RUN_MODES = ['HInt', 'HOpt', 'TInt', 'TOpt']

# return codes supported by bisection bug search
BISECTABLE_RET_CODES = (RetCode.SUCCESS, RetCode.ERROR, RetCode.TIMEOUT)


#
# Utility methods.
#


def RunCommand(cmd, out, err, timeout=5):
  """Executes a command, and returns its return code.

  Args:
    cmd: string, a command to execute
    out: string, file name to open for stdout (or None)
    err: string, file name to open for stderr (or None)
    timeout: int, time out in seconds
  Returns:
    RetCode, return code of running command (forced RetCode.TIMEOUT on timeout)
  """
  devnull = subprocess.DEVNULL
  outf = devnull
  if out is not None:
    outf = open(out, mode='w')
  errf = devnull
  if err is not None:
    errf = open(err, mode='w')
  (_, _, retcode) = RunCommandForOutput(cmd, None, outf, errf, timeout)
  if outf != devnull:
    outf.close()
  if errf != devnull:
    errf.close()
  return retcode


def GetJackClassPath():
  """Returns Jack's classpath."""
  top = GetEnvVariableOrError('ANDROID_BUILD_TOP')
  libdir = top + '/out/host/common/obj/JAVA_LIBRARIES'
  return libdir + '/core-libart-hostdex_intermediates/classes.jack:' \
       + libdir + '/core-oj-hostdex_intermediates/classes.jack'


def GetExecutionModeRunner(device, mode):
  """Returns a runner for the given execution mode.

  Args:
    device: string, target device serial number (or None)
    mode: string, execution mode
  Returns:
    TestRunner with given execution mode
  Raises:
    FatalError: error for unknown execution mode
  """
  if mode == 'ri':
    return TestRunnerRIOnHost()
  if mode == 'hint':
    return TestRunnerArtOnHost(True)
  if mode == 'hopt':
    return TestRunnerArtOnHost(False)
  if mode == 'tint':
    return TestRunnerArtOnTarget(device, True)
  if mode == 'topt':
    return TestRunnerArtOnTarget(device, False)
  raise FatalError('Unknown execution mode')

#
# Execution mode classes.
#


class TestRunner(object):
  """Abstraction for running a test in a particular execution mode."""
  __meta_class__ = abc.ABCMeta

  def GetDescription(self):
    """Returns a description string of the execution mode."""
    return self._description

  def GetId(self):
    """Returns a short string that uniquely identifies the execution mode."""
    return self._id

  @abc.abstractmethod
  def GetBisectionSearchArgs(self):
    """Returns arguments to pass to bisection search.

    Arguments returned should reflect command arguments used to run the test.
    """
    pass

  @abc.abstractmethod
  def CompileAndRunTest(self):
    """Compile and run the generated test.

    Ensures that the current Test.java in the temporary directory is compiled
    and executed under the current execution mode. On success, transfers the
    generated output to the file GetId()_out.txt in the temporary directory.
    Cleans up after itself.

    Most nonzero return codes are assumed non-divergent, since systems may
    exit in different ways. This is enforced by normalizing return codes.

    Returns:
      normalized return code
    """
    pass


class TestRunnerRIOnHost(TestRunner):
  """Concrete test runner of the reference implementation on host."""

  def  __init__(self):
    """Constructor for the RI tester."""
    self._description = 'RI on host'
    self._id = 'RI'

  def CompileAndRunTest(self):
    if RunCommand(['javac', 'Test.java'],
                  out=None, err=None, timeout=30) == RetCode.SUCCESS:
      retc = RunCommand(['java', 'Test'], 'RI_run_out.txt', err=None)
      if retc != RetCode.SUCCESS and retc != RetCode.TIMEOUT:
        retc = RetCode.NOTRUN
    else:
      retc = RetCode.NOTCOMPILED
    return retc

  def GetBisectionSearchArgs(self):
    raise FatalError('Cannot bisect javac.')


class TestRunnerArtOnHost(TestRunner):
  """Concrete test runner of Art on host (interpreter or optimizing)."""

  def  __init__(self, interpreter):
    """Constructor for the Art on host tester.

    Args:
      interpreter: boolean, selects between interpreter or optimizing
    """
    self._art_cmd = ['/bin/bash', 'art', '-cp', 'classes.dex']
    self._bisection_args = ['-cp', 'classes.dex']
    if interpreter:
      self._description = 'Art interpreter on host'
      self._id = 'HInt'
      self._art_cmd += ['-Xint']
    else:
      self._description = 'Art optimizing on host'
      self._id = 'HOpt'
    self._art_cmd += ['Test']
    self._jack_args = ['-cp', GetJackClassPath(), '--output-dex', '.',
                       'Test.java']

  def CompileAndRunTest(self):
    if RunCommand(['jack'] + self._jack_args,
                  out=None, err='jackerr.txt', timeout=30) == RetCode.SUCCESS:
      out = self.GetId() + '_run_out.txt'
      retc = RunCommand(self._art_cmd, out, 'arterr.txt')
      if retc != RetCode.SUCCESS and retc != RetCode.TIMEOUT:
        retc = RetCode.NOTRUN
    else:
      retc = RetCode.NOTCOMPILED
    return retc

  def GetBisectionSearchArgs(self):
    cmd_str = ' '.join(self._art_cmd)
    return ['--raw-cmd={0}'.format(cmd_str), '--timeout', str(30)]


class TestRunnerArtOnTarget(TestRunner):
  """Concrete test runner of Art on target (interpreter or optimizing)."""

  def  __init__(self, device, interpreter):
    """Constructor for the Art on target tester.

    Args:
      device: string, target device serial number (or None)
      interpreter: boolean, selects between interpreter or optimizing
    """
    self._test_env = DeviceTestEnv('javafuzz_', specific_device=device)
    self._dalvik_cmd = ['dalvikvm']
    if interpreter:
      self._description = 'Art interpreter on target'
      self._id = 'TInt'
      self._dalvik_cmd += ['-Xint']
    else:
      self._description = 'Art optimizing on target'
      self._id = 'TOpt'
    self._device = device
    self._jack_args = ['-cp', GetJackClassPath(), '--output-dex', '.',
                       'Test.java']
    self._device_classpath = None

  def CompileAndRunTest(self):
    if RunCommand(['jack'] + self._jack_args,
                  out=None, err='jackerr.txt', timeout=30) == RetCode.SUCCESS:
      self._device_classpath = self._test_env.PushClasspath('classes.dex')
      cmd = self._dalvik_cmd + ['-cp', self._device_classpath, 'Test']
      out = self.GetId() + '_run_out.txt'
      (output, retc) = self._test_env.RunCommand(
          cmd, {'ANDROID_LOG_TAGS': '*:s'})
      with open(out, 'w') as run_out:
        run_out.write(output)
      if retc != RetCode.SUCCESS and retc != RetCode.TIMEOUT:
        retc = RetCode.NOTRUN
    else:
      retc = RetCode.NOTCOMPILED
    return retc

  def GetBisectionSearchArgs(self):
    cmd_str = ' '.join(self._dalvik_cmd + ['-cp', self._device_classpath,
                                           'Test'])
    cmd = ['--raw-cmd={0}'.format(cmd_str), '--timeout', str(30)]
    if self._device:
      cmd += ['--device-serial', self._device]
    else:
      cmd.append('--device')
    return cmd

#
# Tester classes.
#


class JavaFuzzTester(object):
  """Tester that runs JavaFuzz many times and report divergences."""

  def  __init__(self, num_tests, device, ref_mode, test_mode):
    """Constructor for the tester.

    Args:
      num_tests: int, number of tests to run
      device: string, target device serial number (or None)
      ref_mode: string, execution mode for reference runner
      test_mode: string, execution mode for test runner
    """
    self._num_tests = num_tests
    self._device = device
    self._ref_runner = GetExecutionModeRunner(device, ref_mode)
    self._test_runner = GetExecutionModeRunner(device, test_mode)
    self._save_dir = None
    self._tmp_dir = None
    # Statistics.
    self._test = 0
    self._num_success = 0
    self._num_not_compiled = 0
    self._num_not_run = 0
    self._num_timed_out = 0
    self._num_divergences = 0

  def __enter__(self):
    """On entry, enters new temp directory after saving current directory.

    Raises:
      FatalError: error when temp directory cannot be constructed
    """
    self._save_dir = os.getcwd()
    self._tmp_dir = mkdtemp(dir='/tmp/')
    if self._tmp_dir is None:
      raise FatalError('Cannot obtain temp directory')
    os.chdir(self._tmp_dir)
    return self

  def __exit__(self, etype, evalue, etraceback):
    """On exit, re-enters previously saved current directory and cleans up."""
    os.chdir(self._save_dir)
    if self._num_divergences == 0:
      shutil.rmtree(self._tmp_dir)

  def Run(self):
    """Runs JavaFuzz many times and report divergences."""
    print()
    print('**\n**** JavaFuzz Testing\n**')
    print()
    print('#Tests    :', self._num_tests)
    print('Device    :', self._device)
    print('Directory :', self._tmp_dir)
    print('Exec-ref_mode:', self._ref_runner.GetDescription())
    print('Exec-test_mode:', self._test_runner.GetDescription())
    print
    self.ShowStats()
    for self._test in range(1, self._num_tests + 1):
      self.RunJavaFuzzTest()
      self.ShowStats()
    if self._num_divergences == 0:
      print('\n\nsuccess (no divergences)\n')
    else:
      print('\n\nfailure (divergences)\n')

  def ShowStats(self):
    """Shows current statistics (on same line) while tester is running."""
    print('\rTests:', self._test, \
          'Success:', self._num_success, \
          'Not-compiled:', self._num_not_compiled, \
          'Not-run:', self._num_not_run, \
          'Timed-out:', self._num_timed_out, \
          'Divergences:', self._num_divergences)
    sys.stdout.flush()

  def RunJavaFuzzTest(self):
    """Runs a single JavaFuzz test, comparing two execution modes."""
    self.ConstructTest()
    ref_retc = self._ref_runner.CompileAndRunTest()
    test_retc = self._test_runner.CompileAndRunTest()
    self.CheckForDivergence(ref_retc, test_retc)
    self.CleanupTest()

  def ConstructTest(self):
    """Use JavaFuzz to generate next Test.java test.

    Raises:
      FatalError: error when javafuzz fails
    """
    if RunCommand(['javafuzz'], out='Test.java', err=None) != RetCode.SUCCESS:
      raise FatalError('Unexpected error while running JavaFuzz')

  def CheckForDivergence(self, ref_retc, test_retc):
    """Checks for divergences and updates statistics.

    Args:
      ref_retc: int, normalized return code of reference runner
      test_retc: int, normalized return code of test runner
    """
    if ref_retc != test_retc:
      # Non-divergent in return code.
      if ref_retc == RetCode.SUCCESS:
        # Both compilations and runs were successful, inspect generated output.
        ref_runner_out = self._ref_runner.GetId() + '_run_out.txt'
        test_runner_out = self._test_runner.GetId() + '_run_out.txt'
        if not filecmp.cmp(ref_runner_out, test_runner_out, shallow=False):
          if self._test_runner.GetId() in BISECTABLE_RUN_MODES:
            self.RunBisectionSearch(expected_output=ref_runner_out)
          self.ReportDivergence('divergence in output')
        else:
          self._num_success += 1
      elif ref_retc == RetCode.TIMEOUT:
        self._num_timed_out += 1
      elif ref_retc == RetCode.NOTCOMPILED:
        self._num_not_compiled += 1
      else:
        self._num_not_run += 1
    else:
      # Divergent in return code.
      if (ref_retc in BISECTABLE_RET_CODES and test_retc in BISECTABLE_RET_CODES
          and self._test_runner.GetId() in BISECTABLE_RUN_MODES):
        self.RunBisectionSearch(expected_retcode=ref_retc)
      self.ReportDivergence('divergence in return code: ' +
                            ref_retc.name + ' vs. ' + test_retc.name)

  def ReportDivergence(self, reason):
    """Reports and saves a divergence."""
    self._num_divergences += 1
    print('\n', self._test, reason)
    # Save.
    ddir = 'divergence' + str(self._test)
    os.mkdir(ddir)
    for f in glob('*.txt') + ['Test.java']:
      shutil.move(f, ddir)

  def RunBisectionSearch(self, expected_retcode=None, expected_output=None):
    args = self._test_runner.GetBisectionSearchArgs() + ['--logfile',
                                                         'bisection_log.txt']
    if expected_retcode:
      args += ['--expected-retcode', expected_retcode.name]
    if expected_output:
      args += ['--expected-output', expected_output]
    if self._device:
      args += ['--device-serial', self._device]
    bisection_search_path = '{0}/{1}'.format(
        GetEnvVariableOrError('ANDROID_BUILD_TOP'),
        'art/tools/bisection_search/bisection_search.py')
    RunCommand([bisection_search_path] + args, out='bisection_out.txt',
               err=None)

  def CleanupTest(self):
    """Cleans up after a single test run."""
    to_remove = glob('*.dex') + glob('*.class') + glob('*.txt') + glob('*.java')
    for f in to_remove:
      os.remove(f)


def main():
  # Handle arguments.
  parser = argparse.ArgumentParser()
  parser.add_argument('--num_tests', default=10000,
                      type=int, help='number of tests to run')
  parser.add_argument('--device', help='target device serial number')
  parser.add_argument('--ref_mode', default='ri',
                      help='reference execution mode (default: ri)')
  parser.add_argument('--test_mode', default='hopt',
                      help='test execution mode (default: hopt)')
  args = parser.parse_args()
  if args.ref_mode == args.test_mode:
    raise FatalError('Identical execution modes given')
  # Run the JavaFuzz tester.
  with JavaFuzzTester(args.num_tests, args.device,
                      args.ref_mode, args.test_mode) as fuzzer:
    fuzzer.Run()

if __name__ == '__main__':
  main()
