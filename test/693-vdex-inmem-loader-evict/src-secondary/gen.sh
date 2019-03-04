#!/bin/bash
#
# Copyright 2019 The Android Open Source Project
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
TMP=`mktemp -d`
NUM_FILES=30

echo '  private static final byte[][] DEX_CHECKSUMS = new byte[][] {'
for i in $(seq 1 ${NUM_FILES}); do
  if [ ${i} -lt 10 ]; then
    suffix=0${i}
  else
    suffix=${i}
  fi
  (cd "$TMP" && \
      echo "public class MyClass${suffix} { }" > "$TMP/MyClass${suffix}.java" && \
      javac -d "${TMP}" "$TMP/MyClass${suffix}.java" && \
      d8 --output "$TMP" "$TMP/MyClass${suffix}.class" && \
      mv "$TMP/classes.dex" "$TMP/file${suffix}.dex")

  # Dump bytes 8-32 (checksum + signature) that need to change for other files.
  checksum=`head -c 32 -z "$TMP/file${suffix}.dex" | tail -c 24 -z | base64`
  echo '    Base64.getDecoder().decode("'${checksum}'"),'
done
echo '  };'

# Dump first dex file as base.
echo '  private static final byte[] DEX_BYTES_01 = Base64.getDecoder().decode('
base64 "${TMP}/file01.dex" | sed -E 's/^/    "/' | sed ':a;N;$!ba;s/\n/" +\n/g' | sed -E '$ s/$/");/'

# echo '  private static final byte[] DEX_BYTES_B = Base64.getDecoder().decode('
# base64 "${TMP}/classesB.dex" | sed -E 's/^/    "/' | sed ':a;N;$!ba;s/\n/" +\n/g' | sed -E '$ s/$/");/'

# rm -rf "$TMP"
