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

"""Outputs quantitative information about traces outputted by Address Sanitizer."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from collections import Counter
from datetime import datetime
import bisect
import getopt
import os
import sys

out_dir_name = ""

# Returns the category a trace belongs to by searching substrings. Otherwise 0
# is returned
def FindMatch(list_substrings, big_string):
  for ind, substr in enumerate(list_substrings):
    if big_string.find(substr) != -1:
      return ind
  return 0

def PrintUsage():
  # pylint: disable-msg=C6310
  print()
  print( "  usage: " + sys.argv[0] + " [options] [SANITIZER_TRACE_FILE]",
                                     " [SYMBOLIZED_TRACE_FILE] [DEX_START_FILE] [CATEGORIES...]",
                                      sep = "")
  print()
  print( "  -d [OUT_DIR]")
  print( "       directory to output information files to")
  print( "       (defaults to current directory)")
  print()
  print( "  SANITIZER_TRACE_FILE should contain all the filtered")
  print( "       traces that have met a formatting standard")
  print( "       specified by prune_sanitizer_output.py. Should have")
  print( "       the same number of traces as SYMBOLIZED_TRACE_FILE.")
  print()
  print( "  SYMBOLIZED_TRACE_FILE should contain all the symbolized")
  print( "       traces, showing the source of stack trace. Should")
  print( "       have the same number of traces as")
  print( "       SANITIZER_TRACE_FILE.")
  print()
  print( "  DEX_START_FILE should contain lines from logcat that")
  print( "       contain the starting address of dex file in")
  print( "       order to create dex file offsets.")
  print()
  print( "  CATEGORIES are words that are expected to show in")
  print( "       a large subset of traces in SYMBOLIZED_TRACE_FILE")
  print( "       to split output based on each word.")
  print()
  # pylint: enable-msg=C6310
  sys.exit(1)


def TupleToTableFormat(address_list, dex_start_list, cat_list):
  time_format_str = "%H:%M:%S.%f"
  first_address_access_time = datetime.strptime(address_list[0][0], time_format_str)
  for ind, elem in enumerate(address_list):
    elem_date_time = datetime.strptime(elem[0], time_format_str)
    # Shift time values so that first access is at time 0 milliseconds
    seconds_since_start = (elem_date_time - first_address_access_time).total_seconds()
    elem[0] = int(seconds_since_start * 1000)
    address_access = int(elem[1], 16)
    # For each poisoned address, find highest offset less than address
    dex_file_start = dex_start_list[bisect.bisect(dex_start_list, address_access) - 1]
    elem.insert(1, address_access - dex_file_start)
    # Category that a data point belongs to
    elem.insert(2, cat_list[ind])

# Prints information regarding a category and outputs all the traces in the
# category to file specified
def PrintCategoryInfo(symbolized_file_split, outname = "mapped_output",title = ""):
  global out_dir_name
  trace_counts_dict = Counter(symbolized_file_split)
  trace_counts_list_ordered = trace_counts_dict.most_common()
  print("-----------------------------------------------------")
  print(title)
  print("\tNumber of distinct traces: " + str(len(trace_counts_list_ordered)))
  print("\tSum of trace counts: " \
        + str(sum([trace[1] for trace in trace_counts_list_ordered])))
  print("\n\tCount: How many traces appeared with count")
  print("\t" + str(Counter([trace[1] for trace in trace_counts_list_ordered])))
  with open(os.path.join(out_dir_name, outname), "w") as output_file:
    for trace in trace_counts_list_ordered:
      output_file.write("Number of times appeared: " + str(trace[1]) + "\n")
      output_file.write(trace[0].strip())

def main(argv=None):
  global out_dir_name
  if argv is None:
    argv = sys.argv

  try:
    optlist, argv = getopt.getopt(argv[1:], "d:")
  except getopt.GetoptError, unused_error:
    PrintUsage()
  for option, value in optlist:
    if option == "-d":
      if not os.path.exists(value):
        try:
          os.mkdir(value)
        except OSError as ose:
          print("Error with option -d:", ose)
          PrintUsage()
      out_dir_name = value
  if len(argv) < 3:
    PrintUsage()

  categories = argv[3:]
  # Makes sure each trace maps to some category
  categories.insert(0, "Uncategorized")

  logcat_file_name = argv[0]
  with open(logcat_file_name , "r") as log_file:
    logcat_file_data = log_file.readlines()

  symbolized_file_name = argv[1]
  with open(symbolized_file_name , "r") as symbolized_file:
    symbolized_file_split = symbolized_file.read().split("Stack Trace")[1:]

  dex_start_file_name = argv[2]
  with open(dex_start_file_name , "r") as dex_start_file:
    dex_start_file_data = dex_start_file.readlines()

  # Each element is a tuple of time and address accessed
  address_list = [[elem[1] for elem in enumerate(line.split(" ")) if elem[0] in (1, 13)] \
                  for line in logcat_file_data if "use-after-poison" in line]
  # Contains a mapping between traces and the category they belong to
  # based on arguments
  cat_list = [categories[FindMatch(categories, trace)] for trace in symbolized_file_split]
  # Contains a list of starting address of all dex files to calculate dex
  # offsets
  dex_start_list = [int(line.split("@")[1], 16) for line in dex_start_file_data if "@" in line]
  # Formats address_list such that each element is a data point
  TupleToTableFormat(address_list, dex_start_list, cat_list)
  for file_ext, cat_name in enumerate(categories):
    out_file_name = os.path.join(out_dir_name, "time_output_" + str(file_ext) + ".dat")
    with open(out_file_name, "w") as output_file:
      output_file.write("# Category: " + cat_name + "\n")
      output_file.write("# Time, Dex File Offset, Address \n")
      for time, dex_offset, category, address in address_list:
        if category == cat_name:
          output_file.write(str(time) + " " + str(dex_offset) + " #" + \
                            str(address) + "\n")
  # Info of traces containing a call to current category
  for cat_num, cat_name in enumerate(categories[1:]):
    print("\nCategory #%d" % (cat_num + 1))
    cat_handler_split = [trace for trace in symbolized_file_split if cat_name in trace]
    PrintCategoryInfo(cat_handler_split, cat_name.lower() + "cat_output", \
                     "Traces containing: " + cat_name)
    noncat_handler_split = [trace for trace in symbolized_file_split if cat_name not in trace]
    PrintCategoryInfo(noncat_handler_split, "non" + cat_name.lower() + "cat_output", \
                      "Traces not containing: " + cat_name)

  # All traces (including uncategorized) together
  PrintCategoryInfo(symbolized_file_split,"allcat_output", "All traces together:")
  # Traces containing none of keywords
  # Only used if categories are passed in
  if len(categories) > 1:
    noncat_split = [trace for trace in symbolized_file_split if \
                        all(cat_name not in trace for cat_name in categories)]
    PrintCategoryInfo(noncat_split,"noncat_output", \
                      "Uncategorized calls")

if __name__ == '__main__':
  main()
