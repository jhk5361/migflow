// hook_pages.c
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/page_idle.h>
#include <linux/pagemap.h>
#include <linux/rmap.h>
//#include <linux/damon.h>
#include <linux/sched/mm.h>
//#include <linux/pagewalk.h>
#include <linux/hugetlb.h>
#include <linux/mmu_notifier.h>
#include <linux/poll.h>
#include <linux/mman.h>
#include <linux/highmem.h>
#include <linux/io.h>
#include <linux/kmig.h>
#include <linux/rbtree.h>
#include "page_tracker.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("JHK");
MODULE_DESCRIPTION("Device driver to hook alloc_pages and free_pages for a specific PID");

static int init_capacity = 16;
module_param(init_capacity, int, 0644);
MODULE_PARM_DESC(init_capacity, "An integer module parameter");

static int kpebs = 1;
module_param(kpebs, int, 0644);
MODULE_PARM_DESC(kpebs, "Enable the kernel PEBS functionality");

#define DEVICE_NAME "page_tracker"
#define CLASS_NAME "page_tracker"
#define IOCTL_SET_PID _IOW('a', 1, int)
#define IOCTL_SET_HOT_BUF _IOW('a', 2, int)
#define IOCTL_SET_THRESHOLD_WAKEUP _IOW('a', 3, int)
#define IOCTL_MOVE_RB_TAIL _IOW('a', 4, void *)
#define IOCTL_GET_RB_STATUS _IOR('a', 5, int)
#define IOCTL_GET_PERIOD _IOR('a', 6, struct pebs_period)

#define DAMON_SAMPLING (50UL * 1000)
#define DAMON_AGGREGATE (2UL * 1000 * 1000)
#define DAMON_UPDATE (20UL * 1000 * 1000)
#define DAMON_MIN_REGIONS 10000
#define DAMON_MAX_REGIONS 20000

static dev_t dev_num;
static struct class *page_tracker_class = NULL;
static struct cdev page_tracker_cdev;

struct page_tracker_t g_ptracker;

static inline void print_ptracker(void) {
	printk(KERN_INFO "nr_alloc: %lu, nr_accessed: %lu, nr_copied: %lu, max_rb_size: %d, max_young_list_size: %d, max_old_list_size: %d\n", g_ptracker.ptstat.nr_alloc, g_ptracker.ptstat.nr_accessed, g_ptracker.ptstat.nr_copied, g_ptracker.ptstat.max_rb_size, g_ptracker.ptstat.max_young_list_size, g_ptracker.ptstat.max_old_list_size);
}


static void init_anony_list(struct anony_list *list, int capacity) {
	INIT_LIST_HEAD(&list->head);
	list->capacity = capacity;
	list->length = 0;
}

static void add_anony_entry(struct anony_list *list, struct anony_data *new_data) {
    list_add_tail(&new_data->list, &list->head);
    list->length++;
}

static void move_anony_entry(struct anony_list *prev_list, struct anony_list *new_list, struct anony_data *adata) {
	list_move_tail(&adata->list, &new_list->head);
	prev_list->length--;
	new_list->length++;
}

static void remove_anony_entry(struct anony_list *list, struct anony_data *data) {
    list_del(&data->list);
    list->length--;

	//kmem_cache_free(g_ptracker.anony_slab, data);
}

static void splice_anony_list(struct anony_list *list1, struct anony_list *list2) {
	list_splice_tail_init(&list1->head, &list2->head);
	list2->length += list1->length;
	list1->length = 0;

}

static void print_anony_list(struct anony_list *list) {
	struct list_head *pos;
	struct anony_data *adata;
	list_for_each(pos, &list->head) {
		adata = list_entry(pos, struct anony_data, list);
        printk(KERN_INFO "va = %08lx last_accessed: %d\n", adata->va, adata->last_accessed);
	}
}

static void destroy_anony_list(struct anony_list *list) {
	struct list_head *pos, *q;
	struct anony_data *adata;
	spin_lock(&g_ptracker.anony_lock);
	list_for_each_safe(pos, q, &list->head) {
		adata = list_entry(pos, struct anony_data, list);
		remove_anony_entry(list, adata);
		kmem_cache_free(g_ptracker.anony_slab, adata);
	}
	spin_unlock(&g_ptracker.anony_lock);
}

static void init_ptracker(void) {
	int i;
	int thresholds[MAX_NR_RB] = {RB_THRESHOLD_ALLOC, RB_THRESHOLD_DAMON, RB_THRESHOLD_PEBS, 0,};
	struct rb_head_t *rb;
	struct rb_data_t *rb_buf;
	g_ptracker.target_pid = -1;
	g_ptracker.anony_slab = kmem_cache_create("anony cache", sizeof(struct anony_data), 0, SLAB_HWCACHE_ALIGN, NULL);
	g_ptracker.pebs_slab = kmem_cache_create("anony cache", sizeof(struct pebs_data), 0, SLAB_HWCACHE_ALIGN, NULL);
	init_anony_list(&g_ptracker.young_list, init_capacity/2);
	init_anony_list(&g_ptracker.old_list, init_capacity/2);
	INIT_LIST_HEAD(&g_ptracker.pebs_list);



	/*
	g_ptracker.total_rb = (char *)kmalloc(MAX_NR_RB * (RB_HEADER_SIZE + RB_BUF_SIZE), GFP_KERNEL);
	if (!g_ptracker.total_rb) {
		printk(KERN_ERR "Failed to allocate kernel buffer\n");
	}
	printk(KERN_INFO "Success to allocate ring buffer, size: %lu\n", MAX_NR_RB * (RB_HEADER_SIZE + RB_BUF_SIZE));
	*/

	for (i = 0; i < MAX_NR_RB; i++) {
		rb = (struct rb_head_t *)kmalloc(RB_HEADER_SIZE + RB_BUF_SIZE, GFP_KERNEL);
		if (!rb) {
			printk(KERN_ERR "Failed to allocate kernel buffer\n");
		}
		//rb = (struct rb_head_t *)(g_ptracker.total_rb + (i * (RB_HEADER_SIZE + RB_BUF_SIZE)));
		rb->head = rb->tail = 0;
		rb->size = RB_BUF_SIZE/sizeof(struct rb_data_t);
		rb_buf = (struct rb_data_t *)((char *)rb + RB_HEADER_SIZE);

		g_ptracker.rb[i] = rb;
		g_ptracker.rb_buf[i] = rb_buf;
		g_ptracker.threshold_wakeup[i] = thresholds[i];
	}
	g_ptracker.cur_rb_idx = 0;


	//g_ptracker.rb->head = g_ptracker.rb->tail = 0;
	//g_ptracker.rb->size = RB_BUF_SIZE/sizeof(struct rb_data_t);
	//g_ptracker.rb_buf = (struct rb_data_t *)((char *)g_ptracker.rb + RB_HEADER_SIZE);
	init_waitqueue_head(&g_ptracker.poll_wq);

	//printk(KERN_INFO "Ring buffer, data_size : %lu, nr_items: %d threshold: %d\n", sizeof(struct rb_data_t), g_ptracker.rb->size, g_ptracker.rb->size * g_ptracker.threshold_wakeup_user / 100);
	//printk(KERN_INFO "Ring buffer, data_size : %lu, nr_items: %d threshold: %d\n", sizeof(struct rb_data_t), g_ptracker.rb->size, g_ptracker.threshold_wakeup_user);


	g_ptracker.kanonyd = NULL;

	spin_lock_init(&g_ptracker.anony_lock);
	spin_lock_init(&g_ptracker.damon_lock);
	spin_lock_init(&g_ptracker.pebs_lock);
	mutex_init(&g_ptracker.pebs_mutex);
	spin_lock_init(&g_ptracker.g_lock);
	sema_init(&g_ptracker.sem, 0);
	sema_init(&g_ptracker.pid_sem, 0);
	sema_init(&g_ptracker.pebs_sem, 0);

	g_ptracker.target_pidp = NULL;
	g_ptracker.dctx = NULL;
}

static void destroy_ptracker(void) {
	int i;
	g_ptracker.target_pid = -1;
	//g_ptracker.rb->head = g_ptracker.rb->tail = 0;
	for (i = 0; i < MAX_NR_RB; i++) {
		if (g_ptracker.rb[i])
			kfree(g_ptracker.rb[i]);
	}
	//if (g_ptracker.total_rb)
		//kfree(g_ptracker.total_rb);
	destroy_anony_list(&g_ptracker.young_list);
	destroy_anony_list(&g_ptracker.old_list);
	kmem_cache_destroy(g_ptracker.anony_slab);
}

static inline int __copy_anony_to_rb(struct rb_head_t *rb, struct rb_data_t *rb_buf, struct anony_data *adata) {
	if (RB_FULL(rb)) {
		return -1;
	}

	rb_buf[rb->head].data.rb_alloc.va = adata->va;
	rb_buf[rb->head].data.rb_alloc.node = adata->node;
	rb_buf[rb->head].data.rb_alloc.last_accessed = adata->last_accessed;
	rb_buf[rb->head].data.rb_alloc.iter = 0;

	RB_ADD(rb, 1);

	return 0;
}

static inline int __copy_damon_to_rb(struct rb_head_t *rb, struct rb_data_t *rb_buf, struct damon_data *ddata) {
	if (RB_FULL(rb)) {
		return -1;
	}

	rb_buf[rb->head].data.rb_damon.va = ddata->va;
	rb_buf[rb->head].data.rb_damon.nr_pages = ddata->nr_pages;
	rb_buf[rb->head].data.rb_damon.nr_accesses = ddata->nr_accesses;
	rb_buf[rb->head].data.rb_damon.iter = ddata->iter;


	//pr_info("head: %d (%lu), va: %lu, len: %d, nr_accesses: %d\n", rb->head, (unsigned long) (rb_buf + rb->head), rb_buf[rb->head].data.rb_damon.va, rb_buf[rb->head].data.rb_damon.len, rb_buf[rb->head].data.rb_damon.nr_accesses);

	RB_ADD(rb, 1);

	return 0;
}

static inline int __copy_pebs_to_rb(struct rb_head_t *rb, struct rb_data_t *rb_buf, struct pebs_data *pdata) {
	if (RB_FULL(rb)) {
		return -1;
	}

	rb_buf[rb->head].data.rb_pebs.va = pdata->va/PAGE_SIZE * PAGE_SIZE;
	rb_buf[rb->head].data.rb_pebs.node = pdata->node;
	rb_buf[rb->head].data.rb_pebs.type = pdata->type;
	rb_buf[rb->head].data.rb_pebs.iter = pdata->iter;


	//pr_info("head: %d (%lu), va: %lu, len: %d, nr_accesses: %d\n", rb->head, (unsigned long) (rb_buf + rb->head), rb_buf[rb->head].data.rb_damon.va, rb_buf[rb->head].data.rb_damon.len, rb_buf[rb->head].data.rb_damon.nr_accesses);

	RB_ADD(rb, 1);

	return 0;
}

int copy_to_rb(int type, void *data) {
	int ret = -1;

	struct rb_head_t *rb = g_ptracker.rb[type];
	struct rb_data_t *rb_buf = g_ptracker.rb_buf[type];

	switch(type) {
		case RB_ALLOC:
			ret = __copy_anony_to_rb(rb, rb_buf, (struct anony_data *)data);
			break;
		case RB_DAMON:
			ret = __copy_damon_to_rb(rb, rb_buf, (struct damon_data *)data);
			break;
		case RB_PEBS:
			ret = __copy_pebs_to_rb(rb, rb_buf, (struct pebs_data *)data);
			break;
		default:
			ret = -1;
	}

	return ret;
}

bool need_to_wakeup(int type) {
	int len = RB_LEN(g_ptracker.rb[type]);
	//if (type == RB_PEBS)
	//	pr_info("need_to_wakeup type: %d, len: %d, threshold: %d\n", type, len, g_ptracker.threshold_wakeup[type]);
	//if (len > g_ptracker.rb->size * g_ptracker.threshold_wakeup_user / 100)
	if (len > g_ptracker.threshold_wakeup[type])
		return true;
	return false;

}

void wakeup_user(void) {
	//printk(KERN_INFO "wakeup!!!\n");
	wake_up_interruptible(&g_ptracker.poll_wq);
}

// Handler for do_anonymous_page
static int do_page_handler(struct kprobe *p, struct pt_regs *regs) {
    struct task_struct *task = current;
    if (task->tgid == g_ptracker.target_pid) {
		struct vm_fault *vmf = (struct vm_fault *)regs->di;
        unsigned long address = vmf->address;
        //struct vm_area_struct *vma = (struct vm_area_struct *)vmf->vma;
        //printk(KERN_INFO "do_anonymous_page: pid = %d, address = %08lx, vma start = %08lx, vma end = %08lx\n", task->pid, address, vma->vm_start, vma->vm_end);

		gfp_t alloc_mask = GFP_KERNEL;
		struct anony_data *new_data = kmem_cache_alloc(g_ptracker.anony_slab, alloc_mask);

		if (!new_data)
			return 0;

		new_data->va = address;
		new_data->last_accessed = false;
		new_data->node = -1;

		spin_lock(&g_ptracker.anony_lock);
		add_anony_entry(&g_ptracker.young_list, new_data);
		g_ptracker.ptstat.nr_alloc++;
		spin_unlock(&g_ptracker.anony_lock);
    }
    return 0;
}

struct rb_pid_node {
    struct rb_node node;
    int pid;                 // Key: PID
    unsigned long count;      // Value: number of times kprobe was hit
};

// Root of the rbtree
static struct rb_root pid_tree = RB_ROOT;

static unsigned long orders[100];
static unsigned long nr_vma_alloc;
static unsigned long nr_pages_bulk;
static unsigned long hugepages;

// Insert or update a node for the current PID
static struct rb_pid_node *insert_or_update_node(int pid) {
    struct rb_node **new = &(pid_tree.rb_node), *parent = NULL;
    struct rb_pid_node *entry;

    // Traverse the tree to find the right place
    while (*new) {
        entry = rb_entry(*new, struct rb_pid_node, node);

        if (pid < entry->pid) {
            new = &((*new)->rb_left);
        } else if (pid > entry->pid) {
            new = &((*new)->rb_right);
        } else {
            // PID already exists, update count
            entry->count++;
            return entry;
        }

        parent = *new;
    }

    // Allocate a new node
    entry = kmalloc(sizeof(*entry), GFP_KERNEL);
    if (!entry)
        return NULL;

    // Initialize the new node
    entry->pid = pid;
    entry->count = 1;

    // Insert the new node into the tree
    rb_link_node(&entry->node, parent, new);
    rb_insert_color(&entry->node, &pid_tree);

    return entry;
}

struct pid_node {
    int pid;                 // Key: PID
    unsigned long count;     // Value: number of times kprobe was hit
};

static struct pid_node pids[200];

static int do_vma_alloc_handler(struct kprobe *p, struct pt_regs *regs) {
    struct task_struct *task = current;
	int pid = task->pid;
	int i = 0;

	/*
	spin_lock(&g_ptracker.anony_lock);
	for (i = 0; i < 200; i++) {
		if (pids[i].pid == 0) {
			pids[i].pid = pid;
			pids[i].count = 1;
			break;
		}

		if (pids[i].pid == pid) {
			pids[i].count++;
			break;
		}
	}
	//if(insert_or_update_node(task->pid) == NULL){
    //    printk(KERN_ERR "kprobe: Failed to insert or update node for PID %d\n", task->pid);
	//}
	spin_unlock(&g_ptracker.anony_lock);
	*/

    if (task->tgid == g_ptracker.target_pid) {
		//struct vm_fault *vmf = (struct vm_fault *)regs->di;
		gfp_t gfp = (gfp_t)regs->di;
		int order = (int)regs->si;
		//unsigned long nr_pages = (unsigned long)regs->cx;
		unsigned long nr_pages = 0;

		//unsigned long address = regs->cx;
		//bool huge_page = (bool)regs->r8;
        //unsigned long address = vmf->address;
        //struct vm_area_struct *vma = (struct vm_area_struct *)vmf->vma;
        //printk(KERN_INFO "do_anonymous_page: pid = %d, address = %08lx, vma start = %08lx, vma end = %08lx\n", task->pid, address, vma->vm_start, vma->vm_end);

		/*
		gfp_t alloc_mask = GFP_KERNEL;
		struct anony_data *new_data = kmem_cache_alloc(g_ptracker.anony_slab, alloc_mask);

		if (!new_data)
			return 0;
			*/


		spin_lock(&g_ptracker.anony_lock);
		orders[order]++;
		nr_vma_alloc++;
		nr_pages_bulk += nr_pages;
		spin_unlock(&g_ptracker.anony_lock);
		/*
		if (huge_page)
			hugepages++;
			*/

		/*
		new_data->va = address;
		new_data->last_accessed = false;
		new_data->node = -1;

		spin_lock(&g_ptracker.anony_lock);
		add_anony_entry(&g_ptracker.young_list, new_data);
		g_ptracker.ptstat.nr_alloc++;
		spin_unlock(&g_ptracker.anony_lock);
		*/
    }
    return 0;
}

static struct kprobe kp_do_anonymous_page = {
    .symbol_name = "do_anonymous_page",
    .pre_handler = do_page_handler,
};

static struct kprobe kp_do_fault = {
    .symbol_name = "do_fault",
    .pre_handler = do_page_handler,
};

static struct kprobe kp_do_vma_alloc_folio = {
	.symbol_name = "vma_alloc_folio",
	.pre_handler = do_vma_alloc_handler,
};


static int run_kanonyd(void *data) {
	unsigned long sleep_timeout = usecs_to_jiffies(3000);
	struct mm_struct *mm;

    printk(KERN_INFO "start kanonyd\n");

	struct anony_data *adata;
	struct pebs_data *pdata;

	static unsigned long last_folio_sz = PAGE_SIZE;
	//static int nr_moved_items;

	struct anony_list tmp_young_list;
	struct anony_list tmp_old_list;
	int tmp_len;
	int i;

	struct list_head *pos, *q;
	int list_len = 0;

	// TODO: capacity
	init_anony_list(&tmp_young_list, init_capacity/2);
	init_anony_list(&tmp_old_list, init_capacity/2);

	struct list_head tmp_pebs_list;
	INIT_LIST_HEAD(&tmp_pebs_list);


	down(&g_ptracker.pid_sem);

    printk(KERN_INFO "start the loop\n");

	while (!kthread_should_stop()) {

		mutex_lock(&g_ptracker.pebs_mutex);
		list_splice_tail_init(&g_ptracker.pebs_list, &tmp_pebs_list);
		mutex_unlock(&g_ptracker.pebs_mutex);


		int pebs_len = 0;
		spin_lock(&g_ptracker.pebs_lock);
		
		list_for_each_safe(pos, q, &tmp_pebs_list) {
			pdata = list_entry(pos, struct pebs_data , list);
			pebs_len++;
			if (copy_to_rb(RB_PEBS, pdata) < 0)
				break;

			list_del(&pdata->list);
			kmem_cache_free(g_ptracker.pebs_slab, pdata);
		}


		if (need_to_wakeup(RB_PEBS)) {
			smp_mb();
			wakeup_user();
		}

		spin_unlock(&g_ptracker.pebs_lock);

		schedule_timeout_interruptible(sleep_timeout);
	}

    printk(KERN_INFO "stop kanonyd\n");

	tmp_len = 0;
	list_for_each_safe(pos, q, &tmp_pebs_list) {
		pdata = list_entry(pos, struct pebs_data , list);
		list_del(&pdata->list);
		kmem_cache_free(g_ptracker.pebs_slab, pdata);
		tmp_len++;
	}
	pr_info("free tmp_pebs_list: %d\n", tmp_len);

	up(&g_ptracker.sem);

	return 0;
}

void print_gfp_flags(gfp_t gfp_mask) {
    printk(KERN_INFO "gfp_mask: 0x%x\n", gfp_mask);

    if (gfp_mask & __GFP_DMA)
        printk(KERN_INFO "GFP flag: __GFP_DMA\n");
    if (gfp_mask & __GFP_HIGHMEM)
        printk(KERN_INFO "GFP flag: __GFP_HIGHMEM\n");
    if (gfp_mask & __GFP_DMA32)
        printk(KERN_INFO "GFP flag: __GFP_DMA32\n");
    if (gfp_mask & __GFP_MOVABLE)
        printk(KERN_INFO "GFP flag: __GFP_MOVABLE\n");
    if (gfp_mask & __GFP_RECLAIMABLE)
        printk(KERN_INFO "GFP flag: __GFP_RECLAIMABLE\n");
    if (gfp_mask & __GFP_HIGH)
        printk(KERN_INFO "GFP flag: __GFP_HIGH\n");
    if (gfp_mask & __GFP_IO)
        printk(KERN_INFO "GFP flag: __GFP_IO\n");
    if (gfp_mask & __GFP_FS)
        printk(KERN_INFO "GFP flag: __GFP_FS\n");
    if (gfp_mask & __GFP_NOWARN)
        printk(KERN_INFO "GFP flag: __GFP_NOWARN\n");
    if (gfp_mask & __GFP_NOFAIL)
        printk(KERN_INFO "GFP flag: __GFP_NOFAIL\n");
    if (gfp_mask & __GFP_NORETRY)
        printk(KERN_INFO "GFP flag: __GFP_NORETRY\n");
}

static long page_tracker_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
	int ret = 0;
    switch (cmd) {
        case IOCTL_SET_PID:
            if (copy_from_user(&g_ptracker.target_pid, (int __user *)arg, sizeof(g_ptracker.target_pid))) {
                ret = -EACCES;
				goto out;
            }
            printk(KERN_INFO "hook_pages: target PID set to %d\n", g_ptracker.target_pid);
			up(&g_ptracker.pid_sem);

			if (kpebs)
				ret = ksamplingd_init(g_ptracker.target_pid, 0);
			else
				ret = 0;

			if (ret < 0) {
		        printk(KERN_ERR "Failed to start damon: %d\n", ret);
				goto out;
			}

			/*
			ret = init_damon(g_ptracker.target_pid);
			if (ret < 0) {
		        printk(KERN_ERR "Failed to start damon: %d\n", ret);
				goto out;
			}
			*/
            break;
		case IOCTL_SET_HOT_BUF:
			int size;
            if (copy_from_user(&size, (int __user *)arg, sizeof(size))) {
                ret = -EACCES;
				goto out;
            }
			spin_lock(&g_ptracker.anony_lock);
			g_ptracker.young_list.capacity = g_ptracker.old_list.capacity = size/2;
			spin_unlock(&g_ptracker.anony_lock);
			break;
		case IOCTL_SET_THRESHOLD_WAKEUP:
			int threshold;
            if (copy_from_user(&threshold, (int __user *)arg, sizeof(threshold))) {
                ret = -EACCES;
				goto out;
            }

			spin_lock(&g_ptracker.anony_lock);
			g_ptracker.threshold_wakeup[RB_ALLOC] = threshold;
			spin_unlock(&g_ptracker.anony_lock);
			break;
		case IOCTL_MOVE_RB_TAIL:
			//int nr_items;
			struct rb_reply_t reply;
			if (copy_from_user(&reply, (void __user *)arg, sizeof(reply))) {
				ret = -EACCES;
				goto out;
			}
			if (reply.type == RB_ALLOC) {
				spin_lock(&g_ptracker.anony_lock);
				RB_DEL(g_ptracker.rb[reply.type], reply.nr_items);
				g_ptracker.ptstat.nr_copied += reply.nr_items;
				spin_unlock(&g_ptracker.anony_lock);
			} else if (reply.type == RB_DAMON) {
				spin_lock(&g_ptracker.damon_lock);
				RB_DEL(g_ptracker.rb[reply.type], reply.nr_items);
				//g_ptracker.ptstat.nr_copied += reply.nr_items;
				spin_unlock(&g_ptracker.damon_lock);
			} else if (reply.type == RB_PEBS) {
				spin_lock(&g_ptracker.pebs_lock);
				//mutex_lock(&g_ptracker.pebs_mutex);
				RB_DEL(g_ptracker.rb[reply.type], reply.nr_items);
				spin_unlock(&g_ptracker.pebs_lock);
				//mutex_unlock(&g_ptracker.pebs_mutex);
			}
			break;
		case IOCTL_GET_RB_STATUS:
			int flag = 0;

			spin_lock(&g_ptracker.anony_lock);
			if (need_to_wakeup(RB_ALLOC) == true) {
			    flag |= RB_TYPE_TO_FLAG(RB_ALLOC);
			}
			spin_unlock(&g_ptracker.anony_lock);

			spin_lock(&g_ptracker.damon_lock);
			if (need_to_wakeup(RB_DAMON) == true) {
			    flag |= RB_TYPE_TO_FLAG(RB_DAMON);
			}
			spin_unlock(&g_ptracker.damon_lock);

			spin_lock(&g_ptracker.pebs_lock);
			//mutex_lock(&g_ptracker.pebs_mutex);
			int len = RB_LEN(g_ptracker.rb[RB_PEBS]);
			if (need_to_wakeup(RB_PEBS) == true) {
			    flag |= RB_TYPE_TO_FLAG(RB_PEBS);
			}
			spin_unlock(&g_ptracker.pebs_lock);
			//mutex_unlock(&g_ptracker.pebs_mutex);

			//printk("ioctl return flag: %u\n", flag);

	
            if (copy_to_user((int __user *)arg, &flag, sizeof(flag))) {
                ret = -EACCES;
				goto out;
            }
			break;
		case IOCTL_GET_PERIOD:
			struct pebs_period period;
			pebs_get_period(&period.read, &period.write);
			if (copy_to_user((struct pebs_period __user *)arg, &period, sizeof(period))) {
				ret = -EACCES;
				goto out;
			}
			break;
        default:
            ret = -EINVAL;
    }
out:
    return ret;
}

static int page_tracker_mmap(struct file *file, struct vm_area_struct *vma) {
	unsigned long len = vma->vm_end - vma->vm_start;

	if (len % PAGE_SIZE || len != (RB_BUF_SIZE + RB_HEADER_SIZE) || vma->vm_pgoff)
		return -EINVAL;

	if (vma->vm_flags & VM_WRITE)
		return -EPERM;

	if (g_ptracker.cur_rb_idx >= MAX_NR_RB)
		return -EINVAL;

	pr_info("mmap va: %lu\n", vma->vm_start);

	return remap_pfn_range(vma, vma->vm_start,
			       virt_to_phys((void *)(g_ptracker.rb[g_ptracker.cur_rb_idx++])) >> PAGE_SHIFT,
			       len,
			       vma->vm_page_prot);
}

static unsigned int page_tracker_poll(struct file *file, poll_table *wait) {
	unsigned int flag = 0;
    poll_wait(file, &g_ptracker.poll_wq, wait);

	if (need_to_wakeup(RB_ALLOC)) {
	    flag |= RB_TYPE_TO_FLAG(RB_ALLOC);
	}

	if (need_to_wakeup(RB_DAMON)) {
	    flag |= RB_TYPE_TO_FLAG(RB_DAMON);
	}

	if (need_to_wakeup(RB_PEBS)) {
	    flag |= RB_TYPE_TO_FLAG(RB_PEBS);
	}

	//pr_info("poll1 return flag: %u\n", flag);

	if (flag)
		flag |= POLLIN;
	
	//pr_info("poll2 return flag: %u\n", flag);

    return flag;
}

static int page_tracker_open(struct inode *inode, struct file *file) {
    return 0;
}

static int page_tracker_release(struct inode *inode, struct file *file) {
    return 0;
}

static struct file_operations fops = {
    .open = page_tracker_open,
    .release = page_tracker_release,
    .unlocked_ioctl = page_tracker_ioctl,
	.mmap = page_tracker_mmap,
	.poll = page_tracker_poll,
};

// Module initialization
static int __init hook_pages_init(void) {
    int ret;
    // Register char device

    if ((ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME)) < 0) {
        printk(KERN_ERR "Failed to allocate char device region\n");
        return ret;
    }

    page_tracker_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(page_tracker_class)) {
        unregister_chrdev_region(dev_num, 1);
        printk(KERN_ERR "Failed to create device class\n");
        return PTR_ERR(page_tracker_class);
    }

    if (IS_ERR(device_create(page_tracker_class, NULL, dev_num, NULL, DEVICE_NAME))) {
        class_destroy(page_tracker_class);
        unregister_chrdev_region(dev_num, 1);
        printk(KERN_ERR "Failed to create device\n");
        return PTR_ERR(device_create(page_tracker_class, NULL, dev_num, NULL, DEVICE_NAME));
    }

    cdev_init(&page_tracker_cdev, &fops);
    if ((ret = cdev_add(&page_tracker_cdev, dev_num, 1)) < 0) {
        device_destroy(page_tracker_class, dev_num);
        class_destroy(page_tracker_class);
        unregister_chrdev_region(dev_num, 1);
        printk(KERN_ERR "Failed to add cdev\n");
        return ret;
    }

	init_ptracker();

	g_ptracker.kanonyd = kthread_run(run_kanonyd, NULL, "kanonyd");
    if (IS_ERR(g_ptracker.kanonyd)) {
        printk(KERN_ERR "Failed to create kanonyd\n");
		g_ptracker.kanonyd = NULL;
        return 0;
        //return PTR_ERR(g_ptracker.kanonyd);
    }

    printk(KERN_INFO "Page tracker for PID %d\n", g_ptracker.target_pid);
    return 0;
}

// Module cleanup
static void __exit hook_pages_exit(void) {
	int i = 0;
    if (g_ptracker.kanonyd) {
		up(&g_ptracker.pid_sem);
        kthread_stop(g_ptracker.kanonyd);
		down(&g_ptracker.sem);
	}

	if (kpebs) {
		struct list_head *pos, *q;
		struct pebs_data *pdata;
		int len = 0;
		up(&g_ptracker.pid_sem);
		ksamplingd_exit();
		down(&g_ptracker.pebs_sem);
		list_for_each_safe(pos, q, &g_ptracker.pebs_list) {
			pdata = list_entry(pos, struct pebs_data, list);
			list_del(&pdata->list);
			kmem_cache_free(g_ptracker.pebs_slab, pdata);
			len++;
		}
		pr_info("free pebs_list: %d\n", len);
	}

	destroy_ptracker();

    cdev_del(&page_tracker_cdev);
    device_destroy(page_tracker_class, dev_num);
    class_destroy(page_tracker_class);
    unregister_chrdev_region(dev_num, 1);

	printk(KERN_INFO "NR_VMA_ALLOC: %lu\n", nr_vma_alloc);
	printk(KERN_INFO "NR_PAGES_BULK: %lu\n", nr_pages_bulk);
	printk(KERN_INFO "HUGE: %lu\n", hugepages);
	for (i = 0; i < 100; i++) {
		printk(KERN_INFO "%lu ", orders[i]);
	}

	for (i = 0; i < 200; i++) {
		if (pids[i].pid != 0) 
			printk(KERN_INFO "idx: %d, pid: %d, count: %lu", i, pids[i].pid, pids[i].count);
	}

	// Free all nodes in the rbtree
    struct rb_node *node;
    for (node = rb_first(&pid_tree); node; node = rb_next(node)) {
        struct rb_pid_node *entry = rb_entry(node, struct rb_pid_node, node);
		printk(KERN_INFO "pid:%d count:%lu", entry->pid, entry->count);
        kfree(entry);
    }


    printk(KERN_INFO "young_list length = %d\n", g_ptracker.young_list.length);
	//print_anony_list(&g_ptracker.young_list);
    printk(KERN_INFO "old_list length = %d\n", g_ptracker.old_list.length);
    printk(KERN_INFO "old_list length = %d\n", g_ptracker.old_list.length);
	//print_anony_list(&g_ptracker.old_list);

	print_ptracker();
    printk(KERN_INFO "Exit page tracker\n");
}

module_init(hook_pages_init);
module_exit(hook_pages_exit);

