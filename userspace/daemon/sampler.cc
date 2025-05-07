#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "sampler.h"
using namespace std;

static struct pebs_deamon pd;

static int kmig_cxl_mode = false;

#define PERF_PAGES  (1 + (1 << 16))

// perf_event_open()를 사용하여 PEBS 활용하는 함수
long perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
                     int cpu, int group_fd, unsigned long flags) {
    return syscall(__NR_perf_event_open, hw_event, pid, cpu,
                   group_fd, flags);
}

static uint64_t get_pebs_event(enum events e)
{
	switch (e) {
		case DRAMREAD:
			return DRAM_LLC_LOAD_MISS;
		case NVMREAD:
			if (!kmig_cxl_mode)
				return NVM_LLC_LOAD_MISS;
			else
				return N_EVENTS;
		case R_DRAMREAD:
			return REMOTE_DRAM_LLC_LOAD_MISS;
		case R_NVMREAD:
			return REMOTE_NVM_LLC_LOAD_MISS;
		case MEMWRITE:
			return ALL_STORES;
		case CXLREAD:
			if (kmig_cxl_mode)
				return REMOTE_DRAM_LLC_LOAD_MISS;
			else
				return N_EVENTS;
		default:
			return N_EVENTS;
	}
}

unsigned int inst_sample_period = 100007;

int pebs_open(uint64_t config, uint64_t config1, uint64_t type, pid_t pid) {
	int __pid = pid, __cpu = 0;
	struct perf_event_attr pe;
	memset(&pe, 0, sizeof(struct perf_event_attr));

	pe.type = PERF_TYPE_RAW;
	pe.size = sizeof(struct perf_event_attr);
	pe.config = config;
	pe.config1 = config1;
	if (config == ALL_STORES)
		pe.sample_period = inst_sample_period;
	else
		pe.sample_period = get_sample_period(0);
	pe.sample_type = PERF_SAMPLE_IP | PERF_SAMPLE_TID | PERF_SAMPLE_ADDR;
	pe.disabled = 0;
	pe.exclude_kernel = 1;
	pe.exclude_hv = 1;
	pe.exclude_callchain_kernel = 1;
	pe.exclude_callchain_user = 1;
	pe.precise_ip = 1;
	pe.enable_on_exec = 1;
	//pe.inherit = 1;
	//pe.flags = PERF_FLAG_FOLLOW_CHILD;
	//pe.freq = 0; // guess: default is 0


	int event_fd = perf_event_open(&pe, __pid, __cpu, -1, 0);
	if (event_fd <= 0) {
		printf("event_fd made by perf_event_open(): %d\n", event_fd);
		perror("Error opening perf event");
		exit(EXIT_FAILURE);
	}
	printf("event_fd: %d (config: %lx)\n", event_fd, config);
	return event_fd;
}

int pebs_init(int *event_list, struct perf_event_mmap_page **record_addr_list, pid_t pid) {
	int event;
	//int *event_list = (int*)malloc(sizeof(int) * N_EVENTS);
	//memset(event_list, 0, sizeof(int) * N_EVENTS);

    size_t mmap_size = sysconf(_SC_PAGESIZE) * PERF_PAGES;
	for (event=0; event<N_EVENTS; event++) {
		if (get_pebs_event((events)event) == N_EVENTS) {
			event_list[event] = -1;
			continue;
		}
		event_list[event] = pebs_open(get_pebs_event((events)event), 0, event, pid);
		printf("mmap: %ld, %d, %d, %d, %d\n", mmap_size, PROT_READ|PROT_WRITE, MAP_SHARED, event_list[event], 0);
		record_addr_list[event] = (perf_event_mmap_page*)mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, event_list[event], 0);
		if (record_addr_list[event] == (void *)-1)
		{
			perror("asd\n");

		}
	}

	for (event=0; event<N_EVENTS; event++) {
		printf("perf address[%d]: %p\n", event, record_addr_list[event]);
	}
	return 0;
}

struct arg_sampler {
	int *event_list;
	struct perf_event_mmap_page **record_addr_list;
	//buf_t* promoQueue;
};

#define PGDIR_SHIFT 39 // TODO: need to confirm
static bool valid_va(unsigned long addr)
{
	if (!(addr >> (PGDIR_SHIFT + 9)) && addr != 0)
		return true;
	else
		return false;
}

void *sampler(void *arg) {
	struct arg_sampler *args = (struct arg_sampler*)arg;
	int *event_fd_list = args->event_list;
	struct perf_event_mmap_page **record_addr_list = args->record_addr_list;

	for (int i=0; i<N_EVENTS; i++) {
		printf("event %d, fd: %d, addr: %p\n", i, event_fd_list[i], record_addr_list[i]);
	}
	//int sleep_cnt = 0;

	// declaration for sample
	struct perf_event_mmap_page *p;
	char *pbuf;
	struct perf_event_header *ph;
	struct perf_sample* ps;
	uint64_t data_offset;

	uint64_t nr_dram=0, nr_rdram = 0, nr_nvm=0, nr_rnvm = 0, nr_write = 0, nr_throttled = 0, nr_lost = 0, nr_unknown = 0, nr_invalid = 0, nr_sampled = 0;
	uint64_t acc_nr_dram=0, acc_nr_rdram = 0, acc_nr_nvm=0, acc_nr_rnvm = 0, acc_nr_write = 0;
	uint64_t cur_nr_dram=0, cur_nr_rdram = 0, cur_nr_nvm=0, cur_nr_rnvm = 0, cur_nr_write = 0;

	struct pebs_va pva;

	while (!pd.sampler_stop) {
		/* initialization & update */

		for (int i=0; i<N_EVENTS; i++) {
			/* Reading PEBS sample */
			if (get_pebs_event((enum events) i) == N_EVENTS)
				continue;

			p = record_addr_list[i];
			data_offset = p->data_offset; // segfault
			pbuf = (char*)p + data_offset; 
			__sync_synchronize();
			if(p->data_head == p->data_tail) {
				usleep(2000);
				//printf("p->data_head == p->data_tail..\n");
				continue;
			}
			while (p->data_head != p->data_tail) {
				ph = (struct perf_event_header*)(pbuf + (p->data_tail % p->data_size));
				switch(ph->type) {
					case PERF_RECORD_SAMPLE:
						ps = (struct perf_sample*)ph;
						if (ps != NULL) {

							if (!valid_va(ps->addr)) {
								nr_invalid++;
								break;
							}

							if (i == DRAMREAD) {
								nr_dram++;
								acc_nr_dram++;
								cur_nr_dram++;
							} else if (i == R_DRAMREAD) {
								nr_rdram++;
								acc_nr_rdram++;
								cur_nr_rdram++;
							} else if (i == NVMREAD) {
								nr_nvm++;
								acc_nr_nvm++;
								cur_nr_nvm++;
							} else if (i == R_NVMREAD) {
								nr_rnvm++;
								acc_nr_rnvm++;
								cur_nr_rnvm++;
							} else {
								nr_write++;
								acc_nr_write++;
								cur_nr_write++;
							}

							pva = {1, -1, i, 0};

							//pthread_mutex_lock(&pd.mutex);
							//pd.pebs_list.push_back({(void *)ps->addr, pva});
							//pthread_mutex_unlock(&pd.mutex);
							nr_sampled++;

							if (nr_sampled % 1000 == 0) {
								unsigned long nr_reads = nr_dram + nr_rdram + nr_nvm + nr_rnvm;
								unsigned long acc_nr_reads = nr_sampled - acc_nr_write;
								if (nr_reads)
									printf("nr_sampled: %lu, nr_dram: %lu (%lu), nr_rdram: %lu (%lu), nr_nvm: %lu (%lu), nr_rnvm: %lu (%lu), nr_write: %lu (%lu), nr_throttled: %lu, nr_lost: %lu nr_unknown: %lu, nr_invalid: %lu\n", nr_sampled, nr_dram, nr_dram * 10000 / nr_reads, nr_rdram, nr_rdram * 10000 / nr_reads, nr_nvm, nr_nvm * 10000 / nr_reads, nr_rnvm, nr_rnvm * 10000 / nr_reads, nr_write, nr_write * 10000 / 100000, nr_throttled, nr_lost, nr_unknown, nr_invalid);
								
								if (acc_nr_reads)
									printf("nr_sampled: %lu, acc_nr_dram: %lu (%lu), acc_nr_rdram: %lu (%lu), acc_nr_nvm: %lu (%lu), acc_nr_rnvm: %lu (%lu), acc_nr_write: %lu (%lu)\n", nr_sampled, acc_nr_dram, acc_nr_dram * 10000 / acc_nr_reads, acc_nr_rdram, acc_nr_rdram * 10000 / acc_nr_reads, acc_nr_nvm, acc_nr_nvm * 10000 / acc_nr_reads, acc_nr_rnvm, acc_nr_rnvm * 10000 / acc_nr_reads, acc_nr_write, acc_nr_write * 10000 / nr_sampled);
								nr_dram = nr_rdram = nr_nvm = nr_rnvm = nr_write = 0;
							}
						} else {
							printf("ps is NULL\n");
						}
						break;
					case PERF_RECORD_THROTTLE:
					case PERF_RECORD_UNTHROTTLE:
						nr_throttled++;
						break;
					case PERF_RECORD_LOST_SAMPLES:
						nr_lost++;
					default:
						nr_unknown++;
						break;
				}
				p->data_tail = p->data_tail + ph->size;
			}
		}

		usleep(2000);
	}

	// close fd
	for (int i=0; i<N_EVENTS; i++) {
		if (event_fd_list[i] != -1)
			close(event_fd_list[i]);
	}

	// sampling end. 
	free(event_fd_list); 
	free(record_addr_list);
	free(args);

	return NULL;
}

/* pid는 target process에 붙어있는 memlib가 전달하도록 하기 */
void sampler_init(int pid) {
	printf("sampler_init start!\n");
	// pebs event list initialization
	int *event_list = (int*)malloc(sizeof(int) * N_EVENTS); // container of fd
	memset(event_list, 0, sizeof(int) * N_EVENTS);
	printf("created event_list ptr: %p\n", event_list);

	pthread_mutex_init(&pd.mutex, NULL);

	struct perf_event_mmap_page **record_addr_list = (struct perf_event_mmap_page**)malloc(sizeof(struct perf_event_mmap_page*) * N_EVENTS);
	memset(record_addr_list, 0, sizeof(struct perf_event_mmap_page*) * N_EVENTS);

	pebs_init(event_list, record_addr_list, pid); // perf_event_open

	struct arg_sampler* sampler_args = (struct arg_sampler*)malloc(sizeof(struct arg_sampler));
	sampler_args->event_list = event_list;
	sampler_args->record_addr_list = record_addr_list;
	printf("sampler_args->event_list: %p\n", sampler_args->event_list);

	// sampler thread creation
	int ret;
	ret = pthread_create(&pd.sampler_thread, NULL, sampler, (void*)sampler_args);
	if (ret != 0) {
		perror("pthread create");
		exit(EXIT_FAILURE);
	}
	return;
}

void sampler_destroy(void) {
	pd.sampler_stop = 1;
    pthread_join(pd.sampler_thread, NULL);
	printf("sampler thread joined\n");

	return;
}

int sampler_get_sample(list<std::pair<void *, struct pebs_va>> &out_list) {
	int ret;
	pthread_mutex_lock(&pd.mutex);
	ret = pd.pebs_list.size();
	out_list.splice(out_list.end(), pd.pebs_list);
	pthread_mutex_unlock(&pd.mutex);
	return ret;
}
