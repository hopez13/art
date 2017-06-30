#!/usr/bin/python3
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

#
# There are many run-tests which generate their sources automatically.
# It is desirable to keep the checked-in source code, as we re-run generators very rarely.
#
# This script will re-run the generators only if their dependent files have changed and then
# complain if the outputs no longer matched what's in the source tree.
#

import os
import pathlib
import subprocess
import sys
import tempfile

THIS_PATH = os.path.dirname(os.path.realpath(__file__))

TOOLS_GEN_SRCS = [
    # tool -> path to a script to generate a file
    # reference_files -> list of files that the script can generate
    # args -> lambda(path) that generates arguments the 'tool' in order to output to 'path'
    # interesting_files -> which files much change in order to re-run the tool.
    # interesting_to_reference_files: lambda(x,reference_files)
    #                                 given the interesting file 'x' and the list of reference_files,
    #                                 return exactly one reference file that corresponds to it.
    { 'tool' : 'test/988-method-trace/gen_srcs.py',
      'reference_files' : ['test/988-method-trace/src/art/Test988Intrinsics.java' ],
      'args' : lambda output_path: [output_path],
      'interesting_files' : ['compiler/intrinsics_list.h'],
      'interesting_to_reference_file' : lambda interesting, references: references[0],
    },
]

DEBUG = False

def debug_print(msg):
  if DEBUG:
    print("[DEBUG]: " + msg, file=sys.stderr)

def is_interesting(f, tool_dict):
  """
  Returns true if this is a file we want to run this tool before uploading. False otherwise.
  """
  path = pathlib.Path(f)
  return str(path) in tool_dict['interesting_files'] #and path.exists()

def get_changed_files(commit):
  """
  Gets the files changed in the given commit.
  """
  return subprocess.check_output(
      ["git", 'diff-tree', '--no-commit-id', '--name-only', '-r', commit],
      stderr=subprocess.STDOUT,
      universal_newlines=True).split()

def run_tool(f, tool_dict, output):
  """
  Execute this tool by passing the tool args to the tool.
  """
  proc_args = [tool_dict['tool']] + tool_dict['args'](output)
  debug_print("PROC_ARGS: %s" %(proc_args))
  succ = subprocess.call(proc_args)
  return succ

def get_reference_file(changed_file, tool_dict):
   """
   Lookup the file that the tool is generating in response to changing an interesting file
   """
   return tool_dict['interesting_to_reference_file'](changed_file, tool_dict['reference_files'])

def run_diff(changed_file, tool_dict, original_file):
  ref_file = get_reference_file(changed_file, tool_dict)

  return subprocess.call(["diff", ref_file, original_file]) != 0

def run_gen_srcs(files):
  """
  Runs test tools only for interesting files that were changed in this commit.
  """
  if len(files) == 0:
    return

  success = 0
  had_diffs = False

  for tool_dict in TOOLS_GEN_SRCS:
    tool_ran_at_least_once = False
    for f in files:
      if is_interesting(f, tool_dict):
        tmp_file = tempfile.mktemp()

        success = run_tool(f, tool_dict, tmp_file)
        if success != 0:
          sys.exit(success)
        if run_diff(f, tool_dict, tmp_file):
          had_diffs = True
          print("Files diverged; please re-run tools:", file=sys.stderr)
          print("$> %s" %(" ".join([tool_dict['tool']] + tool_dict['args'](get_reference_file(f, tool_dict)))))
        else:
          debug_print("File %s consisted with tool %s" %(get_reference_file(f, tool_dict), tool_dict['tool']))

    if not tool_ran_at_least_once:
      debug_print("Interesting files %s unchanged, skipping tool '%s'" %(tool_dict['interesting_files'], tool_dict['tool']))

  if had_diffs:
    success = 1

  return success


def main():
  if 'PREUPLOAD_COMMIT' in os.environ:
    commit = os.environ['PREUPLOAD_COMMIT']
  else:
    print("WARNING: Not running as a pre-upload hook. Assuming commit to check = 'HEAD'", file=sys.stderr)
    commit = "HEAD"

  os.chdir(os.path.join(THIS_PATH, '..')) # run tool relative to 'art' directory
  debug_print("CWD: %s" %(os.getcwd()))

  changed_files = get_changed_files(commit)
  debug_print("Changed files: %s" %(changed_files))
  return run_gen_srcs(changed_files)

if __name__ == '__main__':
  sys.exit(main())
