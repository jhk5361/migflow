#!/bin/bash
BENCH_BIN=./workloads/XSBench/openmp-threading

#export SKIP_VALIDATION=0
export VERBOSE=1
export KMP_LIBRARY=throughput
export KMP_BLOCKTIME=infinite
export OMP_DYNAMIC=FALSE

MAX_THREADS=$(grep -c processor /proc/cpuinfo)
BENCH_RUN=""

export KMP_LIBRARY=throughput

NTHREADS=24 # enable max threads for graph500
export OMP_NUM_THREADS=${NTHREADS}

if [[ $CONFIG_PINNED == "yes" ]]; then
	export KMP_AFFINITY="compact"
elif [[ $NTHREADS -eq $MAX_THREADS ]]; then
	export KMP_AFFINITY="proclist=[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23],explicit" #,verbose
else
	export KMP_AFFINITY="compact,verbose"
	BENCH_RUN+="taskset -c 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23 "
fi

#BENCH_RUN+="${BENCH_BIN}/XSBench -t 24 -g 1000000 -p 30000000 -l 67 "
BENCH_RUN+="${BENCH_BIN}/XSBench -t 24 -g 1000000 -p 100000000 -l 67 "

export BENCH_RUN
export BENCH_NAME="XSBench"
