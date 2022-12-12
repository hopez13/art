#!/usr/bin/python
from scipy.optimize import differential_evolution
from scipy.stats.mstats import gmean
import subprocess
import re

dbg_str = '{}(cmd) {}(s?) {}(i) {}(fp) {}(data) {}(i*) {}(f*) {}(i/) {}(f/) {}(d/) {}(vcvt) {}(ld) {}(st) {}(bar) {}(br) {}(call) {}(call_) {}(string) {}(nop) {}(bar_) {}(type_check) {}(bench)' 

benchmarks = [
'benchmarks/algorithm/CryptoMD5',
'benchmarks/algorithm/CryptoSHA1',
'benchmarks/algorithm/Linpack',
'benchmarks/algorithm/Sort',
'benchmarks/benchmarksgame/mandelbrot',
'benchmarks/benchmarksgame/nbody',
'benchmarks/benchmarksgame/revcomp',
'benchmarks/caffeinemark/FloatAtom',
'benchmarks/caffeinemark/LoopAtom',
'benchmarks/math/AccessNBody',
'benchmarks/math/MathCordic',
'benchmarks/math/MathPartialSums',
'benchmarks/micro/BitfieldRotate',
'benchmarks/micro/ShifterOperand',
'benchmarks/micro/StringOps',
'benchmarks/micro/SystemArrayCopy',
'benchmarks/micro/intrinsics/Intrinsics',
'benchmarks/micro/intrinsics/MathIntrinsics',
'benchmarks/stanford/Bubblesort',
'benchmarks/stanford/IntMM',
'benchmarks/stanford/Oscar',
'benchmarks/stanford/Queens',
'benchmarks/stanford/RealMM',
]

def run_bench(x):
    scores=[]
    for bench in benchmarks:
        dbg = dbg_str.format('./run.sh',
                            str(int(x[0])),    # --scheduler-strength=$1
                            str(int(x[1])),    # --scheduler-ArmIntegerOpLatency=$2
                            str(int(x[2])),    # --scheduler-ArmFloatingPointOpLatency=$3
                            str(int(x[3])),    # --scheduler-ArmDataProcWithShifterOpLatency=$4
                            str(int(x[4])),    # --scheduler-ArmMulIntegerLatency=$5
                            str(int(x[5])),    # --scheduler-ArmMulFloatingPointLatency=$6
                            str(int(x[6])),    # --scheduler-ArmDivIntegerLatency=$7
                            str(int(x[7])),    # --scheduler-ArmDivFloatLatency=$8
                            str(int(x[8])),    # --scheduler-ArmDivDoubleLatency=$9
                            str(int(x[9])),    # --scheduler-ArmTypeConversionFloatingPointIntegerLatency=$10
                            str(int(x[10])),   # --scheduler-ArmMemoryLoadLatency=$11
                            str(int(x[11])),   # --scheduler-ArmMemoryStoreLatency=$12
                            str(int(x[12])),   # --scheduler-ArmMemoryBarrierLatency=$13
                            str(int(x[13])),   # --scheduler-ArmBranchLatency=$14
                            str(int(x[14])),   # --scheduler-ArmCallLatency=$15
                            str(int(x[15])),   # --scheduler-ArmCallInternalLatency=$16
                            str(int(x[16])),   # --scheduler-ArmLoadStringInternalLatency=$17
                            str(int(x[17])),   # --scheduler-ArmNopLatency=$18
                            str(int(x[18])),   # --scheduler-ArmLoadWithBakerReadBarrierLatency=$19
                            str(int(x[19])),   # --scheduler-ArmRuntimeTypeCheckLatency=$20
                            bench,
                            )
        cmd = re.sub('\([a-z\?_\*\/]*\)', '', dbg)
        # print 'running:', cmd # make sure the testing string is correctly assembled from dgb string.
        f = float(subprocess.check_output(cmd, shell=True))
        print dbg, f
        scores.append(f)
    gmean_score = gmean(scores)
    print 'gmean_score:', gmean_score, '\n'
    return gmean_score

sched_params_bounds = [
    (1,20), # --scheduler-strength=$1
    (1,3),  # --scheduler-ArmIntegerOpLatency=$2
    (5,15), # --scheduler-ArmFloatingPointOpLatency=$3
    (2,5),  # --scheduler-ArmDataProcWithShifterOpLatency=$4
    (5,10), # --scheduler-ArmMulIntegerLatency=$5
    (5,30), # --scheduler-ArmMulFloatingPointLatency=$6
    (5,30), # --scheduler-ArmDivIntegerLatency=$7
    (5,50), # --scheduler-ArmDivFloatLatency=$8
    (5,50), # --scheduler-ArmDivDoubleLatency=$9
    (5,50), # --scheduler-ArmTypeConversionFloatingPointIntegerLatency=$10
    (5,15), # --scheduler-ArmMemoryLoadLatency=$11
    (5,15), # --scheduler-ArmMemoryStoreLatency=$12
    (5,30), # --scheduler-ArmMemoryBarrierLatency=$13
    (3,10), # --scheduler-ArmBranchLatency=$14
    (5,20), # --scheduler-ArmCallLatency=$15
    (5,30), # --scheduler-ArmCallInternalLatency=$16
    (5,30), # --scheduler-ArmLoadStringInternalLatency=$17
    (1,3),  # --scheduler-ArmNopLatency=$18
    (5,30), # --scheduler-ArmLoadWithBakerReadBarrierLatency=$19
    (20,50) # --scheduler-ArmRuntimeTypeCheckLatency=$20
    ]

result = differential_evolution(run_bench, sched_params_bounds, popsize=5, maxiter=5, disp=1)

print 'Done differential_evolution:'
print 'The optimal params after differential_evolution: ', result.x
print 'the benchmark optimal result: ', result.fun
