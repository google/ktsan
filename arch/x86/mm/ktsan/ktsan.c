#include "ktsan.h"

#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/sched.h>

ktsan_ctx_t ktsan_ctx;

/* XXX: for debugging. */
#define REPEAT_N_AND_STOP(n) \
	static int scary_counter_##__LINE__ = 0; \
	if (++scary_counter_##__LINE__ < (n))

void ktsan_init(void)
{
	ktsan_ctx_t *ctx;
	ktsan_thr_t *thr;

	ctx = &ktsan_ctx;
	thr = &current->ktsan;
	BUG_ON(ctx->enabled);
	BUG_ON(thr->inside);
	thr->inside = true;

	ktsan_tab_init(&ctx->synctab, 10007, sizeof(ktsan_sync_t));

	thr->inside = false;
	ctx->enabled = 1;
}

void ktsan_sync_acquire(void *addr)
{
	ktsan_thr_t *thr;

	thr = &current->ktsan;
	if (thr->inside)
		return;
	thr->inside = true;
	ktsan_acquire(thr, (uptr_t)_RET_IP_, (uptr_t)addr);
	thr->inside = false;
}
EXPORT_SYMBOL(ktsan_sync_acquire);

void ktsan_sync_release(void *addr)
{
	ktsan_thr_t *thr;

	thr = &current->ktsan;
	if (thr->inside)
		return;
	thr->inside = true;
	ktsan_release(thr, (uptr_t)_RET_IP_, (uptr_t)addr);
	thr->inside = false;
}
EXPORT_SYMBOL(ktsan_sync_release);

void ktsan_mtx_pre_lock(void *addr, bool write, bool try)
{
	ktsan_thr_t *thr;

	thr = &current->ktsan;
	if (thr->inside)
		return;
	thr->inside = true;
	ktsan_pre_lock(thr, (uptr_t)_RET_IP_, (uptr_t)addr, write, try);
	thr->inside = false;
}
EXPORT_SYMBOL(ktsan_mtx_pre_lock);

void ktsan_mtx_post_lock(void *addr, bool write, bool try)
{
	ktsan_thr_t *thr;

	thr = &current->ktsan;
	if (thr->inside)
		return;
	thr->inside = true;
	ktsan_post_lock(thr, (uptr_t)_RET_IP_, (uptr_t)addr, write, try);
	thr->inside = false;
}
EXPORT_SYMBOL(ktsan_mtx_post_lock);

void ktsan_mtx_pre_unlock(void *addr, bool write)
{
	ktsan_thr_t *thr;

	thr = &current->ktsan;
	if (thr->inside)
		return;
	thr->inside = true;
	ktsan_pre_unlock(thr, (uptr_t)_RET_IP_, (uptr_t)addr, write);
	thr->inside = false;
}
EXPORT_SYMBOL(ktsan_mtx_pre_unlock);
