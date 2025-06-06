#!/bin/bash

BENCH_BIN=./workloads/SPEC/spec-wrapper.py

export SKIP_VALIDATION=0
export VERBOSE=1
export KMP_LIBRARY=throughput
export KMP_BLOCKTIME=infinite
export OMP_DYNAMIC=FALSE

MAX_THREADS=$(grep -c processor /proc/cpuinfo)
BENCH_RUN=""

export KMP_LIBRARY=throughput

NTHREADS=2 # deepsjeng has 16 process!
export OMP_NUM_THREADS=${NTHREADS}

if [[ $CONFIG_PINNED == "yes" ]]; then
	export KMP_AFFINITY="compact"
elif [[ $NTHREADS -eq $MAX_THREADS ]]; then
	export KMP_AFFINITY="proclist=[0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30,32,34,36,38,40,42,46],explicit" #,verbose
else
	export KMP_AFFINITY="compact,verbose"
	BENCH_RUN+="taskset -c 0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30,32,34,36,38,40,42,46 "
fi

if [[ "${BENCH_SIZE}" == "small" ]]; then
	BENCH_RUN+="${BENCH_BIN} 631 0 4"
elif [[ "${BENCH_SIZE}" == "large" ]]; then
	BENCH_RUN+="${BENCH_BIN} 631 0 32"
else
	err "ERROR: Retry with available SIZE. refer to benches/_deepsjeng.sh"
	exit -1
fi

export BENCH_RUN
export BENCH_NAME="deepsjeng_s_base.gcc-baseline-o2-m64"
