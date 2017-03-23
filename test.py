#!/usr/bin/env python
#
# Copyright 2017, The Android Open Source Project
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

# --run-test : To run run-test
# --gtest : To run gtest
# -j : Number of jobs
# --host: for host tests
# --target: for target tests
# All the other arguments will be passed to the run-test testrunner.
import sys
import subprocess
import os
import argparse

test_set = set()
test_args = []
ANDROID_BUILD_TOP = os.environ.get('ANDROID_BUILD_TOP', os.getcwd())

# Parses arguments required for this script,
# and generates a list of arguments that are meant for
# testrunner script.
for arg in sys.argv[1:]:
  if arg == '--run-test':
    test_set.add('run-test')
  elif arg == '--gtest':
    test_set.add('gtest')
  else:
    if arg is '--target' or arg is '--host':
      test_args.add(arg[2:])
    test_args.append(arg)

if not test_set or 'run-test' in test_set:
  testrunner = os.path.join('./',
                       ANDROID_BUILD_TOP,
                       'art/test/testrunner/testrunner.py')
  test_runner_cmd = [testrunner] + test_args
  print test_runner_cmd
  if subprocess.call(test_runner_cmd):
    sys.exit(1)

if not test_set or 'gtest' in test_set:
  build_target = ''
  if '--host' in test_args:
    build_target += ' test-art-host-gtest'
  if '--target' in test_args:
    build_target += ' test-art-target-gtest'

  build_command = 'make'

  parser = argparse.ArgumentParser()
  parser.add_argument('-j', type=int, dest='n_threads')
  options, unknown = parser.parse_known_args(test_args)
  if options.n_threads:
    n_thread = max(1, options.n_threads)
  build_command += ' -j' + str(n_thread)

  build_command += ' -C ' + ANDROID_BUILD_TOP
  build_command += ' ' + 'test-art-host-gtest'
  # Add 'dist' to avoid Jack issues b/36169180.
  build_command += ' dist'

  print build_command

  if subprocess.call(build_command.split()):
      sys.exit(1)

sys.exit(0)
