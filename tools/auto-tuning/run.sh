#!/bin/bash
BENCH=${21}
INPUTS="-Xcompiler-option --scheduler-strength=$1  \
        -Xcompiler-option --scheduler-ArmIntegerOpLatency=$2  \
        -Xcompiler-option --scheduler-ArmFloatingPointOpLatency=$3  \
        -Xcompiler-option --scheduler-ArmDataProcWithShifterOpLatency=$4  \
        -Xcompiler-option --scheduler-ArmMulIntegerLatency=$5  \
        -Xcompiler-option --scheduler-ArmMulFloatingPointLatency=$6  \
        -Xcompiler-option --scheduler-ArmDivIntegerLatency=$7  \
        -Xcompiler-option --scheduler-ArmDivFloatLatency=$8  \
        -Xcompiler-option --scheduler-ArmDivDoubleLatency=$9  \
        -Xcompiler-option --scheduler-ArmTypeConversionFloatingPointIntegerLatency=${10}  \
        -Xcompiler-option --scheduler-ArmMemoryLoadLatency=${11}  \
        -Xcompiler-option --scheduler-ArmMemoryStoreLatency=${12}  \
        -Xcompiler-option --scheduler-ArmMemoryBarrierLatency=${13}  \
        -Xcompiler-option --scheduler-ArmBranchLatency=${14}  \
        -Xcompiler-option --scheduler-ArmCallLatency=${15}  \
        -Xcompiler-option --scheduler-ArmCallInternalLatency=${16}  \
        -Xcompiler-option --scheduler-ArmLoadStringInternalLatency=${17}  \
        -Xcompiler-option --scheduler-ArmNopLatency=${18}  \
        -Xcompiler-option --scheduler-ArmLoadWithBakerReadBarrierLatency=${19}  \
        -Xcompiler-option --scheduler-ArmRuntimeTypeCheckLatency=${20}  \
"

sleep 1
adb logcat -c
adb shell "rm -f /data/dalvik-cache/arm/data*"
adb shell "cd /data/local/tmp && dalvikvm32 $INPUTS -cp /data/local/tmp/bench.apk org/linaro/bench/RunBench $BENCH" | sed "s/:/ /g" | awk 'BEGIN{sum=0.0}{sum=sum+$2}END{print sum}'
