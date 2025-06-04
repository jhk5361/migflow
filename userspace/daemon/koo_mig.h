#ifndef __KOO_MIG_H
#define __KOO_MIG_H

#include <pthread.h>
#include <atomic>
#include <map>
#include <unordered_map>
#include <set>
#include <vector>
#include <list>
#include "my_rb.h"

#define ABORT_WITH_LOG() do { \
    fprintf(stderr, "Abort called at %s:%d\n", __FILE__, __LINE__); \
    abort(); \
} while(0)

#define PAGE_SIZE 4096UL


#define MAX_NODES 4

#define NR_MOVE_PAGES 512

#define NR_HIST_BINS 32
#define HOTNESS_WEIGHT 0.5

#define COOLING_INTERVAL 2000000 // for PEBS
//#define COOLING_INTERVAL 2000 // for PEBS
#define PERIOD_INTERVAL 10 // 10s
			
#define MIG_INTERVAL 10 // 10s
#define NR_PROMOTE_PAGES 51200UL
#define NR_DEMOTE_PAGES 51200UL


//#define MIG_INTERVAL 1 // 10s
//#define NR_PROMOTE_PAGES 5120UL
//#define NR_DEMOTE_PAGES 5120UL

//#define NR_MARGIN_PAGES (1024UL * 512) //2GB
#define NR_MARGIN_PAGES (1024UL * 256)
//#define NR_MARGIN_PAGES (0)
//#define NR_MARGIN_PAGES (1024UL)
#define NR_MAX_MIG_PAGES (100UL * 1024 * 1024)

#define NONE_PAGE -1
#define NO_MIG -1
#define UNKNOWN_NODE -1
#define FLAG_QD 16

#define USER_PEBS 3 // start from 3
					
#define PROF_PEBS 0

struct mig_stat {
	uint64_t nr_alloc_iters;
	uint64_t nr_alloc_drains;
	uint64_t nr_alloc_real;
	uint64_t nr_alloc_already;
	uint64_t nr_alloc_move_failed;
	uint64_t nr_alloc_move_success;
	uint64_t nr_alloc_move_err_retry;
	uint64_t nr_alloc_move_retry;
	uint64_t nr_alloc_unique_pages;
	uint64_t nr_alloc_org_tier[MAX_NODES];
};

struct alloc_stat {
	unsigned long long nr_iters;
	unsigned long long nr_drain_pages;
	unsigned long long nr_cold_pages;
	unsigned long long nr_not_cold_pages;
	unsigned long long nr_reaccessed_cold_pages;
	unsigned long long nr_reaccessed_not_cold_pages;

	unsigned long long nr_alloc_pages[MAX_NODES];
};

struct profile_stat {
	unsigned long long nr_iters;
	unsigned long long nr_profiled;
	unsigned long long nr_pages;
	unsigned long long nr_max_pages;
};

struct promo_stat {
	unsigned long long nr_iters;
	unsigned long long nr_try_pages;
	unsigned long long nr_moved_pages;
	unsigned long long nr_successed_pages;
	unsigned long long nr_move_from_to[MAX_NODES][MAX_NODES];
};

struct demo_stat {
	unsigned long long nr_iters;
	unsigned long long nr_try_pages;
	unsigned long long nr_moved_pages;
	unsigned long long nr_successed_pages;
	unsigned long long nr_move_from_to[MAX_NODES][MAX_NODES];
};

struct alloc_metadata_t {
	std::list<std::pair<void *, struct page_profile *>> pages_to_move_lists[MAX_NODES];
	std::unordered_map<void *, std::list<std::pair<void *, struct page_profile *>>::iterator> pages_to_move_dict;
	std::map<void *,int> unique_pages;
};


struct hist_bin {
	unsigned long nr_pages;
	unsigned long nr_pages_tier[MAX_NODES];
	unsigned long nr_regions;
	unsigned long nr_accesses;
	long nr_added;
	long nr_deleted;
	std::unordered_map<void *, std::list<std::pair<void *, struct page_profile *>>::iterator> va_set;
	std::list<std::pair<void *, struct page_profile *>> va_lists[MAX_NODES];
	std::unordered_map<void *, struct page_profile *> updated_va_set;
	pthread_mutex_t lock;
};

struct page_profile {
	uint64_t hotness;
	int age;
	int node;
	int next_node; // used for migration
	int bin_idx; // histogram bin index
};

struct pebs_va {
	int nr_accesses;
	int node;
	int type;
	unsigned int iter;
};

struct pebs_period {
	uint64_t read;
	uint64_t write;
};

struct promo_path {
	int bin;
	int src;
	int dst;
};

struct pebs_metadata_t {
	std::unordered_map<void *, struct pebs_va> profiled_va;
	std::unordered_map<void *,struct page_profile *> drained_pages;
	int profile_iter;
	int cooling_interval;
	bool need_cooling;
	unsigned int cur_iter;
	std::map<void *,int> unique_pages;
	struct pebs_period period;
	uint64_t period_iter;
};

struct opts {
	int idx;
	char *exename;

	int do_quick_demotion;
	int do_demotion_for_alloc;
	int do_cb_promo;
	int do_mig;
	int do_pebs;
	int is_fork;
	int print_itv;
	int verbose_level;
};

struct koo_mig {
	int fd;
	int pid;
	struct rb_head_t *rb[MAX_NR_RB];
	struct rb_data_t *rb_buf[MAX_NR_RB];
	struct alloc_metadata_t alloc_meta;
	struct pebs_metadata_t pebs_meta;
	int profiled_accesses;
	std::unordered_map<void *,struct page_profile *> g_page_map;
	unsigned long long nr_cap_tier_pages[MAX_NODES];
	struct hist_bin hist[NR_HIST_BINS];

	pthread_t tid;
	int profile_iter;
	int age;
	std::atomic<bool> thread_stop;
	struct mig_stat mstat;
	struct opts opts;

	struct alloc_stat astat;
	struct profile_stat pfstat;
	struct promo_stat pstat;
	struct demo_stat dstat;
	struct demo_stat oddstat;
	struct demo_stat qdstat;
};

#define PRINT_NONE 0
#define PRINT_ERR 1
#define PRINT_KEY 2
#define PRINT_DEBUG 3
#define PRINT_DEBUG_MORE 4

int koo_mig_init (int, void *); // TODO: add interval, etc, .. later
void destroy_koo_mig (void);
void koo_mig_print (int level, const char *fmt, ...);

#endif
