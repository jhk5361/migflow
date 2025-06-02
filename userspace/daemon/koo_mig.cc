// set_pid.c
#include "koo_mig.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <numa.h>
#include <numaif.h>
#include <iostream>
#include <limits.h>
#include <string.h>
#include <map>
#include <unordered_map>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <future>
#include <cstdarg>
#include <unordered_set>
#include <signal.h>
#include "profile.h"
#include "sampler.h"
using namespace std;
using namespace std::chrono;

static koo_mig kmig;

unordered_set<unsigned long> dbg;

/*
static int tier_lat[MAX_NODES] = {80, 130, 300, 350}; // 4 tier PMM
static int cost_mp[MAX_NODES][MAX_NODES] = {
	{INT_MAX, 2107, 2071, 2080},
	{2509, INT_MAX, 2472, 2490},
	{3223, 3250, INT_MAX, 3207},
	{4315, 4310, 4257, INT_MAX}};
static int cost_mc[MAX_NODES][MAX_NODES] = {
	{INT_MAX, 1091, 1917, 3879},
	{1162, INT_MAX, 2032, 3956},
	{1934, 1802, INT_MAX, 4249},
	{3005, 2736, 2686, INT_MAX}};
*/

void koo_mig_print(int level, const char *format, ...) {
	    if (level <= kmig.opts.verbose_level) {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
    }
}

static void print_alloc_stat() {
	struct alloc_stat stat = kmig.astat;
	koo_mig_print(PRINT_NONE, "\n--- START ALLOC STAT PRINT ---\n");
	koo_mig_print(PRINT_NONE, "nr_iters: %lu\n \
			\rnr_drain_pages: %lu\n \
			\rnr_cold_pages: %lu\n \
			\rnr_not_cold_pages: %lu\n \
			\rnr_reaccessed_cold_pages: %lu\n \
			\rnr_reaccessed_not_cold_pages: %lu\n",
			stat.nr_iters,
			stat.nr_drain_pages,
			stat.nr_cold_pages,
			stat.nr_not_cold_pages,
			stat.nr_reaccessed_cold_pages,
			stat.nr_reaccessed_not_cold_pages);
	koo_mig_print(PRINT_NONE, "nr_alloc_pages per tier\n");
	for (int i = 0; i < MAX_NODES; i++) {
		koo_mig_print(PRINT_NONE, "%lu ", stat.nr_alloc_pages[i]);
	}
	koo_mig_print(PRINT_NONE, "\n");
	koo_mig_print(PRINT_NONE, "\n--- END PRINT ---\n");
}

static void print_profile_stat() {
	struct profile_stat stat = kmig.pfstat;
	koo_mig_print(PRINT_NONE, "\n--- START PROFILE STAT PRINT ---\n");
	koo_mig_print(PRINT_NONE, "nr_iters: %lu\n \
			\rnr_profiled: %lu\n \
			\rnr_pages: %lu\n \
			\rnr_max_pages: %lu\n",
			stat.nr_iters,
			stat.nr_profiled,
			stat.nr_pages,
			stat.nr_max_pages);
	/*
	koo_mig_print(PRINT_NONE, "nr_move_from_to\n");
	for (int from = 0; from < MAX_NODES; from++) {
		for (int to = 0; to < MAX_NODES; to++)
			koo_mig_print(PRINT_NONE, "%lu ", stat.nr_move_from_to[from][to]);
		koo_mig_print(PRINT_NONE, "\n");
	}
	*/
	koo_mig_print(PRINT_NONE, "\n--- END PRINT ---\n");
}

static void print_promo_stat() {
	struct promo_stat stat = kmig.pstat;
	koo_mig_print(PRINT_NONE, "\n--- START PROMO STAT PRINT ---\n");
	koo_mig_print(PRINT_NONE, "nr_iters: %lu\n \
			\rnr_try_pages: %lu\n \
			\rnr_moved_pages: %lu\n \
			\rnr_successed_pages: %lu\n",
			stat.nr_iters,
			stat.nr_try_pages,
			stat.nr_moved_pages,
			stat.nr_successed_pages);
	koo_mig_print(PRINT_NONE, "nr_move_from_to\n");
	for (int from = 0; from < MAX_NODES; from++) {
		for (int to = 0; to < MAX_NODES; to++)
			koo_mig_print(PRINT_NONE, "%lu ", stat.nr_move_from_to[from][to]);
		koo_mig_print(PRINT_NONE, "\n");
	}
	koo_mig_print(PRINT_NONE, "\n--- END PRINT ---\n");
}

static void print_demo_stat() {
	struct demo_stat stat = kmig.dstat;
	koo_mig_print(PRINT_NONE, "\n--- START DEMO STAT PRINT ---\n");
	koo_mig_print(PRINT_NONE, "nr_iters: %lu\n \
			\rnr_try_pages: %lu\n \
			\rnr_moved_pages: %lu\n \
			\rnr_successed_pages: %lu\n",
			stat.nr_iters,
			stat.nr_try_pages,
			stat.nr_moved_pages,
			stat.nr_successed_pages);
	koo_mig_print(PRINT_NONE, "nr_move_from_to\n");
	for (int from = 0; from < MAX_NODES; from++) {
		for (int to = 0; to < MAX_NODES; to++)
			koo_mig_print(PRINT_NONE, "%lu ", stat.nr_move_from_to[from][to]);
		koo_mig_print(PRINT_NONE, "\n");
	}
	koo_mig_print(PRINT_NONE, "\n--- END PRINT ---\n");
}

static void print_oddemo_stat() {
	struct demo_stat stat = kmig.oddstat;
	koo_mig_print(PRINT_NONE, "\n--- START ON-DMAND DEMO STAT PRINT ---\n");
	koo_mig_print(PRINT_NONE, "nr_iters: %lu\n \
			\rnr_try_pages: %lu\n \
			\rnr_moved_pages: %lu\n \
			\rnr_successed_pages: %lu\n",
			stat.nr_iters,
			stat.nr_try_pages,
			stat.nr_moved_pages,
			stat.nr_successed_pages);
	koo_mig_print(PRINT_NONE, "nr_move_from_to\n");
	for (int from = 0; from < MAX_NODES; from++) {
		for (int to = 0; to < MAX_NODES; to++)
			koo_mig_print(PRINT_NONE, "%lu ", stat.nr_move_from_to[from][to]);
		koo_mig_print(PRINT_NONE, "\n");
	}
	koo_mig_print(PRINT_NONE, "\n--- END PRINT ---\n");
}

static void print_qdemo_stat() {
	struct demo_stat stat = kmig.qdstat;
	koo_mig_print(PRINT_NONE, "\n--- START QUICK DEMO STAT PRINT ---\n");
	koo_mig_print(PRINT_NONE, "nr_iters: %lu\n \
			\rnr_try_pages: %lu\n \
			\rnr_moved_pages: %lu\n \
			\rnr_successed_pages: %lu\n",
			stat.nr_iters,
			stat.nr_try_pages,
			stat.nr_moved_pages,
			stat.nr_successed_pages);
	koo_mig_print(PRINT_NONE, "nr_move_from_to\n");
	for (int from = 0; from < MAX_NODES; from++) {
		for (int to = 0; to < MAX_NODES; to++)
			koo_mig_print(PRINT_NONE, "%lu ", stat.nr_move_from_to[from][to]);
		koo_mig_print(PRINT_NONE, "\n");
	}
	koo_mig_print(PRINT_NONE, "\n--- END PRINT ---\n");
}

static inline long long get_numa_nr_free_pages(int node) {
	long long fr;
	numa_node_size64(node, &fr);
	fr /= PAGE_SIZE;

	return fr;
}

static inline int get_numa_node_of_va(void *va) {
	int status;
	if (move_pages(kmig.pid, 1, &va, NULL, &status, MPOL_MF_MOVE_ALL) < 0) {
		//std::cout << "failed to inquiry pages because " << strerror(errno) << std::endl;
		return NONE_PAGE;
	}

	if (status < 0) {
		if (status != -EFAULT && status != -ENOENT)
			std::cout << "get_numa_node err " << strerror(-status) << std::endl;
		return NONE_PAGE;
	}

	return status;
}


// idx 0 --> 0
// idx 1 --> 1, 2
// idx 2 --> 3, 4, 5, 6
// idx 3 --> 7, 8, 9, 10, 11, 12, 13, 14
// ...
static inline int get_idx(uint64_t num) {
	int cnt = 0;

	if (num == UINT64_MAX)
		return -1;

	num++;
	while (1) {
		num = num >> 1;
		if (num)
			cnt++;
		else
			return cnt;

		if(cnt == NR_HIST_BINS - 1)
			break;
	}

	return cnt;
}

static inline uint64_t __calc_hotness_general(uint64_t old_hotness, unsigned int nr_accesses, int age_diff, double weight) {
	if (old_hotness == UINT64_MAX)
		return (uint64_t)nr_accesses;
		//return (unsigned int)((1.0 - weight) * nr_accesses);

	while(age_diff--) {
		old_hotness = (uint64_t)(old_hotness * weight);
	}
	return old_hotness + (uint64_t)nr_accesses;
}

static inline uint64_t calc_hotness(uint64_t old_hotness, unsigned int nr_accesses, int age_diff, double weight, bool is_pebs = false) {
	// hotness calculation for the cost benefit promotion
	if (kmig.opts.do_cb_promo && is_pebs) 
		nr_accesses = nr_accesses * kmig.pebs_meta.period.read;

	if (old_hotness == UINT64_MAX)
		return (uint64_t)nr_accesses;
		//return (unsigned int)((1.0 - weight) * nr_accesses);

	while(age_diff--) {
		old_hotness = (uint64_t)((double)old_hotness * weight);
	}
	return old_hotness + (uint64_t)nr_accesses;
}

static inline void add_hist_bin_va(struct hist_bin *bin, unsigned long va, struct page_profile *page_info, int node) {

	bin->va_lists[node].push_back({(void*)va, page_info});
	auto last = bin->va_lists[node].end();
	auto res = bin->va_set.insert({(void*)va, prev(last)});
	if (res.second == false) {
		printf("add_hist_bin_va\n");
		ABORT_WITH_LOG();
	}

	bin->nr_pages_tier[node]++;
	bin->nr_pages++;

	if (bin->nr_pages != bin->va_set.size()) {
		printf("add_hist_bin_va nr_pages: %lu, set_size: %lu\n", bin->nr_pages, bin->va_set.size());
		ABORT_WITH_LOG();
	}
	bin->nr_added++;
}

static inline void delete_hist_bin_va(struct hist_bin *bin, unsigned long va, int node) {

	auto it = bin->va_set.find((void *)va);

	if (it == bin->va_set.end()) { 
		printf("delete bin no va set\n");
		ABORT_WITH_LOG();
	}

	if (node < 0 || node >= MAX_NODES) {
		printf("delete bin invalid node num: %d\n", node);
		ABORT_WITH_LOG();
	}

	bin->va_lists[node].erase(it->second);
	bin->va_set.erase(it);

	bin->nr_pages_tier[node]--;
	bin->nr_pages--;

	if (bin->nr_pages != bin->va_set.size()) {
		printf("delete_hist_bin_va nr_pages: %lu, set_size: %lu\n", bin->nr_pages, bin->va_set.size());
		ABORT_WITH_LOG();
	}
	bin->nr_deleted++;
}


int update_hist(unsigned long va, uint64_t old_hotness, uint64_t new_hotness, int old_node, int new_node, struct page_profile *page_info, struct hist_bin *hist) {
	int old_bin = get_idx(old_hotness);
	int new_bin = get_idx(new_hotness);

	/*
	if (old_bin == new_bin)
		return 0;
	*/

	if (old_bin != -1) {
		if (page_info && old_bin != page_info->bin_idx) {
			printf("update_hist old bin\n");
			ABORT_WITH_LOG();
		}

		delete_hist_bin_va(hist + old_bin, va, old_node);

		if (page_info)
			page_info->bin_idx = -1;
	}

	if (new_bin != -1) {
		if (page_info->bin_idx != -1) {
			printf("update_hist new bin\n");
			ABORT_WITH_LOG();
		}
		add_hist_bin_va(hist + new_bin, va, page_info, new_node);
		page_info->bin_idx = new_bin;
	}

	return old_bin < new_bin;
}

void print_hist(struct hist_bin *hist, bool clear_stat) {
	static int iter = 0;
	unsigned long total_nr_pages = 0;
	struct hist_bin *bin;
	koo_mig_print(PRINT_DEBUG, "[Print hist] iter %d\n", iter++);
	for (int i = 0; i < NR_HIST_BINS; i++) {
		bin = hist + i;
		total_nr_pages += bin->nr_pages;
		koo_mig_print(PRINT_DEBUG, "BIN %d, nr_pages: %lu (%luKB) (%ld pages than before), nr_added: %lu, nr_deleted: %lu\n", i, bin->nr_pages, bin->nr_pages * PAGE_SIZE / 1024, bin->nr_added - bin->nr_deleted, bin->nr_added, bin->nr_deleted);
		if (clear_stat)
			bin->nr_added = bin->nr_deleted = 0;
	}
	koo_mig_print(PRINT_DEBUG, "Total size of the histogram: %luMB\n", total_nr_pages * PAGE_SIZE / 1024 / 1024);

}

void print_koo_mig() {
	struct mig_stat &mstat = kmig.mstat;
	koo_mig_print(PRINT_NONE, "nr_alloc_iters: %lu\n \
			\rnr_alloc_drains: %lu\n \
			\rnr_alloc_real: %lu\n \
			\rnr_alloc_already: %lu\n \
			\rnr_alloc_move_failed: %lu\n \
			\rnr_alloc_move_err_retry: %lu\n \
			\rnr_alloc_move_retry: %lu\n \
			\rnr_alloc_unique_pages: %lu\n \
			\rnr_alloc_move_success: %lu\n",
			mstat.nr_alloc_iters,
			mstat.nr_alloc_drains,
			mstat.nr_alloc_real,
			mstat.nr_alloc_already,
			mstat.nr_alloc_move_failed,
			mstat.nr_alloc_move_err_retry,
			mstat.nr_alloc_move_retry,
			mstat.nr_alloc_unique_pages,
			mstat.nr_alloc_move_success);
	for (int i = 0; i < MAX_NODES; i++) {
		koo_mig_print(PRINT_NONE, "\rnr_alloc_org_tier[%d]: %lu\n", i, mstat.nr_alloc_org_tier[i]);
	}
	koo_mig_print(PRINT_NONE, "\r--- END KOO MIG PRINT ---\n");

	print_alloc_stat();
	print_profile_stat();
	print_promo_stat();
	print_demo_stat();
	print_oddemo_stat();
	print_qdemo_stat();
	print_hist(kmig.hist, false);
}

void sig_handler_usr (int signo) {
	if (signo == SIGUSR1)
		print_koo_mig();

}

int __move_pages(int pid, int count, void **target_pages, int *nodes, int *status) {
	int moved_pages = 0;

	if (pid == -1)
		return -1;

	static unordered_map<int,unsigned long> err_map;


    auto start = high_resolution_clock::now();

	while (moved_pages < count) {
		int nr_move_pages = min(count - moved_pages, NR_MOVE_PAGES);

		if (move_pages(pid, nr_move_pages, target_pages + moved_pages, nodes == NULL ? NULL : nodes + moved_pages, status + moved_pages, MPOL_MF_MOVE_ALL) < 0) {
			if (err_map.count(errno) == 0)
				err_map.insert({errno,1});
			else
				err_map[errno]++;

			break;
		}

		moved_pages += nr_move_pages;
	}

    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(stop - start);

	koo_mig_print(PRINT_DEBUG, "[__move_pages: %ldms] moved_pages: %d\n", duration.count(), moved_pages);
	for (auto err : err_map) {
		koo_mig_print(PRINT_DEBUG, "errno: %d, cnt: %lu\n", err.first, err.second);
	}

	return moved_pages;
}

int drain_rb_alloc (rb_head_t *rb, rb_data_t *rb_buf, struct alloc_metadata_t &alloc_meta) {
	int head, tail, len, rbuf_idx;
	struct rb_data_alloc_t rb_alloc;
	unsigned long va;
	head = rb->head;
	tail = rb->tail;
	len = (head + rb->size - tail) % rb->size;

	//if (len == 0 || !kmig.opts.do_quick_demotion)
	//	return len;
	if (len == 0)
		return len;

	//static unsigned long iter = 0;


    auto start = high_resolution_clock::now();

	static unsigned long long nr_drain_pages = 0;
	static unsigned long long nr_existing_pages = 0;
	static unsigned long long nr_not_accessed_pages = 0;
	static unsigned long long nr_err_pages = 0;
	static vector<unsigned long long> nr_alloc_pages(MAX_NODES, 0);
	static unordered_map<int,unsigned long> err_map;

	unordered_map<void *,struct page_profile *>::iterator it;
	struct page_profile *page_info;
	unsigned int last_accessed;
	uint64_t old_hotness = UINT64_MAX, new_hotness = UINT64_MAX;
	int old_node, new_node;
	int cur_age = kmig.age;

	for (int i = 0; i < len; i++) {
		rbuf_idx = (tail + i) % rb->size;
		rb_alloc = rb_buf[rbuf_idx].data.rb_alloc;

		old_node = new_node = UNKNOWN_NODE;

		if (rb_alloc.va % PAGE_SIZE) {
			koo_mig_print(PRINT_NONE, "Not aligned %ld\n", rb_alloc.va);
			continue;
		}

		nr_drain_pages++;
		kmig.astat.nr_drain_pages++;

		if (rb_alloc.node < 0) {
			if (err_map.count(rb_alloc.node) == 0)
				err_map.insert({rb_alloc.node,1});
			else
				err_map[rb_alloc.node]++;
			nr_err_pages++;
			continue;
		}

		old_hotness = UINT64_MAX;

		va = rb_alloc.va;
		last_accessed = rb_alloc.last_accessed;

		nr_alloc_pages[rb_alloc.node]++;
		kmig.astat.nr_alloc_pages[rb_alloc.node]++;

		// if the address is obtained first,
		// insert it to global page map.
		// if not, update the corresponding page map entry
		it = kmig.g_page_map.find((void*)va);
		if (it == kmig.g_page_map.end()) {
			page_info = new page_profile;
			kmig.g_page_map.insert({(void*)va, page_info});
			page_info->bin_idx = -1;

			new_hotness = calc_hotness(UINT64_MAX, last_accessed, 0, HOTNESS_WEIGHT);
			new_node = rb_alloc.node;
		} else {
			nr_existing_pages++;

			page_info = it->second;

			old_hotness = page_info->hotness;
			new_hotness = calc_hotness(old_hotness, last_accessed, cur_age - page_info->age, HOTNESS_WEIGHT);

			old_node = page_info->node;
			new_node = rb_alloc.node;
		}

		auto it = alloc_meta.pages_to_move_dict.find((void*)va);
		if (it != alloc_meta.pages_to_move_dict.end()) {
			if (it->second != alloc_meta.pages_to_move_lists[0].end()) {
				int node_num = it->second->second->node;
				alloc_meta.pages_to_move_lists[node_num].erase(it->second);
			}
			alloc_meta.pages_to_move_dict.erase(it);
		}

		// if the allocated page was not accessed,
		// it is inserted into a quick demotion fifo queue
		if (!last_accessed) {
			nr_not_accessed_pages++;
			kmig.astat.nr_cold_pages++;
			if (kmig.opts.do_quick_demotion && page_info->bin_idx == -1) {
				if (new_node < 0 || new_node >= MAX_NODES) {
					ABORT_WITH_LOG();
				}
				alloc_meta.pages_to_move_lists[new_node].push_back({(void *)va, page_info});
				auto last = alloc_meta.pages_to_move_lists[new_node].end();
				alloc_meta.pages_to_move_dict.insert({(void*)va, prev(last)});
			}
		} else {
			kmig.astat.nr_not_cold_pages++;
			if (kmig.opts.do_quick_demotion && page_info->bin_idx == -1) {
				if (new_node < 0 || new_node >= MAX_NODES) {
					ABORT_WITH_LOG();
				}
				auto iter_end = alloc_meta.pages_to_move_lists[0].end();
				alloc_meta.pages_to_move_dict.insert({(void*)va, iter_end});
			}
		}

		// update histogram
		*page_info = (struct page_profile) {new_hotness, cur_age, new_node, NO_MIG, page_info->bin_idx};

		if (!kmig.opts.do_quick_demotion || page_info->bin_idx != -1 || last_accessed) {
			if (page_info->bin_idx == -1) {
				old_hotness = UINT64_MAX;
				old_node = UNKNOWN_NODE;
			}
			update_hist(va, old_hotness, new_hotness, old_node, new_node, page_info, kmig.hist);
		}
	}

	kmig.astat.nr_iters++;

    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(stop - start);

	koo_mig_print(PRINT_DEBUG_MORE, "[Alloc drain page: %ldms] nr_drain_pages: %d, nr_drain_pages: %llu, nr_existing_pages: %llu, nr_not_accessed_pages: %llu, nr_err_pages: %llu\n", duration.count(), len, nr_drain_pages, nr_existing_pages, nr_not_accessed_pages, nr_err_pages);
	for (auto err : err_map) {
		koo_mig_print(PRINT_DEBUG_MORE, "errno: %d, cnt: %lu\n", err.first, err.second);
	}
	koo_mig_print(PRINT_DEBUG_MORE, "nr_alloc_pages\n");
	for (auto nr : nr_alloc_pages) {
		koo_mig_print(PRINT_DEBUG_MORE, "%llu ", nr);
	}
	koo_mig_print(PRINT_DEBUG_MORE, "\n");


	return len;
}

int drain_rb_pebs (rb_head_t *rb, rb_data_t *rb_buf, struct pebs_metadata_t &pebs_meta) {
	int head, tail, len, rbuf_idx;
	unsigned long va;
	struct rb_data_pebs_t pdata;
	head = rb->head;
	tail = rb->tail;
	len = (head + rb->size - tail) % rb->size;

	//unsigned long total_nr_accesses = 0;
	static unsigned long acc_nr_profiled = 0;

	unordered_map<void *,struct pebs_va>::iterator it;
	struct pebs_va pva;

	auto start = high_resolution_clock::now();

	//unsigned int iter = UINT_MAX;

	for (int i = 0; i < len; i++) {
		rbuf_idx = (tail + i) % rb->size;
		va = rb_buf[rbuf_idx].data.rb_pebs.va;
		pdata = rb_buf[rbuf_idx].data.rb_pebs;

		if (va % PAGE_SIZE) {
			koo_mig_print(PRINT_NONE, "Not aligned %ld\n", va);
			continue;
		}

		it = pebs_meta.profiled_va.find((void *)va);
		if (it == pebs_meta.profiled_va.end()) {
			pva = {1, pdata.node, pdata.type, pdata.iter};
			pebs_meta.profiled_va.insert({(void *)va, pva});
		} else {
			it->second.nr_accesses++;
		}
		
	}

	acc_nr_profiled += len;
	kmig.pfstat.nr_profiled += len;
	kmig.pfstat.nr_pages += pebs_meta.profiled_va.size();;
	kmig.pfstat.nr_iters++;
	
    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(stop - start);

	koo_mig_print(PRINT_DEBUG, "[PEBS drain: %ldms] nr_profiled: %d, nr_unique_profiled: %ld, acc_nr_profiled: %ld\n", duration.count(), len, pebs_meta.profiled_va.size(), acc_nr_profiled);

	return len;
}

int drain_user_pebs (struct pebs_metadata_t &pebs_meta) {
	static unsigned long acc_nr_profiled = 0;

	unordered_map<void *,struct pebs_va>::iterator it;
	list<pair<void *, struct pebs_va>> sample_list;

	auto start = high_resolution_clock::now();

	int nr_profiled = sampler_get_sample(sample_list);

	for (auto &item : sample_list) {
		it = pebs_meta.profiled_va.find(item.first);
		if (it == pebs_meta.profiled_va.end()) {
			pebs_meta.profiled_va.insert(item);
		} else {
			it->second.nr_accesses++;
		}

	}

	acc_nr_profiled += nr_profiled;
	kmig.pfstat.nr_profiled += nr_profiled;
	kmig.pfstat.nr_pages += pebs_meta.profiled_va.size();;
	kmig.pfstat.nr_iters++;
	
    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(stop - start);

	koo_mig_print(PRINT_DEBUG, "[PEBS drain: %ldms] nr_profiled: %d, nr_unique_profiled: %ld, acc_nr_profiled: %ld\n", duration.count(), nr_profiled, pebs_meta.profiled_va.size(), acc_nr_profiled);

	return nr_profiled;
}

int drain (int type, rb_head_t **rb, rb_data_t **rb_buf) {
	int ret;
	bool is_rb = false;
	switch(type) {
		case RB_ALLOC:
			ret = drain_rb_alloc(rb[type], rb_buf[type], kmig.alloc_meta);
			is_rb = true;
			break;
		case RB_PEBS:
			ret = drain_rb_pebs(rb[type], rb_buf[type], kmig.pebs_meta);
			is_rb = true;
			break;
		default:
			ret = -1;
	}

	if (is_rb == false)
		return ret;

	struct rb_reply_t reply;
	reply.type = type;
	reply.nr_items = ret;

	if (ioctl(kmig.fd, IOCTL_MOVE_RB_TAIL, &reply) < 0) {
		perror("Failed to send a reply");
		close(kmig.fd);
		return -1;
	}

	return ret;
}

int profile_pages_pebs(struct pebs_metadata_t &pebs_meta, int cur_age, struct alloc_metadata_t &alloc_meta) {
	static unsigned long total_tried_pages = 0;
	static unsigned long total_added_pages = 0;
	static unsigned long total_accessed_pages = 0;
	static unsigned long total_quiry_pages = 0;
	static unsigned long total_not_mapped_pages = 0;

	if (pebs_meta.profiled_va.size() == 0)
		return 0;


	int nr_accesses = 0;
	unordered_map<void *,struct page_profile *>::iterator it;
	unordered_map<void *, list<pair<void *, struct page_profile *>>::iterator>::iterator alloc_it;
	uint64_t old_hotness, new_hotness;
	int old_age, old_node, new_node;
	struct page_profile *page_info;

    auto start = high_resolution_clock::now();

	for (auto &pva : pebs_meta.profiled_va) {
		nr_accesses += pva.second.nr_accesses;
		total_accessed_pages++;
		total_tried_pages++;
		old_hotness = new_hotness = UINT64_MAX;
		old_node = UNKNOWN_NODE;

		it = kmig.g_page_map.find(pva.first);
		if (it == kmig.g_page_map.end()) {
			int node_num = get_numa_node_of_va(pva.first);

			if (node_num < 0) {
				total_not_mapped_pages++;
				page_info = NULL;
			} else {
				total_added_pages++;

				page_info = new page_profile;
				page_info->node = node_num;
				page_info->bin_idx = -1;

				new_hotness = calc_hotness(UINT64_MAX, pva.second.nr_accesses, 0, HOTNESS_WEIGHT, true);
				kmig.g_page_map.insert({pva.first, page_info});
			}
		} else {
			page_info = it->second;
			old_hotness = page_info->hotness;
			old_age = page_info->age;
			old_node = page_info->node;

			if (old_node == NONE_PAGE) {
				ABORT_WITH_LOG();
				new_hotness = UINT64_MAX;
				kmig.g_page_map.erase(it->first);
				delete page_info;
				page_info = NULL;
			} else {
				new_hotness = calc_hotness(old_hotness, pva.second.nr_accesses, cur_age - old_age, HOTNESS_WEIGHT, true);
			}
		}

		alloc_it = alloc_meta.pages_to_move_dict.find(pva.first);
		if (alloc_it != alloc_meta.pages_to_move_dict.end()) {
			if (alloc_it->second != alloc_meta.pages_to_move_lists[0].end()) {
				int node_num = alloc_it->second->second->node;
				alloc_meta.pages_to_move_lists[node_num].erase(alloc_it->second);
				alloc_meta.pages_to_move_dict.erase(alloc_it);
				kmig.astat.nr_reaccessed_cold_pages++;
				old_node = UNKNOWN_NODE; // no old bin
				old_hotness = UINT64_MAX;
			} else {
				kmig.astat.nr_reaccessed_not_cold_pages++;
				alloc_meta.pages_to_move_dict.erase(alloc_it);
			}
		}

		if (page_info) {
			*page_info = (struct page_profile) {new_hotness, cur_age, page_info->node, NO_MIG, page_info->bin_idx};
			new_node = page_info->node;
			// FIXME: it is for the no bin
			if (page_info->bin_idx == -1) {
				old_hotness = UINT64_MAX;
				old_hotness = UNKNOWN_NODE;
			}
		} else {
			new_node = UNKNOWN_NODE;
			new_hotness = UINT64_MAX;
		}


		update_hist((unsigned long)pva.first, old_hotness, new_hotness, old_node, new_node, page_info, kmig.hist);

	}

	kmig.pfstat.nr_max_pages = max(kmig.pfstat.nr_max_pages, (unsigned long long)kmig.g_page_map.size());

    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(stop - start);

	koo_mig_print(PRINT_DEBUG, "[PEBS profile: %ldms] total_tried_pages: %lu, total_added_pages: %lu total_accessed_pages: %lu, total_quiry_pages: %lu, total_not_mapped_pages: %lu (%.2f%%)\n", duration.count(), total_tried_pages, total_added_pages, total_accessed_pages, total_quiry_pages, total_not_mapped_pages, (double)total_not_mapped_pages/total_quiry_pages*100);
	

	return nr_accesses;
}

int profile_pages(int type, int age, bool do_mig) {
	int ret = 0;
    auto start = high_resolution_clock::now();

	if (!do_mig) {
		if (type == RB_PEBS) kmig.pebs_meta.profiled_va.clear();
		return ret;
	}

	if (type == PROF_PEBS) {
		ret = profile_pages_pebs(kmig.pebs_meta, age, kmig.alloc_meta);
		kmig.pebs_meta.profiled_va.clear();
	}

    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(stop - start);

	koo_mig_print(PRINT_DEBUG, "[Profile pages: %ldms]\n", duration.count());

	kmig.profiled_accesses += ret;


	return ret;
}

int update_thresholds (struct hist_bin *hist, int *thresholds, unsigned long long *nr_cap_tier_pages) {
	int bin_idx = NR_HIST_BINS - 1;
	struct hist_bin *bin;

	/*
	long long nr_free_pages[MAX_NODES];

	for (int i = 0; i < MAX_NODES; i++) {
	
	nr_free_pages[i] = get_numa_nr_free_pages(i) - (long long)NR_MARGIN_PAGES;
	}
	*/

	for (int cur_tier = 0; cur_tier < MAX_NODES; cur_tier++) {
		unsigned long total_pages = 0;
		while (bin_idx >= 0) {
			bin = hist + bin_idx;
			//if (bin->nr_pages + total_pages <= nr_free_pages[cur_tier]) {
			if (total_pages + bin->nr_pages <= nr_cap_tier_pages[cur_tier] - NR_MARGIN_PAGES) {
				total_pages += bin->nr_pages;
				bin_idx--;
			} else {
				break;
			}
		}
		thresholds[cur_tier] = bin_idx + 1;
	}

	return 0;
}

pair<vector<int>,vector<int>> __select_promo_cand_greedy (int nr_pages, int cur_age, struct hist_bin *hist, vector<unordered_map<void *, struct page_profile *>> &promo_target_pages, unsigned long long *nr_cap_tier_pages) {
	int bin_idx = NR_HIST_BINS - 1;
	struct hist_bin *bin;
	int nr_promo_pages = 0;
	int promo_target = 0;
	struct page_profile *page_info;

	unsigned long nr_lookup_pages = 0;
	unsigned long nr_lookup_real_pages = 0;

	unsigned long nr_none_pages = 0;
	unsigned long nr_old_pages = 0;
	unsigned long nr_old_same_pages = 0;
	unsigned long nr_old_hotter_pages = 0;
	unsigned long nr_old_colder_pages = 0;
	//unsigned long nr_none_pages_again = 0;
	unsigned int nr_promo_target[MAX_NODES][MAX_NODES] = {0,};

	vector<int> nr_promo_from(MAX_NODES, 0);
	vector<int> nr_promo_to(MAX_NODES, 0);


    auto start = high_resolution_clock::now();

	//unsigned long total_pages = 0;
	while(bin_idx >= 0 && promo_target < MAX_NODES - 1 && nr_promo_pages < nr_pages) {
		bin = hist + bin_idx;
		
		// check whether the current tier can accomodate this bin.
		// yes --> find the target
		// no  --> next tier
		/*
		if (total_pages + bin->nr_pages <= nr_cap_tier_pages[promo_target]) {
			total_pages += bin->nr_pages;
		} else {
			promo_target++;
			total_pages = 0;
			continue;
		}
		*/

		// find promotion targets
		for (auto va : bin->va_set) {
			page_info = va.second->second;

			nr_lookup_pages++;

			if (page_info->node == NONE_PAGE) {
				ABORT_WITH_LOG();
				//page_info->node = get_numa_node_of_va(va.first);
				nr_none_pages++;
				continue;
			}

			/*
			if (page_info->node == NONE_PAGE) {
				nr_none_pages_again++;
				continue;
			}
			*/

			while (nr_lookup_pages > nr_cap_tier_pages[promo_target]) {
				nr_lookup_pages -= nr_cap_tier_pages[promo_target];
				promo_target++;

				if (promo_target == MAX_NODES - 1)
					break;
					//return selected_pages;
			}

			nr_lookup_real_pages++;

			if (page_info->age < cur_age) {
				nr_old_pages++;
				if (page_info->node < promo_target) {
					nr_old_hotter_pages++;
				} else if (page_info->node == promo_target) {
					nr_old_same_pages++;
				} else {
					nr_old_colder_pages++;
				}
			}

			if (page_info->node > promo_target) {
				//target_pages[nr_promo_pages] = va.first;
				//nodes[nr_promo_pages] = promo_target;
				// va, {original,target}

				if (page_info->next_node != NO_MIG) {
					koo_mig_print(PRINT_ERR, "NOT NO_MIG cur_node: %d, next_node: %d\n", page_info->node, page_info->next_node);
					page_info->next_node = NO_MIG;
					ABORT_WITH_LOG();
				}
				page_info->next_node = promo_target;
				promo_target_pages[page_info->node].insert({va.first, page_info});
				nr_promo_target[page_info->node][promo_target]++;
				nr_promo_pages++;
				nr_promo_from[page_info->node]++;
				nr_promo_to[promo_target]++;
			}

			if (nr_promo_pages == nr_pages) {
				break;
			}
		}

		bin_idx--;
	}

    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(stop - start);

	koo_mig_print(PRINT_KEY, "[Select promo: %ldms] nr_lookup_pages: %lu, nr_none_pages: %lu, nr_lookup_real_pages: %lu, nr_promo_pages: %d, nr_old_pages: %lu, nr_old_same_pages: %lu, nr_old_hotter_pages: %lu, nr_old_colder_pages: %lu\n", duration.count(), nr_lookup_pages, nr_none_pages, nr_lookup_real_pages, nr_promo_pages, nr_old_pages, nr_old_same_pages, nr_old_hotter_pages, nr_old_colder_pages);
	//printf("[Select promo: %ldms] nr_lookup_pages: %lu, nr_none_pages: %lu, nr_promo_pages: %d\n", duration.count(), nr_lookup_pages, nr_none_pages, nr_promo_pages);

	koo_mig_print(PRINT_KEY, "nr_promo_target (row:original/col:target):\n");
	for (int i = 0; i < MAX_NODES; i++) {
		for (int j = 0; j < MAX_NODES; j++) {
			koo_mig_print(PRINT_KEY, "%u ", nr_promo_target[i][j]);
		}
		koo_mig_print(PRINT_KEY, "\n");
	}
	koo_mig_print(PRINT_KEY, "nr_promo_from\n");
	for (auto nr : nr_promo_from) {
		koo_mig_print(PRINT_KEY, "%d ", nr);
	}
	koo_mig_print(PRINT_KEY, "\n");
	koo_mig_print(PRINT_KEY, "nr_promo_to\n");
	for (auto nr : nr_promo_to) {
		koo_mig_print(PRINT_KEY, "%d ", nr);
	}
	koo_mig_print(PRINT_KEY, "\n");


	//return nr_promo_pages;
	//return nr_promo_cand;
	return {nr_promo_from, nr_promo_to};
}

vector<struct promo_path> calc_promo_order (bool use_mp, int iter) {
	static uint64_t tier_lat[MAX_NODES] = {80, 130, 300, 350}; // 4 tier PMM
	static uint64_t cost_mp[MAX_NODES][MAX_NODES] = {
		{INT_MAX, 2107, 2071, 2080},
		{2509, INT_MAX, 2472, 2490},
		{3223, 3250, INT_MAX, 3207},
		{4315, 4310, 4257, INT_MAX}};
	static uint64_t cost_mc[MAX_NODES][MAX_NODES] = {
		{INT_MAX, 1091, 1917, 3879},
		{1162, INT_MAX, 2032, 3956},
		{1934, 1802, INT_MAX, 4249},
		{3005, 2736, 2686, INT_MAX}};
	static uint64_t hist_min[NR_HIST_BINS];

	vector<vector<uint64_t>> threshold(MAX_NODES, vector<uint64_t>(MAX_NODES, UINT64_MAX));
	vector<struct promo_path> promo_order;
	map<uint64_t,pair<int,int>> promo_priority;
	map<int64_t,tuple<int,int,int>> promo_priority_bin;

	koo_mig_print(PRINT_DEBUG, "[Hist min]\n");
	for (uint64_t i = 0; i < NR_HIST_BINS; i++) {
		hist_min[i] = pow((uint64_t)2, i) - 1;
		//hist_min[i] = hist_min[i] * 10 / iter; // accesses per 10 seconds
		hist_min[i] = hist_min[i] * 200; // TODO
		koo_mig_print(PRINT_DEBUG, "%lu ", hist_min[i]);
	}
	koo_mig_print(PRINT_DEBUG, "\n");

	for (int src = 0; src < MAX_NODES; src++) {
		for (int dst = 0; dst < MAX_NODES; dst++) {
			if (src <= dst) { // same node and demotion cases
				continue;
			}

			uint64_t cost = use_mp ? cost_mp[src][dst] : cost_mc[src][dst];
			cost *= 1000; // us to ns
			uint64_t benefit_per_access = tier_lat[src] - tier_lat[dst];
			
			threshold[src][dst] = cost / benefit_per_access;

			promo_priority.insert({threshold[src][dst], {src,dst}});
		}
	}

	for (int i = 0; i < NR_HIST_BINS; i++) {
		int64_t min_access = hist_min[i];
		for (int src = 0; src < MAX_NODES; src++) {
			for (int dst = 0; dst < MAX_NODES; dst++) {
				if (src <= dst) {
					continue;
				}
				int64_t cost = use_mp ? cost_mp[src][dst] : cost_mc[src][dst];
				cost *= 1000; // us to ns
				int64_t benefit_per_access = tier_lat[src] - tier_lat[dst];
				int64_t benefit = min_access * benefit_per_access;
			
				promo_priority_bin.insert({benefit - cost, {i,src,dst}});
			}
		}
	}


	koo_mig_print(PRINT_DEBUG, "[Promo threshold] cost: %s\n", use_mp ? "move_pages()" : "memcpy()");
	for (int src = 0; src < MAX_NODES; src++) {
		for (int dst = 0; dst < MAX_NODES; dst++) {
			koo_mig_print(PRINT_DEBUG, "%lu ", threshold[src][dst]);
		}
		koo_mig_print(PRINT_DEBUG, "\n");
	}

	koo_mig_print(PRINT_DEBUG, "[Promo priority] threshold, src, dst\n");
	for (auto item : promo_priority) {
		koo_mig_print(PRINT_DEBUG, "%lu, %d, %d\n", item.first, item.second.first, item.second.second);
	}


	koo_mig_print(PRINT_DEBUG, "[Promo priority (bin)] benefit - cost, bin, src, dst\n");
	for (auto item : promo_priority_bin) {
		koo_mig_print(PRINT_DEBUG, "%ld, %d, %d, %d\n", item.first, get<0>(item.second), get<1>(item.second), get<2>(item.second));
		//if (item.first <= 0)
		//	continue;
		promo_order.push_back({get<0>(item.second), get<1>(item.second), get<2>(item.second)});
	}
	reverse(promo_order.begin(), promo_order.end());

	set<pair<int,int>> dedup;
	vector<struct promo_path> ret;
	for (auto promo_path : promo_order) {
		if (promo_path.dst != 0)
			continue;
		if (dedup.count({promo_path.bin, promo_path.src}) == 0) {
			dedup.insert({promo_path.bin, promo_path.src});
			ret.push_back(promo_path);
		}
		koo_mig_print(PRINT_DEBUG, "%d %d %d\n", promo_path.bin, promo_path.src, promo_path.dst);
	}

	koo_mig_print(PRINT_DEBUG, "[Promo order] bin, src, dst\n");
	for (auto promo_path : ret) {
		koo_mig_print(PRINT_DEBUG, "%d %d %d\n", promo_path.bin, promo_path.src, promo_path.dst);
	}

	return ret;
}

pair<vector<int>,vector<int>> __select_promo_cand_cost_benefit (int nr_pages, int cur_age, struct hist_bin *hist, vector<unordered_map<void *, struct page_profile *>> &promo_target_pages, unsigned long long *nr_cap_tier_pages) {
	struct hist_bin *bin;
	int nr_promo_pages = 0;
	int promo_target = 0;
	struct page_profile *page_info;

	unsigned long nr_lookup_pages = 0;
	unsigned long nr_lookup_real_pages = 0;

	unsigned long nr_none_pages = 0;
	unsigned long nr_old_pages = 0;
	unsigned long nr_old_same_pages = 0;
	unsigned long nr_old_hotter_pages = 0;
	unsigned long nr_old_colder_pages = 0;
	//unsigned long nr_none_pages_again = 0;
	unsigned int nr_promo_target[MAX_NODES][MAX_NODES] = {0,};

	vector<int> nr_promo_from(MAX_NODES, 0);
	vector<int> nr_promo_to(MAX_NODES, 0);

    auto start = high_resolution_clock::now();


	auto promo_order = calc_promo_order(false, kmig.pebs_meta.period_iter); 

	for (auto &promo_path : promo_order) {
		bin = hist + promo_path.bin;

		if (bin->va_set.empty())
			continue;

		for (auto &cand_page : bin->va_lists[promo_path.src]) {
			if (nr_promo_pages == nr_pages)
				break;

			page_info = cand_page.second;
			promo_target = promo_path.dst;

			if (page_info->node == NONE_PAGE || page_info->next_node != NO_MIG) {
				printf("bin: %d, src: %d, dst: %d, node: %d, next_node: %d, hotness: %lu\n", promo_path.bin, promo_path.src, promo_path.dst, page_info->node, page_info->next_node, page_info->hotness);
				void *va = cand_page.first;
				for (int i = 0; i < NR_HIST_BINS; i++) {
					bin = hist + i;
					auto it = bin->va_set.find(va);
					if (it != bin->va_set.end()) {
						for (int j = 0; j < MAX_NODES; j++) {
							for (auto &item : bin->va_lists[j]) {
								if (item.first == va) {
									printf("bin: %d, cur_node: %d\n", i, j);
								}

							}
						}
					}
				}
				ABORT_WITH_LOG();
			}

			page_info->next_node = promo_path.dst;

			promo_target_pages[page_info->node].insert({cand_page.first, page_info});
			nr_promo_target[page_info->node][promo_target]++;
			nr_promo_pages++;
			nr_promo_from[page_info->node]++;
			nr_promo_to[promo_target]++;
		}
		if (nr_promo_pages == nr_pages)
			break;
	}

    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(stop - start);

	koo_mig_print(PRINT_KEY, "[Select promo: %ldms] nr_lookup_pages: %lu, nr_none_pages: %lu, nr_lookup_real_pages: %lu, nr_promo_pages: %d, nr_old_pages: %lu, nr_old_same_pages: %lu, nr_old_hotter_pages: %lu, nr_old_colder_pages: %lu\n", duration.count(), nr_lookup_pages, nr_none_pages, nr_lookup_real_pages, nr_promo_pages, nr_old_pages, nr_old_same_pages, nr_old_hotter_pages, nr_old_colder_pages);
	//printf("[Select promo: %ldms] nr_lookup_pages: %lu, nr_none_pages: %lu, nr_promo_pages: %d\n", duration.count(), nr_lookup_pages, nr_none_pages, nr_promo_pages);

	koo_mig_print(PRINT_KEY, "nr_promo_target (row:original/col:target):\n");
	for (int i = 0; i < MAX_NODES; i++) {
		for (int j = 0; j < MAX_NODES; j++) {
			koo_mig_print(PRINT_KEY, "%u ", nr_promo_target[i][j]);
		}
		koo_mig_print(PRINT_KEY, "\n");
	}
	koo_mig_print(PRINT_KEY, "nr_promo_from\n");
	for (auto nr : nr_promo_from) {
		koo_mig_print(PRINT_KEY, "%d ", nr);
	}
	koo_mig_print(PRINT_KEY, "\n");
	koo_mig_print(PRINT_KEY, "nr_promo_to\n");
	for (auto nr : nr_promo_to) {
		koo_mig_print(PRINT_KEY, "%d ", nr);
	}
	koo_mig_print(PRINT_KEY, "\n");


	//return nr_promo_pages;
	//return nr_promo_cand;
	return {nr_promo_from, nr_promo_to};
}

pair<vector<int>,vector<int>> select_promo_cand (int nr_pages, int cur_age, struct hist_bin *hist, vector<unordered_map<void *, struct page_profile *>> &promo_target_pages, unsigned long long *nr_cap_tier_pages, bool is_cb) {
	if (is_cb)
		return __select_promo_cand_cost_benefit(nr_pages, cur_age, hist, promo_target_pages, nr_cap_tier_pages);
	else
		return __select_promo_cand_greedy(nr_pages, cur_age, hist, promo_target_pages, nr_cap_tier_pages);
}

int do_promotion (int pid, int count, void **target_pages, int *nodes, int *status, unordered_map<void *, struct page_profile *> &promo_target_pages) {
	int idx = 0;

	if (!count)
		return 0;

    auto start = high_resolution_clock::now();

	for (auto promo_target_page : promo_target_pages) {
		target_pages[idx] = promo_target_page.first; // va
		nodes[idx] = promo_target_page.second->next_node; // target node num
		status[idx] = INT_MAX;
		idx++;
	}
	if (idx != count) {
		printf("promotion invalid count-idx count: %d idx: %d\n", count, idx);
		ABORT_WITH_LOG();
	}


	int nr_moved = __move_pages(pid, count, target_pages, nodes, status);

	kmig.pstat.nr_iters++;
	kmig.pstat.nr_try_pages += count;
	kmig.pstat.nr_moved_pages += nr_moved;

	int nr_promo_successes = 0;
	int old_node;
	static int nr_promo_failed = 0;
	static int nr_promo_failed_expected = 0;
	static int nr_promo_failed_not_err = 0;
	static unordered_map<int,unsigned long> err_map;

	struct page_profile *page_info;

	for (int i = 0; i < count; i++) {
		if (status[i] == INT_MAX) {
			page_info = kmig.g_page_map[target_pages[i]];
			page_info->next_node = NO_MIG;
		} else if (status[i] == nodes[i]) {
			page_info = kmig.g_page_map[target_pages[i]];
			kmig.pstat.nr_successed_pages++;
			kmig.pstat.nr_move_from_to[page_info->node][nodes[i]]++;

			old_node = page_info->node;
			page_info->node = nodes[i];
			page_info->next_node = NO_MIG;

			delete_hist_bin_va(kmig.hist + page_info->bin_idx, (unsigned long)target_pages[i], old_node);
			add_hist_bin_va(kmig.hist + page_info->bin_idx, (unsigned long)target_pages[i], page_info, page_info->node);

			//kmig.g_page_map[target_pages[i]]->node = nodes[i];
			nr_promo_successes++;
		} else if (status[i] < 0) {
			nr_promo_failed++;
			page_info = kmig.g_page_map[target_pages[i]];
			if (status[i] == -EFAULT || status[i] == -ENOENT) {
				nr_promo_failed_expected++;
				delete_hist_bin_va(kmig.hist + page_info->bin_idx, (unsigned long)target_pages[i], page_info->node);
				kmig.g_page_map.erase(target_pages[i]);
				delete page_info;
			} else {
				if (err_map.count(status[i]) == 0)
					err_map.insert({status[i],1});
				else
					err_map[status[i]]++;

				page_info->next_node = NO_MIG;
			}

			//page_info->node = NONE_PAGE;
			//page_info->next_node = NO_MIG;
		} else {
			nr_promo_failed_not_err++;
			page_info = kmig.g_page_map[target_pages[i]];

			old_node = page_info->node;
			page_info->node = status[i];
			page_info->next_node = NO_MIG;

			delete_hist_bin_va(kmig.hist + page_info->bin_idx, (unsigned long)target_pages[i], old_node);
			add_hist_bin_va(kmig.hist + page_info->bin_idx, (unsigned long)target_pages[i], page_info, page_info->node);
		}
	}

	promo_target_pages.clear();

    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(stop - start);

	koo_mig_print(PRINT_KEY, "[Do promotion: %ldms] nr_promo_try: %d, nr_promo_successes: %d, nr_promo_failed: %d, nr_promo_failed_expected: %d, nr_promo_failed_not_err: %d\n", duration.count(), count, nr_promo_successes, nr_promo_failed, nr_promo_failed_expected, nr_promo_failed_not_err);
	for (auto err : err_map) {
		koo_mig_print(PRINT_KEY, "errno: %d, cnt: %lu\n", err.first, err.second);
	}


	return nr_promo_successes;
}

/*
pair<vector<int>,vector<int>> select_quick_demo_cand(vector<int> nr_promo_pages, struct hist_bin *hist, int cur_age, int *demo_target_nodes, vector<unordered_map<void *, struct page_profile *>> &demo_target_pages) {
	int demo_target = MAX_NODES - 1;

	//auto it = alloc_meta.pages_to_move_list.begin();

}
*/

//vector<int> select_demo_cand(unsigned long long *nr_cap_tier_pages, vector<int> nr_promo_pages, unordered_map<void *, struct page_profile *> &promo_target_pages, struct hist_bin *hist, int cur_age, int *demo_target_node, unordered_map<void *, struct page_profile *> &demo_target_pages) {
//pair<vector<int>,vector<int>> select_demo_cand(unsigned long long *nr_cap_tier_pages, vector<int> nr_promo_pages, struct hist_bin *hist, int cur_age, int *demo_target_nodes, vector<unordered_map<void *, struct page_profile *>> &demo_target_pages, bool do_quick_demotion) {
pair<vector<int>,vector<int>> select_demo_cand(vector<int> nr_promo_pages, struct hist_bin *hist, int cur_age, int *demo_target_nodes, vector<unordered_map<void *, struct page_profile *>> &demo_target_pages, bool do_quick_demotion) {
	long long nr_free_pages[MAX_NODES];
	for (int i = 0; i < MAX_NODES; i++) {
		nr_free_pages[i] = get_numa_nr_free_pages(i) - (long long)NR_MARGIN_PAGES;
	}

	vector<long long> nr_demo_pages(MAX_NODES, 0);
	unsigned long nr_lookup_pages = 0, nr_qd_lookup_pages = 0, nr_select_pages = 0, nr_qd_select_pages = 0;
	vector<unsigned long> nr_lookup_real_pages(MAX_NODES, 0);
	//unsigned long nr_none_pages = 0;
	//unsigned long nr_already_mig_pages = 0;

	vector<int> nr_demo_from(MAX_NODES, 0);
	vector<int> nr_demo_to(MAX_NODES, 0);
	unsigned int nr_demo_target[MAX_NODES][MAX_NODES] = {0,};

	vector<int> nr_qdemo_from(MAX_NODES, 0);
	vector<int> nr_qdemo_to(MAX_NODES, 0);
	unsigned int nr_qdemo_target[MAX_NODES][MAX_NODES] = {0,};

	vector<unsigned int> nr_demo_per_bins(NR_HIST_BINS, 0);

	int bin_idx = 0;
	struct hist_bin *bin;
	struct page_profile *page_info;
	int demo_target;
	int cur_node;
	
	//volatile bool need_to_check = false;
	volatile bool need_to_scan = false;

    auto start = high_resolution_clock::now();

	for (int i = 0; i < MAX_NODES; i++) {
		nr_demo_pages[i] = nr_promo_pages[i] - nr_free_pages[i];
		nr_demo_pages[i] = min(nr_demo_pages[i], (long long)NR_DEMOTE_PAGES);
		if (i < MAX_NODES-1 && nr_demo_pages[i] > 0)
			need_to_scan = true;
	}

	if (!need_to_scan)
		return {nr_demo_from, nr_demo_to};


	koo_mig_print(PRINT_KEY, "[Demotion free pages] tier #, nr_promo #, nr_free #, nr_demo #\n");
	for (int i = 0; i < MAX_NODES; i++) {
		koo_mig_print(PRINT_KEY, "%d %d %lld %lld\n", i, nr_promo_pages[i], nr_free_pages[i], nr_demo_pages[i]);
	}
	//nr_demo_pages[MAX_NODES-1] = 0;

	/*
	if (do_quick_demotion) {

	}
	*/



	for (int i = 0; i < MAX_NODES - 1; i++) {
		cur_node = i;
		if (nr_demo_pages[cur_node] <= 0) continue;

		if (do_quick_demotion) {
			struct alloc_metadata_t &alloc_meta= kmig.alloc_meta;
			for (auto &cand_page : alloc_meta.pages_to_move_lists[cur_node]) {
				nr_qd_lookup_pages++;
				demo_target = MAX_NODES - 1;
				while (demo_target > cur_node && nr_free_pages[demo_target] <= 0) {
					demo_target--;
				}

				if (demo_target <= cur_node) {
					ABORT_WITH_LOG();
				}

				page_info = cand_page.second;
				page_info->next_node = demo_target | FLAG_QD;

				nr_demo_pages[cur_node]--;
				nr_demo_pages[demo_target]++;
				nr_qdemo_from[cur_node]++;
				nr_qdemo_to[demo_target]++;
				nr_qdemo_target[cur_node][demo_target]++;

				demo_target_pages[cur_node].insert({cand_page.first, page_info});
				nr_qd_select_pages++;

				if (nr_demo_pages[cur_node] <= 0)
					break;
			}
		}

		bin_idx = 0;
		demo_target = demo_target_nodes[cur_node];
		if (demo_target == -1)
			continue;

		while (bin_idx < NR_HIST_BINS && nr_demo_pages[cur_node] > 0) {
			bin = hist + bin_idx;
			for (auto &cand_page : bin->va_lists[cur_node]) {
				nr_lookup_pages++;
				page_info = cand_page.second;

				//if (page_info->node == NONE_PAGE || page_info->next_node != NO_MIG)
				if (page_info->node == NONE_PAGE)
					ABORT_WITH_LOG();

				if (page_info->next_node != NO_MIG)
					continue;

				page_info->next_node = demo_target;

				nr_demo_pages[cur_node]--;
				nr_demo_pages[demo_target]++;
				nr_demo_from[cur_node]++;
				nr_demo_to[demo_target]++;
				nr_demo_target[cur_node][demo_target]++;

				demo_target_pages[cur_node].insert({cand_page.first, page_info});

				nr_select_pages++;

				if (nr_demo_pages[cur_node] <= 0)
					break;
			}
			bin_idx++;
		}
	}

#if 0
	while (bin_idx < NR_HIST_BINS && need_to_scan) {
		bin = hist + bin_idx;

		for (auto va :  bin->va_set) {
			need_to_check = false;
			page_info = va.second->second;

			nr_lookup_pages++;

			if (page_info->node == NONE_PAGE) {
				abort();
				nr_none_pages++;
				continue;
			}

			if (page_info->next_node != NO_MIG) {
				nr_already_mig_pages++;
				continue;
			}

			//nr_lookup_real_pages++;
			nr_lookup_real_pages[page_info->node]++;

			cur_node = page_info->node;
			demo_target = demo_target_nodes[cur_node];

			if (demo_target != -1 && nr_demo_pages[cur_node] > 0) {
				nr_demo_pages[cur_node]--;
				if (demo_target_nodes[demo_target] != -1)
					nr_demo_pages[demo_target]++;

				page_info->next_node = demo_target;
				nr_demo_from[cur_node]++;
				nr_demo_to[demo_target]++;
				nr_demo_target[cur_node][demo_target]++;

				nr_demo_per_bins[bin_idx]++;

				demo_target_pages[cur_node].insert({va.first, page_info});
			}

			if (nr_demo_pages[cur_node] <= 0)
				need_to_check = true;

			if (need_to_check) {
				need_to_scan = false;
				for (auto nr : nr_demo_pages) {
					if (nr > 0) {
						need_to_scan = true;
						break;
					}
				}
			}

			if (need_to_scan == false)
				break;
		}
		bin_idx++;
	}
#endif

    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(stop - start);

	koo_mig_print(PRINT_KEY, "[Select demo: %ldms] nr_lookup_pages: %lu, nr_qd_lookup_pages: %lu, nr_select_pages: %lu, nr_qd_select_pages: %lu\n", duration.count(), nr_lookup_pages, nr_qd_lookup_pages, nr_select_pages, nr_qd_select_pages);

	koo_mig_print(PRINT_KEY, "nr_lookup_real_pages\n");
	for (auto nr : nr_lookup_real_pages) {
		koo_mig_print(PRINT_KEY, "%lu ", nr);
	}
	koo_mig_print(PRINT_KEY, "\n");

	koo_mig_print(PRINT_KEY, "nr_demo_target (row:original/col:target):\n");
	for (int i = 0; i < MAX_NODES; i++) {
		for (int j = 0; j < MAX_NODES; j++) {
			koo_mig_print(PRINT_KEY, "%u ", nr_demo_target[i][j]);
		}
		koo_mig_print(PRINT_KEY, "\n");
	}
	koo_mig_print(PRINT_KEY, "nr_demo_from\n");
	for (auto nr : nr_demo_from) {
		koo_mig_print(PRINT_KEY, "%d ", nr);
	}
	koo_mig_print(PRINT_KEY, "\n");
	koo_mig_print(PRINT_KEY, "nr_demo_to\n");
	for (auto nr : nr_demo_to) {
		koo_mig_print(PRINT_KEY, "%d ", nr);
	}
	koo_mig_print(PRINT_KEY, "\n");


	koo_mig_print(PRINT_KEY, "nr_demo_per_bins\n");
	for (auto nr : nr_demo_per_bins) {
		koo_mig_print(PRINT_KEY, "%u ", nr);
	}
	koo_mig_print(PRINT_KEY, "\n");

	koo_mig_print(PRINT_KEY, "nr_qdemo_target (row:original/col:target):\n");
	for (int i = 0; i < MAX_NODES; i++) {
		for (int j = 0; j < MAX_NODES; j++) {
			koo_mig_print(PRINT_KEY, "%u ", nr_qdemo_target[i][j]);
		}
		koo_mig_print(PRINT_KEY, "\n");
	}
	koo_mig_print(PRINT_KEY, "nr_qdemo_from\n");
	for (auto nr : nr_qdemo_from) {
		koo_mig_print(PRINT_KEY, "%d ", nr);
	}
	koo_mig_print(PRINT_KEY, "\n");
	koo_mig_print(PRINT_KEY, "nr_qdemo_to\n");
	for (auto nr : nr_qdemo_to) {
		koo_mig_print(PRINT_KEY, "%d ", nr);
	}
	koo_mig_print(PRINT_KEY, "\n");

	for (int i = 0; i < MAX_NODES - 1; i++) {
		nr_demo_from[i] += nr_qdemo_from[i];
		nr_demo_to[i] += nr_qdemo_to[i];

	}

	return {nr_demo_from, nr_demo_to};
}

int do_demotion (int pid, int count, void **target_pages, int *nodes, int *status, unordered_map<void *, struct page_profile *> &demo_target_pages, bool is_odd=false) {
	int idx = 0;

	struct demo_stat &dstat = is_odd ? kmig.oddstat : kmig.dstat;
	struct demo_stat &qdstat = kmig.qdstat;

	if (count < 0) {
		printf("demotion invalid count %d\n", count);
		ABORT_WITH_LOG();
	}

	if (!count)
		return 0;

    auto start = high_resolution_clock::now();

	for (auto demo_target_page : demo_target_pages) {
		target_pages[idx] = demo_target_page.first; // va
		nodes[idx] = demo_target_page.second->next_node & (FLAG_QD-1); // target node num
		status[idx] = INT_MAX;
		idx++;
	}
	if (idx != count) {
		printf("demotion invalid count-idx count: %d idx: %d\n", count, idx);
		ABORT_WITH_LOG();
	}

	int nr_moved = __move_pages(pid, count, target_pages, nodes, status);

	dstat.nr_iters++;
	dstat.nr_try_pages += count;
	dstat.nr_moved_pages += nr_moved;


	int nr_demo_successes = 0;
	int old_node;
	static int nr_demo_failed = 0;
	static int nr_demo_failed_expected = 0;
	static int nr_demo_failed_not_err = 0;
	static unordered_map<int,unsigned long> err_map;

	struct page_profile *page_info;

	bool is_qd;

	for (int i = 0; i < count; i++) {
		if (status[i] == INT_MAX) {
			page_info = kmig.g_page_map[target_pages[i]];
			page_info->next_node = NO_MIG;
		} else if (status[i] == nodes[i]) {
			page_info = kmig.g_page_map[target_pages[i]];

			is_qd = page_info->next_node & FLAG_QD ? true : false;

			old_node = page_info->node;
			page_info->node = nodes[i];
			page_info->next_node = NO_MIG;
			nr_demo_successes++;

			if (is_qd) {
				qdstat.nr_successed_pages++;
				qdstat.nr_move_from_to[old_node][nodes[i]]++;

				auto it = kmig.alloc_meta.pages_to_move_dict.find(target_pages[i]);
				if (it == kmig.alloc_meta.pages_to_move_dict.end())
					ABORT_WITH_LOG();

				kmig.alloc_meta.pages_to_move_lists[old_node].erase(it->second);
				kmig.alloc_meta.pages_to_move_dict.erase(it);
			} else {
				dstat.nr_successed_pages++;
				dstat.nr_move_from_to[old_node][nodes[i]]++;

				delete_hist_bin_va(kmig.hist + page_info->bin_idx, (unsigned long)target_pages[i], old_node);
				add_hist_bin_va(kmig.hist + page_info->bin_idx, (unsigned long)target_pages[i], page_info, page_info->node);
			}
		} else if (status[i] < 0) {
			nr_demo_failed++;
			page_info = kmig.g_page_map[target_pages[i]];

			old_node = page_info->node;

			is_qd = page_info->next_node & FLAG_QD ? true : false;

			if (status[i] == -EFAULT || status[i] == -ENOENT) {
				nr_demo_failed_expected++;
				if (is_qd) {
					auto it = kmig.alloc_meta.pages_to_move_dict.find(target_pages[i]);
					if (it == kmig.alloc_meta.pages_to_move_dict.end())
						ABORT_WITH_LOG();

					kmig.alloc_meta.pages_to_move_lists[old_node].erase(it->second);
					kmig.alloc_meta.pages_to_move_dict.erase(it);
				} else {
					delete_hist_bin_va(kmig.hist + page_info->bin_idx, (unsigned long)target_pages[i], old_node);
				}
				kmig.g_page_map.erase(target_pages[i]);
				delete page_info;
			} else {
				if (err_map.count(status[i]) == 0)
					err_map.insert({status[i],1});
				else
					err_map[status[i]]++;

				page_info->next_node = NO_MIG;
			}
			//page_info->node = NONE_PAGE;
			//page_info->next_node = NO_MIG;
		} else {
			page_info = kmig.g_page_map[target_pages[i]];

			is_qd = page_info->next_node & FLAG_QD ? true : false;

			old_node = page_info->node;
			page_info->node = status[i];
			page_info->next_node = NO_MIG;
			nr_demo_failed_not_err++;

			if (is_qd) {
				auto it = kmig.alloc_meta.pages_to_move_dict.find(target_pages[i]);
				if (it == kmig.alloc_meta.pages_to_move_dict.end())
					ABORT_WITH_LOG();

				kmig.alloc_meta.pages_to_move_lists[old_node].erase(it->second);
				kmig.alloc_meta.pages_to_move_dict.erase(it);
			} else {
				delete_hist_bin_va(kmig.hist + page_info->bin_idx, (unsigned long)target_pages[i], old_node);
				add_hist_bin_va(kmig.hist + page_info->bin_idx, (unsigned long)target_pages[i], page_info, page_info->node);
			}
		}
	}

	demo_target_pages.clear();

    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(stop - start);

	koo_mig_print(PRINT_KEY, "[Do demotion: %ldms] nr_demo_try: %d, nr_demo_successes: %d, nr_demo_failed: %d, nr_demo_failed_expected: %d, nr_demo_failed_not_err: %d\n", duration.count(), count, nr_demo_successes, nr_demo_failed, nr_demo_failed_expected, nr_demo_failed_not_err);
	for (auto err : err_map) {
		koo_mig_print(PRINT_KEY, "errno: %d, cnt: %lu\n", err.first, err.second);
	}

	return nr_demo_successes;
}



int do_migration (int nr_promo_pages, int cur_age, struct hist_bin *hist, void **target_pages, int *nodes, int *status, bool do_quick_demotion, bool do_cb_promo) {

	int pid = kmig.pid;

	static unsigned long long accum_promo_count = 0, accum_demo_count = 0;
	static vector<unsigned long long> accum_promo_counts(MAX_NODES, 0);
	static vector<unsigned long long> accum_demo_counts(MAX_NODES, 0);

	auto start = high_resolution_clock::now();

	vector<unordered_map<void *, struct page_profile *>> promo_target_pages(MAX_NODES, unordered_map<void *, struct page_profile *>());
	vector<unordered_map<void *, struct page_profile *>> demo_target_pages(MAX_NODES, unordered_map<void *, struct page_profile *>());

	auto [nr_promo_result_from, nr_promo_result_to] = select_promo_cand(nr_promo_pages, cur_age, hist, promo_target_pages, kmig.nr_cap_tier_pages, do_cb_promo);

	int demo_target_nodes[MAX_NODES];
	for (int i = 0; i < MAX_NODES; i++) {
		demo_target_nodes[i] = i+1;
	}
	demo_target_nodes[MAX_NODES-1] = -1;

	auto [nr_demo_result_from, nr_demo_result_to] = select_demo_cand(nr_promo_result_to, hist, cur_age, demo_target_nodes, demo_target_pages, do_quick_demotion);

	int demo_count = 0;
	for (int i = MAX_NODES - 1; i >= 0; i--) {
		nr_demo_result_from[i] = do_demotion(pid, nr_demo_result_from[i], target_pages, nodes, status, demo_target_pages[i], false);
		demo_count += nr_demo_result_from[i];
		accum_demo_counts[i] += nr_demo_result_from[i];
	}

	int promo_count = 0;
	for (int i = MAX_NODES - 1; i >= 0; i--) {
		nr_promo_result_from[i] = do_promotion(pid, nr_promo_result_from[i], target_pages, nodes, status, promo_target_pages[i]);
		promo_count += nr_promo_result_from[i];
		accum_promo_counts[i] += nr_promo_result_from[i];
	}

    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(stop - start);

	koo_mig_print(PRINT_KEY, "[Do migration: %ldms] nr_promo_pages: %d, nr_demo_pages: %d\n", duration.count(), promo_count, demo_count);
	koo_mig_print(PRINT_KEY, "Successed promo/demo nr_pages (from/to): %d, %d\n", promo_count, demo_count);
	for (int i = 0; i < MAX_NODES; i++) {
		koo_mig_print(PRINT_KEY, "%d ", nr_promo_result_from[i]);
	}
	koo_mig_print(PRINT_KEY, "\n");
	for (int i = 0; i < MAX_NODES; i++) {
		koo_mig_print(PRINT_KEY, "%d ", nr_promo_result_to[i]);
	}
	koo_mig_print(PRINT_KEY, "\n");
	for (int i = 0; i < MAX_NODES; i++) {
		koo_mig_print(PRINT_KEY, "%d ", nr_demo_result_from[i]);
	}
	koo_mig_print(PRINT_KEY, "\n");
	for (int i = 0; i < MAX_NODES; i++) {
		koo_mig_print(PRINT_KEY, "%d ", nr_demo_result_to[i]);
	}
	koo_mig_print(PRINT_KEY, "\n");


	accum_promo_count += promo_count;
	accum_demo_count += demo_count;
	koo_mig_print(PRINT_KEY, "Accumulated successed promo/demo nr_pages: %llu, %llu\n", accum_promo_count, accum_demo_count);
	for (int i = 0; i < MAX_NODES; i++) {
		koo_mig_print(PRINT_KEY, "%llu ", accum_promo_counts[i]);
	}
	koo_mig_print(PRINT_KEY, "\n");
	for (int i = 0; i < MAX_NODES; i++) {
		koo_mig_print(PRINT_KEY, "%llu ", accum_demo_counts[i]);
	}
	koo_mig_print(PRINT_KEY, "\n");

	return 0;
}

int do_demotion_if_needed(struct hist_bin *hist, int cur_age, void **target_pages, int *nodes, int *status, bool do_quick_demotion) {

	int pid = kmig.pid;

	auto start = high_resolution_clock::now();

	vector<unordered_map<void *, struct page_profile *>> demo_target_pages(MAX_NODES, unordered_map<void *, struct page_profile *>());

	vector<int> tmp (MAX_NODES, 0);

	int demo_target_nodes[MAX_NODES];
	for (int i = 0; i < MAX_NODES; i++) {
		demo_target_nodes[i] = i+1;
	}
	demo_target_nodes[MAX_NODES-1] = -1;

	auto [nr_demo_result_from, nr_demo_result_to] = select_demo_cand(tmp, hist, cur_age, demo_target_nodes, demo_target_pages, do_quick_demotion);

	int demo_count = 0;
	for (int i = MAX_NODES-1; i >= 0; i--) {
		demo_count += do_demotion(pid, nr_demo_result_from[i], target_pages, nodes, status, demo_target_pages[i], true);
	}


    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(stop - start);

	koo_mig_print(PRINT_KEY, "[Do demotion_if_needed: %ldms] nr_demo_pages: %d\n", duration.count(), demo_count);
	//printf("[Do migration: %ldms] nr_promo_try: %d, nr_promo_successes: %d, nr_promo_failed: %d, nr_promo_failed_expected: %d, nr_promo_failed_not_err: %d\n", duration.count(), count, nr_promo_successes, nr_promo_failed, nr_promo_failed_expected, nr_promo_failed_not_err);


	return 0;

}

unsigned long cooling_one_bin(struct hist_bin *bin, int cur_age) {
	uint64_t old_hotness, new_hotness;
	int old_age;
	int new_bin;
	struct page_profile *page_info;
	unsigned long va;

	unsigned long nr_none_pages = 0;
	unsigned long nr_lookup = 0;
	unsigned long nr_real_lookup = 0;
	unsigned long nr_cooled_pages = 0;

	auto start = high_resolution_clock::now();

	//for (auto &item : bin->va_set) {
	for (auto it = bin->va_set.begin(); it != bin->va_set.end();) {
		page_info = it->second->second;
		//page_info = item.second;
		nr_lookup++;
		if (page_info->node == NONE_PAGE) {
			ABORT_WITH_LOG();
			nr_none_pages++;
			it++;
			continue;
		}
		nr_real_lookup++;

		va = (unsigned long)it->first;
		old_age = page_info->age;
		old_hotness = page_info->hotness;

		new_hotness = calc_hotness(old_hotness, 0, cur_age - old_age, HOTNESS_WEIGHT);
		new_bin = get_idx(new_hotness);

		if (kmig.hist + new_bin == bin) {
			page_info->age = cur_age;
			page_info->hotness = new_hotness;
			page_info->bin_idx = new_bin;
			it++;
			continue;
		}

		/*
		if (kmig.hist + new_bin == bin) {
			printf("new bin is the same as the old bin, old_hotness: %u, old_bin_idx: %d, new_hotness: %u, new_bin_idx: %d\n", old_hotness, page_info->bin_idx, new_hotness, new_bin);
			abort();
		}
		*/


		/*
		if(update_hist(va, old_hotness, new_hotness, page_info, kmig.hist)) {
			
			nr_cooled_pages++;
		}
		*/

		// add items into new bin
		add_hist_bin_va(kmig.hist + new_bin, va, page_info, page_info->node);
		page_info->age = cur_age;
		page_info->hotness = new_hotness;
		page_info->bin_idx = new_bin;


		// delete items from old bin
		bin->va_lists[page_info->node].erase(it->second);
		it = bin->va_set.erase(it);
		bin->nr_deleted++;
		bin->nr_pages--;
		bin->nr_pages_tier[page_info->node]--;

		nr_cooled_pages++;
	}

    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(stop - start);

	koo_mig_print(PRINT_DEBUG, "[coolin_one_bin: %ldms] nr_lookup_pages: %ld, nr_none_pages: %ld, nr_real_lookup_pages: %ld, nr_cooled_pages: %ld\n", duration.count(), nr_lookup, nr_none_pages, nr_real_lookup, nr_cooled_pages);

	return nr_cooled_pages;
}

unsigned long do_cooling(struct hist_bin *hist, int cur_age) {
	//int pid = kmig.pid;

	auto start = high_resolution_clock::now();

	vector<unordered_map<void *, struct page_profile *>> demo_target_pages(MAX_NODES, unordered_map<void *, struct page_profile *>());

	vector<unsigned long> res (NR_HIST_BINS, 0);
	unsigned long total = 0;

	for (int i = 1; i < NR_HIST_BINS; i++) {
		res[i] = cooling_one_bin(hist + i, cur_age);
		total += res[i];
	}
	
    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(stop - start);

	koo_mig_print(PRINT_KEY, "[Do cooling: %ldms] nr_cooled_pages: %d\n", duration.count(), total);

	return total;
}

static inline int get_rb_status(int fd) {
	int flag = 0;
    if (ioctl(fd, IOCTL_GET_RB_STATUS, &flag) < 0) {
		perror("Failed to get rb status");
        close(fd);
        return -1;
    }
	return flag;
}

static inline int get_pebs_period(int fd) {
    if (ioctl(fd, IOCTL_GET_PERIOD, &kmig.pebs_meta.period) < 0) {
		perror("Failed to get rb status");
        close(fd);
		return -1;
    }
	kmig.pebs_meta.period_iter++;
	return 0;
}

void *kmig_run (void *arg) {
	int epoll_fd;
    struct epoll_event ev, events[MAX_EVENTS];




	if (setup_drain() < 0) {
		close(kmig.fd);
		return (void *)-1;
	}

	if (start_profile() < 0) {
		close(kmig.fd);
		return (void *)-1;
	}

	int pid = kmig.pid;
	koo_mig_print(PRINT_DEBUG, "passed pid: %d, thread pid: %d\n", pid, getpid());


	int nfds, n, flag;

	bool alloc_occured, pebs_profile_occured;

	bool do_quick_demotion = kmig.opts.do_quick_demotion;
	bool do_demotion_for_alloc = kmig.opts.do_demotion_for_alloc;
	bool do_mig = kmig.opts.do_mig;
	bool do_cb_promo = kmig.opts.do_cb_promo;
	int print_itv = kmig.opts.print_itv;

	//int max_nr_items = RB_BUF_SIZE/sizeof(struct rb_data_t);
	int max_nr_items = NR_MAX_MIG_PAGES;
	void **target_pages = (void **)calloc(max_nr_items, sizeof(void *));
	int *nodes = (int *)calloc(max_nr_items, sizeof(void *));
	int *status = (int *)calloc(max_nr_items, sizeof(void *));

	auto cur_time = high_resolution_clock::now();
	auto prev_print_time = cur_time;
	//auto prev_profile_time = cur_time;
	auto prev_period_time = cur_time;
	auto prev_mig_time = cur_time;
	auto prev_cooling_time = cur_time;
	auto duration = duration_cast<milliseconds>(prev_print_time - cur_time);

	if (do_cb_promo) {
		get_pebs_period(kmig.fd);
		koo_mig_print(PRINT_NONE, "[Initial PEBS period] read: %lu, write: %lu\n", kmig.pebs_meta.period.read, kmig.pebs_meta.period.write);
		//kmig.pebs_meta.promo_threshold = calc_promo_threshold(true);
		//kmig.pebs_meta.promo_order = calc_promo_threshold(true);
	} else {
		kmig.pebs_meta.period.read = 1;
		kmig.pebs_meta.period.write = 1;
	}

	while (1) {
		if (kmig.thread_stop)
			break;

		alloc_occured = pebs_profile_occured = false;

        nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, EPOLL_TIMEOUT);
        if (nfds == -1) {
            perror("epoll_wait");
            close(kmig.fd);
	        return (void *)-1;
        }

        for (n = 0; n < nfds; ++n) {
            if (events[n].data.fd == kmig.fd && events[n].events == EPOLLIN) {
				flag = get_rb_status(kmig.fd);
				//printf("poll return %d, events: %d flag: %d\n",nfds, events[n].events, flag);
				if (flag & RB_TYPE_TO_FLAG(RB_ALLOC)) {
					drain(RB_ALLOC, kmig.rb, kmig.rb_buf);
					alloc_occured = true;
				}

				if (flag & RB_TYPE_TO_FLAG(RB_PEBS)) {
					drain(RB_PEBS, kmig.rb, kmig.rb_buf);
					kmig.profile_iter++;
					pebs_profile_occured = true;
				}
		    }
        }

		if (kmig.opts.do_pebs && drain_user_pebs(kmig.pebs_meta)) {
			kmig.profile_iter++;
			pebs_profile_occured = true;
		}

		if (alloc_occured && !do_quick_demotion) {
			for (int i = 0; i < MAX_NODES; i++) {
				kmig.alloc_meta.pages_to_move_lists[i].clear();
			}
			kmig.alloc_meta.pages_to_move_dict.clear();
		}

		if (pebs_profile_occured)
			profile_pages(PROF_PEBS, kmig.age, do_mig);

		if (do_demotion_for_alloc)
			do_demotion_if_needed(kmig.hist, kmig.age, target_pages, nodes, status, do_quick_demotion);


		cur_time = high_resolution_clock::now();
		duration = duration_cast<milliseconds>(cur_time - prev_print_time);
		if (print_itv != -1 && duration.count() >= print_itv * 1000) {
			print_koo_mig();
			prev_print_time = cur_time;
		}

		duration = duration_cast<milliseconds>(cur_time - prev_period_time);
		if (do_cb_promo && duration.count() >= PERIOD_INTERVAL * 1000) {
			get_pebs_period(kmig.fd);
			prev_period_time = cur_time;
		}

		duration = duration_cast<milliseconds>(cur_time - prev_mig_time);
		if (do_mig && duration.count() >= MIG_INTERVAL * 1000) {
			print_hist(kmig.hist, false);

			do_migration(NR_PROMOTE_PAGES, kmig.age, kmig.hist, target_pages, nodes, status, do_quick_demotion, do_cb_promo);
			prev_mig_time = cur_time;
		}

		duration = duration_cast<milliseconds>(cur_time - prev_cooling_time);
		if (do_mig && kmig.profiled_accesses >= COOLING_INTERVAL) {
			print_hist(kmig.hist, true);
			kmig.age++;
			do_cooling(kmig.hist, kmig.age);
			kmig.profiled_accesses = 0;
			prev_cooling_time = cur_time;
		}
    }


	return NULL;
}

int koo_mig_init(int pid, void *opts) {
	memcpy(&kmig.opts, opts, sizeof(struct opts));

	// open the device and initialize the ring buffer for draining profiling data
	if (init_profile() < 0) {
		perror("Failed to initialize profiling fd: %d, pid: %d\n", kmig.fd, kmig.pid);
		return -1;
	}

	// user PEBS, if enabled
	if (kmig.opts.do_pebs) {
		sampler_init(kmig.pid);
	}

	for (int i = 0; i < MAX_NODES; i++) {
		kmig.nr_cap_tier_pages[i] = get_numa_nr_free_pages(i) - (long long)NR_MARGIN_PAGES;
		koo_mig_print(PRINT_DEBUG, "NUMA %d nr_pages: %llu (%lluGB)\n", i, kmig.nr_cap_tier_pages[i], kmig.nr_cap_tier_pages[i] * PAGE_SIZE / 1024 / 1024 / 1024);
	}

	kmig.thread_stop = false;
	signal(SIGUSR1, sig_handler_usr);
	pthread_create(&kmig.tid, NULL, kmig_run, NULL);
	pthread_setname_np(kmig.tid, "koo_mig");

    return 0;
}

void destroy_koo_mig(void) {
	kmig.thread_stop = true;
	pthread_join(kmig.tid, NULL);

	if (kmig.opts.do_pebs) {
		sampler_destroy();
	}

	destroy_profile();

	print_koo_mig();
	return;
}