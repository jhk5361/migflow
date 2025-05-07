#ifndef __SAMPLER_H__
#define __SAMPLER_H__
#include <pthread.h>
#include <stdint.h>
#include <unistd.h>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <asm/unistd.h>
#include <iostream>
#include <atomic>
#include <list>
#include "koo_mig.h"

/* pebs events */
#define DRAM_LLC_LOAD_MISS  0x1d3
#define REMOTE_DRAM_LLC_LOAD_MISS   0x2d3
#define NVM_LLC_LOAD_MISS   0x80d1
#define REMOTE_NVM_LLC_LOAD_MISS 0x10d3
#define ALL_STORES	    0x82d0
#define ALL_LOADS	    0x81d0
#define STLB_MISS_STORES    0x12d0
#define STLB_MISS_LOADS	    0x11d0

/**/
#define DRAM_ACCESS_LATENCY 80
#define NVM_ACCESS_LATENCY  270
#define CXL_ACCESS_LATENCY  170
#define DELTA_CYCLES	(NVM_ACCESS_LATENCY - DRAM_ACCESS_LATENCY)

#define pcount 30
/* only prime numbers */
static const unsigned int pebs_period_list[pcount] = {
	199,    // 200 - min
	293,    // 300
	401,    // 400
	499,    // 500
	599,    // 600
	701,    // 700
	797,    // 800
	907,    // 900
	997,    // 1000
	1201,   // 1200
	1399,   // 1400
	1601,   // 1600
	1801,   // 1800
	1999,   // 2000
	2503,   // 2500
	3001,   // 3000
	3499,   // 3500
	4001,   // 4000
	4507,   // 4507
	4999,   // 5000
	6007,   // 6000
	7001,   // 7000
	7993,   // 8000
	9001,   // 9000
	10007,  // 10000
	12007,  // 12000
	13999,  // 14000
	16001,  // 16000
	17989,  // 18000
	19997,  // 20000 - max
};

#define pinstcount 5
/* this is for store instructions */
static const unsigned int pebs_inst_period_list[pinstcount] = {
	100003, // 0.1M
	300007, // 0.3M
	600011, // 0.6M
	1000003,// 1.0M
	1500003,// 1.5M
};

struct perf_sample {
	struct perf_event_header header;
	__u64 ip;
	__u32 pid, tid;
	__u64 addr;
};

enum events {
	DRAMREAD = 0,
	R_DRAMREAD = 1,
	NVMREAD = 2,
	R_NVMREAD = 3,
	MEMWRITE = 4,
	TLB_MISS_LOADS = 5,
	TLB_MISS_STORES = 6,
	CXLREAD = 7, // emulated by remote DRAM node
	N_EVENTS
};

struct pebs_deamon {
	pthread_t sampler_thread;
	std::atomic<bool> sampler_stop;
	std::list<std::pair<void *, struct pebs_va>> pebs_list;
	pthread_mutex_t mutex;
};

static inline unsigned long get_sample_period(unsigned long cur) {
	if (cur < 0)
		return 0;
	else if (cur < pcount)
		return pebs_period_list[cur];
	else
		return pebs_period_list[pcount - 1];
}

static inline unsigned long get_sample_inst_period(unsigned long cur) {
	if (cur < 0)
		return 0;
	else if (cur < pinstcount)
		return pebs_inst_period_list[cur];
	else
		return pebs_inst_period_list[pinstcount - 1];
}
#if 1
static inline void increase_sample_period(unsigned long *llc_period,
		unsigned long *inst_period) {
	unsigned long p;
	p = *llc_period;
	if (++p < pcount)
		*llc_period = p;

	p = *inst_period;
	if (++p < pinstcount)
		*inst_period = p;
}

static inline void decrease_sample_period(unsigned long *llc_period,
		unsigned long *inst_period) {
	unsigned long p;
	p = *llc_period;
	if (p > 0)
		*llc_period = p - 1;

	p = *inst_period;
	if (p > 0)
		*inst_period = p - 1;
}
#else
static inline unsigned int increase_sample_period(unsigned int cur,
		unsigned int next) {
	do {
		cur++;
	} while (pebs_period_list[cur] < next && cur < pcount);

	return cur < pcount ? cur : pcount - 1;
}

static inline unsigned int decrease_sample_period(unsigned int cur,
		unsigned int next) {
	do {
		cur--;
	} while (pebs_period_list[cur] > next && cur > 0);

	return cur;
}
#endif



//#define PAGE_SIZE 4096UL

//void sampler_init(pid_t pid);
void sampler_init(int pid);
void sampler_destroy(void);
int sampler_get_sample(std::list<std::pair<void *, struct pebs_va>> &out_list);

#endif
