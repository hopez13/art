#!/usr/bin/python3
#
# Copyright (C) 2023 The Android Open Source Project
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

import argparse
import os
import subprocess
import sys
import logging as log

# Get the directory to the latest stable version of Clang.
def get_clang_bin():
  # Remove "llvm-symbolizer" from path.
  return (os.environ.get("ASAN_SYMBOLIZER_PATH").replace("llvm-symbolizer", ""))

def main():
  desc = """
    Conditionally include an ELF file in a blueprint module depending on if the
    condition is set.

    If 'condition' is set to true then 'input' will be moved to 'output'.
    If 'condition' is set to false (or not set) then a valid empty ELF file
    will be output, which can be linked but will contain nothing.
  """
  parser = argparse.ArgumentParser(description=desc)

  parser.add_argument("input", help="input ELF file that may be output.")
  parser.add_argument("output", help="ELF file to be output")
  parser.add_argument("condition", help="if true then move the 'input' to the 'output'.", nargs="?")

  parser.add_argument("--script", help="if 'condition' is not true, call this script instead and "
                                     + "pass 'output'. E.g: <script> <output> [script-args...]. "
                                     + "Script will be invoked via the shell.")
  parser.add_argument("--script_args", help="additional args passed to '--script'.")
  parser.add_argument("-v", "--verbose", action="store_true")
  args = parser.parse_args()

  if (args.verbose):
    log.basicConfig(format="%(levelname)s: %(message)s", level=log.DEBUG)
  else:
    log.basicConfig(format="%(levelname)s: %(message)s")

  if (args.condition == "true"):
    log.info("Condition is true, using input...")
    subprocess.run(f"mv {args.input} {args.output}", shell=True, check=True)
  elif (args.script != None):
    log.info("Condition is not true, running script..")
    subprocess.run(f"{args.script} {args.script_args} {args.output}", shell=True, check=True)
  else:
    log.info("Condition is not true, stripping everything...")
    clang_bin = get_clang_bin()
    # Build rules (blueprints) do not support arbitrary conditions and so will always expect to see
    # a binary file to link. Stripping everything from the existing binary allows us to "link" it
    # safely without actually linking anything at all.
    subprocess.run(f"{clang_bin}/llvm-strip --strip-all {args.input} -o {args.output}",
        shell=True, check=True)


if __name__ == "__main__":
  main()
