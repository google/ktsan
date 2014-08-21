#include "ktsan.h"

#include <linux/atomic.h>
#include <linux/irqflags.h>
#include <linux/kernel.h>
#include <linux/nmi.h>
#include <linux/preempt.h>
#include <linux/printk.h>
#include <linux/sched.h>
#include <linux/slab.h>

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
	unsigned long flags;					\
	int inside;						\
								\
	if (IN_INTERRUPT())					\
		return;						\
								\
	if (!kt_ctx.enabled)					\
		return;						\
	if (!current)						\
		return;						\
	if (!current->ktsan.thr)				\
		return;						\
								\
	DISABLE_INTERRUPTS(flags);				\
								\
	thr = current->ktsan.thr;				\
	pc = (uptr_t)_RET_IP_;					\
								\
	inside = atomic_cmpxchg(&thr->inside, 0, 1);		\
	if (inside != 0) {					\
		ENABLE_INTERRUPTS(flags);			\
		return;						\
	}							\
/**/

#define LEAVE()							\
	inside = atomic_cmpxchg(&thr->inside, 1, 0);		\
	BUG_ON(inside != 1);					\
								\
	ENABLE_INTERRUPTS(flags)				\
/**/

void ktsan_init(void)
{
	kt_ctx_t *ctx;
	kt_thr_t *thr;
	int inside;

	ctx = &kt_ctx;

	thr = kzalloc(sizeof(*thr), GFP_KERNEL);
	kt_thr_create(NULL, (uptr_t)_RET_IP_, thr, current->pid);
	current->ktsan.thr = thr;

	BUG_ON(ctx->enabled);
	inside = atomic_cmpxchg(&thr->inside, 0, 1);
	BUG_ON(inside != 0);

	ctx->cpus = alloc_percpu(kt_cpu_t);
	kt_tab_init(&ctx->slabtab, 10007,
		    sizeof(kt_tab_slab_t), 128 * (1UL << 20));
	kt_tab_init(&ctx->synctab, 10007,
		    sizeof(kt_tab_sync_t), 2 * (1UL << 30));
	kt_stat_init();
	kt_tests_init();

	inside = atomic_cmpxchg(&thr->inside, 1, 0);
	BUG_ON(inside != 1);
	ctx->enabled = 1;

	pr_err("TSan: enabled.\n");
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

void ktsan_thr_create(struct ktsan_thr_s *new, int tid)
{
	ENTER();
	new->thr = kzalloc(sizeof(*new->thr), GFP_KERNEL);
	kt_thr_create(thr, pc, new->thr, tid);
	LEAVE();
}

void ktsan_thr_finish(void)
{
	ENTER();
	kt_thr_finish(thr, pc);
	kfree(thr);
	current->ktsan.thr = NULL;
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

void ktsan_slab_alloc(struct kmem_cache *cache, void *obj)
{
	ENTER();
	kt_slab_alloc(thr, pc, (uptr_t)obj, cache->object_size);
	LEAVE();
}

void ktsan_slab_free(struct kmem_cache *cache, void *obj)
{
	ENTER();
	kt_slab_free(thr, pc, (uptr_t)obj, cache->object_size);
	LEAVE();
}

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
