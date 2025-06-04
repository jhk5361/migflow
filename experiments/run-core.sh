#!/bin/bash

export SCRIPTDIR=$(pwd)

# set print style
source $SCRIPTDIR/_message.sh

# bin set
TIME="/usr/bin/time"
NUMA_EXEC=""
export NUMA_EXEC

# config set 
DATE=$(date +%Y%m%d%H%M)
MACHINE=$(uname -n)

# result files
RESULT_FILE=""
LOG_FILE=""
TRACE_FILE=""
VMSTAT_FILE=""
DMESG_FILE=""
VMSTAT_NODE_FILE=""

export RESULT_FILE
export LOG_FILE
export BENCH_FILE
export TRACE_FILE
export VMSTAT_FILE
export DMESG_FILE
export VMSTAT_NODE_FILE

# global variable set
TEST="default"
CONFIG_AUTONUMA="no"
AUTONUMA="off"
CONFIG_INTELTIERING="no"
CONFIG_AUTOTIERING="no"
CONFIG_TPP="no"
CONFIG_IDT="no"
CONFIG_MTM="no"
CONFIG_OURS="no"
OURS="off"
CONFIG_NVM="no"
CONFIG_MULTITIER="no"
CONFIG_MGLRU="no"
CONFIG_LAST="no"
INS_PATH="${SCRIPTDIR}/../linux-kernels/migflow/module"

export CONFIG_AUTONUMA
export CONFIG_MEMTIERING
export CONFIG_AUTOTIERING
export CONFIG_MULTITIER
export CONFIG_MGLRU

# not used
export CONFIG_IDT
export CONFIG_INTELTIERING
export CONFIG_NVM
export CONFIG_LAST

# import scripts
source $SCRIPTDIR/_config_autotiering.sh
source $SCRIPTDIR/_config_autonuma.sh
source $SCRIPTDIR/_config_tpp.sh
source $SCRIPTDIR/_config_mglru.sh
source $SCRIPTDIR/_config_ours.sh
source $SCRIPTDIR/_utils.sh

#source $SCRIPTDIR/not_used/_config_idt.sh
#source $SCRIPTDIR/not_used/_config_inteltiering.sh

function func_run_bench() {
    ${TIME} -f "execution_time %e (s)" ${NUMA_EXEC} ${BENCH_RUN} 2>&1 | tee ${LOG_FILE}
}

function func_prepare() {
    info "Preparing benchmark start..."

    # set nthreads
    func_set_nthreads

    # MSR
    sudo modprobe msr
    sleep 1

    info "Disable prefetcher..."
    sudo ./init_scripts/prefetchers.sh disable > /dev/null

    info "Disable turbo-boost..."
    sudo ./init_scripts/turbo-boost.sh disable > /dev/null

    info "Set max CPU frequency..."
    sudo ./init_scripts/set_max_freq.sh > /dev/null

    sleep 1

    # config default linux autonuma
    func_config_autonuma ${AUTONUMA} 
    sleep 1

    # config default MGLRU
    func_config_mglru
    sleep 1

    # config autotiering
    func_config_autotiering
    sleep 1

	# not used
    # config intel tiering
    # func_config_inteltiering
    #sleep 1

	# config tpp
	func_config_tpp
	sleep 1

	# not used
    # config idt
    # func_config_idt
    # sleep 1

    # config ours
    func_config_ours ${OURS}
    sleep 1

	info "NUMA_EXEC = ${NUMA_EXEC}..."

    # Drop page cache
    func_clean_page_cache
    sleep 1

	func_clear_dmesg
	sleep 1

	func_clear_trace
	sleep 1

	func_ins_pt ${INS_PATH} ${DELAY} ${MSEC}

	swapoff -a

    export BENCH_SIZE
    export BENCH_NAME

    # The source will bring ${BENCH_RUN}
    if [[ -e ./benches/_${BENCH_NAME}.sh ]]; then
        source ./benches/_${BENCH_NAME}.sh
    else
        err "_${BENCH_NAME}.sh does not exist"
        func_err
    fi
}

function func_main() {
    info "Run benchmark main..."

    info "Bechmark: ${BENCH_NAME} with ${BENCH_SIZE}"
    info "Start date $(date)"

    func_set_files

    cat /sys/devices/system/node/node0/vmstat >> ${VMSTAT_NODE_FILE}
    cat /sys/devices/system/node/node1/vmstat >> ${VMSTAT_NODE_FILE}
    cat /sys/devices/system/node/node2/vmstat >> ${VMSTAT_NODE_FILE}
    cat /sys/devices/system/node/node3/vmstat >> ${VMSTAT_NODE_FILE}
	cat /proc/vmstat >> ${VMSTAT_FILE}
    
    func_run_bench &
    WAIT_PID=$!
    info "Wait pid: ${WAIT_PID}"
	info "bench name ${BENCH_NAME}"

    
    # wait unitl find pid
    while [ $(pidof ${BENCH_NAME} | wc | cut -c7) == "0" ]; do
        sleep 1
    done
    PID=$(pidof ${BENCH_NAME})
	echo $BENCH_NAME > pid.log
	echo $PID >> pid.log
    export PID
    info "Benchmark pid: ${PID}"

	# not used
    # configure idt pid (register to DAMON)
    # func_config_pid_idt #KOO

	# Set YCSB client's ip and port properly
	if [ "$BENCH_NAME" == "redis-server" ]; then
		ssh koo@10.150.21.55 -p2222 "cd ~/src/YCSB && ./bin/ycsb load redis -s -P workloads/workloadd -p "redis.host=10.150.21.36" -p "redis.port=6379" -p "redis.timeout=10000000"  -threads 100 && ./bin/ycsb run redis -s -P workloads/workloadd -p "redis.host=10.150.21.36" -p "redis.port=6379" -p "redis.timeout=10000000"  -threads 100" >> ${BENCH_FILE} 2>&1

		sleep 5

		sudo kill -SIGINT ${PID}
	fi

	# not used
	#if [ "$CONFIG_IDT" == "yes" ]; then
	#	cd /home/koo/src/IDT/IDT-Userspace

	#	python3 driver.py

	#	IDT_PID=$!
	#	cd /home/koo/src/IDT/experiment
	#fi

    # wait untill bench end
    wait ${WAIT_PID}

    func_generate_result

	# not used
	#if [ "$CONFIG_IDT" == "yes" ]; then
	#	sudo kill -SIGINT ${IDT_PID}
	#	IDT_PID=$!
	#fi

    func_finish

    info "End date $(date)"
}

function func_finish() {
    # finish for each test
    case $TEST in
        "tiering")
            info "Get demoted and promoted pages..."
            grep -H -E 'pgpromote|pgdemote' /sys/devices/system/node/node*/vmstat >> ${RESULT_FILE}
            ;;
        "autonuma")
			func_initialize_migration_path
            ;;
        "memtiering")
			func_initialize_migration_path
            ;;
        "memtiering-mglru")
			func_initialize_migration_path
            ;;
        "autotiering")
			func_initialize_migration_path
            ;;
		"tpp")
			info "Get demoted and promoted pages..."
			cat /proc/vmstat | grep -E "promote|demote|migrate" >> ${RESULT_FILE}
			;;
        "idt")
            info "Disable demotion..."
			sudo bash -c "echo 'false' >> /sys/kernel/mm/numa/demotion_enabled"

			cat /proc/idt_state >> ${RESULT_FILE}

            info "Copy trained model to pre-trained path..."
			#cd /home/koo/src/IDT/IDT-Userspace
            rm -r -f pre-trained/checkpoint.old
            mv pre-trained/checkpoint pre-trained/checkpoint.old
            cp -r -f chkpt/* pre-trained/checkpoint
			#cd /home/koo/src/IDT/experiment
            ;;
		"mtm")
			;;
		"ours-base")
			;;
		"ours-td")
			;;
		"ours-td-qd")
			;;
		"ours-td-qd-cb")
			;;
		"default")
			;;
		"inteltiering")
			;;
        *)
            err "Usage: invalid experiment $TEST"
            func_err
            ;;
    esac

	func_rm_pt

    cat /sys/devices/system/node/node0/vmstat >> ${VMSTAT_NODE_FILE}
    cat /sys/devices/system/node/node1/vmstat >> ${VMSTAT_NODE_FILE}
    cat /sys/devices/system/node/node2/vmstat >> ${VMSTAT_NODE_FILE}
    cat /sys/devices/system/node/node3/vmstat >> ${VMSTAT_NODE_FILE}
	cat /sys/kernel/debug/tracing/trace >> ${TRACE_FILE}
	cat /proc/vmstat >> ${VMSTAT_FILE}
	dmesg >> ${DMESG_FILE}

    # Initialization to linux default configuration
    #sudo sysctl kernel.numa_balancing=1
    sudo sysctl kernel.numa_balancing=0
    sleep 1

	echo 0 | sudo tee /sys/kernel/mm/numa/demotion_enabled
	sleep 1

    func_clean_page_cache
    sleep 1

	func_clear_dmesg
	sleep 1

	func_clear_trace
	sleep 1

    # Post-processing of specific benchmarks
    source ./benches/finish.sh
    sleep 1
}

function func_pebs_analysis() {
	bash pebs-analysis.sh
}

ARGS=`getopt -o b:w: --long benchmark:,wss:,msec:,delay:,autonuma,memtiering-MGLRU,memtiering,inteltiering,autotiering,tpp,idt,mtm,ours-base,ours-td,ours-td-qd,ours-td-qd-cb,default,multitier,twotier,last -n run-bench.sh -- "$@"`
if [ $? -ne 0 ]; then
    echo "Terminating..." >&2
    exit -1
fi

eval set -- "${ARGS}"

# parse argument
while true; do
    case "$1" in
        -b|--benchmark)
            BENCH_NAME+=( "$2" )
            shift 2
            ;;
        -w|--wss)
            BENCH_SIZE="$2"
            shift 2
            ;;
		--msec)
			MSEC="$2"
			shift 2
			;;
		--delay)
			DELAY="$2"
			shift 2
			;;
        --autonuma)
            TEST="autonuma"
            CONFIG_AUTONUMA="yes"
            AUTONUMA=AN
            shift 1
            ;;
		--memtiering-MGLRU)
            TEST="memtiering-mglru"
			CONFIG_MGLRU="yes"
            CONFIG_MEMTIERING="yes"
            AUTONUMA=MT
            shift 1
            ;;
		--memtiering)
            TEST="memtiering"
            CONFIG_MEMTIERING="yes"
            AUTONUMA=MT
            shift 1
            ;;
        --inteltiering)
            TEST="inteltiering"
            CONFIG_INTELTIERING="yes"
            shift 1
            ;;
        --autotiering)
            TEST="autotiering"
            CONFIG_AUTOTIERING="yes"
			INS_PATH="${SCRIPTDIR}/../linux-kernels/autotiering/module"

            # autonuma should be enabled
            AUTONUMA=AN
            shift 1
            ;;
		--tpp)
			TEST="tpp"
			CONFIG_TPP="yes"
			INS_PATH="${SCRIPTDIR}/../linux-kernels/tpp/module"
			shift 1
			;;
        --idt)
            TEST="idt"
            CONFIG_IDT="yes"
            shift 1
            ;;
		--mtm)
			TEST="mtm"
			CONFIG_OURS="yes"
			OURS=MTM
			shift 1
			;;
		--ours-base)
			TEST="ours-base"
			CONFIG_OURS="yes"
			OURS=BASE
			shift 1
			;;
		--ours-td)
			TEST="ours-td"
			CONFIG_OURS="yes"
			OURS=TD
			shift 1
			;;
		--ours-td-qd)
			TEST="ours-td-qd"
			CONFIG_OURS="yes"
			OURS=QD
			shift 1
			;;
		--ours-td-qd-cb)
			TEST="ours-td-qd-cb"
			CONFIG_OURS="yes"
			OURS=CB
			shift 1
			;;
		--default)
			TEST="default"
			shift 1
			;;
		--multitier)
			CONFIG_MULTITIER="yes"
			shift 1
			;;
		--twotier)
			CONFIG_MULTITIER="no"
			shift 1
			;;
		--last)
			CONFIG_LAST="yes"
			shift 1
			;;
        --)
            break
            ;;
        *)
            err "Unrecognized option $1"
            exit -1
            ;;
    esac
done

if [ -z  "${BENCH_NAME}" ]; then
    err "Benchmark name parameter must be specified"
    func_usage
    exit -1
fi

func_prepare
func_main

