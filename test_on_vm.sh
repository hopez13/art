#!/bin/bash
#
# Copyright 2022, The Android Open Source Project
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

unset ART_TEST_ANDROID_ROOT
unset CUSTOM_TARGET_LINKER
unset ART_TEST_ANDROID_ART_ROOT
unset ART_TEST_ANDROID_RUNTIME_ROOT
unset ART_TEST_ANDROID_I18N_ROOT
unset ART_TEST_ANDROID_TZDATA_ROOT

#export ANDROID_SERIAL=040672592M100808
export ANDROID_SERIAL=nonexistent
export SOONG_ALLOW_MISSING_DEPENDENCIES=true

. ./build/envsetup.sh
lunch aosp_riscv64-userdebug
#lunch aosp_arm-userdebug
art/tools/buildbot-build.sh --target -j72 #--installclean

#exit
export ART_TEST_SSH_USER=ubuntu
export ART_TEST_SSH_HOST=localhost
export ART_TEST_SSH_PORT=10001
export ART_TEST_ON_VM=true
#export ART_TEST_CHROOT=/data/local/art-test-chroot

. art/tools/buildbot-utils.sh
#art/tools/buildbot-cleanup-device.sh
#art/tools/buildbot-setup-device.sh
art/tools/buildbot-sync.sh

#art/test/run-test --chroot $ART_TEST_CHROOT --64 --interpreter -O --no-relocate --no-image 001-HelloWorld
#art/test/run-test --chroot $ART_TEST_CHROOT --64 --interpreter -O --never-clean --no-relocate --runtime-option -Xcheck:jni --runtime-option -verbose:jni,startup,threads,class --no-image 001-HelloWorld
#art/test/run-test --chroot $ART_TEST_CHROOT --64 --interpreter -O --never-clean --no-relocate --runtime-option -Xcheck:jni --no-image 001-HelloWorld

art/test.py -j8 --target -r --no-prebuild --ndebug --no-image --64 --interpreter $@

art/tools/run-gtests.sh

#art/test/run-test --chroot $ART_TEST_CHROOT --64 --interpreter -O --never-clean --no-relocate --no-image 001-HelloWorld

#art/tools/buildbot-cleanup-device.sh
