#!/bin/bash
#
# Copyright (C) 2019 The Android Open Source Project
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

# Script to run all gtests located under $ART_TEST_CHROOT/data/nativetest{64}

ADB="${ADB:-adb}"
all_tests=()
failing_tests=()

# Arguments:
#   ${1}: a path to tests
function add_tests {
  all_tests+=$(${ADB} shell "test -d $ART_TEST_CHROOT/$1 && chroot $ART_TEST_CHROOT find $1 -name \*_test")
}

function fail {
  failing_tests+=($1)
}

# Arguments:
#   ${1}: a path to gtest
function run_gtest {
  ${ADB} shell "chroot $ART_TEST_CHROOT env LD_LIBRARY_PATH= ANDROID_ROOT='/system' ANDROID_RUNTIME_ROOT=/system $1"
}

function run_gtests {
  for i in ${all_tests}; do
    run_gtest "${i}" || fail "${i}"
  done
}

# Arguments: -j <number of parallel jobs>
function run_gtests_parallel {
  if [[ ${#all_tests[@]} -eq 0 ]]; then
    return
  fi

  export -f run_gtest

  local jobs=7
  if [[ "$1" == -j && $# -ge 2 ]]; then
    jobs=$2
  fi

  local joblog_out_dir="${ANDROID_PRODUCT_OUT}"
  if [[ -z "${joblog_out_dir}" ]]; then
    joblog_out_dir=$(mktemp -d)
  fi
  local -r joblog="${joblog_out_dir}/log_jobs_gtest.txt"

  echo "Running gtests with the job count: ${jobs}"

  for i in ${all_tests}; do
    echo "${i}"
  done | parallel --jobs "${jobs}" --joblog "${joblog}" run_gtest {}

  local return_code=$?
  if [[ ${return_code} -ne 0 ]]; then
    # Format of log_jobs_gtest.txt:
    #   Seq Host Starttime JobRuntime Send Receive Exitval Signal Command
    for i in $(awk '{if ($7 != 0) print $10}' "${joblog}"  | grep data); do
      fail "${i}"
    done
  fi
  rm -f "${joblog}"
  return "${return_code}"
}

add_tests "/data/nativetest"
add_tests "/data/nativetest64"

if [[ -n "$(which parallel)" ]]; then
  run_gtests_parallel "$@"
else
  run_gtests
fi

if [[ -n "${failing_tests}" ]]; then
  echo ""
  for i in "${failing_tests[@]}"; do
    echo "Failed test: $i"
  done
  exit 1
fi
