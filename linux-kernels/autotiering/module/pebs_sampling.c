/*
 * memory access sampling for hugepage-aware tiered memory management.
 */
#include <linux/kthread.h>
#include <linux/memcontrol.h>
#include <linux/mempolicy.h>
#include <linux/sched.h>
#include <linux/perf_event.h>
#include <linux/delay.h>
#include <linux/sched/cputime.h>

#include "/home/koo/src/IDT/linux-kernels/linux-5.3.18-autotiering/kernel/events/internal.h"
#include <linux/kmig.h>

#include "page_tracker.h"

//MODULE_LICENSE("GPL");

unsigned int kmig_sample_period = 199;
unsigned int kmig_inst_sample_period = 100007;
//unsigned int kmig_thres_hot = 1;
//unsigned int kmig_cooling_period = 2000000;
//unsigned int kmig_adaptation_period = 100000;
//unsigned int kmig_split_period = 2; /* used to shift the wss of memcg */
unsigned int ksampled_min_sample_ratio = 50; // 50%
unsigned int ksampled_max_sample_ratio = 10; // 10%
//unsigned int kmig_demotion_period_in_ms = 500;
//unsigned int kmig_promotion_period_in_ms = 500;
//unsigned int kmig_thres_split = 2; 
//unsigned int kmig_nowarm = 0; // enabled: 0, disabled: 1
//unsigned int kmig_util_weight = 10; // no impact (unused)
unsigned int kmig_mode = 1;
//unsigned int kmig_gamma = 4; /* 0.4; divide this by 10 */
bool kmig_cxl_mode = false;
//bool kmig_skip_cooling = true;
//unsigned int kmig_thres_cooling_alloc = 256 * 1024 * 10; // unit: 4KiB, default: 10GB
unsigned int ksampled_soft_cpu_quota = 50; // 5 %
//unsigned int ksampled_soft_cpu_quota = 0; // 0 %

extern struct page_tracker_t g_ptracker; 

struct task_struct *access_sampling = NULL;
struct perf_event ***mem_event;



static bool valid_va(unsigned long addr)
{
	if (!(addr >> (PGDIR_SHIFT + 9)) && addr != 0)
		return true;
	else
		return false;
}

static __u64 get_pebs_event(enum events e)
{
	switch (e) {
		case DRAMREAD:
			return DRAM_LLC_LOAD_MISS;
		case NVMREAD:
			if (!kmig_cxl_mode)
				return NVM_LLC_LOAD_MISS;
			else
				return N_KMIGEVENTS;
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
				return N_KMIGEVENTS;
		default:
			return N_KMIGEVENTS;
	}
}

static int __perf_event_open(__u64 config, __u64 config1, __u64 cpu,
		__u64 type, __u32 pid)
{
	struct perf_event_attr attr;
	struct file *file;
	int event_fd, __pid;

	memset(&attr, 0, sizeof(struct perf_event_attr));

	attr.type = PERF_TYPE_RAW;
	attr.size = sizeof(struct perf_event_attr);
	attr.config = config;
	attr.config1 = config1;
	if (config == ALL_STORES)
		attr.sample_period = kmig_inst_sample_period;
	else
		attr.sample_period = get_sample_period(0);
	attr.sample_type = PERF_SAMPLE_IP | PERF_SAMPLE_TID |  PERF_SAMPLE_ADDR;
	attr.disabled = 0;
	attr.exclude_kernel = 1;
	attr.exclude_hv = 1;
	attr.exclude_callchain_kernel = 1;
	attr.exclude_callchain_user = 1;
	attr.precise_ip = 1;
	attr.enable_on_exec = 1;
	attr.inherit = 1;
	//attr.flags = PERF_FLAG_FOLLOW_CHILD;

	if (pid == 0)
		__pid = -1;
	else
		__pid = pid;


	event_fd = kmig__perf_event_open(&attr, __pid, cpu, -1, 0);
	if (event_fd <= 0) {
		printk("[error kmig__perf_event_open failure] event_fd: %d\n", event_fd);
		return -1;
	}

	file = fget(event_fd);
	if (!file) {
		printk("invalid file\n");
		return -1;
	}
	mem_event[cpu][type] = fget(event_fd)->private_data;
	return 0;
}

static int pebs_init(pid_t pid, int node)
{
	int cpu, event;

	mem_event = kzalloc(sizeof(struct perf_event **) * CPUS_PER_SOCKET, GFP_KERNEL);
	for (cpu = 0; cpu < CPUS_PER_SOCKET; cpu++) {
		mem_event[cpu] = kzalloc(sizeof(struct perf_event *) * N_KMIGEVENTS, GFP_KERNEL);
	}

	printk("pebs_init pid %d, cpus_per_socket %d\n", pid, CPUS_PER_SOCKET);   
	for (cpu = 0; cpu < CPUS_PER_SOCKET; cpu++) {
		for (event = 0; event < N_KMIGEVENTS; event++) {
			if (get_pebs_event(event) == N_KMIGEVENTS) {
				mem_event[cpu][event] = NULL;
				continue;
			}

			if (__perf_event_open(get_pebs_event(event), 0, cpu, event, pid))
				return -1;
			if (kmig__perf_event_init(mem_event[cpu][event], BUFFER_SIZE))
				return -1;
		}
	}

	return 0;
}

static void pebs_disable(void)
{
	int cpu, event;

	printk("pebs disable\n");
	for (cpu = 0; cpu < CPUS_PER_SOCKET; cpu++) {
		for (event = 0; event < N_KMIGEVENTS; event++) {
			if (mem_event[cpu][event])
				perf_event_disable(mem_event[cpu][event]);
		}
	}
}

static void pebs_enable(void)
{
	int cpu, event;

	//printk("pebs enable\n");
	for (cpu = 0; cpu < CPUS_PER_SOCKET; cpu++) {
		for (event = 0; event < N_KMIGEVENTS; event++) {
			if (mem_event[cpu][event])
				perf_event_enable(mem_event[cpu][event]);
		}
	}
}

static void pebs_update_period(uint64_t value, uint64_t inst_value)
{
	int cpu, event;

	for (cpu = 0; cpu < CPUS_PER_SOCKET; cpu++) {
		for (event = 0; event < N_KMIGEVENTS; event++) {
			int ret;
			if (!mem_event[cpu][event])
				continue;

			switch (event) {
				case DRAMREAD:
				case NVMREAD:
				case R_DRAMREAD:
				case R_NVMREAD:
				case CXLREAD:
					ret = perf_event_period_direct(mem_event[cpu][event], value);
					break;
				case MEMWRITE:
					ret = perf_event_period_direct(mem_event[cpu][event], inst_value);
					break;
				default:
					ret = 0;
					break;
			}

			if (ret == -EINVAL)
				printk("failed to update sample period");
		}
	}
}

void pebs_get_period(uint64_t *read_period, uint64_t *write_period)
{
	int cpu, event;

	uint64_t ret_read = 0, ret_write = 0;
	int cnt_read = 0, cnt_write = 0;

	for (cpu = 0; cpu < CPUS_PER_SOCKET; cpu++) {
		for (event = 0; event < N_KMIGEVENTS; event++) {
			uint64_t ret;
			if (!mem_event[cpu][event])
				continue;

			switch (event) {
				case DRAMREAD:
				case NVMREAD:
				case R_DRAMREAD:
				case R_NVMREAD:
				case CXLREAD:
					ret_read += perf_event_get_period(mem_event[cpu][event]);
					cnt_read++;
					break;
				case MEMWRITE:
					ret_write += perf_event_get_period(mem_event[cpu][event]);
					cnt_write++;
					break;
				default:
					ret = 0;
					break;
			}

			if (ret == -EINVAL)
				printk("failed to get sample period");
		}
	}
	*read_period = cnt_read ? ret_read / cnt_read : 0;
	*write_period = cnt_write ? ret_write / cnt_write : 0;
}

static int ksamplingd(void *data)
{
	unsigned long long nr_sampled = 0, nr_dram = 0, nr_nvm = 0, nr_rdram = 0, nr_rnvm = 0, nr_write = 0, acc_nr_dram = 0, acc_nr_rdram = 0, acc_nr_nvm = 0, acc_nr_rnvm = 0, acc_nr_write = 0;
	unsigned long long nr_throttled = 0, nr_lost = 0, nr_unknown = 0, nr_copy_failed = 0;
	unsigned long long nr_skip = 0;

	/* used for calculating average cpu usage of ksampled */
	struct task_struct *t = current;
	/* a unit of cputime: permil (1/1000) */
	u64 total_runtime, exec_runtime, cputime = 0;
	u64 read_period = 0, write_period = 0;
	unsigned long total_cputime, elapsed_cputime, cur;
	/* used for periodic checks*/
	unsigned long cpucap_period = msecs_to_jiffies(15000); // 15s
	unsigned long sample_period = 0;
	unsigned long sample_inst_period = 0;
	/* report cpu/period stat */
	unsigned long trace_cputime, trace_period = msecs_to_jiffies(3000); // 3s
	unsigned long bw_cputime, bw_period = msecs_to_jiffies(1000);
	unsigned long long nr_accesses_per_sec;
	unsigned long trace_runtime, bw_runtime;
	/* for timeout */ 
	unsigned long sleep_timeout;

	/* for analytic purpose */
	unsigned long hr_dram = 0, hr_nvm = 0, hr_rdram = 0, hr_rnvm = 0, hr_write = 0;

	/* orig impl: see read_sum_exec_runtime() */
	trace_runtime = total_runtime = exec_runtime = bw_runtime = t->se.sum_exec_runtime;

	trace_cputime = total_cputime = elapsed_cputime = bw_cputime = jiffies;
	sleep_timeout = usecs_to_jiffies(2000);

	struct pebs_data *pdata;
	unsigned iter = 0;
	bool sample_occured;

	int nr_sampled_per_loop;

	struct list_head tmp_list;
	INIT_LIST_HEAD(&tmp_list);


	pebs_update_period(get_sample_period(sample_period),
			get_sample_inst_period(sample_inst_period));

	/* TODO implements per-CPU node ksamplingd by using pg_data_t */
	/* Currently uses a single CPU node(0) */
	/*
	const struct cpumask *cpumask = cpumask_of_node(0);
	if (!cpumask_empty(cpumask))
		do_set_cpus_allowed(access_sampling, cpumask);
	*/

	while (!kthread_should_stop()) {
		int cpu, event, cond = false;

		if (kmig_mode == KMIG_NO_MIG) {
			msleep_interruptible(10000);
			continue;
		}

		sample_occured = false;

		nr_sampled_per_loop = 0;

		//mutex_lock(&g_ptracker.pebs_mutex);
		for (cpu = 0; cpu < CPUS_PER_SOCKET; cpu++) {
			for (event = 0; event < N_KMIGEVENTS; event++) {
				do {
					struct ring_buffer *rb;
					struct perf_event_mmap_page *up;
					struct perf_event_header *ph;
					struct kmig_event *he;
					unsigned long pg_index, offset;
					int page_shift;
					__u64 head;

					if (!mem_event[cpu][event]) {
						//continue;
						break;
					}

					__sync_synchronize();

					rb = mem_event[cpu][event]->rb;
					if (!rb) {
						printk("event->rb is NULL\n");
						return -1;
					}
					/* perf_buffer is ring buffer */
					up = READ_ONCE(rb->user_page);
					head = READ_ONCE(up->data_head);
					if (head == up->data_tail) {
						if (cpu < 16)
							nr_skip++;
						//continue;
						break;
					}

					head -= up->data_tail;
					if (head > (BUFFER_SIZE * ksampled_max_sample_ratio / 100)) {
						cond = true;
					} else if (head < (BUFFER_SIZE * ksampled_min_sample_ratio / 100)) {
						cond = false;
					}

					/* read barrier */
					smp_rmb();

					page_shift = PAGE_SHIFT + page_order(rb);
					/* get address of a tail sample */
					offset = READ_ONCE(up->data_tail);
					pg_index = (offset >> page_shift) & (rb->nr_pages - 1);
					offset &= (1 << page_shift) - 1;

					ph = (void*)(rb->data_pages[pg_index] + offset);
					switch (ph->type) {
						case PERF_RECORD_SAMPLE:
							he = (struct kmig_event *)ph;
							if (!valid_va(he->addr)) {
								break;
							}

							//update_pginfo(he->pid, he->addr, event);
							//count_vm_event(KMIG_NR_SAMPLED);
							

							nr_sampled++;
							sample_occured = true;

							//do_logging_to_file(he, event, log_msg);

							if (event == DRAMREAD) {
								nr_dram++;
								acc_nr_dram++;
								hr_dram++;
							} else if (event == R_DRAMREAD) {
								nr_rdram++;
								acc_nr_rdram++;
								hr_rdram++;
							} else if (event == NVMREAD) {
								nr_nvm++;
								acc_nr_nvm++;
								hr_nvm++;
							} else if (event == R_NVMREAD) {
								nr_rnvm++;
								acc_nr_rnvm++;
								hr_rnvm++;
							} else {
								nr_write++;
								acc_nr_write++;
								hr_write++;
							}

							pdata = kmem_cache_alloc(g_ptracker.pebs_slab, GFP_KERNEL);
							nr_sampled_per_loop++;

							if (!pdata)
								continue;

							pdata->va = he->addr;
							pdata->node = -1;
							pdata->type = (int)event;
							pdata->iter = iter;
							INIT_LIST_HEAD(&pdata->list);

							//list_add_tail(&pdata->list, &g_ptracker.pebs_list);
							list_add_tail(&pdata->list, &tmp_list);

							break;
						case PERF_RECORD_THROTTLE:
						case PERF_RECORD_UNTHROTTLE:
							nr_throttled++;
							break;
						case PERF_RECORD_LOST_SAMPLES:
							nr_lost++;
							break;
						default:
							nr_unknown++;
							break;
					}
					if (nr_sampled % 100000 == 0) {
						unsigned long nr_reads = nr_dram + nr_rdram + nr_nvm + nr_rnvm;
						unsigned long acc_nr_reads = nr_sampled - acc_nr_write;
						if (nr_reads)
							trace_printk("nr_sampled: %llu, nr_dram: %llu (%llu), nr_rdram: %llu (%llu), nr_nvm: %llu (%llu), nr_rnvm: %llu (%llu), nr_write: %llu (%llu), nr_copy_failed: %llu, nr_throttled: %llu \n", nr_sampled, nr_dram, nr_dram * 10000 / nr_reads, nr_rdram, nr_rdram * 10000 / nr_reads, nr_nvm, nr_nvm * 10000 / nr_reads, nr_rnvm, nr_rnvm * 10000 / nr_reads, nr_write, nr_write * 10000 / 100000, nr_copy_failed, nr_throttled);
						if (acc_nr_reads)
							trace_printk("nr_sampled: %llu, acc_nr_dram: %llu (%llu), acc_nr_rdram: %llu (%llu), acc_nr_nvm: %llu (%llu), acc_nr_rnvm: %llu (%llu), acc_nr_write: %llu (%llu), nr_copy_failed: %llu, nr_throttled: %llu \n", nr_sampled, acc_nr_dram, acc_nr_dram * 10000 / acc_nr_reads, acc_nr_rdram, acc_nr_rdram * 10000 / acc_nr_reads, acc_nr_nvm, acc_nr_nvm * 10000 / acc_nr_reads, acc_nr_rnvm, acc_nr_rnvm * 10000 / acc_nr_reads, acc_nr_write, acc_nr_write * 10000 / nr_sampled, nr_copy_failed, nr_throttled);
						//trace_printk("nr_sampled: %llu, nr_dram: %llu, nr_rdram: %llu, nr_nvm: %llu, nr_rnvm: %llu, nr_write: %llu, nr_copy_failed: %llu, nr_throttled: %llu \n", nr_sampled, nr_dram, nr_rdram, nr_nvm, nr_rnvm, nr_write, nr_copy_failed,	nr_throttled);
						//trace_printk("acc_nr_dram: %llu, acc_nr_rdram: %llu, acc_nr_nvm: %llu, acc_nr_rnvm: %llu, acc_nr_write: %llu\n", acc_nr_dram, acc_nr_rdram, acc_nr_nvm, acc_nr_rnvm, acc_nr_write);
						nr_dram = 0;
						nr_rdram = 0;
						nr_nvm = 0;
						nr_rnvm = 0;
						nr_write = 0;
					}
					/* read, write barrier */
					smp_mb();
					WRITE_ONCE(up->data_tail, up->data_tail + ph->size);
				} while (cond);
			}
		}

		if (sample_occured) {
			pebs_get_period(&read_period, &write_period);
			iter++;
		}

		cur = jiffies;
		if ((cur - bw_cputime) >= bw_period) {
			unsigned long long nr_read_period = 0, nr_write_period = 0;
			u64 cur_runtime = t->se.sum_exec_runtime;
			bw_runtime = cur_runtime - bw_runtime; //ns
			bw_cputime = jiffies_to_usecs(cur - bw_cputime); //us

			pebs_get_period(&read_period, &write_period);

			nr_read_period = hr_dram + hr_rdram + hr_nvm + hr_rnvm;
			nr_write_period = hr_write;

			nr_read_period *= read_period;
			nr_write_period *= read_period;

			//nr_accesses_per_sec = (nr_read_period + nr_write_period) / (bw_runtime / 1000 / 1000); //ms
			nr_accesses_per_sec = nr_read_period + nr_write_period;
																								   

			printk("read_period: %llu, write_period: %llu, nr_read_period: %llu, nr_write_period: %llu, nr_accesses_per_sec: %llu\n", read_period, write_period, nr_read_period, nr_write_period, nr_accesses_per_sec);

			hr_dram = hr_rdram = hr_nvm = hr_rnvm = hr_write = 0;

			bw_cputime = cur;
			bw_runtime = cur_runtime;
			//pebs_enable();
		}


		mutex_lock(&g_ptracker.pebs_mutex);
		//spin_lock(&g_ptracker.pebs_lock);
		list_splice_tail_init(&tmp_list, &g_ptracker.pebs_list);
		/*
		if (need_to_wakeup(RB_PEBS)) {
			smp_mb();
			wakeup_user();
		}
		*/
		//spin_unlock(&g_ptracker.pebs_lock);
		mutex_unlock(&g_ptracker.pebs_mutex);
		/* if ksampled_soft_cpu_quota is zero, disable dynamic pebs feature */
		if (!ksampled_soft_cpu_quota)
			continue;

		/* sleep */
		schedule_timeout_interruptible(sleep_timeout);

		/* check elasped time */
		cur = jiffies;
		if ((cur - elapsed_cputime) >= cpucap_period) {
			u64 cur_runtime = t->se.sum_exec_runtime;
			exec_runtime = cur_runtime - exec_runtime; //ns
			elapsed_cputime = jiffies_to_usecs(cur - elapsed_cputime); //us
			if (!cputime) {
				u64 cur_cputime = div64_u64(exec_runtime, elapsed_cputime);
				// EMA with the scale factor (0.2)
				cputime = ((cur_cputime << 3) + (cputime << 1)) / 10;
			} else
				cputime = div64_u64(exec_runtime, elapsed_cputime);

			/* to prevent frequent updates, allow for a slight variation of +/- 0.5% */
			if (cputime > (ksampled_soft_cpu_quota + 5) &&
					sample_period != pcount) {
				/* need to increase the sample period */
				/* only increase by 1 */
				unsigned long tmp1 = sample_period, tmp2 = sample_inst_period;
				increase_sample_period(&sample_period, &sample_inst_period);
				if (tmp1 != sample_period || tmp2 != sample_inst_period)
					pebs_update_period(get_sample_period(sample_period),
							get_sample_inst_period(sample_inst_period));
			} else if (cputime < (ksampled_soft_cpu_quota - 5) && sample_period) {
				unsigned long tmp1 = sample_period, tmp2 = sample_inst_period;
				decrease_sample_period(&sample_period, &sample_inst_period);
				if (tmp1 != sample_period || tmp2 != sample_inst_period)
					pebs_update_period(get_sample_period(sample_period),
							get_sample_inst_period(sample_inst_period));
			}
			/* does it need to prevent ping-pong behavior? */

			elapsed_cputime = cur;
			exec_runtime = cur_runtime;
		}

		/* This is used for reporting the sample period and cputime */
		if (cur - trace_cputime >= trace_period) {
			unsigned long hr = 0;
			u64 cur_runtime = t->se.sum_exec_runtime;
			trace_runtime = cur_runtime - trace_runtime;
			trace_cputime = jiffies_to_usecs(cur - trace_cputime);
			trace_cputime = div64_u64(trace_runtime, trace_cputime);

			if (hr_dram + hr_nvm == 0)
				hr = 0;
			else
				hr = hr_dram * 10000 / (hr_dram + hr_rdram + hr_nvm + hr_rnvm);
			trace_printk("sample_period: %lu || cputime: %lu  || hit ratio: %lu\n",
					get_sample_period(sample_period), trace_cputime, hr);

			hr_dram = hr_rdram = hr_nvm = hr_rnvm = 0;
			trace_cputime = cur;
			trace_runtime = cur_runtime;
		}
	}

	total_runtime = (t->se.sum_exec_runtime) - total_runtime; // ns
	total_cputime = jiffies_to_usecs(jiffies - total_cputime); // us

	printk("nr_sampled: %llu, nr_copy_failed: %llu, nr_throttled: %llu, nr_lost: %llu\n", nr_sampled, nr_copy_failed, nr_throttled, nr_lost);
	printk("total runtime: %llu ns, total cputime: %lu us, cpu usage: %llu\n",
			total_runtime, total_cputime, (total_runtime) / total_cputime);

	unsigned long nr_reads = nr_sampled - acc_nr_write;

	if (nr_reads)
		printk("nr_sampled: %llu, acc_nr_dram: %llu (%llu), acc_nr_rdram: %llu (%llu), acc_nr_nvm: %llu (%llu), acc_nr_rnvm: %llu (%llu), acc_nr_write: %llu (%llu), nr_copy_failed: %llu, nr_throttled: %llu \n", nr_sampled, acc_nr_dram, acc_nr_dram * 10000 / nr_reads, acc_nr_rdram, acc_nr_rdram * 10000 / nr_reads, acc_nr_nvm, acc_nr_nvm * 10000 / nr_reads, acc_nr_rnvm, acc_nr_rnvm * 10000 / nr_reads, acc_nr_write, acc_nr_write * 10000 / nr_sampled, nr_copy_failed, nr_throttled);

	up(&g_ptracker.pebs_sem);

	return 0;
}

static int ksamplingd_run(void)
{
	int err = 0;

	if (!access_sampling) {
		//access_sampling = kthread_run(ksamplingd, NULL, "ksamplingd");
		access_sampling = kthread_create_on_node(ksamplingd, NULL, 0, "ksamplingd");
		if (IS_ERR(access_sampling)) {
			err = PTR_ERR(access_sampling);
			access_sampling = NULL;
		} else {

			const struct cpumask *cpumask = cpumask_of_node(0);
			if (!cpumask_empty(cpumask))
				kthread_bind_mask(access_sampling, cpumask);
			//kthread_bind(access_sampling, 0);
			wake_up_process(access_sampling);
		}
		
	}
	return err;
}

int ksamplingd_init(pid_t pid, int node)
{
	int ret;

	if (access_sampling)
		return 0;

	//open_logging_file();

	ret = pebs_init(pid, node);
	if (ret) {
		printk("kmig__perf_event_init failure... ERROR:%d\n", ret);
		return 0;
	}
	//pebs_enable();

	return ksamplingd_run();
}

void ksamplingd_exit(void)
{
	if (access_sampling) {
		kthread_stop(access_sampling);
		access_sampling = NULL;
		//close_logging_file();
	}
	pebs_disable();
}
