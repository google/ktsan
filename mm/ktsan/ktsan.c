#include "ktsan.h"

#include <linux/atomic.h>
#include <linux/irqflags.h>
#include <linux/kernel.h>
#include <linux/nmi.h>
#include <linux/preempt.h>
#include <linux/printk.h>
#include <linux/sched.h>

kt_ctx_t kt_ctx;

#define DISABLE_INTERRUPTS(flags)	\
	preempt_disable();		\
	local_irq_save(flags);		\
	stop_nmi()			\
/**/

#define ENABLE_INTERRUPTS(flags)	\
	restart_nmi();			\
	local_irq_restore(flags);	\
	preempt_enable()		\
/**/

#define IN_INTERRUPT()			\
	(in_irq() ||			\
	 in_softirq() ||		\
	 in_interrupt() ||		\
	 in_serving_softirq() ||	\
	 in_nmi())			\
/**/

#define ENTER()							\
	kt_thr_t *thr;						\
	uptr_t pc;						\
	unsigned long kt_flags;					\
	int kt_inside_was;					\
	bool event_handled;					\
								\
	event_handled = false;					\
								\
	/* Sometimes thread #1 is scheduled without calling	\
	   ktsan_thr_start(). Some of such cases are caused	\
	   by interrupts. Ignoring them for now. */		\
	if (IN_INTERRUPT())					\
		goto exit;					\
								\
	if (!kt_ctx.enabled)					\
		goto exit;					\
	if (!current)						\
		goto exit;					\
	if (!current->ktsan.thr)				\
		goto exit;					\
								\
	DISABLE_INTERRUPTS(kt_flags);				\
								\
	thr = current->ktsan.thr;				\
	pc = (uptr_t)_RET_IP_;					\
								\
	kt_inside_was = atomic_cmpxchg(&thr->inside, 0, 1);	\
	if (kt_inside_was != 0) {				\
		ENABLE_INTERRUPTS(kt_flags);			\
		goto exit;					\
	}							\
								\
	event_handled = true;					\
/**/

#define LEAVE()							\
	/* thr might become NULL in ktsan_thread_destroy. */	\
	thr = current->ktsan.thr;				\
								\
	if (thr) {						\
		kt_inside_was =					\
			atomic_cmpxchg(&thr->inside, 1, 0);	\
		BUG_ON(kt_inside_was != 1);			\
	}							\
								\
	ENABLE_INTERRUPTS(kt_flags);				\
								\
	exit:							\
/**/

void __init ktsan_init_early(void)
{
	kt_ctx_t *ctx = &kt_ctx;

	kt_tab_init(&ctx->sync_tab, 10007,
		    sizeof(kt_tab_sync_t), 70000);
	kt_tab_init(&ctx->memblock_tab, 10007,
		    sizeof(kt_tab_memblock_t), 60000);
	kt_tab_init(&ctx->test_tab, 13, sizeof(kt_tab_test_t), 20);
	kt_cache_init(&ctx->thr_cache, sizeof(kt_thr_t), KT_MAX_THREAD_ID);
}

void ktsan_init(void)
{
	kt_ctx_t *ctx;
	kt_thr_t *thr;
	int inside;

	ctx = &kt_ctx;

	thr = kt_cache_alloc(&ctx->thr_cache);
	BUG_ON(thr == NULL); /* Out of memory. */
	kt_thr_create(NULL, (uptr_t)_RET_IP_, thr, current->pid);
	kt_thr_start(thr, (uptr_t)_RET_IP_);
	current->ktsan.thr = thr;

	BUG_ON(ctx->enabled);
	inside = atomic_cmpxchg(&thr->inside, 0, 1);
	BUG_ON(inside != 0);

	ctx->cpus = alloc_percpu(kt_cpu_t);
	kt_stat_init();
	kt_tests_init();

	/* These stats were not recorded in kt_thr_create. */
	kt_stat_inc(thr, kt_stat_thread_create);
	kt_stat_inc(thr, kt_stat_threads);

	inside = atomic_cmpxchg(&thr->inside, 1, 0);
	BUG_ON(inside != 1);
	ctx->enabled = 1;

	pr_err("TSan: enabled.\n");
}

void ktsan_thr_create(struct ktsan_thr_s *new, int tid)
{
	ENTER();
	new->thr = kt_cache_alloc(&kt_ctx.thr_cache);
	BUG_ON(new->thr == NULL); /* Out of memory. */
	kt_thr_create(thr, pc, new->thr, tid);
	LEAVE();
}

void ktsan_thr_destroy(struct ktsan_thr_s *old)
{
	ENTER();
	kt_thr_destroy(thr, pc, old->thr);
	kt_cache_free(&kt_ctx.thr_cache, old->thr);
	BUG_ON(old->thr == current->ktsan.thr && old != &current->ktsan);
	old->thr = NULL;
	LEAVE();
}

void ktsan_thr_start(void)
{
	ENTER();
	kt_thr_start(thr, pc);
	LEAVE();
}

void ktsan_thr_stop(void)
{
	ENTER();
	kt_thr_stop(thr, pc);
	LEAVE();
}

void ktsan_sync_acquire(void *addr)
{
	ENTER();
	kt_sync_acquire(thr, pc, (uptr_t)addr);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_sync_acquire);

void ktsan_sync_release(void *addr)
{
	ENTER();
	kt_sync_release(thr, pc, (uptr_t)addr);
	LEAVE();
}
EXPORT_SYMBOL(kt_sync_release);

void ktsan_memblock_alloc(void *addr, size_t size)
{
	ENTER();
	kt_memblock_alloc(thr, pc, (uptr_t)addr, size);
	LEAVE();
}

void ktsan_memblock_free(void *addr, size_t size)
{
	ENTER();
	kt_memblock_free(thr, pc, (uptr_t)addr, size);
	LEAVE();
}

void ktsan_mtx_pre_lock(void *addr, bool write, bool try)
{
	ENTER();
	kt_mtx_pre_lock(thr, pc, (uptr_t)addr, write, try);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_mtx_pre_lock);

void ktsan_mtx_post_lock(void *addr, bool write, bool try)
{
	ENTER();
	kt_mtx_post_lock(thr, pc, (uptr_t)addr, write, try);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_mtx_post_lock);

void ktsan_mtx_pre_unlock(void *addr, bool write)
{
	ENTER();
	kt_mtx_pre_unlock(thr, pc, (uptr_t)addr, write);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_mtx_pre_unlock);

int ktsan_atomic32_read(const void *addr)
{
	int rv;

	ENTER();
	rv = kt_atomic32_read(thr, pc, (uptr_t)addr);
	LEAVE();

	if (!event_handled)
		return kt_atomic32_pure_read(addr);
	return rv;
}
EXPORT_SYMBOL(ktsan_atomic32_read);

void ktsan_atomic32_set(void *addr, int value)
{
	ENTER();
	kt_atomic32_set(thr, pc, (uptr_t)addr, value);
	LEAVE();

	if (!event_handled)
		kt_atomic32_pure_set(addr, value);
}
EXPORT_SYMBOL(ktsan_atomic32_set);

void ktsan_read1(void *addr)
{
	ENTER();
	kt_access(thr, pc, (uptr_t)addr, 0, true);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_read1);

void ktsan_read2(void *addr)
{
	ENTER();
	kt_access(thr, pc, (uptr_t)addr, 1, true);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_read2);

void ktsan_read4(void *addr)
{
	ENTER();
	kt_access(thr, pc, (uptr_t)addr, 2, true);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_read4);

void ktsan_read8(void *addr)
{
	ENTER();
	kt_access(thr, pc, (uptr_t)addr, 3, true);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_read8);

void ktsan_read16(void *addr)
{
	ENTER();
	kt_access_range(thr, pc, (uptr_t)addr, 16, true);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_read16);

void ktsan_write1(void *addr)
{
	ENTER();
	kt_access(thr, pc, (uptr_t)addr, 0, false);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_write1);

void ktsan_write2(void *addr)
{
	ENTER();
	kt_access(thr, pc, (uptr_t)addr, 1, false);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_write2);

void ktsan_write4(void *addr)
{
	ENTER();
	kt_access(thr, pc, (uptr_t)addr, 2, false);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_write4);

void ktsan_write8(void *addr)
{
	ENTER();
	kt_access(thr, pc, (uptr_t)addr, 3, false);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_write8);

void ktsan_write16(void *addr)
{
	ENTER();
	kt_access(thr, pc, (uptr_t)addr, 16, false);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_write16);

void ktsan_func_entry(void *call_pc)
{
	ENTER();
	/* TODO. */
	LEAVE();
}
EXPORT_SYMBOL(ktsan_func_entry);

void ktsan_func_exit(void *call_pc)
{
	ENTER();
	/* TODO. */
	LEAVE();
}
EXPORT_SYMBOL(ktsan_func_exit);

void __tsan_read1(unsigned long addr) __attribute__ ((alias ("ktsan_read1")));
EXPORT_SYMBOL(__tsan_read1);

void __tsan_read2(unsigned long addr) __attribute__ ((alias ("ktsan_read2")));
EXPORT_SYMBOL(__tsan_read2);

void __tsan_read4(unsigned long addr) __attribute__ ((alias ("ktsan_read4")));
EXPORT_SYMBOL(__tsan_read4);

void __tsan_read8(unsigned long addr) __attribute__ ((alias ("ktsan_read8")));
EXPORT_SYMBOL(__tsan_read8);

void __tsan_read16(unsigned long addr) __attribute__ ((alias ("ktsan_read16")));
EXPORT_SYMBOL(__tsan_read16);

void __tsan_write1(unsigned long addr) __attribute__ ((alias ("ktsan_write1")));
EXPORT_SYMBOL(__tsan_write1);

void __tsan_write2(unsigned long addr) __attribute__ ((alias ("ktsan_write2")));
EXPORT_SYMBOL(__tsan_write2);

void __tsan_write4(unsigned long addr) __attribute__ ((alias ("ktsan_write4")));
EXPORT_SYMBOL(__tsan_write4);

void __tsan_write8(unsigned long addr) __attribute__ ((alias ("ktsan_write8")));
EXPORT_SYMBOL(__tsan_write8);

void __tsan_write16(unsigned long addr) __attribute__ ((alias ("ktsan_write16")));
EXPORT_SYMBOL(__tsan_write16);

void __tsan_func_entry(unsigned long addr) __attribute__ ((alias ("ktsan_func_entry")));
EXPORT_SYMBOL(__tsan_func_entry);

void __tsan_func_exit(unsigned long addr) __attribute__ ((alias ("ktsan_func_exit")));
EXPORT_SYMBOL(__tsan_func_exit);
