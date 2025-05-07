#ifndef __MY_RB_H__
#define __MY_RB_H__

#define MAX_NR_RB 5UL

#define RB_THRESHOLD_ALLOC 1024
#define RB_THRESHOLD_DAMON 1
#define RB_THRESHOLD_PEBS 1024
#define RB_BUF_SIZE (3UL * 1024 * 1024)
#define RB_HEADER_SIZE 4096

#define RB_FULL(rb) (((rb->head + 1) % rb->size) == rb->tail)
#define RB_EMPTY(rb) (rb->head == rb->tail)
#define RB_LEN(rb) ((rb->head + rb->size - rb->tail)%rb->size)
#define RB_ADD(rb,nr) (rb->head = (rb->head + nr)%rb->size)
#define RB_DEL(rb,nr) (rb->tail = (rb->tail + nr)%rb->size)

#define RB_ALLOC 0x0000
#define RB_PEBS 0x0001
#define RB_DAMON 0x0002

#define PEBS_DRAMREAD 0
#define PEBS_R_DRAMREAD 1
#define PEBS_NVMREAD 2
#define PEBS_R_NVMREAD 3
#define PEBS_MEMWRITE 4
#define PEBS_TLB_MISS_LOADS 5
#define PEBS_TLB_MISS_STORES 6
#define PEBS CXLREAD 7
#define PEBS_NEVENTS 8

#define RB_TYPE_TO_FLAG(type) (1 << (type + 1))

struct rb_reply_t {
	int type;
	int nr_items;
};


static inline int get_rb_idx(int type) {
	int ret = 0;
	switch(type) {
		case RB_ALLOC:
			ret = 0;
			break;
		case RB_DAMON:
			ret = 1;
			break;
		default:
			ret = MAX_NR_RB - 1;
	}

	return ret;
}

struct rb_data_alloc_t {
	unsigned long va;
	int node;
	int last_accessed;
	unsigned int iter;
};

struct rb_data_damon_t {
	unsigned long va;
	unsigned int nr_pages;
	unsigned int nr_accesses;
	unsigned int iter;
};

struct rb_data_pebs_t {
	unsigned long va;
	int node;
	int type;
	unsigned int iter;
};

struct rb_data_t {
	union {
		struct rb_data_alloc_t rb_alloc;
		struct rb_data_damon_t rb_damon;
		struct rb_data_pebs_t rb_pebs;
	} data;

};

struct rb_head_t {
	//char *buf;
	int head;
	int tail;
	int size;
};

#endif
