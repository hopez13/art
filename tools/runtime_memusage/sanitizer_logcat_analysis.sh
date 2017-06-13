#!/system/bin/sh
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
#
# Note: Requires $ANDROID_BUILD_TOP/build/envsetup.sh to have been run.
#
# This script takes in a logcat containing Sanitizer traces and outputs several
# files, prints information regarding the traces, and plots information as well.
USE_TEMP=true
# EXACT_ARG and MIN_ARG are passed to prune_sanitizer_output.py
EXACT_ARG=""
MIN_ARG=""
usage() {
  echo "Usage: $0 [options] [LOGCAT_FILE_FILE] [CATEGORIES...]"
  echo "    -e"
  echo "        All traces will have exactly the same number"
  echo "        of categories which is specified by either"
  echo "        the -m argument or by prune_sanitizer_output.py"
  echo
  echo "    -m  [MINIMUM_CALLS_PER_TRACE]"
  echo "        Filters out all traces that do not have"
  echo "        at least MINIMUM_CALLS_PER_TRACE lines."
  echo "        default: specified by prune_sanitizer_output.py"
  echo
  echo "    -o  OUT_DIRECTORY"
  echo "        Puts all output in specified directory."
  echo "        If not given, output will be put in a local"
  echo "        temp folder which will be deleted after"
  echo "        execution."
  echo
  echo "    CATEGORIES are words that are expected to show in"
  echo "       a large subset of symbolized traces. Splits"
  echo "       output based on each word."
  echo
  echo "    LOGCAT_FILE_FILE is the piped output from adb logcat."
  echo
}


while [[ $# -gt 1 ]]; do
case $1 in
  -d)
  shift
  USE_TEMP=false
  OUT_DIR=$1
  shift
  break
  ;;
  -e)
  shift
  EXACT_ARG='-e'
  ;;
  -m)
  shift
  MIN_ARG='-m '"$1"''
  shift
  ;;
  *)
  usage
  exit
esac
done

if [ $# -lt 1 ]; then
  usage
  exit
fi

LOGCAT_FILE=$1
NUM_CAT=$(($# - 1))

# Use a temp directory that will be deleted
if [ $USE_TEMP = true ]; then
  OUT_DIR=$(mktemp -d --tmpdir=$PWD)
fi

if [ ! -d "$OUT_DIR" ]; then
  mkdir $OUT_DIR
fi
# Folder that holds all file output
echo "Using folder: $OUT_DIR"
echo "Extracting ASAN output"
grep "app_process64" $LOGCAT_FILE > $OUT_DIR/asan_output

echo "Extracting Start of Dex File(s)"
grep "RegisterDexFile" $LOGCAT_FILE > $OUT_DIR/dex_start

echo "Filtering/Cleaning ASAN output"
python $ANDROID_BUILD_TOP/art/tools/runtime_memusage/prune_sanitizer_output.py \
  $EXACT_ARG $MIN_ARG -d $OUT_DIR $OUT_DIR/asan_output

echo "Retrieving symbolized traces"
$ANDROID_BUILD_TOP/development/scripts/stack $OUT_DIR/asan_output_filtered > $OUT_DIR/sym_filtered

# Only the category names are needed for the commands giving final output
shift
echo "Creating Categorized Time Table"
python $ANDROID_BUILD_TOP/art/tools/runtime_memusage/symbol_trace_info.py \
  -d $OUT_DIR $OUT_DIR/asan_output_filtered $OUT_DIR/sym_filtered $OUT_DIR/dex_start $@

# Contains the category names used for legend of gnuplot
PLOT_CATS=`echo \"Uncategorized $@\"`
echo "Plotting Categorized Time Table"
# Plots the information from logcat
gnuplot --persist -e \
  'filename(n) = sprintf("'"$OUT_DIR"'/time_output_%d.dat", n);
   catnames = '"$PLOT_CATS"';
   set title "Dex File Offset vs. Time accessed since App Start";
   set xlabel "Time (milliseconds)";
   set ylabel "Dex File Offset (bytes)";
   plot for [i=0:'"$NUM_CAT"'] filename(i) using 1:2 title word(catnames, i + 1);'

if [ $USE_TEMP = true ]; then
  echo "Removing temp directory and files"
  rm -rf $OUT_DIR
fi
