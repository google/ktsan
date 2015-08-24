#include "ktsan.h"

#include <linux/spinlock.h>

void kt_mtx_pre_lock(kt_thr_t *thr, uptr_t pc, uptr_t addr, bool wr, bool try)
{
	/* Will be used for deadlock detection.
	   We can also put sleeps for random time here. */
}

void kt_mtx_post_lock(kt_thr_t *thr, uptr_t pc, uptr_t addr, bool wr, bool try)
{
	kt_tab_sync_t *sync;

	kt_trace_add_event(thr, kt_event_type_lock, pc);
	kt_clk_tick(&thr->clk, thr->id);
	kt_sync_acquire(thr, pc, addr);

	/* FIXME(xairy): double tab access. */
	sync = kt_tab_access(&kt_ctx.sync_tab, addr, NULL, false);
	BUG_ON(sync == NULL);
	sync->lock_tid = thr->id;
	spin_unlock(&sync->tab.lock);
}

void kt_mtx_pre_unlock(kt_thr_t *thr, uptr_t pc, uptr_t addr, bool wr)
{
	kt_tab_sync_t *sync;

	kt_trace_add_event(thr, kt_event_type_unlock, pc);
	kt_clk_tick(&thr->clk, thr->id);
	kt_sync_release(thr, pc, addr);

	/* FIXME(xairy): double tab access. */
	sync = kt_tab_access(&kt_ctx.sync_tab, addr, NULL, false);
	BUG_ON(sync == NULL);
	BUG_ON(wr && sync->lock_tid == -1);
	BUG_ON(wr && sync->lock_tid != thr->id);
	sync->lock_tid = -1;
	spin_unlock(&sync->tab.lock);
}

void kt_mtx_post_unlock(kt_thr_t *thr, uptr_t pc, uptr_t addr, bool wr)
{
	/* TODO. */
}
