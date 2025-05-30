#!/bin/bash

if [ $EUID -ne 0 ]; then
    echo "Please run as root or sudo"
    exit -1
fi

NUM_ITER=1

TIER="--multitier"
#TIER="--twotier"

# choose schemes to run
for SCHEME in "--ours-base"; do
	# choose workloads to run
	for WL in "XSBench"; do
		# interate experiments ITER times
		for ITER in $(seq 1 ${NUM_ITER}); do
			for DELAY in "10"; do
				for MSEC in "100"; do
					bash run-core.sh --benchmark ${WL} --wss large ${SCHEME} ${TIER} --msec ${MSEC} --delay ${DELAY}
				done
			done
		done
	done
done
