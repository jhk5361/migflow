#!/bin/basu

# configure autonuma
function func_config_ours() {
    local OURS_OPT=$1

    info "Config OURS ${OURS_OPT}..."

    case $OURS_OPT in
        [Oo][Ff][Ff]) # no mig
			NUMA_EXEC="${SCRIPTDIR}/userspace/daemon/koo_mig -q 0 -c 0 -m 0 -d 0 -f 0 -v 1 -i -1 /usr/local/bin/numactl --cpunodebind=0 --physcpubind=0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23 -- "
			export NUMA_EXEC
            ;;
		[Mm][Tt][Mm]) # MTM = preferred 2
            sudo sysctl kernel.numa_balancing=0
			echo 0 | sudo tee /sys/kernel/mm/numa/demotion_enabled
			NUMA_EXEC="${SCRIPTDIR}/userspace/daemon/koo_mig -q 0 -c 0 -m 1 -d 0 -f 0 -v 1 -i -1 /usr/local/bin/numactl --cpunodebind=0 --physcpubind=0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23 --preferred=2 -- "
			export NUMA_EXEC
			;;
        [Bb][Aa][Ss][Ee]) # Base
            sudo sysctl kernel.numa_balancing=0
			echo 0 | sudo tee /sys/kernel/mm/numa/demotion_enabled
			NUMA_EXEC="${SCRIPTDIR}/userspace/daemon/koo_mig -q 0 -c 0 -m 1 -d 0 -f 0 -v 1 -i -1 /usr/local/bin/numactl --cpunodebind=0 --physcpubind=0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23 -- "
			export NUMA_EXEC
            ;;
		[Tt][Dd]) # + top-down allocation
            sudo sysctl kernel.numa_balancing=0
			echo 0 | sudo tee /sys/kernel/mm/numa/demotion_enabled
			NUMA_EXEC="${SCRIPTDIR}/userspace/daemon/koo_mig -q 0 -c 0 -m 1 -d 0 -f 0 -v 1 -i -1 /usr/local/bin/numactl --cpunodebind=0 --physcpubind=0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23 --top-down=0,1,2,3 -- "
			export NUMA_EXEC
			;;
		[Qq][Dd]) # + quick demotion
            sudo sysctl kernel.numa_balancing=0
			echo 0 | sudo tee /sys/kernel/mm/numa/demotion_enabled
			NUMA_EXEC="${SCRIPTDIR}/userspace/daemon/koo_mig -q 1 -c 0 -m 1 -d 0 -f 0 -v 1 -i -1 /usr/local/bin/numactl --cpunodebind=0 --physcpubind=0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23 --top-down=0,1,2,3 -- "
			export NUMA_EXEC
			;;
		[Cc][Bb]) # + cost-benefit
            sudo sysctl kernel.numa_balancing=0
			echo 0 | sudo tee /sys/kernel/mm/numa/demotion_enabled
			NUMA_EXEC="${SCRIPTDIR}/userspace/daemon/koo_mig -q 1 -c 1 -m 1 -d 0 -f 0 -v 1 -i -1 /usr/local/bin/numactl --cpunodebind=0 --physcpubind=0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23 --top-down=0,1,2,3 -- "
			export NUMA_EXEC
			;;
        *)
            err "Usage: {OFF|MTM|BASE|TD|QD|CB} are available ours mode"
            func_err
            ;;
    esac
}
