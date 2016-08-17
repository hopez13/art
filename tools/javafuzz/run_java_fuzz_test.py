#!/usr/bin/env python2
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
import subprocess
import sys
import os

from tempfile import mkdtemp
from threading import Timer

# Normalized return codes.
EXIT_SUCCESS = 0
EXIT_TIMEOUT = 1
EXIT_NOTCOMPILED = 2
EXIT_NOTRUN = 3

#
# Utility methods.
#

def RunCommand(cmd, args, out, err, timeout = 5):
  """Executes a command, and returns its return code.

  Args:
    cmd: string, a command to execute
    args: string, arguments to pass to command (or None)
    out: string, file name to open for stdout (or None)
    err: string, file name to open for stderr (or None)
    timeout: int, time out in seconds
  Returns:
    return code of running command (forced EXIT_TIMEOUT on timeout)
  """
  if args != None:
    cmd = cmd + ' ' + args
  outf = None
  if out != None:
    outf = open(out, mode='w')
  errf = None
  if err != None:
    errf = open(err, mode='w')
  proc = subprocess.Popen(cmd, stdout=outf, stderr=errf, shell=True)
  timer = Timer(timeout, proc.kill)  # enforces timeout
  timer.start()
  proc.communicate()
  if timer.is_alive():
    timer.cancel()
    returncode = proc.returncode
  else:
    returncode = EXIT_TIMEOUT
  if outf != None:
    outf.close()
  if errf != None:
    errf.close()
  #print 'Running', cmd, 'yields', returncode
  return returncode

def GetJackClassPath():
  """Returns Jack's classpath."""
  top = os.environ.get('ANDROID_BUILD_TOP')
  if top == None:
    raise FatalError('Cannot find AOSP build top')
  libdir = top + '/out/host/common/obj/JAVA_LIBRARIES'
  return libdir + '/core-libart-hostdex_intermediates/classes.jack:' \
       + libdir + '/core-oj-hostdex_intermediates/classes.jack'

def GetExecutionModeRunner(mode):
  """Returns a return for the given execution mode.

  Args:
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
  raise FatalError('Unknown execution mode')

#
# Execution mode classes.
#
# TODO: Provide more concrete implementations, also on target, and reuse
#       staszkiewicz' module for properly setting up dalvikvm on host/target.
#

class TestRunner(object):
  """Abstraction for running a test in a particular execution mode."""
  __meta_class__ = abc.ABCMeta

  @abc.abstractmethod
  def GetDescription(self):
    """Returns a description string of the execution mode."""
    pass

  @abc.abstractmethod
  def GetId(self):
    """Returns a short string that uniquely identifies the execution mode."""
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
    pass

  def GetDescription(self):
    return 'RI on host'

  def GetId(self):
    return 'RI'

  def CompileAndRunTest(self):
    if RunCommand('javac', 'Test.java', None, None, timeout=10) == EXIT_SUCCESS:
      retc = RunCommand('java', 'Test', 'RI_run_out.txt', None)
      if retc != EXIT_SUCCESS and retc != EXIT_TIMEOUT:
        retc = EXIT_NOTRUN
    else:
      retc = EXIT_NOTCOMPILED
    # Cleanup and return.
    RunCommand('rm', '-f Test.class', None, None)
    return retc

class TestRunnerArtOnHost(TestRunner):
  """Concrete test runner of Art on host (interpreter or optimizing)."""

  def  __init__(self, interpreter):
    self._interpreter = interpreter;
    self._art_args = '-cp classes.dex Test'
    if interpreter:
      self._art_args = '-Xint ' + self._art_args
    self._jack_args = '-cp ' + GetJackClassPath() + ' --output-dex . Test.java'

  def GetDescription(self):
    if self._interpreter:
      return 'Art interpreter on host'
    return 'Art optimizing on host'

  def GetId(self):
    if self._interpreter:
      return 'ArtI'
    return 'ArtO'

  def CompileAndRunTest(self):
    if RunCommand('jack', self._jack_args,
                  None, 'jackerr.txt', timeout=10) == EXIT_SUCCESS:
      out = self.GetId() + '_run_out.txt'
      retc = RunCommand('art', self._art_args, out, 'arterr.txt')
      if retc != EXIT_SUCCESS and retc != EXIT_TIMEOUT:
        retc = EXIT_NOTRUN
    else:
      retc = EXIT_NOTCOMPILED
    # Cleanup and return.
    RunCommand('rm',
               '-rf classes.dex jackerr.txt arterr.txt android-data*',
               None, None)
    return retc

#
# Tester classes.
#

class FatalError(Exception):
  """Fatal error in the tester."""
  pass

class JavaFuzzTester(object):
  """Tester that runs JavaFuzz many times and report divergences."""

  def  __init__(self, num_tests, mode1, mode2):
    """Constructor for the tester.

    Args:
    num_tests: int, number of tests to run
    mode1: string, execution mode for first runner
    mode2: string, execution mode for second runner
    """
    self._num_tests = num_tests
    self._runner1 = GetExecutionModeRunner(mode1)
    self._runner2 = GetExecutionModeRunner(mode2)
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
    self._tmp_dir = mkdtemp(dir="/tmp/")
    if self._tmp_dir == None:
      raise FatalError('Cannot obtain temp directory')
    os.chdir(self._tmp_dir)
    return self

  def __exit__(self, etype, evalue, etraceback):
    """On exit, re-enters previously saved current directory and cleans up."""
    os.chdir(self._save_dir)
    if self._num_divergences == 0:
      RunCommand('rm', '-rf ' + self._tmp_dir, None, None)

  def Run(self):
    """Runs JavaFuzz many times and report divergences."""
    print
    print '**\n**** JavaFuzz Testing\n**'
    print
    print '#Tests    :', self._num_tests
    print 'Directory :', self._tmp_dir
    print 'Exec-mode1:', self._runner1.GetDescription()
    print 'Exec-mode2:', self._runner2.GetDescription()
    print
    self.ShowStats()
    for self._test in range(1, self._num_tests + 1):
      self.RunJavaFuzzTest()
      self.ShowStats()
    if self._num_divergences == 0:
      print '\n\nsuccess (no divergences)\n'
    else:
      print '\n\nfailure (divergences)\n'

  def ShowStats(self):
    """Shows current statistics (on same line) while tester is running."""
    print '\rTests:', self._test, \
        'Success:', self._num_success, \
        'Not-compiled:', self._num_not_compiled, \
        'Not-run:', self._num_not_run, \
        'Timed-out:', self._num_timed_out, \
        'Divergences:', self._num_divergences,
    sys.stdout.flush()

  def RunJavaFuzzTest(self):
    """Runs a single JavaFuzz test, comparing two execution modes."""
    self.ConstructTest()
    retc1 = self._runner1.CompileAndRunTest()
    retc2 = self._runner2.CompileAndRunTest()
    self.CheckForDivergence(retc1, retc2)
    self.CleanupTest()

  def ConstructTest(self):
    """Use JavaFuzz to generate next Test.java test.

    Raises:
      FatalError: error when javafuzz fails
    """
    if RunCommand('javafuzz', None, 'Test.java', None) != EXIT_SUCCESS:
      raise FatalError('Unexpected error while running JavaFuzz')

  def CheckForDivergence(self, retc1, retc2):
    """Checks for divergences and updates statistics.

    Args:
      retc1: int, normalized return code of first runner
      retc2: int, normalized return code of second runner
    """
    if retc1 == retc2:
      # Non-divergent in return code.
      if retc1 == EXIT_SUCCESS:
        # Both compilations and runs were successful, inspect generated output.
        args = self._runner1.GetId() + '_run_out.txt ' \
             + self._runner2.GetId() + '_run_out.txt'
        if RunCommand('diff', args, None, None) != EXIT_SUCCESS:
          self.ReportDivergence('Divergence in output')
        else:
          self._num_success += 1
      elif retc1 == EXIT_TIMEOUT:
        self._num_timed_out += 1
      else:
        self._num_not_run += 1
    else:
      # Divergent in return code.
      self.ReportDivergence('Divergence in return code: '
                            + str(retc1) + " vs. " + str(retc2))

  def ReportDivergence(self, reason):
    """Reports and saves a divergence."""
    self._num_divergences += 1
    print '\n', self._test, reason
    # Save.
    ddir = 'divergence' + str(self._test)
    RunCommand('mkdir', ddir, None, None)
    RunCommand('mv', 'Test.java *.txt ' + ddir, None, None)

  def CleanupTest(self):
    """Cleans up after a single test run."""
    RunCommand('rm', '-f Test.java *.txt', None, None)


def main():
  # Handle arguments.
  parser = argparse.ArgumentParser()
  parser.add_argument('--num_tests', default=10000,
                      type=int, help='number of tests to run')
  parser.add_argument('--mode1', default='ri',
                      help='execution mode 1 (default: ri)')
  parser.add_argument('--mode2', default='hopt',
                      help='execution mode 2 (default: hopt)')
  args = parser.parse_args()
  if args.mode1 == args.mode2:
    raise FatalError("Identical execution modes given")
  # Run the JavaFuzz tester.
  with JavaFuzzTester(args.num_tests, args.mode1, args.mode2) as fuzzer:
    fuzzer.Run()

if __name__ == "__main__":
  main()
