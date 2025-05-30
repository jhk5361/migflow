#!/bin/bash

if [ $EUID -ne 0 ]; then
    echo "Please run as root or sudo"
    exit -1
fi

################ SCHEMES ###################
# migflow kernel
#SCHEME="--default" # no mig
#SCHEME="--autonuma" # balanced autonuma
#SCHEME="--memtiering-MGLRU" # tiered autonuma
#SCHEME="--mtm" # mtm
#SCHEME="--ours-base" # migflow baseline (no qd, no td, no cb)
#SCHEME="--ours-td" # baseline + td
#SCHEME="--ours-td-qd" # baseline + td + qd
#SCHEME="--ours-td-qd-cb" # baseline + td + qd + cb

# autotiering kernel
#SCHEME="--autotiering" # autotiering

# tpp kernel
#SCHEME="--tpp" # tpp

# not used
#SCHEME="--memtiering"
#SCHEME="--inteltiering"
#SCHEME="--idt"
###########################################


NUM_ITER=1

TIER="--multitier"
#TIER="--twotier"

# choose schemes to run

#for SCHEME in "--ours-td-qd-cb" "--mtm" "--autonuma" "--default" "--ours-base" "--ours-td" "--ours-td-qd" "--memtiering-MGLRU"; do
#for SCHEME in "--autotiering"; do
#for SCHEME in "--inteltiering"; do
#for SCHEME in "--tpp"; do
#for SCHEME in "--default" "--autonuma" "--memtiering-MGLRU" "--mtm" "--ours-td-qd-cb"; do
#for SCHEME in "--ours-td-qd-cb"; do
#for SCHEME in  "--tpp" ; do
#for SCHEME in  "--autonuma" "--memtiering-MGLRU" "--mtm" ; do
#for SCHEME in "--autotiering"; do
#for SCHEME in "--memtiering-MGLRU"; do
#for SCHEME in "--ours-td"; do
#for SCHEME in "--idt"; do
#for SCHEME in "--default" "--ours-td-qd-cb" "--mtm" "--ours-base" "--ours-td" "--ours-td-qd" "--autonuma" "--memtiering-MGLRU"; do
#for SCHEME in "--mtm" "--ours-base" "--ours-td" "--ours-td-qd" "--default" "--autonuma" "--memtiering-MGLRU"; do
#for SCHEME in "--ours-base" "--ours-td" "--ours-td-qd" "--autonuma" "--memtiering-MGLRU"; do
#for SCHEME in "--ours-td" "--ours-td-qd" "--ours-base"; do
#for SCHEME in "--autonuma"; do
#for SCHEME in "--autonuma" "--memtiering-MGLRU"; do
#for SCHEME in "--ours-td-qd-cb"; do
for SCHEME in "--default" "--autonuma" "--memtiering-MGLRU" "--mtm" "--ours-base" "--ours-td" "--ours-td-qd" "--ours-td-qd-cb"; do

	# choose workloads to run

	#for WL in "redis-server" "graph500"; do
	#for WL in "redis-server"; do
	#for WL in "gups-pebs"; do
	#for WL in "graph500"; do
	#for WL in "silo"; do
	#for WL in "XSBench" "btree"; do
	#for WL in "silo" "XSBench" "redis-server"; do
	#for WL in "graph500"; do
	#for WL in "btree"; do
	#for WL in "bwaves"; do
	#for WL in "XSBench"; do
	for WL in "XSBench" "btree" "silo" "redis-server" "graph500"; do

		# interate experiments ITER times
		for ITER in $(seq 1 ${NUM_ITER}); do
			#for MSEC in "1000"; do
			#for MSEC in "100" "200" "400" "800" "1600" "3200" "6400" "12800"; do
			#for MSEC in "10" "400" "1600" "6400" "25600" "102400" "409600"; do
			#for DELAY in "10" "1000"; do

			for DELAY in "10"; do
				#for MSEC in "100000" "10000" "1000" "100" "10" "1"; do
				for MSEC in "100"; do
					#bash run-core.sh --benchmark ${WL} --wss 400GB ${SCHEME} ${TIER}
					bash run-core.sh --benchmark ${WL} --wss large ${SCHEME} ${TIER} --msec ${MSEC} --delay ${DELAY}
				done
			done
		done
	done
	#for ITER in $(seq 1 ${NUM_ITER}); do
		#bash run-core.sh --benchmark redis-server --wss large ${SCHEME} ${TIER}
		#bash run-core.sh --benchmark graph500 --wss 400GB ${SCHEME} ${TIER}
		#bash run-core.sh --benchmark imagick --wss small ${SCHEME} ${TIER}
		#bash run-core.sh --benchmark xz --wss tiny ${SCHEME} ${TIER}
		#bash run-core.sh --benchmark roms --wss small ${SCHEME} ${TIER}
		#bash run-core.sh --benchmark bc --wss small ${SCHEME} ${TIER}
		#bash run-core.sh --benchmark bfs --wss small ${SCHEME} ${TIER}
		#bash run-core.sh --benchmark pr --wss small ${SCHEME} ${TIER}
		#bash run-core.sh --benchmark deepsjeng --wss small ${SCHEME} ${TIER}
		#bash run-core.sh --benchmark bwaves --wss small ${SCHEME} ${TIER}
		#bash run-core.sh --benchmark cactuBSSN --wss tiny ${SCHEME} ${TIER}
	
		#bash run-core.sh --benchmark gups-pebs --wss small ${SCHEME} ${TIER}
		#bash run-core.sh --benchmark gups-random --wss small ${SCHEME} ${TIER}
		#bash run-core.sh --benchmark gups-instantaneous --wss small ${SCHEME} ${TIER}


		#bash run-core.sh --benchmark graph500 --wss 400GB ${SCHEME} ${TIER}
		#bash run-core.sh --benchmark imagick --wss large ${SCHEME} ${TIER}
		#bash run-core.sh --benchmark xz --wss large ${SCHEME} ${TIER}
		#bash run-core.sh --benchmark roms --wss large ${SCHEME} ${TIER}
		#bash run-core.sh --benchmark bc --wss large ${SCHEME} ${TIER}
		#bash run-core.sh --benchmark bfs --wss large ${SCHEME} ${TIER}
		#bash run-core.sh --benchmark pr --wss large ${SCHEME} ${TIER}
		#bash run-core.sh --benchmark deepsjeng --wss large ${SCHEME} ${TIER}
		#bash run-core.sh --benchmark bwaves --wss large ${SCHEME} ${TIER}
		#bash run-core.sh --benchmark cactuBSSN --wss large ${SCHEME} ${TIER}
		#bash run-core.sh --benchmark gups-pebs --wss large ${SCHEME} ${TIER}
		#bash run-core.sh --benchmark gups-random --wss large ${SCHEME} ${TIER}
		#bash run-core.sh --benchmark gups-instantaneous --wss large ${SCHEME} ${TIER}
	#done
done
