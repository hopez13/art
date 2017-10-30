#!/bin/bash
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

green='\033[0;32m'
nc='\033[0m'

# Setup as root, as device cleanup requires it.
adb root
adb wait-for-device

if [[ -n "$ART_TEST_CHROOT" ]]; then
  # Check that ART_TEST_CHROOT is correctly defined.
  [[ "x$ART_TEST_CHROOT" = x/* ]] || { echo "$ART_TEST_CHROOT is not an absolute path"; exit 1; }

  echo -e "${green}Clean up /system in chroot${nc}"
  adb shell rm -rf "$ART_TEST_CHROOT/system"

  echo -e "${green}Clean up some subdirs in /data in chroot${nc}"
  adb shell rm -rf \
    "$ART_TEST_CHROOT/data/art-test" \
    "$ART_TEST_CHROOT/data/nativetest" \
    "$ART_TEST_CHROOT/data/nativetest64" \
    "$ART_TEST_CHROOT/data/run-test" \
    "$ART_TEST_CHROOT/data/dalvik-cache/*" \
    "$ART_TEST_CHROOT/data/misc/trace/*"
else
  adb shell rm -rf \
    /data/local/tmp /data/art-test /data/nativetest /data/nativetest64 '/data/misc/trace/*'
fi
