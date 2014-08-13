#include "ktsan.h"

#include <linux/kernel.h>

void kt_sync_acquire(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	kt_sync_t *sync;
	bool created;

	sync = kt_tab_access(&kt_ctx.synctab, addr, &created, false);
	if (created) {
	}
}

void kt_sync_release(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	kt_sync_t *sync;
	bool created;

	sync = kt_tab_access(&kt_ctx.synctab, addr, &created, false);
	if (created) {
	}
}

void kt_mtx_pre_lock(kt_thr_t *thr, uptr_t pc, uptr_t addr, bool wr, bool try)
{
}

void kt_mtx_post_lock(kt_thr_t *thr, uptr_t pc, uptr_t addr, bool wr, bool try)
{
/*
	unsigned long addr = (unsigned long)lock;
	int thread_id = current_thread_id();
	spinlock_t *spin_lock = (spinlock_t *)lock;

	REPEAT_N_AND_STOP(20) pr_err(
		"TSan: Thread #%d locked %lu.\n", thread_id, addr);

	if (!spin_lock->clock) {
		spin_lock->clock = kzalloc(
			sizeof(unsigned long)*KTSAN_MAX_THREAD_ID, GFP_KERNEL);
		spin_lock->clock =
			(void *)__get_free_pages(GFP_KERNEL | __GFP_NOTRACK, 3);
	}
	//BUG_ON(!spin_lock->clock);
*/
}

void kt_mtx_pre_unlock(kt_thr_t *thr, uptr_t pc, uptr_t addr, bool wr)
{
/*
	unsigned long addr = (unsigned long)lock;
	int thread_id = current_thread_id();
	REPEAT_N_AND_STOP(20) pr_err(
		"TSan: Thread #%d unlocked %lu.\n", thread_id, addr);
*/
}
