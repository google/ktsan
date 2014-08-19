#include "ktsan.h"

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/spinlock.h>

void kt_sync_acquire(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	kt_sync_t *sync;
	bool created;

	sync = kt_tab_access(&kt_ctx.synctab, addr, &created, false);
	BUG_ON(sync == NULL); /* Ran out of memory. */
	BUG_ON(!spin_is_locked(&sync->tab.lock));
	spin_unlock(&sync->tab.lock);
}

void kt_sync_release(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	kt_sync_t *sync;
	bool created;

	/*sync = kt_tab_access(&kt_ctx.synctab, addr, &created, false);
	spin_unlock(&sync->tab.lock);
	if (created) {
	}*/
}

void kt_mtx_pre_lock(kt_thr_t *thr, uptr_t pc, uptr_t addr, bool wr, bool try)
{
}

void kt_mtx_post_lock(kt_thr_t *thr, uptr_t pc, uptr_t addr, bool wr, bool try)
{
	kt_clk_tick(&thr->clk, thr->id);
	kt_sync_acquire(thr, pc, addr);
}

void kt_mtx_pre_unlock(kt_thr_t *thr, uptr_t pc, uptr_t addr, bool wr)
{
	kt_clk_tick(&thr->clk, thr->id);
	kt_sync_release(thr, pc, addr);
}
