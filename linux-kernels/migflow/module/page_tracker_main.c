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
#include <linux/damon.h>
#include <linux/sched/mm.h>
#include <linux/pagewalk.h>
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

// old queue delay
static int init_msec = 100;
module_param(init_msec, int, 0644);
MODULE_PARM_DESC(init_msec, "An integer module parameter");

// young queue delay
static int delay_msec = 10;
module_param(delay_msec, int, 0644);
MODULE_PARM_DESC(delay_msec, "An integer module parameter");

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
	printk(KERN_INFO "nr_alloc: %lu, nr_accessed: %lu, nr_copied: %lu, nr_copied_accessed: %lu, max_rb_size: %d, max_young_list_size: %d, max_old_list_size: %d\n", g_ptracker.ptstat.nr_alloc, g_ptracker.ptstat.nr_accessed, g_ptracker.ptstat.nr_copied, g_ptracker.ptstat.nr_copied_accessed, g_ptracker.ptstat.max_rb_size, g_ptracker.ptstat.max_young_list_size, g_ptracker.ptstat.max_old_list_size);
}

/*
 * Get an online page for a pfn if it's in the LRU list.  Otherwise, returns
 * NULL.
 *
 * The body of this function is stolen from the 'page_idle_get_folio()'.  We
 * steal rather than reuse it because the code is quite simple.
 */
struct folio *page_tracker_get_folio(unsigned long pfn)
{
	struct page *page = pfn_to_online_page(pfn);
	struct folio *folio;

	if (!page || PageTail(page))
		return NULL;

	folio = page_folio(page);
	if (!folio_test_lru(folio) || !folio_try_get(folio))
		return NULL;
	if (unlikely(page_folio(page) != folio || !folio_test_lru(folio))) {
		folio_put(folio);
		folio = NULL;
	}
	return folio;
}

void page_tracker_ptep_mkold(pte_t *pte, struct vm_area_struct *vma, unsigned long addr)
{
	struct folio *folio = page_tracker_get_folio(pte_pfn(ptep_get(pte)));

	if (!folio)
		return;

	if (ptep_clear_young_notify(vma, addr, pte))
		folio_set_young(folio);

	folio_set_idle(folio);
	folio_put(folio);
}

void page_tracker_pmdp_mkold(pmd_t *pmd, struct vm_area_struct *vma, unsigned long addr)
{
#ifdef CONFIG_TRANSPARENT_HUGEPAGE
	struct folio *folio = page_tracker_get_folio(pmd_pfn(pmdp_get(pmd)));

	if (!folio)
		return;

	if (pmdp_clear_young_notify(vma, addr, pmd))
		folio_set_young(folio);

	folio_set_idle(folio);
	folio_put(folio);
#endif /* CONFIG_TRANSPARENT_HUGEPAGE */
}

static inline struct task_struct *page_tracker_get_task_struct(int pid) {
	return get_pid_task(find_get_pid(pid), PIDTYPE_PID);
	//return get_pid_task(pid, PIDTYPE_PID);
}

static struct mm_struct *page_tracker_get_mm(int pid) {
	struct task_struct *task = page_tracker_get_task_struct(pid);
	struct mm_struct *mm;

	task = page_tracker_get_task_struct(pid);
	
	if (!task)
		return NULL;

	mm = get_task_mm(task);
	put_task_struct(task);

	return mm;
}

/*
static bool page_tracker_va_target_valid(int pid)
{
	struct task_struct *task;

	task = page_tracker_get_task_struct(pid);
	if (task) {
		put_task_struct(task);
		return true;
	}

	return false;
}
*/


static int page_tracker_mkold_pmd_entry(pmd_t *pmd, unsigned long addr,
		unsigned long next, struct mm_walk *walk)
{
	pte_t *pte;
	pmd_t pmde;
	spinlock_t *ptl;

	if (pmd_trans_huge(pmdp_get(pmd))) {
		ptl = pmd_lock(walk->mm, pmd);
		pmde = pmdp_get(pmd);

		if (!pmd_present(pmde)) {
			spin_unlock(ptl);
			return 0;
		}

		if (pmd_trans_huge(pmde)) {
			page_tracker_pmdp_mkold(pmd, walk->vma, addr);
			spin_unlock(ptl);
			return 0;
		}
		spin_unlock(ptl);
	}

	pte = pte_offset_map_lock(walk->mm, pmd, addr, &ptl);
	if (!pte) {
		walk->action = ACTION_AGAIN;
		return 0;
	}
	if (!pte_present(ptep_get(pte)))
		goto out;
	page_tracker_ptep_mkold(pte, walk->vma, addr);
out:
	pte_unmap_unlock(pte, ptl);
	return 0;
}

#ifdef CONFIG_HUGETLB_PAGE
static void page_tracker_hugetlb_mkold(pte_t *pte, struct mm_struct *mm,
				struct vm_area_struct *vma, unsigned long addr)
{
	bool referenced = false;
	pte_t entry = huge_ptep_get(pte);
	struct folio *folio = pfn_folio(pte_pfn(entry));
	unsigned long psize = huge_page_size(hstate_vma(vma));

	folio_get(folio);

	if (pte_young(entry)) {
		referenced = true;
		entry = pte_mkold(entry);
		set_huge_pte_at(mm, addr, pte, entry, psize);
	}

#ifdef CONFIG_MMU_NOTIFIER
	if (mmu_notifier_clear_young(mm, addr,
				     addr + huge_page_size(hstate_vma(vma))))
		referenced = true;
#endif /* CONFIG_MMU_NOTIFIER */

	if (referenced)
		folio_set_young(folio);

	folio_set_idle(folio);
	folio_put(folio);
}

static int page_tracker_mkold_hugetlb_entry(pte_t *pte, unsigned long hmask,
				     unsigned long addr, unsigned long end,
				     struct mm_walk *walk)
{
	struct hstate *h = hstate_vma(walk->vma);
	spinlock_t *ptl;
	pte_t entry;

	ptl = huge_pte_lock(h, walk->mm, pte);
	entry = huge_ptep_get(pte);
	if (!pte_present(entry))
		goto out;

	page_tracker_hugetlb_mkold(pte, walk->mm, walk->vma, addr);

out:
	spin_unlock(ptl);
	return 0;
}
#else
#define page_tracker_mkold_hugetlb_entry NULL
#endif /* CONFIG_HUGETLB_PAGE */

static const struct mm_walk_ops page_tracker_mkold_ops = {
	.pmd_entry = page_tracker_mkold_pmd_entry,
	.hugetlb_entry = page_tracker_mkold_hugetlb_entry,
	.walk_lock = PGWALK_RDLOCK,
};

static void page_tracker_va_mkold(struct mm_struct *mm, unsigned long addr)
{
	mmap_read_lock(mm);
	walk_page_range(mm, addr, addr + 1, &page_tracker_mkold_ops, NULL);
	mmap_read_unlock(mm);
}

struct page_tracker_young_walk_private {
	/* size of the folio for the access checked virtual memory address */
	unsigned long *folio_sz;
	bool young;
};

static int page_tracker_young_pmd_entry(pmd_t *pmd, unsigned long addr,
		unsigned long next, struct mm_walk *walk)
{
	pte_t *pte;
	pte_t ptent;
	spinlock_t *ptl;
	struct folio *folio;
	struct page_tracker_young_walk_private *priv = walk->private;

#ifdef CONFIG_TRANSPARENT_HUGEPAGE
	if (pmd_trans_huge(pmdp_get(pmd))) {
		pmd_t pmde;

		ptl = pmd_lock(walk->mm, pmd);
		pmde = pmdp_get(pmd);

		if (!pmd_present(pmde)) {
			spin_unlock(ptl);
			return 0;
		}

		if (!pmd_trans_huge(pmde)) {
			spin_unlock(ptl);
			goto regular_page;
		}
		folio = page_tracker_get_folio(pmd_pfn(pmde));
		if (!folio)
			goto huge_out;
		if (pmd_young(pmde) || !folio_test_idle(folio) ||
					mmu_notifier_test_young(walk->mm,
						addr))
			priv->young = true;
		*priv->folio_sz = HPAGE_PMD_SIZE;
		folio_put(folio);
huge_out:
		spin_unlock(ptl);
		return 0;
	}

regular_page:
#endif	/* CONFIG_TRANSPARENT_HUGEPAGE */

	pte = pte_offset_map_lock(walk->mm, pmd, addr, &ptl);
	if (!pte) {
		walk->action = ACTION_AGAIN;
		return 0;
	}
	ptent = ptep_get(pte);
	if (!pte_present(ptent))
		goto out;
	folio = page_tracker_get_folio(pte_pfn(ptent));
	if (!folio)
		goto out;
	if (pte_young(ptent) || !folio_test_idle(folio) ||
			mmu_notifier_test_young(walk->mm, addr))
		priv->young = true;
	*priv->folio_sz = folio_size(folio);
	folio_put(folio);
out:
	pte_unmap_unlock(pte, ptl);
	return 0;
}

#ifdef CONFIG_HUGETLB_PAGE
static int page_tracker_young_hugetlb_entry(pte_t *pte, unsigned long hmask,
				     unsigned long addr, unsigned long end,
				     struct mm_walk *walk)
{
	struct page_tracker_young_walk_private *priv = walk->private;
	struct hstate *h = hstate_vma(walk->vma);
	struct folio *folio;
	spinlock_t *ptl;
	pte_t entry;

	ptl = huge_pte_lock(h, walk->mm, pte);
	entry = huge_ptep_get(pte);
	if (!pte_present(entry))
		goto out;

	folio = pfn_folio(pte_pfn(entry));
	folio_get(folio);

	if (pte_young(entry) || !folio_test_idle(folio) ||
	    mmu_notifier_test_young(walk->mm, addr))
		priv->young = true;
	*priv->folio_sz = huge_page_size(h);

	folio_put(folio);

out:
	spin_unlock(ptl);
	return 0;
}
#else
#define page_tracker_young_hugetlb_entry NULL
#endif /* CONFIG_HUGETLB_PAGE */

static const struct mm_walk_ops page_tracker_young_ops = {
	.pmd_entry = page_tracker_young_pmd_entry,
	.hugetlb_entry = page_tracker_young_hugetlb_entry,
	.walk_lock = PGWALK_RDLOCK,
};

static bool page_tracker_va_young(struct mm_struct *mm, unsigned long addr,
		unsigned long *folio_sz)
{
	struct page_tracker_young_walk_private arg = {
		.folio_sz = folio_sz,
		.young = false,
	};

	mmap_read_lock(mm);
	walk_page_range(mm, addr, addr + 1, &page_tracker_young_ops, &arg);
	mmap_read_unlock(mm);
	return arg.young;
}

static void do_pages_stat_array(struct mm_struct *mm, unsigned long nr_pages,
                const unsigned long *pages, int *status)
{
    unsigned long i;

    mmap_read_lock(mm);

    for (i = 0; i < nr_pages; i++) {
        unsigned long addr = *pages;
        struct vm_area_struct *vma;
        struct page *page;
        int err = -EFAULT;

        vma = vma_lookup(mm, addr);
        if (!vma)
            goto set_status;

        /* FOLL_DUMP to ignore special (like zero) pages */
        page = follow_page(vma, addr, FOLL_GET | FOLL_DUMP);

        err = PTR_ERR(page);
        if (IS_ERR(page))
            goto set_status;

        err = -ENOENT;
        if (!page)
            goto set_status;

        if (!is_zone_device_page(page))
            err = page_to_nid(page);

        put_page(page);
set_status:
        *status = err;

        pages++;
        status++;
    }

    mmap_read_unlock(mm);
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

static int destroy_anony_list(struct anony_list *list) {
	struct list_head *pos, *q;
	struct anony_data *adata;
	int len = 0;
	spin_lock(&g_ptracker.anony_lock);
	list_for_each_safe(pos, q, &list->head) {
		adata = list_entry(pos, struct anony_data, list);
		remove_anony_entry(list, adata);
		kmem_cache_free(g_ptracker.anony_slab, adata);
		len++;
	}
	spin_unlock(&g_ptracker.anony_lock);
	return len;
}

static void init_ptracker(void) {
	int i;
	int thresholds[MAX_NR_RB] = {RB_THRESHOLD_ALLOC, RB_THRESHOLD_DAMON, RB_THRESHOLD_PEBS, 0,};
	struct rb_head_t *rb;
	struct rb_data_t *rb_buf;
	g_ptracker.anony_msec = init_msec;
	g_ptracker.delay_msec = delay_msec;
	g_ptracker.target_pid = -1;
	g_ptracker.anony_slab = kmem_cache_create("anony_cache", sizeof(struct anony_data), 0, SLAB_HWCACHE_ALIGN | SLAB_MEM_SPREAD, NULL);
	g_ptracker.pebs_slab = kmem_cache_create("pebs_cache", sizeof(struct pebs_data), 0, SLAB_HWCACHE_ALIGN | SLAB_MEM_SPREAD, NULL);
	init_anony_list(&g_ptracker.young_list, init_capacity/2);
	init_anony_list(&g_ptracker.old_list, init_capacity/2);
	INIT_LIST_HEAD(&g_ptracker.pebs_list);

	pr_info("delay: %d, check: %d\n", g_ptracker.delay_msec, g_ptracker.anony_msec);



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
	pr_info("free young_list len: %d\n", destroy_anony_list(&g_ptracker.young_list));
	pr_info("free old_list len: %d\n", destroy_anony_list(&g_ptracker.old_list));
	kmem_cache_destroy(g_ptracker.anony_slab);
	kmem_cache_destroy(g_ptracker.pebs_slab);
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

static int demo_after_aggregation(struct damon_ctx *c) {
	struct damon_target *t;
	int nr_copied = 0;
	static unsigned int iter = 0;

	damon_for_each_target(t, c) {
		struct damon_region *r;
		unsigned long wss = 0;
		unsigned long nr_regions = 0;
		unsigned long nr_accessed_regions = 0;
		unsigned long total_region_bytes = 0;
		struct damon_data ddata;

		damon_for_each_region(r, t) {
			nr_regions++;
			total_region_bytes += r->ar.end - r->ar.start;
			if (r->nr_accesses > 0) {
				nr_accessed_regions++;
				wss += r->ar.end - r->ar.start;
				if (r->ar.start % 4096) {
					pr_info("not aligned!!! %lu\n", r->ar.start);
				}
			}
			ddata.va = r->ar.start;
			ddata.nr_pages = (r->ar.end - r->ar.start)/4096;
			ddata.nr_accesses = r->nr_accesses;
			ddata.iter = iter;

			if (copy_to_rb(RB_DAMON, &ddata) < 0)
				break;
			else
				nr_copied++;
		}
		pr_info("accessed_regions: %lu out of %lu, wss: %luKB out of %luKB, nr_copied: %d\n", nr_accessed_regions, nr_regions, wss/1024, total_region_bytes/1024, nr_copied);
	}

	spin_lock(&g_ptracker.damon_lock);
	if (need_to_wakeup(RB_DAMON)) {
		smp_mb();
		wakeup_user();
	}
	iter++;
	spin_unlock(&g_ptracker.damon_lock);

	return 0;
}

static int init_damon(int pid) {
	struct damon_target *target;
	struct damon_ctx *ctx;
	struct pid *target_pidp;
	struct damon_attrs attrs = {
		.min_nr_regions = DAMON_MIN_REGIONS, .max_nr_regions = DAMON_MAX_REGIONS,
		.sample_interval = DAMON_SAMPLING, .aggr_interval = DAMON_AGGREGATE,
		.ops_update_interval = DAMON_UPDATE,};
	int ret;

	ctx = damon_new_ctx();
	if (!ctx) {
		printk(KERN_ERR "Failed to create a damon ctx\n");
		return -ENOMEM;
	}

	ret = damon_select_ops(ctx, DAMON_OPS_VADDR);
	if (ret) {
		printk(KERN_ERR "Failed to select the vaddr ops\n");
		damon_destroy_ctx(ctx);
		return ret;
	}

	//sample interval, aggregate interval, ops update interval, min nr region, max_nr_region
	ret = damon_set_attrs(ctx, &attrs);
	if (ret) {
		printk(KERN_ERR "Failed to set attrs for damon\n");
		damon_destroy_ctx(ctx);
		return ret;
	}

	//target = damon_new_target((unsigned long)target_pidp);
	target = damon_new_target();
	if (!target) {
		damon_destroy_ctx(ctx);
		printk(KERN_ERR "Failed to create a damon target\n");
		return -ENOMEM;
	}


	target_pidp = find_get_pid(pid);
	if (!target_pidp) {
		printk(KERN_ERR "Failed to find the pidp for damon\n");
		return -EINVAL;
	}


	target->pid = target_pidp;
	ctx->callback.after_aggregation = demo_after_aggregation;
	//ctx->call

	damon_add_target(ctx, target);

	g_ptracker.dctx = ctx;
	g_ptracker.target_pidp = target_pidp;

	return damon_start(&ctx, 1, true);
}

static void destroy_damon(void) {
	if (g_ptracker.dctx) {
		damon_stop(&g_ptracker.dctx, 1);
		damon_destroy_ctx(g_ptracker.dctx);
	}
	if (g_ptracker.target_pidp)
		put_pid(g_ptracker.target_pidp);
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
		new_data->ts_jiffies = jiffies;

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
	unsigned long sleep_timeout = usecs_to_jiffies(1000);
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

	unsigned long current_jiffies;;

	// TODO: capacity
	init_anony_list(&tmp_young_list, init_capacity/2);
	init_anony_list(&tmp_old_list, init_capacity/2);

	struct list_head tmp_pebs_list;
	INIT_LIST_HEAD(&tmp_pebs_list);


	down(&g_ptracker.pid_sem);

    printk(KERN_INFO "start the loop\n");

	while (!kthread_should_stop()) {

		current_jiffies = jiffies;

		// TODO:: time-aware management
		spin_lock(&g_ptracker.anony_lock);

		while(!list_empty(&g_ptracker.young_list.head)) {
			adata = list_first_entry(&g_ptracker.young_list.head, struct anony_data, list);
			if (time_before(current_jiffies, adata->ts_jiffies + msecs_to_jiffies(g_ptracker.delay_msec)))
				break;

			adata->ts_jiffies = current_jiffies;
			move_anony_entry(&g_ptracker.young_list, &tmp_young_list, adata);
		}

		while(!list_empty(&g_ptracker.old_list.head)) {
			adata = list_first_entry(&g_ptracker.old_list.head, struct anony_data, list);
			if (time_before(current_jiffies, adata->ts_jiffies + msecs_to_jiffies(g_ptracker.anony_msec)))
				break;

			move_anony_entry(&g_ptracker.old_list, &tmp_old_list, adata);
		}

		spin_unlock(&g_ptracker.anony_lock);

		mm = page_tracker_get_mm(g_ptracker.target_pid);
		if (!mm) {
			//schedule_timeout_interruptible(sleep_timeout);
			continue;
		}

		list_len = 0;
		list_for_each(pos, &tmp_young_list.head) {
			adata = list_entry(pos, struct anony_data, list);
			page_tracker_va_mkold(mm, adata->va);
			list_len++;
			//if (list_len % 10000)
				//printk(KERN_INFO "somthing wrong mkold access young_list len: %d, old_list len: %d, tmp_yound_list len: %d (%d), tmp_old_list len: %d\n", g_ptracker.young_list.length, g_ptracker.old_list.length, tmp_young_list.length,list_len,tmp_old_list.length);
		}

		//printk(KERN_INFO "before mkold access young_list len: %d, old_list len: %d, tmp_yound_list len: %d (%d), tmp_old_list len: %d\n", g_ptracker.young_list.length, g_ptracker.old_list.length, tmp_young_list.length,list_len,tmp_old_list.length);

		list_len = 0;
		list_for_each(pos, &tmp_old_list.head) {
			adata = list_entry(pos, struct anony_data, list);
			if (adata->node != -1 || list_len > 131071)
				break;

			adata->last_accessed = page_tracker_va_young(mm, adata->va, &last_folio_sz);
			do_pages_stat_array(mm, 1, &adata->va, &adata->node);
			list_len++;

			if (adata->last_accessed) {
				g_ptracker.ptstat.nr_accessed++;
				//remove_anony_entry(&tmp_old_list, adata);
				//kmem_cache_free(g_ptracker.anony_slab, adata);
			}
		}
		mmput(mm);


		/*
		if (tmp_old_list.length || tmp_old_list.length)
			printk(KERN_INFO "after access young_list len: %d, old_list len: %d, tmp_yound_list len: %d, tmp_old_list len: %d\n", g_ptracker.young_list.length, g_ptracker.old_list.length, tmp_young_list.length, tmp_old_list.length);
		*/

		spin_lock(&g_ptracker.anony_lock);

		splice_anony_list(&tmp_young_list, &g_ptracker.old_list);
		
		list_for_each_safe(pos, q, &tmp_old_list.head) {
			adata = list_entry(pos, struct anony_data, list);

			if (copy_to_rb(RB_ALLOC, adata) < 0)
				break;

			if (adata->last_accessed) {
				g_ptracker.ptstat.nr_copied_accessed++;
			}

			remove_anony_entry(&tmp_old_list, adata);
			kmem_cache_free(g_ptracker.anony_slab, adata);
		}

		if (need_to_wakeup(RB_ALLOC)) {
			smp_mb();
			wakeup_user();
		}

		spin_unlock(&g_ptracker.anony_lock);

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

		//pr_info("PEBS TMP LEN: %d\n", pebs_len);

		if (need_to_wakeup(RB_PEBS)) {
			smp_mb();
			wakeup_user();
		}

		spin_unlock(&g_ptracker.pebs_lock);

		g_ptracker.ptstat.max_rb_size = max(g_ptracker.ptstat.max_rb_size, RB_LEN(g_ptracker.rb[RB_ALLOC]));

		schedule_timeout_interruptible(sleep_timeout);
	}

	pr_info("yound_list: %d old_list: %d, tmp_young_list: %d, tmp_old_list: %d\n",
			g_ptracker.young_list.length,
			g_ptracker.old_list.length,
			tmp_young_list.length,
			tmp_old_list.length);


	pr_info("free tmp_young_list len: %d\n", destroy_anony_list(&tmp_young_list));
	pr_info("free tmp_old_list len: %d\n", destroy_anony_list(&tmp_old_list));

	tmp_len = 0;
	list_for_each_safe(pos, q, &tmp_pebs_list) {
		pdata = list_entry(pos, struct pebs_data , list);
		list_del(&pdata->list);
		kmem_cache_free(g_ptracker.pebs_slab, pdata);
		tmp_len++;
	}
	pr_info("free tmp_pebs_list: %d\n", tmp_len);


    printk(KERN_INFO "stop kanonyd\n");

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

    page_tracker_class = class_create(CLASS_NAME);
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

    ret = register_kprobe(&kp_do_anonymous_page);
    if (ret < 0) {
        printk(KERN_ERR "Failed to register kprobe for do_anonymous_pages: %d\n", ret);
        return ret;
    }

    ret = register_kprobe(&kp_do_fault);
    if (ret < 0) {
        printk(KERN_ERR "Failed to register kprobe for do_fault: %d\n", ret);
        return ret;
    }

	/*
    ret = register_kprobe(&kp_do_vma_alloc_folio);
    if (ret < 0) {
        printk(KERN_ERR "Failed to register kprobe for do_fault: %d\n", ret);
        return ret;
    }
	*/

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
    unregister_kprobe(&kp_do_anonymous_page);
    unregister_kprobe(&kp_do_fault);
	//unregister_kprobe(&kp_do_vma_alloc_folio);

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

	/*
	destroy_damon();
	*/

	destroy_ptracker();

    cdev_del(&page_tracker_cdev);
    device_destroy(page_tracker_class, dev_num);
    class_destroy(page_tracker_class);
    unregister_chrdev_region(dev_num, 1);

	printk(KERN_INFO "NR_VMA_ALLOC: %lu\n", nr_vma_alloc);
	printk(KERN_INFO "NR_PAGES_BULK: %lu\n", nr_pages_bulk);
	printk(KERN_INFO "HUGE: %lu\n", hugepages);
	/*
	for (i = 0; i < 100; i++) {
		printk(KERN_INFO "%lu ", orders[i]);
	}
	*/

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

