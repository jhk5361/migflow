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

int init_profile(void);
void destroy_profile(void);
int setup_drain(void);
int start_profile(void);

#endif // __PROFILE_H