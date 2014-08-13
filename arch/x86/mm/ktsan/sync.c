#include "ktsan.h"

#include <linux/kernel.h>

void ktsan_acquire(ktsan_thr_t *thr, uptr_t pc, uptr_t addr)
{
	ktsan_sync_t *sync;
	bool created;

	sync = ktsan_tab_access(&ktsan_ctx.synctab, addr, &created, false);
	if (created) {
	}
}

void ktsan_release(ktsan_thr_t *thr, uptr_t pc, uptr_t addr)
{
	ktsan_sync_t *sync;
	bool created;

	sync = ktsan_tab_access(&ktsan_ctx.synctab, addr, &created, false);
	if (created) {
	}
}

void ktsan_pre_lock(ktsan_thr_t *thr, uptr_t pc, uptr_t addr,
		    bool write, bool try)
{
}

void ktsan_post_lock(ktsan_thr_t *thr, uptr_t pc, uptr_t addr,
		     bool write, bool try)
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

void ktsan_pre_unlock(ktsan_thr_t *thr, uptr_t pc, uptr_t addr, bool write)
{
/*
	unsigned long addr = (unsigned long)lock;
	int thread_id = current_thread_id();
	REPEAT_N_AND_STOP(20) pr_err(
		"TSan: Thread #%d unlocked %lu.\n", thread_id, addr);
*/
}
