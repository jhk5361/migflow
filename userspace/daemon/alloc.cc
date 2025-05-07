// set_pid.c
#include "koo_mig.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>
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
using namespace std;
using namespace std::chrono;

extern koo_mig kmig;

int move_pages_alloc(struct alloc_metadata_t &alloc_meta, void **target_pages, int *nodes, int *status) {
	int count = 0;
	int pid = kmig.pid;

	if (!alloc_meta.pages_to_move.size())
		return 0;

	int alloc_target;

	long long nr_free_pages[MAX_NODES];

	static unsigned long long nr_try_pages = 0;
	static unsigned long long nr_successes_pages = 0;
	static unsigned long long nr_err_pages = 0;
	static unsigned long long nr_not_expected_pages = 0;
	static unsigned long long nr_alloc_pages[MAX_NODES] = {0,};

    auto start = high_resolution_clock::now();

	for (int i = 0; i < MAX_NODES; i++) {
		nr_free_pages[i] = get_numa_nr_free_pages(i) - (long long)NR_MARGIN_PAGES;
	}

	alloc_target = MAX_NODES - 1;
	while (alloc_target >= 0) {
		if (nr_free_pages[alloc_target] - (long long)alloc_meta.pages_to_move.size() >= 0)
			break;
		alloc_target--;
	}


	for (auto it = alloc_meta.pages_to_move.begin(); it != alloc_meta.pages_to_move.end();it++) {
		target_pages[count] = it->first;
		nodes[count] = alloc_target;
		status[count] = INT_MIN;
		count++;
	}

	nr_try_pages += count;
	kmig.astat.nr_try_pages += count;

	int nr_moved = __move_pages(pid, count, target_pages, nodes, status);

	kmig.astat.nr_moved_pages += nr_moved;

	for (int i = 0; i < count; i++) {
		if (status[i] == INT_MAX) {
			continue;
		} else if (status[i] == nodes[i]) {
			//kmig.mstat.nr_alloc_move_success++;
			nr_successes_pages++;
			nr_alloc_pages[nodes[i]]++;
			kmig.astat.nr_successed_pages++;
			kmig.astat.nr_alloc_move_pages[nodes[i]]++;
			kmig.astat.nr_move_from_to[alloc_meta.pages_to_move[i].second->node][nodes[i]]++;
			alloc_meta.pages_to_move[i].second->node = status[i];
			//alloc_meta.pages_to_move.erase(target_pages[i]);
		} else if (status[i] < 0) {
			alloc_meta.pages_to_move[i].second->node = NONE_PAGE;
			nr_err_pages++;
			if (status[i] != -EFAULT && status[i] != -ENOENT) {
				//alloc_meta.pages_to_move[target_pages[i]]++;
				//kmig.mstat.nr_alloc_move_err_retry++;
			} else {
				//alloc_meta.pages_to_move.erase(target_pages[i]);
			}
		} else {
			nr_not_expected_pages++;
			alloc_meta.pages_to_move[i].second->node = status[i];
			//alloc_meta.pages_to_move[target_pages[i]]++;
			//kmig.mstat.nr_alloc_move_retry++;
		}
	}

	alloc_meta.pages_to_move.clear();

    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(stop - start);

	koo_mig_print(PRINT_DEBUG, "[Alloc: %ldms] nr_try_pages: %llu, nr_successes_pages: %llu, nr_err_pages: %llu, nr_not_expected_pages: %llu\n", duration.count(), nr_try_pages, nr_successes_pages, nr_err_pages, nr_not_expected_pages);

	koo_mig_print(PRINT_DEBUG, "nr_alloc_pages (per node)\n");
	for (int i = 0; i < MAX_NODES; i++) {
		koo_mig_print(PRINT_DEBUG, "%llu ", nr_alloc_pages[i]);
	}
	koo_mig_print(PRINT_DEBUG, "\n");

	return nr_moved;
}

int drain_rb_alloc (rb_head_t *rb, rb_data_t *rb_buf, struct alloc_metadata_t &alloc_meta) {
	int head, tail, len, rbuf_idx;
	struct rb_data_alloc_t rb_alloc;
	unsigned long va;
	head = rb->head;
	tail = rb->tail;
	len = (head + rb->size - tail) % rb->size;

	if (len == 0)
		return 0;

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
	unsigned int old_hotness = UINT_MAX, new_hotness = UINT_MAX;
	int cur_age = kmig.age;

	for (int i = 0; i < len; i++) {
		rbuf_idx = (tail + i) % rb->size;
		rb_alloc = rb_buf[rbuf_idx].data.rb_alloc;

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

		old_hotness = UINT_MAX;

		va = rb_alloc.va;
		last_accessed = rb_alloc.last_accessed;

		nr_alloc_pages[rb_alloc.node]++;
		kmig.astat.nr_alloc_pages[rb_alloc.node]++;

		it = kmig.g_page_map.find((void*)va);
		if (it == kmig.g_page_map.end()) {
			page_info = new page_profile;
			kmig.g_page_map.insert({(void*)va, page_info});
			page_info->bin_idx = -1;

			new_hotness = calc_hotness(UINT_MAX, last_accessed, 0, HOTNESS_WEIGHT);
			//new_hotness = calc_hotness(old_hotness, last_accessed, page, cur_age - page_info->age, HOTNESS_WEIGHT);
		} else {
			nr_existing_pages++;

			page_info = it->second;

			old_hotness = page_info->hotness;
			new_hotness = calc_hotness(old_hotness, last_accessed, cur_age - page_info->age, HOTNESS_WEIGHT);
			//new_hotness = calc_hotness(old_hotness, last_accessed, cur_age - page_info->age, HOTNESS_WEIGHT);
		}

		if (!last_accessed) {
			nr_not_accessed_pages++;
			if (kmig.opts.do_quick_demotion) {
				page_info->next_node = MAX_NODES - 1;
				alloc_meta.pages_to_move.push_back({(void *)va, page_info});
				//if (alloc_meta.pages_to_move.count((void*)va) == 0)
				//	alloc_meta.pages_to_move[(void*)va] = 0;
			}
		}

		// update histogram
		*page_info = (struct page_profile) {new_hotness, cur_age, rb_alloc.node, NO_MIG, page_info->bin_idx};

		update_hist(va, old_hotness, new_hotness, page_info, kmig.hist);
	}

	kmig.astat.nr_iters++;

    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(stop - start);

	koo_mig_print(PRINT_DEBUG, "[Alloc drain page: %ldms] nr_drain_pages: %d, nr_drain_pages: %llu, nr_existing_pages: %llu, nr_not_accessed_pages: %llu, nr_err_pages: %llu\n", duration.count(), len, nr_drain_pages, nr_existing_pages, nr_not_accessed_pages, nr_err_pages);
	for (auto err : err_map) {
		koo_mig_print(PRINT_DEBUG, "errno: %d, cnt: %lu\n", err.first, err.second);
	}
	koo_mig_print(PRINT_DEBUG, "nr_alloc_pages\n");
	for (auto nr : nr_alloc_pages) {
		koo_mig_print(PRINT_DEBUG, "%llu ", nr);
	}
	koo_mig_print(PRINT_DEBUG, "\n");


	return len;
}

