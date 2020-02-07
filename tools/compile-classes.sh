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
# This script compiles Java language source files into arm64 odex files.
#
# Before running this script, do lunch and build for an arm64 device.
#
# This script many hardcoded paths, so it will be prone to bitrot. Please update
# as needed.
#

set -e

SCRATCH=`mktemp -d`
DEX_FILE=classes.dex
ODEX_FILE=classes.odex

javac -d $SCRATCH $1
d8 $SCRATCH/*.class

BOOT_CLASSPATH=\
$OUT/apex/com.android.art.debug/javalib/core-oj.jar:\
$OUT/apex/com.android.art.debug/javalib/core-libart.jar:\
$OUT/apex/com.android.art.debug/javalib/core-icu4j.jar:\
$OUT/apex/com.android.art.debug/javalib/okhttp.jar:\
$OUT/apex/com.android.art.debug/javalib/bouncycastle.jar:\
$OUT/apex/com.android.art.debug/javalib/apache-xml.jar:\
$OUT/system/framework/framework.jar:\
$OUT/system/framework/ext.jar:\
$OUT/system/framework/telephony-common.jar:\
$OUT/system/framework/voip-common.jar:\
$OUT/system/framework/ims-common.jar:\
$OUT/system/framework/framework-atb-backward-compatibility.jar

BOOT_CLASSPATH_LOCATIONS=\
/apex/com.android.art/javalib/core-oj.jar:\
/apex/com.android.art/javalib/core-libart.jar:\
/apex/com.android.art/javalib/core-icu4j.jar:\
/apex/com.android.art/javalib/okhttp.jar:\
/apex/com.android.art/javalib/bouncycastle.jar:\
/apex/com.android.art/javalib/apache-xml.jar:\
/system/framework/framework.jar:\
/system/framework/ext.jar:\
/system/framework/telephony-common.jar:\
/system/framework/voip-common.jar:\
/system/framework/ims-common.jar:\
/system/framework/framework-atb-backward-compatibility.jar

BOOT_IMAGE=$OUT/apex/com.android.art.debug/javalib/boot.art

$ANDROID_HOST_OUT/bin/dex2oatd \
    --boot-image=$BOOT_IMAGE \
    --dex-file=$DEX_FILE \
    --oat-file=$ODEX_FILE \
    --instruction-set=arm64 \
    --abort-on-hard-verifier-error \
    --force-determinism \
    --compiler-filter=speed \
    --compilation-reason=prebuilt \
    --android-root=$ANDROID_HOST_OUT \
    --runtime-arg \
    -Xbootclasspath:$BOOT_CLASSPATH \
    --runtime-arg \
    -Xbootclasspath-locations:$BOOT_CLASSPATH_LOCATIONS \
    --generate-debug-info \
    --generate-mini-debug-info \
    --dump-cfg=output.cfg \

rm -rf $SCRATCH

echo
echo "OAT file is at $ODEX_FILE"
echo
echo "View it with one of the following commands:"
echo
echo "    oatdump --oat-file=$ODEX_FILE"
echo
echo "    aarch64-linux-android-objdump -d $ODEX_FILE"
echo
echo "The CFG is dumped to output.cfg for inspection of individual compiler passes."
echo
