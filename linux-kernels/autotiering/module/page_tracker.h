#ifndef __PAGE_TRACKER_H__
#define __PAGE_TRACKER_H__

#include <linux/list.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/pid.h>
#include <linux/mutex.h>

#include "../../common/my_rb.h"

struct anony_data {
	unsigned long va;
	struct list_head list;
	bool last_accessed;
	int node;
};

struct damon_data {
	unsigned long va;
	unsigned int nr_pages;
	unsigned int nr_accesses;
	unsigned int iter;
};

struct pebs_data {
	unsigned long va;
	int node;
	int type;
	int iter;
	struct list_head list;
};

struct anony_list {
	struct list_head head;
	int length;
	int capacity;
};

struct page_tracker_stat {
	unsigned long nr_alloc;
	unsigned long nr_accessed;
	unsigned long nr_copied;
	int max_rb_size;
	int max_young_list_size;
	int max_old_list_size;
};


struct page_tracker_t {
	int target_pid;
	struct pid *target_pidp;
	struct damon_ctx *dctx;
	spinlock_t g_lock;
	struct anony_list young_list;
	struct anony_list old_list;
	//struct anony_list pebs_list;
	struct list_head pebs_list;
	struct kmem_cache *anony_slab;
	struct kmem_cache *pebs_slab;
	struct task_struct *kanonyd;
	spinlock_t anony_lock;
	spinlock_t damon_lock;
	spinlock_t pebs_lock;
	struct mutex pebs_mutex;
	struct rb_head_t *rb[MAX_NR_RB];
	struct rb_data_t *rb_buf[MAX_NR_RB];
	int cur_rb_idx;
	int threshold_wakeup[MAX_NR_RB];
	wait_queue_head_t poll_wq;
	struct page_tracker_stat ptstat;
	struct semaphore sem;
	struct semaphore pid_sem;
	struct semaphore pebs_sem;
}; 

int copy_to_rb(int type, void *data);
bool need_to_wakeup(int type);
void wakeup_user(void);

struct pebs_period {
	uint64_t read;
	uint64_t write;
};

void pebs_get_period(uint64_t *read_period, uint64_t *write_period);

#endif
