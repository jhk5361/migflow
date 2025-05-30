#!/bin/bash

BENCH_BIN=./workloads/SPEC/spec-wrapper.py

#export SKIP_VALIDATION=1
export VERBOSE=1
export KMP_LIBRARY=throughput
export KMP_BLOCKTIME=infinite
export OMP_DYNAMIC=FALSE

MAX_THREADS=$(grep -c processor /proc/cpuinfo)
BENCH_RUN=""

export KMP_LIBRARY=throughput

NTHREADS=1 # bwaves has already 8 processes!
export OMP_NUM_THREADS=${NTHREADS}

export KMP_AFFINITY="proclist=[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23],explicit" #,verbose


if [[ "${BENCH_SIZE}" == "small" ]]; then
	export OMP_NUM_THREADS=${NTHREADS}
	BENCH_RUN+="${BENCH_BIN} 603 0 2"
elif [[ "${BENCH_SIZE}" == "large" ]]; then
	#BENCH_RUN+="${BENCH_BIN} 603 0 8"
	BENCH_RUN+="${BENCH_BIN} 603 0 32"
else
	err "ERROR: Retry with available SIZE. refer to benches/_bwaves.sh"
	exit -1
fi

export BENCH_RUN
export BENCH_NAME="speed_bwaves_base.gcc-baseline-o2-m64"
