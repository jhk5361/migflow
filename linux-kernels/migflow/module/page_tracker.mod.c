#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/export-internal.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

#ifdef CONFIG_UNWINDER_ORC
#include <asm/orc_header.h>
ORC_HEADER;
#endif

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif



static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x587f22d7, "devmap_managed_key" },
	{ 0xb1e9a8c3, "perf_event_get_period" },
	{ 0xe3ec2f2b, "alloc_chrdev_region" },
	{ 0x13c49cc2, "_copy_from_user" },
	{ 0x7f02188f, "__msecs_to_jiffies" },
	{ 0x8810754a, "_find_first_bit" },
	{ 0xca9360b5, "rb_next" },
	{ 0xda204bd7, "get_pid_task" },
	{ 0xe7cdf7c3, "class_destroy" },
	{ 0x6befd8cc, "__mmap_lock_do_trace_acquire_returned" },
	{ 0x725f0da3, "fget" },
	{ 0x60693ef7, "__put_task_struct" },
	{ 0xcf2a6966, "up" },
	{ 0x72d79d83, "pgdir_shift" },
	{ 0x79d18751, "remap_pfn_range" },
	{ 0x37a0cba, "kfree" },
	{ 0x54496b4, "schedule_timeout_interruptible" },
	{ 0xa20d01ba, "__trace_bprintk" },
	{ 0x5d0eeadb, "perf_event_period" },
	{ 0xb23ae567, "pcpu_hot" },
	{ 0x81de99f8, "__put_devmap_managed_page_refs" },
	{ 0x6de3349d, "find_get_pid" },
	{ 0xb3f7646e, "kthread_should_stop" },
	{ 0xbf10fbe5, "__tracepoint_mmap_lock_acquire_returned" },
	{ 0xe2964344, "__wake_up" },
	{ 0xe39804f4, "__pte_offset_map_lock" },
	{ 0x8fcf771c, "kmem_cache_create" },
	{ 0xc0477369, "__tracepoint_mmap_lock_released" },
	{ 0xba8fbd64, "_raw_spin_lock" },
	{ 0xcc5005fe, "msleep_interruptible" },
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0xbdb8412e, "wake_up_process" },
	{ 0x7f24de73, "jiffies_to_usecs" },
	{ 0x81e31bd6, "kmig__perf_event_init" },
	{ 0x65487097, "__x86_indirect_thunk_rax" },
	{ 0x122c3a7e, "_printk" },
	{ 0xf0fdf6cb, "__stack_chk_fail" },
	{ 0x296695f, "refcount_warn_saturate" },
	{ 0x257fb0e8, "kmem_cache_alloc" },
	{ 0x87a21cb3, "__ubsan_handle_out_of_bounds" },
	{ 0x7cd8d75e, "page_offset_base" },
	{ 0xbdc225b6, "__mmu_notifier_clear_young" },
	{ 0xf0b69b83, "cdev_add" },
	{ 0xbcb36fe4, "hugetlb_optimize_vmemmap_key" },
	{ 0xc13a0463, "pmdp_test_and_clear_young" },
	{ 0x1d19f77b, "physical_mask" },
	{ 0x2469810f, "__rcu_read_unlock" },
	{ 0xb5300453, "device_create" },
	{ 0x6626afca, "down" },
	{ 0xab40b679, "class_create" },
	{ 0x4c03a563, "random_kmalloc_seed" },
	{ 0x4dfa8d4b, "mutex_lock" },
	{ 0x439a9299, "kmem_cache_free" },
	{ 0x4c9d28b0, "phys_base" },
	{ 0x4ce26811, "get_task_mm" },
	{ 0x9ed12e20, "kmalloc_large" },
	{ 0xbe1a2f0f, "kthread_stop" },
	{ 0xcefb0c9f, "__mutex_init" },
	{ 0x9231c4f1, "ptep_test_and_clear_young" },
	{ 0x6f9fd00e, "pfn_to_online_page" },
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0x17de3d5, "nr_cpu_ids" },
	{ 0x6b10bee1, "_copy_to_user" },
	{ 0xd9a5ea54, "__init_waitqueue_head" },
	{ 0x8ba7cc89, "kthread_bind_mask" },
	{ 0xece784c2, "rb_first" },
	{ 0xcd28ab81, "follow_page" },
	{ 0x668b19a1, "down_read" },
	{ 0x15ba50a6, "jiffies" },
	{ 0x1ef1b505, "kthread_create_on_node" },
	{ 0xb3e60b36, "pv_ops" },
	{ 0x62649401, "__mmap_lock_do_trace_start_locking" },
	{ 0x97651e6c, "vmemmap_base" },
	{ 0xa648e561, "__ubsan_handle_shift_out_of_bounds" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0x5954769f, "mmput" },
	{ 0x3213f038, "mutex_unlock" },
	{ 0x8c11119a, "__mmu_notifier_test_young" },
	{ 0xb2c82688, "__tracepoint_mmap_lock_start_locking" },
	{ 0xb5761c5, "__folio_put" },
	{ 0x19a0e68a, "device_destroy" },
	{ 0x416b16a3, "__mmap_lock_do_trace_released" },
	{ 0xfcca5424, "register_kprobe" },
	{ 0x63026490, "unregister_kprobe" },
	{ 0x99fb2320, "perf_event_disable" },
	{ 0x8d883c65, "kmalloc_trace" },
	{ 0x54b1fac6, "__ubsan_handle_load_invalid_value" },
	{ 0x77164690, "param_ops_int" },
	{ 0x1887d29b, "walk_page_range" },
	{ 0xb5b54b34, "_raw_spin_unlock" },
	{ 0x53b954a2, "up_read" },
	{ 0x45d246da, "node_to_cpumask_map" },
	{ 0xe5696f52, "cdev_init" },
	{ 0xcb1be50d, "kmig__perf_event_open" },
	{ 0x87660638, "kmalloc_caches" },
	{ 0x833cf420, "cdev_del" },
	{ 0x7a477100, "kmem_cache_destroy" },
	{ 0x4e8f828, "mtree_load" },
	{ 0xd1d053d, "module_layout" },
};

MODULE_INFO(depends, "");

