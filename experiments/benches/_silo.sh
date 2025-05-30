#!/bin/bash
BENCH_BIN=./workloads/silo/out-perf.masstree/benchmarks


MAX_THREADS=$(grep -c processor /proc/cpuinfo)
BENCH_RUN=""


NTHREADS=24

#memory delta: 496093 MB
BENCH_RUN+="${BENCH_BIN}/dbtest --verbose --bench tpcc --num-threads ${NTHREADS} --scale-factor 28 --ops-per-worker=50000000"
#BENCH_RUN+="${BENCH_BIN}/dbtest --verbose --bench tpcc --num-threads ${NTHREADS} --scale-factor 28 --ops-per-worker=5000"

export BENCH_RUN
export BENCH_NAME="dbtest"
