#ifndef __PROFILE_H
#define __PROFILE_H
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "my_rb.h"

#define DEVICE_NAME "/dev/page_tracker"
#define IOCTL_SET_PID _IOW('a', 1, int)
#define IOCTL_SET_HOT_BUF _IOW('a', 2, int)
#define IOCTL_SET_THRESHOLD_WAKEUP _IOW('a', 3, int)
#define IOCTL_MOVE_RB_TAIL _IOW('a', 4, void *)
#define IOCTL_GET_RB_STATUS _IOR('a', 5, int)
#define IOCTL_GET_PERIOD _IOR('a', 6, struct pebs_period)

#define MAX_EVENTS 10
#define EPOLL_TIMEOUT 10

int init_profile(int pid);
void destroy_profile(void);
int setup_drain(void);
int start_profile(void);
int drain(bool &alloc_occured, bool &profile_occured);
void print_hist(struct hist_bin *hist, bool clear_stat);
int profile_pages(int type, int age, bool do_mig);
unsigned long do_cooling(struct hist_bin *hist, int cur_age);


void delete_hist_bin_va(struct hist_bin *bin, unsigned long va, int node);
void add_hist_bin_va(struct hist_bin *bin, unsigned long va, struct page_profile *page_info, int node);

#endif // __PROFILE_H