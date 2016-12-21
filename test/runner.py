#!/usr/bin/python3 -E
#
# Copyright (C) 2015 The Android Open Source Project
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
Run a set of run-tests.
"""

import argparse
import collections
import concurrent.futures
import subprocess
import sys

class TestGroup(collections.namedtuple("TestGroup", ['args', 'tests'])):
  pass

class Test(collections.namedtuple("TestGroup", ['args', 'test'])):
  def run_test(self):
    return subprocess.call(["art/test/run-test"] + list(self.args.split()) + [self.test]) == 0

class TestResult(collections.namedtuple("TestGroup", ['args', 'test', 'result'])):
  def test_command(self):
    return "art/test/run-test {} {}".format(self.args, self.test)

  def succeeded(self):
    return self.result

def run_test(test):
  return TestResult(test.args, test.test, test.run_test())

def main():
  parser = argparse.ArgumentParser(
      description="Program to invoke run-test with various arguments and tests")
  parser.add_argument("-j", "--jobs",
                      dest="jobs",
                      metavar="NUM",
                      type=int,
                      default=1,
                      action="store",
                      help="How many threads to run the tests with")
  parser.add_argument("--show-success",
                      default=False,
                      action="store_true",
                      help="Print out the cmd-lines of successful test runs.")
  parser.add_argument("--test-group",
                      dest="group",
                      nargs=2,
                      action="append",
                      metavar=("ARGS", "TESTS"),
                      help="Add a test group where we run the given TESTS with the given ARGS")
  args = parser.parse_args()
  tests = [Test(v[0], t) for v in args.group for t in v[1].split()]
  with concurrent.futures.ProcessPoolExecutor(args.jobs) as executor:
    test_results = list(executor.map(run_test, tests))
  if args.show_success:
    print("TEST SUCCESSES")
    for result in test_results:
      if result.succeeded():
        print("Succeeded: {}".format(result.test_command()))
  print("TEST FAILURES")
  failures = False
  for result in test_results:
    if not result.succeeded():
      failures = True
      print("Failed: {}".format(result.test_command()))
  if not failures:
    print("NO TESTS FAILED")
  return 0

if __name__ == "__main__":
  sys.exit(main())
