#!/bin/bash

BENCH_BIN=./workloads/GAPBS/gapbs-wrapper.py

export SKIP_VALIDATION=1
export VERBOSE=1
export KMP_LIBRARY=throughput
export KMP_BLOCKTIME=infinite
export OMP_DYNAMIC=FALSE

MAX_THREADS=$(grep -c processor /proc/cpuinfo)
BENCH_RUN=""

export KMP_LIBRARY=throughput
export OMP_NUM_THREADS=${NTHREADS}

if [[ $CONFIG_PINNED == "yes" ]]; then
	export KMP_AFFINITY="compact"
elif [[ $NTHREADS -eq $MAX_THREADS ]]; then
	export KMP_AFFINITY="proclist=[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23],explicit" #,verbose
else
	export KMP_AFFINITY="compact,verbose"
	BENCH_RUN+="taskset -c 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23 "
fi

if [[ "${BENCH_SIZE}" == "small" ]]; then
	NTHREADS=2
	export OMP_NUM_THREADS=${NTHREADS}
	BENCH_RUN+="${BENCH_BIN} pr twitter 0 6"
elif [[ "${BENCH_SIZE}" == "large" ]]; then
	BENCH_RUN+="${BENCH_BIN} pr twitter 0 32"
else
	err "ERROR: Retry with available SIZE. refer to benches/_pr.sh"
	exit -1
fi

export BENCH_RUN
export BENCH_NAME="pr"
