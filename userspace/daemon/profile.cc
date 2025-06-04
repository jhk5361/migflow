#include <stdio.h>
#include <sys/epoll.h>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <errno.h>
#include <iostream>
#include <string.h>
#include <numaif.h>
#include "koo_mig.h"
#include "profile.h"
#include "utils.h"
using namespace std;
using namespace std::chrono;

extern struct koo_mig kmig;
static int epoll_fd;
static struct epoll_event ev, events[MAX_EVENTS];

static inline int get_rb_status(int fd) {
	int flag = 0;
    if (ioctl(fd, IOCTL_GET_RB_STATUS, &flag) < 0) {
		perror("Failed to get rb status");
        close(fd);
        return -1;
    }
	return flag;
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

void add_hist_bin_va(struct hist_bin *bin, unsigned long va, struct page_profile *page_info, int node) {

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

void delete_hist_bin_va(struct hist_bin *bin, unsigned long va, int node) {

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

	return 0;
}

int start_profile() {
	// start the profiling
    if (ioctl(kmig.fd, IOCTL_SET_PID, &kmig.pid) < -1) {
        perror("Failed to set PID");
        return -1;
    }
	return 0;
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
				auto iter_end = alloc_meta.pages_to_move_lists[0].end(); // dummy data
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

static int __drain (int type, rb_head_t **rb, rb_data_t **rb_buf) {
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

int drain(bool &alloc_occured, bool &profile_occured) {
	int nfds, n, flag;
    nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, EPOLL_TIMEOUT);
    if (nfds == -1) {
        perror("epoll_wait");
		return -1;
    }

	for (n = 0; n < nfds; ++n) {
		if (events[n].data.fd == kmig.fd && events[n].events == EPOLLIN) {
			flag = get_rb_status(kmig.fd);
			//printf("poll return %d, events: %d flag: %d\n",nfds, events[n].events, flag);
			if (flag & RB_TYPE_TO_FLAG(RB_ALLOC)) {
				__drain(RB_ALLOC, kmig.rb, kmig.rb_buf);
				alloc_occured = true;
			}

			if (flag & RB_TYPE_TO_FLAG(RB_PEBS)) {
				__drain(RB_PEBS, kmig.rb, kmig.rb_buf);
				kmig.profile_iter++;
				profile_occured = true;
			}
		}
	}

	return 0;
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

static unsigned long cooling_one_bin(struct hist_bin *bin, int cur_age) {
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