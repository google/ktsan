#include "ktsan.h"

#include <linux/spinlock.h>

void kt_mtx_pre_lock(kt_thr_t *thr, uptr_t pc, uptr_t addr, bool wr, bool try)
{
	/* Will be used for deadlock detection.
	   We can also put sleeps for random time here. */
}

void kt_mtx_post_lock(kt_thr_t *thr, uptr_t pc, uptr_t addr, bool wr, bool try,
		      bool success)
{
	kt_tab_sync_t *sync;

	/* Sometimes even locks that are not trylocks might fail.
	   For example a thread calling mutex_lock might be rescheduled.
	   In that case we call mtx_post_lock(try = false, success = false). */
	/* BUG_ON(!try && !success); */

	if (!success)
		return;

#if KT_DEBUG
	kt_trace_add_event(thr, kt_event_type_lock, kt_pc_compress(pc));
	kt_clk_tick(&thr->clk, thr->id);
#endif /* KT_DEBUG */
	kt_sync_acquire(thr, pc, addr);

	/* FIXME(xairy): double tab access. */
	sync = kt_tab_access(&kt_ctx.sync_tab, addr, NULL, false);
	BUG_ON(sync == NULL);
	/* The following BUG_ON currently fails on scheduler rq lock.
	 * Which is bad.
	 */
	/* BUG_ON(sync->lock_tid != -1); */
	if (wr)
		sync->lock_tid = thr->id;
	sync->last_lock_time = kt_clk_get(&thr->clk, thr->id);
	kt_spin_unlock(&sync->tab.lock);
}

void kt_mtx_pre_unlock(kt_thr_t *thr, uptr_t pc, uptr_t addr, bool wr)
{
	kt_tab_sync_t *sync;

#if KT_DEBUG
	kt_trace_add_event(thr, kt_event_type_unlock, kt_pc_compress(pc));
	kt_clk_tick(&thr->clk, thr->id);
#endif /* KT_DEBUG */
	kt_sync_release(thr, pc, addr);

	/* FIXME(xairy): double tab access. */
	sync = kt_tab_access(&kt_ctx.sync_tab, addr, NULL, false);
	BUG_ON(sync == NULL);
	if (wr) {
		BUG_ON(sync->lock_tid == -1);
		if (wr && sync->lock_tid != thr->id)
			kt_report_bad_mtx_unlock(thr, sync, _RET_IP_);
		sync->lock_tid = -1;
	}
	BUG_ON(sync->lock_tid != -1);
	sync->last_unlock_time = kt_clk_get(&thr->clk, thr->id);
	kt_spin_unlock(&sync->tab.lock);
}

void kt_mtx_post_unlock(kt_thr_t *thr, uptr_t pc, uptr_t addr, bool wr)
{
}
