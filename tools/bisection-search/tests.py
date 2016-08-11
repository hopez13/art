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

"""Tests for bisection-search module."""

import unittest

from unittest.mock import Mock

from bisection_search import BugSearch
from bisection_search import Dex2OatWrapperTestable
from bisection_search import FatalError
from bisection_search import MANDATORY_PASSES


class BugSearchTestCase(unittest.TestCase):
  """BugSearch method test case.

  Attributes:
    _METHODS: list of strings, methods compiled by testable
    _PASSES: list of strings, passes run by testable
    _FAILING_METHOD: string, name of method which fails in some tests
    _FAILING_PASS: string, name of pass which fails in some tests
    _MANDATORY_PASS: string, name of a mandatory pass
  """
  _METHODS = ['method_{0}'.format(i) for i in range(93)]
  _PASSES = ['pass_{0}'.format(i) for i in range(73)]
  _FAILING_METHOD = _METHODS[37]
  _FAILING_PASS = _PASSES[44]
  _MANDATORY_PASS = MANDATORY_PASSES[0]

  def setUp(self):
    self.testable_mock = Mock(spec=Dex2OatWrapperTestable)
    self.testable_mock.GetAllMethods.return_value = self._METHODS
    self.testable_mock.GetAllPassesForMethod.return_value = self._PASSES

  def MethodFailsForAnyPasses(self, compiled_methods, run_passes=None):
    return self._FAILING_METHOD not in compiled_methods

  def MethodFailsForAPass(self, compiled_methods, run_passes=None):
    return (self._FAILING_METHOD not in compiled_methods or
            (run_passes is not None and self._FAILING_PASS not in run_passes))

  def test_BugSearch_NeverFails_NoneNone(self):
    self.testable_mock.Test.return_value = True
    res = BugSearch(self.testable_mock)
    self.assertEqual(res, (None, None))

  def test_BugSearch_AlwaysFails_FatalError(self):
    self.testable_mock.Test.return_value = False
    with self.assertRaises(FatalError):
      BugSearch(self.testable_mock)

  def test_BugSearch_AMethodFailsForAnyPasses_FailingMethodNone(self):
    self.testable_mock.Test.side_effect = self.MethodFailsForAnyPasses
    res = BugSearch(self.testable_mock)
    self.assertEqual(res, (self._FAILING_METHOD, None))

  def test_BugSearch_AMethodFailsForAPass_FailingMethodFailingPass(self):
    self.testable_mock.Test.side_effect = self.MethodFailsForAPass
    res = BugSearch(self.testable_mock)
    self.assertEqual(res, (self._FAILING_METHOD, self._FAILING_PASS))

  def test_BugSearch_MandatoryPassPresent_AlwaysRunMandatory(self):
    self.testable_mock.GetAllPassesForMethod.return_value += (
        [self._MANDATORY_PASS])
    self.testable_mock.Test.side_effect = self.MethodFailsForAPass
    BugSearch(self.testable_mock)
    for (ordered_args, keyword_args) in self.testable_mock.Test.call_args_list:
      passes = None
      if 'run_passes' in keyword_args:
        passes = keyword_args['run_passes']
      if len(ordered_args) > 1:  # run_passes passed as ordered argument
        passes = ordered_args[1]
      if passes is not None:
        self.assertIn(self._MANDATORY_PASS, passes)

if __name__ == '__main__':
  unittest.main()
