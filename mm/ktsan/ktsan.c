#include "ktsan.h"

#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/sched.h>

kt_ctx_t kt_ctx;

/* XXX: for debugging. */
#define REPEAT_N_AND_STOP(n)				\
	static int scary_counter_##__LINE__; /* = 0; */ \
	if (++scary_counter_##__LINE__ < (n))

#define ENTER() 		\
	kt_thr_t *thr;		\
	uptr_t pc;		\
				\
	thr = &current->ktsan;	\
	if (thr->inside)	\
		return;		\
	thr->inside = true;	\
	pc = (uptr_t)_RET_IP_	\
/**/

#define LEAVE()			\
	thr->inside = false	\
/**/

void ktsan_init(void)
{
	kt_ctx_t *ctx;
	kt_thr_t *thr;

	ctx = &kt_ctx;
	thr = &current->ktsan;
	BUG_ON(ctx->enabled);
	BUG_ON(thr->inside);
	thr->inside = true;

	kt_tab_init(&ctx->synctab, 10007, sizeof(kt_sync_t));

	thr->inside = false;
	ctx->enabled = 1;
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

void ktsan_thr_create(ktsan_thr_t *new, int tid)
{
	ENTER();
	kt_thr_create(thr, pc, new, tid);
	LEAVE();
}

void ktsan_thr_finish(void)
{
	ENTER();
	kt_thr_finish(thr, pc);
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
