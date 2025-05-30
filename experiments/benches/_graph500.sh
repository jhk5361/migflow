#!/bin/bash
BENCH_BIN=./workloads/graph500/omp-csr

export SKIP_VALIDATION=1
export VERBOSE=1
export KMP_LIBRARY=throughput
export KMP_BLOCKTIME=infinite
export OMP_DYNAMIC=FALSE

MAX_THREADS=24
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

if [[ "${BENCH_SIZE}" == "8GB" ]]; then
	BENCH_RUN+="${BENCH_BIN}/omp-csr -s 24 -e 16 -V"
elif [[ "${BENCH_SIZE}" == "30GB" ]]; then
	NTHREADS=12
	export OMP_NUM_THREADS=${NTHREADS}
	BENCH_RUN+="${BENCH_BIN}/omp-csr -s 25 -e 28"
elif [[ "${BENCH_SIZE}" == "80GB" ]]; then
	BENCH_RUN+="${BENCH_BIN}/omp-csr -s 27 -e 19"
elif [[ "${BENCH_SIZE}" == "96GB" ]]; then
	BENCH_RUN+="${BENCH_BIN}/omp-csr -s 27 -e 23"
#elif [[ "${BENCH_SIZE}" == "400GB" ]]; then
elif [[ "${BENCH_SIZE}" == "large" ]]; then
	#BENCH_RUN+="${BENCH_BIN}/omp-csr -s 30 -e 12 -V"
	BENCH_RUN+="${BENCH_BIN}/omp-csr-nbfs-numa -s 30 -e 12 -n 8 -V"
	#BENCH_RUN+="${BENCH_BIN}/omp-csr -s 29 -e 30 -V"
	#BENCH_RUN+="${BENCH_BIN}/omp-csr-nbfs -s 29 -e 30 -n 8 -V"
else
	err "ERROR: Retry with available SIZE. refer to benches/_graph500.sh"
	exit -1
fi

export BENCH_RUN
export BENCH_NAME="omp-csr-nbfs-numa"
