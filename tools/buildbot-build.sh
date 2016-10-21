#!/bin/bash
#
# Copyright (C) 2015 The Android Open Source Project
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

if [ ! -f build/envsetup.sh ]; then
  echo "Script needs to be run at the root of the android tree"
  exit 1
fi

# Get all the android functions like "get_build_var"
source "build/envsetup.sh"

# Get the out directories from the build, which already respects a custom $OUT_DIR if one was set.
out_dir="$(get_build_var OUT_DIR)"
HOST_OUT="$(get_build_var HOST_OUT)"
TARGET_OUT="$(get_build_var TARGET_OUT)"

common_targets="vogar core-tests apache-harmony-jdwp-tests-hostdex jsr166-tests mockito-target $HOST_OUT/bin/jack"
mode="target"
j_arg="-j$(nproc)"
showcommands=
make_command=

while true; do
  if [[ "$1" == "--host" ]]; then
    mode="host"
    shift
  elif [[ "$1" == "--target" ]]; then
    mode="target"
    shift
  elif [[ "$1" == -j* ]]; then
    j_arg=$1
    shift
  elif [[ "$1" == "--showcommands" ]]; then
    showcommands="showcommands"
    shift
  elif [[ "$1" == "" ]]; then
    break
  fi
done

# Workaround for repo incompatibilities on the Chromium buildbot.
# TODO: Remove this workaround once https://bugs.chromium.org/p/chromium/issues/detail?id=646329
# is addressed.
repo=$(which repo)
if [[ $repo == *"depot_tools"* ]]; then
  ln -s build/soong/root.bp Android.bp
  ln -s build/soong/bootstrap.bash bootstrap.bash
  echo "include build/core/main.mk" > Makefile
fi

if [[ $mode == "host" ]]; then
  make_command="make $j_arg $showcommands build-art-host-tests $common_targets"
  make_command+=" $HOST_OUT/lib/libjavacoretests.so "
  make_command+=" $HOST_OUT/lib64/libjavacoretests.so"
elif [[ $mode == "target" ]]; then
  make_command="make $j_arg $showcommands build-art-target-tests $common_targets"
  make_command+=" libjavacrypto libjavacoretests libnetd_client linker toybox toolbox sh"
  make_command+=" $HOST_OUT/bin/adb libstdc++ "
  make_command+=" $TARGET_OUT/etc/public.libraries.txt"
fi

echo "Executing $make_command"
$make_command
