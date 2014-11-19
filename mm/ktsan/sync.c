#include "ktsan.h"

#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/spinlock.h>

static kt_tab_sync_t *kt_sync_ensure_created(kt_thr_t *thr, uptr_t addr)
{
	kt_tab_sync_t *sync;
	bool created;
	uptr_t memblock_addr;
	kt_tab_memblock_t *memblock;

	sync = kt_tab_access(&kt_ctx.sync_tab, addr, &created, false);
	BUG_ON(sync == NULL); /* Ran out of memory. */

	if (created) {
		kt_stat_inc(thr, kt_stat_sync_objects);
		kt_stat_inc(thr, kt_stat_sync_alloc);

		kt_clk_init(thr, &sync->clk);
		sync->next = NULL;
		sync->lock_tid = -1;

		memblock_addr = kt_memblock_addr(addr);
		memblock = kt_tab_access(&kt_ctx.memblock_tab,
				memblock_addr, &created, false);
		BUG_ON(memblock == NULL); /* Ran out of memory. */

		if (created) {
			kt_stat_inc(thr, kt_stat_memblock_objects);
			kt_stat_inc(thr, kt_stat_memblock_alloc);

			memblock->head = NULL;
		}

		sync->next = memblock->head;
		memblock->head = sync;

		spin_unlock(&memblock->tab.lock);
	}

	return sync;
}

void kt_sync_acquire(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	kt_tab_sync_t *sync;

	sync = kt_sync_ensure_created(thr, addr);
	kt_clk_acquire(thr, &thr->clk, &sync->clk);
	spin_unlock(&sync->tab.lock);
}

void kt_sync_release(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	kt_tab_sync_t *sync;

	sync = kt_sync_ensure_created(thr, addr);
	kt_clk_acquire(thr, &sync->clk, &thr->clk);
	spin_unlock(&sync->tab.lock);
}

void kt_mtx_pre_lock(kt_thr_t *thr, uptr_t pc, uptr_t addr, bool wr, bool try)
{
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

int kt_atomic32_read(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	/* TODO(xairy). */
	return kt_atomic32_pure_read((const void *)addr);
}

void kt_atomic32_set(kt_thr_t *thr, uptr_t pc, uptr_t addr, int value)
{
	/* TODO(xairy). */
	kt_atomic32_pure_set((void *)addr, value);
}
