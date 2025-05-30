#!/bin/bash
BENCH_BIN=./workloads/redis

BENCH_RUN=""

BENCH_RUN+="${BENCH_BIN}/redis-server ${BENCH_BIN}/redis.summary.conf"

export BENCH_RUN
export BENCH_NAME="redis-server"
