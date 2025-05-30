#!/bin/bash
BENCH_BIN=./workloads/vmitosis-workloads/bin


BENCH_RUN+="${BENCH_BIN}/bench_btree_mt"

export BENCH_RUN
export BENCH_NAME="bench_btree_mt"
