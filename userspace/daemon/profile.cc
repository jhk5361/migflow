#include <stdio.h>
#include <sys/epoll.h>
#include "koo_mig.h"
#include "profile.h"
#include "utils.h"

extern struct koo_mig kmig;
static int epoll_fd;
static struct epoll_event ev, events[MAX_EVENTS];

int init_profile(int pid) {
	kmig.fd = -1;
	kmig.pid = -1;

    int fd = open(DEVICE_NAME, O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        perror("Failed to open the device");
        return -1;
    }

	for (unsigned long i = 0; i < MAX_NR_RB; i++) {
		char *mapped_mem = (char *)mmap(NULL, RB_BUF_SIZE + RB_HEADER_SIZE, PROT_READ, MAP_SHARED, fd, 0);
	    if (mapped_mem == MAP_FAILED) {
			perror("mmap");
		    close(fd);
		    return -1;
	    }

		kmig.rb[i] = (struct rb_head_t *)mapped_mem;
		kmig.rb_buf[i] = (struct rb_data_t *)(mapped_mem + RB_HEADER_SIZE);

		koo_mig_print(PRINT_DEBUG, "mmap rb: %lu, rb_buf: %lu\n", (unsigned long)kmig.rb[i], (unsigned long)kmig.rb_buf[i]);
	}
	kmig.fd = fd;

	// get pid of the process to be monitored
	kmig.pid = kmig.opts.is_fork ? get_pid_of_ppid(pid) : pid;
	while (kmig.opts.is_fork && kmig.pid == -1) {
		usleep(1000);
		kmig.pid = get_pid_of_ppid(pid); 
	}
	koo_mig_print(PRINT_NONE, "koo_mig_init is_fork: %d, pid: %d\n", kmig.opts.is_fork, kmig.pid);

    return 0;
}

void destroy_profile() {
	kmig.pid = -1;

	for (unsigned long i = 0; i < MAX_NR_RB; i++) {
		if (kmig.rb[i] != NULL) {
			munmap(kmig.rb[i], RB_BUF_SIZE + RB_HEADER_SIZE);
			kmig.rb[i] = NULL;
			kmig.rb_buf[i] = NULL;
		}
	}

	if (kmig.fd >= 0) {
		close(kmig.fd);
		kmig.fd = -1;
	}
}

int setup_drain() {
	epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1");
        return -1;
    }

    ev.events = EPOLLIN;
    ev.data.fd = kmig.fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, kmig.fd, &ev) == -1) {
        perror("epoll_ctl: fd");
        return -1;
    }
}

int start_profile() {
	// start the profiling
    if (ioctl(kmig.fd, IOCTL_SET_PID, &kmig.pid) < -1) {
        perror("Failed to set PID");
        return -1;
    }
}