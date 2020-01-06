#!/bin/bash
#
# Copyright (C) 2020 The Android Open Source Project
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

set -e
mkdir redefine_class_path
mkdir redefine_classes
mkdir redefine_dexs

NUMBER=$(echo ${TEST_NAME} | cut -d '-' -f1)
mkdir -p src/art/test_${NUMBER}_transforms/

echo BUILDING TEST $NUMBER

# First create fake TARGET files without any data so we can compile everything else.
find ./redefine_targets -type f | while read jva; do
  res=src/art/test_${NUMBER}_transforms/$(basename $jva)
  $ANDROID_BUILD_TOP/art/test/jvmti-redefine-gen/redefine-gen.py -o $res art.test_${NUMBER}_transforms.$(basename ${jva} | cut -d'.' -f1) /dev/null /dev/null
  echo BUILD INITIAL $res
done

# Build these files.
./default-build "$@"

# Build the class files for the redefined version
$ANDROID_BUILD_TOP/art/tools/javac-helper.sh --show-commands --core-only -d redefine_class_path -cp classes $(find ./redefine_targets -type f)

# Build the dex files for the redefined version. Move the class files to base locations
find ./redefine_class_path -type f | while read cls; do
  out_name=redefine_dexs/$(basename $cls | cut -d '.' -f1)
  echo Dexing $cls to $out_name.dex
  d8 --output $out_name.zip $cls
  unzip -p $out_name.zip classes.dex > $out_name.dex
  cp $cls redefine_classes/$(basename $cls)
done

# Remake the TARGET files
find ./redefine_targets -type f | while read jva; do
  dex_file=redefine_dexs/$(basename $jva | cut -f1 -d'.').dex
  class_file=redefine_classes/$(basename $jva | cut -f1 -d'.').class
  res=src/art/test_${NUMBER}_transforms/$(basename $jva)
  $ANDROID_BUILD_TOP/art/test/jvmti-redefine-gen/redefine-gen.py -o $res art.test_${NUMBER}_transforms.$(basename ${jva} | cut -d'.' -f1) $class_file $dex_file
  echo BUILD FINAL $res
done

# Clear out the old jar and dex files
for d in *.{dex,jar}; do
  echo mv $d PRE_REDEFINE_$d
  mv $d PRE_REDEFINE_$d
done

# Rebuild these files.
./default-build "$@"