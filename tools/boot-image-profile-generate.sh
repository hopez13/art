#!/bin/bash
#
# Copyright (C) 2020 The Android Open Source Project
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

#
# This script creates a boot image profile from a device which executed CUJs.
#

if [[ -z "$ANDROID_BUILD_TOP" ]]; then
  echo "You must run on this after running envsetup.sh and launch target"
  exit 1
fi

if [[ "$#" -lt 3 ]]; then
  echo "Usage $0 <boot.zip-location> <preloaded-blacklist-location> <output-dir> <profman args>"
  echo "Without any profman args the script will use defaults."
  echo "Example: generate-boot-image-profile.sh boot.zip frameworks/base/config/preloaded-classes-blacklist --profman-arg --upgrade-startup-to-hot=true"
  echo "preloaded.black-list is usually in frameworks/base/config/preloaded-classes-blacklist"
  exit 1
fi

echo "Creating work dir"
WORK_DIR=/tmp/android-bcp
mkdir -p $WORK_DIR

BOOT_ZIP=$1
PRELOADED_BLACKLIST=$2
OUT_DIR=$3
shift 3

OUT_BOOT_PROFILE=$OUT_DIR/boot-image-profile.txt
OUT_PRELOADED_CLASSES=$OUT_DIR/preloaded-classes
OUT_SYSTEM_SERVER=$OUT_DIR/art-profile

# Read the profman args.
profman_args=()
while [[ "$#" -ge 2 ]] && [[ "$1" = '--profman-arg' ]]; do
  profman_args+=("$2")
  shift 2
done

echo "Changing dirs to the build top"
cd $ANDROID_BUILD_TOP

echo "Snapshoting platform profiles"
adb shell cmd package snapshot-profile android
adb pull /data/misc/profman/android.prof $WORK_DIR/

echo "Unziping boot.zip"
BOOT_UNZIP_DIR=$WORK_DIR/boot-dex
ART_JARS=$BOOT_UNZIP_DIR/dex_artjars_input
BOOT_JARS=$BOOT_UNZIP_DIR/dex_bootjars_input
SYSTEM_SERVER_JAR=$BOOT_UNZIP_DIR/system/framework/services.jar

unzip -o $BOOT_ZIP -d $BOOT_UNZIP_DIR

echo "Processing boot image jar files"
jar_args=()
for entry in "$ART_JARS"/*
do
  jar_args+=" --apk=$entry"
done
for entry in "$BOOT_JARS"/*
do
  jar_args+=" --apk=$entry"
done
profman_args+=("${jar_args[@]}")

echo "Running profman for boot image profiles"
# NOTE:
# You might want to adjust the default generation arguments based on the data
# For example, to update the selection thresholds you could specify:
#  --method-threshold=10 \
#  --class-threshold=10 \
#  --preloaded-class-threshold=10 \
#  --special-package=android:1 \
#  --special-package=com.android.systemui:1 \
# The treshold is percentage of total aggregation, that is, a method/class is
# included in the profile only if it's used by at least x% of the packages.
# (from 0% - include everything to 100% - include only the items that
# are used by all packages on device).
# The --special-package allows you to give a prioriority to certain packages,
# meaning, if the methods is used by that package then the algorithm will use a
# different selection thresholds.
# (system server is indentified as the "android" package)
profman \
  --generate-boot-image-profile \
  --profile-file=$WORK_DIR/android.prof \
  --out-profile-path=$OUT_BOOT_PROFILE \
  --out-preloaded-classes-path=$OUT_PRELOADED_CLASSES \
  --preloaded-classes-blacklist=$PRELOADED_BLACKLIST \
  --special-package=android:1 \
  --special-package=com.android.systemui:1 \
  ${profman_args[@]}

echo "Done boot image profile"

echo "Running profman for system server"
# For system server profile we want to include everything usually
# We also don't have a preloaded-classes file for it, so we ignore the argument.
profman \
  --generate-boot-image-profile \
  --profile-file=$WORK_DIR/android.prof \
  --out-profile-path=$OUT_SYSTEM_SERVER \
  --apk=$SYSTEM_SERVER_JAR \
  --method-threshold=0 \
  --class-threshold=0

echo "Done system server"

echo ""
echo "Boot profile methods+classes count: $(wc -l $OUT_BOOT_PROFILE)"
echo "Preloaded classes count: " $(wc -l $OUT_PRELOADED_CLASSES)
echo "Sysmte server profile methods+classes count: " $(wc -l $OUT_SYSTEM_SERVER)

CLEAN_UP=true
if [[ "$CLEAN_UP" = "true" ]]; then
  rm -rf $WORK_DIR
fi