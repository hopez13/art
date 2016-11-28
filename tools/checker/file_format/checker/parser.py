# Copyright (C) 2014 The Android Open Source Project
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

from common.archs               import archs_list
from common.logger              import Logger
from file_format.common         import SplitStream
from file_format.checker.struct import CheckerFile, TestCase, TestAssertion, TestExpression

import re

def __isCheckerLine(line):
  return line.startswith("///") or line.startswith("##")

def __extractLine(prefix, line, arch = None, debuggable = False, allow_condition = False):
  """ Attempts to parse a check line. The regex searches for a comment symbol
      followed by the CHECK keyword, given attribute and a colon at the very
      beginning of the line. Whitespaces are ignored. The check line is returned
      in the first value. If a condition is allowed and is found, it is returned
      in the second value; otherwise, None is returned.
  """
  rIgnoreWhitespace = r"\s*"
  rCommentSymbols = [r"///", r"##"]
  arch_specifier = r"-%s" % arch if arch is not None else r""
  dbg_specifier = r"-DEBUGGABLE" if debuggable else r""
  # Create a (named) capturing group for the possible condition.
  opt_cond_specifier = r"(?:-IF\((?P<cond>[^:]*)\))?" if allow_condition else r""
  regexPrefix = rIgnoreWhitespace + \
                r"(" + r"|".join(rCommentSymbols) + r")" + \
                rIgnoreWhitespace + \
                prefix + arch_specifier + dbg_specifier + opt_cond_specifier + r":"

  # The 'match' function succeeds only if the pattern is matched at the
  # beginning of the line.
  match = re.match(regexPrefix, line)
  if match is not None:
    if allow_condition:
      return line[match.end():].strip(), match.group('cond')
    else:
      return line[match.end():].strip(), None
  else:
    return None, None

def __processLine(line, lineNo, prefix, fileName):
  """ This function is invoked on each line of the check file and returns a triplet
      which instructs the parser how the line should be handled. If the line is
      to be included in the current check group, it is returned in the first
      value. If the line starts a new check group, the name of the group is
      returned in the second value. The third value indicates whether the line
      contained an architecture-specific, a debuggable suffix and/or a condition.
  """
  if not __isCheckerLine(line):
    return None, None, None

  # Lines beginning with 'CHECK-START' start a new test case.
  # We currently only consider the architecture suffix in "CHECK-START" lines.
  # Likewise for conditions.
  for debuggable in [True, False]:
    for arch in [None] + archs_list:
      startLine, condition = __extractLine(prefix + "-START", line, arch, debuggable, True)
      if startLine is not None:
        return None, startLine, (arch, debuggable, condition)

  # Lines starting only with 'CHECK' are matched in order.
  plainLine, condition = __extractLine(prefix, line)
  # Currently, only "CHECK-START" lines can have a condition.
  assert condition is None
  if plainLine is not None:
    return (plainLine, TestAssertion.Variant.InOrder, lineNo), None, None

  # 'CHECK-NEXT' lines are in-order but must match the very next line.
  nextLine, condition = __extractLine(prefix + "-NEXT", line)
  # Currently, only "CHECK-START" lines can have a condition.
  assert condition is None
  if nextLine is not None:
    return (nextLine, TestAssertion.Variant.NextLine, lineNo), None, None

  # 'CHECK-DAG' lines are no-order assertions.
  dagLine, condition = __extractLine(prefix + "-DAG", line)
  # Currently, only "CHECK-START" lines can have a condition.
  assert condition is None
  if dagLine is not None:
    return (dagLine, TestAssertion.Variant.DAG, lineNo), None, None

  # 'CHECK-NOT' lines are no-order negative assertions.
  notLine, condition = __extractLine(prefix + "-NOT", line)
  # Currently, only "CHECK-START" lines can have a condition.
  assert condition is None
  if notLine is not None:
    return (notLine, TestAssertion.Variant.Not, lineNo), None, None

  # 'CHECK-EVAL' lines evaluate a Python expression.
  evalLine, condition = __extractLine(prefix + "-EVAL", line)
  # Currently, only "CHECK-START" lines can have a condition.
  assert condition is None
  if evalLine is not None:
    return (evalLine, TestAssertion.Variant.Eval, lineNo), None, None

  Logger.fail("Checker assertion could not be parsed: '" + line + "'", fileName, lineNo)

def __isMatchAtStart(match):
  """ Tests if the given Match occurred at the beginning of the line. """
  return (match is not None) and (match.start() == 0)

def __firstMatch(matches, string):
  """ Takes in a list of Match objects and returns the minimal start point among
      them. If there aren't any successful matches it returns the length of
      the searched string.
  """
  starts = map(lambda m: len(string) if m is None else m.start(), matches)
  return min(starts)

def ParseCheckerAssertion(parent, line, variant, lineNo):
  """ This method parses the content of a check line stripped of the initial
      comment symbol and the CHECK-* keyword.
  """
  assertion = TestAssertion(parent, variant, line, lineNo)
  isEvalLine = (variant == TestAssertion.Variant.Eval)

  # Loop as long as there is something to parse.
  while line:
    # Search for the nearest occurrence of the special markers.
    if isEvalLine:
      # The following constructs are not supported in CHECK-EVAL lines
      matchWhitespace = None
      matchPattern = None
      matchVariableDefinition = None
    else:
      matchWhitespace = re.search(r"\s+", line)
      matchPattern = re.search(TestExpression.Regex.regexPattern, line)
      matchVariableDefinition = re.search(TestExpression.Regex.regexVariableDefinition, line)
    matchVariableReference = re.search(TestExpression.Regex.regexVariableReference, line)

    # If one of the above was identified at the current position, extract them
    # from the line, parse them and add to the list of line parts.
    if __isMatchAtStart(matchWhitespace):
      # A whitespace in the check line creates a new separator of line parts.
      # This allows for ignored output between the previous and next parts.
      line = line[matchWhitespace.end():]
      assertion.addExpression(TestExpression.createSeparator())
    elif __isMatchAtStart(matchPattern):
      pattern = line[0:matchPattern.end()]
      pattern = pattern[2:-2]
      line = line[matchPattern.end():]
      assertion.addExpression(TestExpression.createPattern(pattern))
    elif __isMatchAtStart(matchVariableReference):
      var = line[0:matchVariableReference.end()]
      line = line[matchVariableReference.end():]
      name = var[2:-2]
      assertion.addExpression(TestExpression.createVariableReference(name))
    elif __isMatchAtStart(matchVariableDefinition):
      var = line[0:matchVariableDefinition.end()]
      line = line[matchVariableDefinition.end():]
      colonPos = var.find(":")
      name = var[2:colonPos]
      body = var[colonPos+1:-2]
      assertion.addExpression(TestExpression.createVariableDefinition(name, body))
    else:
      # If we're not currently looking at a special marker, this is a plain
      # text match all the way until the first special marker (or the end
      # of the line).
      firstMatch = __firstMatch([ matchWhitespace,
                                  matchPattern,
                                  matchVariableReference,
                                  matchVariableDefinition ],
                                line)
      text = line[0:firstMatch]
      line = line[firstMatch:]
      if isEvalLine:
        assertion.addExpression(TestExpression.createPlainText(text))
      else:
        assertion.addExpression(TestExpression.createPatternFromPlainText(text))
  return assertion

def ParseCheckerStream(fileName, prefix, stream):
  checkerFile = CheckerFile(fileName)
  fnProcessLine = lambda line, lineNo: __processLine(line, lineNo, prefix, fileName)
  fnLineOutsideChunk = lambda line, lineNo: \
      Logger.fail("Checker line not inside a group", fileName, lineNo)
  for caseName, caseLines, startLineNo, testData in \
      SplitStream(stream, fnProcessLine, fnLineOutsideChunk):
    testArch = testData[0]
    forDebuggable = testData[1]
    condition = testData[2]
    testCase = TestCase(checkerFile, caseName, startLineNo, testArch, forDebuggable, condition)
    for caseLine in caseLines:
      ParseCheckerAssertion(testCase, caseLine[0], caseLine[1], caseLine[2])
  return checkerFile
