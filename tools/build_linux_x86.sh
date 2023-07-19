#!/bin/bash
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

set -e

if [ ! -d art ]; then
  echo "Script needs to be run at the root of the android tree"
  exit 1
fi

if [ -z "$OUT_DIR" ]; then
  echo "OUT_DIR should be set for this script to run"
  exit 1
fi

if [ -z "$DIST_DIR" ]; then
  echo "DIST_DIR should be set when this script is run"
  exit 1
fi

# Build and zip some binaries for the linux-x86 host
build/soong/soong_ui.bash --make-mode USE_HOST_MUSL=true BUILD_HOST_static=true ${OUT_DIR}/host/linux-x86/bin/dex2oat64 ${OUT_DIR}/host/linux-x86/bin/dex2oatd64 ${OUT_DIR}/host/linux-x86/bin/dex2oat ${OUT_DIR}/host/linux-x86/bin/dex2oatd ${OUT_DIR}/host/linux-x86/bin/deapexer ${OUT_DIR}/host/linux-x86/bin/debugfs_static
prebuilts/build-tools/linux-x86/bin/soong_zip -o ${DIST_DIR}/host_binaries.zip -f ${OUT_DIR}/host/linux-x86/bin/dex2oat64 -f ${OUT_DIR}/host/linux-x86/bin/dex2oatd64 -f ${OUT_DIR}/host/linux-x86/bin/dex2oat -f ${OUT_DIR}/host/linux-x86/bin/dex2oatd -f ${OUT_DIR}/host/linux-x86/bin/deapexer -f ${OUT_DIR}/host/linux-x86/bin/debugfs_static
