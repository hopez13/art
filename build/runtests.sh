#!/bin/bash

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
#

# Run ART APEX tests.

# This script requires guestmount and guestunmount from libguestfs.
# On Debian-based systems, these tools can be installed with:
#
#   sudo aptitude install libguestfs-tools
#

if [[ -z "$ANDROID_PRODUCT_OUT" ]]; then
  echo "You need to source and lunch before you can use this script"
  exit 1
fi

set -e # fail early

echo "Running tests"

work_dir=$(mktemp -d)
mount_point="$work_dir/image"

# Garbage collection.
function finish {
  guestunmount "$mount_point"
  rmdir "$mount_point"
  rm -rf "$work_dir"
}

trap finish EXIT

function die {
  echo "$0: $*"
  exit 1
}

art_apex="com.android.art"

# Build the ART APEX package.
# TODO: Make this build step optional.
make "$art_apex"
system_apexdir="$ANDROID_PRODUCT_OUT/system/apex"
art_apex_package="$system_apexdir/$art_apex.apex"

# Extract the image from the ART APEX.
image_filename="image.img"
unzip -q "$art_apex_package" "$image_filename" -d "$work_dir"
mkdir "$mount_point"
image_file="$work_dir/$image_filename"

# Check filesystems in the image.
image_filesystems="$work_dir/image_filesystems"
virt-filesystems -a "$image_file" >"$image_filesystems"
# We expect a single partition (/dev/sda) in the image.
partition="/dev/sda"
echo "$partition" | cmp "$image_filesystems" -

# Mount the image from the ART APEX.
guestmount -a "$image_file" -m "$partition" "$mount_point"

# Check that the mounted image contains a manifest.
[[ -f "$mount_point/manifest.json" ]]

function check_binary {
  [[ -x "$mount_point/bin/$1" ]] || die "Cannot find binary \`$1\` in mounted image"
}

function check_multilib_binary {
  # TODO: Use $TARGET_ARCH (e.g. check whether it is "arm" or "arm64") to improve
  # the precision of this test?
  [[ -x "$mount_point/bin/$1" ]] || [[ -x "$mount_point/bin/${1}64" ]] \
    || die "Cannot find binary \`$1\` in mounted image"
}

function check_library {
  # TODO: Use $TARGET_ARCH (e.g. check whether it is "arm" or "arm64") to improve
  # the precision of this test?
  [[ -f "$mount_point/lib/$1" ]] || [[ -f "$mount_point/lib64/$1" ]] \
    || die "Cannot find library \`$1\` in mounted image"
}

# Check that the mounted image contains ART base binaries.
check_multilib_binary dalvikvm
check_binary dex2oat
check_binary dexoptanalyzer
check_binary profman

# Check that the mounted image contains ART tools binaries.
check_binary dexdiag
check_binary dexdump
check_binary dexlist
check_binary oatdump

# Check that the mounted image contains ART debug binaries.
check_binary dex2oatd
check_binary dexoptanalyzerd
check_binary profmand

# Check that the mounted image contains ART libraries.
check_library libart-compiler.so
check_library libart.so
check_library libopenjdkjvm.so
check_library libopenjdkjvmti.so
# TODO: Check why this library is currently not included in the generated APEX,
# despite being an explicit dependency.
: check_library libadbconnection
# TODO: Should we check for these libraries too, even if they are not explicitly
# listed as dependencies in the ART APEX module rule?
check_library libartbase.so
check_library libart-dexlayout.so
check_library libart-disassembler.so
check_library libdexfile.so
check_library libprofile.so

# Check that the mounted image contains ART debug libraries.
check_library libartd-compiler.so
check_library libartd.so
check_library libdexfiled.so
check_library libopenjdkd.so
check_library libopenjdkjvmd.so
check_library libopenjdkjvmtid.so
# TODO: Check why this library is currently not included in the generated APEX,
# despite being an explicit dependency.
: check_library libadbconnectiond
# TODO: Should we check for these libraries too, even if they are not explicitly
# listed as dependencies in the ART APEX module rule?
check_library libartbased.so
check_library libartd-dexlayout.so
check_library libprofiled.so

# TODO: Should we check for other libraries, such as:
#
#   libbacktrace.so
#   libbase.so
#   liblog.so
#   libsigchain.so
#   libtombstoned_client.so
#   libunwindstack.so
#   libvixl-arm64.so
#   libvixl-arm.so
#   libvixld-arm64.so
#   libvixld-arm.so
#   ...
#
# ?

echo "Passed"
