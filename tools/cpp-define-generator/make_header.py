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

import sys, re

# Find all defines in the compiler generated assembly file.
asm_define_re = re.compile(r'">>(\w+) (?:\$|#)([-0-9]+) (?:\$|#)(0|1)<<"')
asm_defines = asm_define_re.findall(open(sys.argv[1], "r").read())
if not asm_defines:
  sys.exit(1)  # Failed to find any matches.
for asm_define in sorted(asm_defines):
  name = asm_define[0]
  value = int(asm_define[1])
  negative_value = (asm_define[2] == "1")
  if value < 0 and not negative_value:
    # Overflow - uint64_t constant was pretty printed as negiative int64_t.
    value += 2 ** 64
  print("#define " + name.upper() + " " + format(value, "#x"))
