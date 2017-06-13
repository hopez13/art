#!/usr/bin/env python
#
# Copyright (C) 2017 The Android Open Source Project
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

"""Cleans up overlapping portions of traces provided by logcat."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import getopt
import os
import sys

check_exact = False
stack_size_check = 4
required_checks = ['use-after-poison', 'READ'] + ['#%d ' % i for i in range(stack_size_check)]
hard_checks = ['#%d ' % i for i in range(stack_size_check, 100)]

stack_divider = "================================================================="

def PrintUsage():
  """Print usage and exit with error."""
  print("")
  print( "  usage: " + sys.argv[0] + " [options] [SANITIZER_LOGCAT]")
  print("")
  print( "  -d [OUT_DIR]")
  print( "       directory to output information files to")
  print( "       default: current directory")
  print("")
  print( "  -e")
  print( "       forces each trace to be cut to have minimum")
  print( "       number of lines")
  print( "       default: not enabled")
  print("")
  print( "  -m [MINIMUM_CALLS_PER_TRACE]")
  print( "       minimum number of lines a trace should have")
  print( "       default: 4")
  print("")
  print( "  SANITIZER_LOGCAT should only contain lines from logcat")
  print( "       that pertain to traces outputted by Address")
  print( "       Sanitizer.")
  print("")
  sys.exit(1)

def HasMinimumLines(trace):
  try:
    trace_indices = [trace.index(required_checks[check_ind]) \
                     for check_ind in range(len(required_checks))]
    if check_exact:
      trace = PruneTraceSub(trace)
    return all(trace_indices[trace_ind] < trace_indices[trace_ind + 1] \
               for trace_ind in range(len(trace_indices) - 1))
  except ValueError:
    return False;
  return True

# Removes all of the trace that comes after the (stack_size_check)th
def PruneTraceSub(trace):
  trace_indices = [trace.index(required_checks[check_ind]) \
                   for check_ind in range(len(required_checks))]
  new_line_index = trace.index("\n",trace_indices[-1])
  return trace[:new_line_index + 1]

# Removes overlapping line numbers and lines out of order
def MakeUnique(trace):
  last_ind = -1
  for str_check in required_checks + hard_checks:
    try:
      location_ind = trace.index(str_check)
      if last_ind > location_ind:
        trace = trace[:trace[:location_ind].find("\n") + 1]
      last_ind = location_ind
      try:
        next_location_ind = trace.index(str_check, location_ind + 1)
        trace = trace[:next_location_ind]
      except ValueError:
        pass
    except ValueError:
      pass
  return trace

def main(argv=None):
  global required_checks
  global stack_size_check
  if argv is None:
    argv = sys.argv

  try:
    optlist, argv = getopt.getopt(argv[1:], "d:em:")
  except getopt.GetoptError, unused_error:
    PrintUsage()
  out_dir_name = ""
  for option, value in optlist:
    if option == "-d":
      if not os.path.exists(value):
        try:
          os.mkdir(path)
        except OSError as ose:
          print("Error with option -d:", ose)
          PrintUsage()
    elif option == "-e":
      check_exact = True
    elif option == "-m":
      try:
        stack_size_check = int(value)
      except ValueError:
        PrintUsage()
  if len(argv) != 1:
    PrintUsage()
  else:
    required_checks = ['use-after-poison', 'READ'] + \
                      ['#%d ' % i for i in range(stack_size_check)]
    hard_checks = ['#%d ' % i for i in range(stack_size_check, 100)]
    stack_trace_file = open(argv[0], "r")
    outfile = os.path.join(out_dir_name,argv[0] + '_filtered')

  stack_trace_data = stack_trace_file.read()
  stack_trace_file.close()
  stack_trace_split = stack_trace_data.split(stack_divider)

  stack_trace_sorted_split = [MakeUnique(trace) for trace in stack_trace_split if HasMinimumLines(trace)]
  # HasMinimumLines is called again because removing lines can prune too much
  stack_trace_clean_split = [trace for trace in stack_trace_sorted_split if HasMinimumLines(trace)]

  stack_trace_clean_data = stack_divider.join(stack_trace_clean_split)
  with open(outfile, "w") as output_file:
    output_file.write(stack_trace_clean_data)

  filter_percent = 100.0 - float(len(stack_trace_clean_split)) / len(stack_trace_split) * 100
  filter_amount = len(stack_trace_split) - len(stack_trace_clean_split)
  print( "Filtered out %d (%f%%) of %d." % (filter_amount, filter_percent, len(stack_trace_split)))

if __name__ == "__main__":
  main()

# vi: ts=2 sw=2
