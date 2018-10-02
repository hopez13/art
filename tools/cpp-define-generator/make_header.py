#!/usr/bin/env python
#
# Copyright (C) 2018 The Android Open Source Project
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

# This script looks through compiled object file (stored human readable text),
# and looks for the compile-time constants (added through custom "asm" block).
#   For example:  .ascii  ">>OBJECT_ALIGNMENT_MASK $7 $0<<"
#
# It will transform each such line to #define which is usabe in assembly code.
#   For exmpale:  #define OBJECT_ALIGNMENT_MASK 0x7
#
# Usage: make_header.py out/soong/.intermediates/.../asm_defines.o
#

import sys, re, argparse

def convert(input):
  # Find all defines in the compiler generated assembly file.
  # Sort them to keep the generated ouptput deteministic.
  asm_define_re = re.compile(r'">>(\w+) (?:\$|#)([-0-9]+) (?:\$|#)(0|1)<<"')
  asm_defines = sorted(asm_define_re.findall(input))
  if not asm_defines:
    raise Exception("Failed to find any asm defines in the input")

  # Convert the found constants to #define pragmas.
  output = list()
  for asm_define in asm_defines:
    name = asm_define[0]
    value = int(asm_define[1])
    negative_value = (asm_define[2] == "1")
    if value < 0 and not negative_value:
      # Overflow - uint64_t constant was pretty printed as negative value.
      value += 2 ** 64  # Python will use arbitrary precision arithmetic.
    output.append("#define " + name.upper() + " " + format(value, "#x"))
  return "\n".join(output)

if __name__ == "__main__":
  parser = argparse.ArgumentParser()
  parser.add_argument('input', help="Object file as text")
  args = parser.parse_args()
  print(convert(open(args.input, "r").read()))
