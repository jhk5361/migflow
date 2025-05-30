#!/bin/bash

# configure mgrlu
function func_config_mglru() {
	info "Config mgrlu ${CONFIG_MGLRU}..."

    case $CONFIG_MGLRU in
    	[Nn][Oo]) # MGLRU OFF
            info "Disable mgrlu..."
			sudo bash -c "echo n >/sys/kernel/mm/lru_gen/enabled"
			cat /sys/kernel/mm/lru_gen/enabled
            ;;
        [Yy][Ee][Ss]) # MGLRU ON
            info "Enable mgrlu..."
            echo 1 > /sys/kernel/mm/numa/demotion_enabled
            echo 2 | sudo tee /proc/sys/kernel/numa_balancing
            echo 20 | sudo tee /proc/sys/kernel/numa_balancing_rate_limit_mbps
			echo 20 | sudo tee /proc/sys/kernel/numa_balancing_promote_rate_limit_MBps

			sudo bash -c "echo y >/sys/kernel/mm/lru_gen/enabled"
			cat /sys/kernel/mm/lru_gen/enabled
            ;;

        *)
            err "Usage: invalid parameter $CONFIG_MGLRU for MGLRU"
            func_err
            ;;

	esac
}
