#!/usr/bin/env python3
#
# Copyright (C) 2021 The Android Open Source Project
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

import os
import shutil

script_dir = os.path.dirname(os.path.realpath(__file__))

lib_dir = "lib"
if script_dir.endswith("64"):
    lib_dir += "64"

libart_so = "libart-with-public-symbols.so"

libart_path = os.path.join(
        script_dir, "..", "..", "..", "system", lib_dir, libart_so)
libart_path = os.path.normpath(libart_path)

dest_path = os.path.join(script_dir, libart_so)

print(f"shutil.copyfile({libart_path}, {dest_path})")
shutil.copyfile(libart_path, dest_path)
