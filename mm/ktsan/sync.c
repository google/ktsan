#include "ktsan.h"

#include <linux/list.h>
#include <linux/mmzone.h>
#include <linux/spinlock.h>

static bool is_page_struct_addr(uptr_t addr)
{
	uptr_t start, end;

	/* TODO: works only with UMA. */
	start = (uptr_t)pfn_to_page(NODE_DATA(0)->node_start_pfn);
	end = (uptr_t)(pfn_to_page(NODE_DATA(0)->node_start_pfn)
		+ NODE_DATA(0)->node_spanned_pages);
	return start <= addr && addr < end;
}


kt_tab_sync_t *kt_sync_ensure_created(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	kt_tab_sync_t *sync;
	bool created;
	uptr_t memblock_addr;

	/* Ignore all atomics that are stored inside page_struct structs.
	   This significantly reduces the number of syncs objects. This might
	   potentially lead to false positives, but none were observed. */
	if (is_page_struct_addr(addr))
		return NULL;

	sync = kt_tab_access(&kt_ctx.sync_tab, addr, &created, false);
	if (sync == NULL) {
		pr_err("KTSAN: limit on number of sync objects is reached\n");
#if KT_DEBUG
		kt_report_sync_usage();
#endif
		BUG();
	}

	if (created) {
		kt_clk_init(&sync->clk);
		sync->lock_tid = -1;
		INIT_LIST_HEAD(&sync->list);
		sync->pc = pc;

		memblock_addr = kt_memblock_addr(addr);
		kt_memblock_add_sync(thr, memblock_addr, sync);

		/* Allocated unique id for the sync object. */
		if (thr->cpu->sync_uid_pos == thr->cpu->sync_uid_end) {
			const u64 batch = 64;
			thr->cpu->sync_uid_pos = kt_atomic64_fetch_add_no_ktsan
				(&kt_ctx.sync_uid_gen, batch);
			thr->cpu->sync_uid_end = thr->cpu->sync_uid_pos + batch;
		}
		sync->uid = thr->cpu->sync_uid_pos++;

		kt_stat_inc(thr, kt_stat_sync_objects);
		kt_stat_inc(thr, kt_stat_sync_alloc);
	}

	return sync;
}

/* Removes sync object from hash table and frees it. */
void kt_sync_free(kt_thr_t *thr, uptr_t addr)
{
	kt_tab_sync_t *sync;

	sync = kt_tab_access(&kt_ctx.sync_tab, addr, NULL, true);
	BUG_ON(sync == NULL);

	kt_spin_unlock(&sync->tab.lock);
	kt_cache_free(&kt_ctx.sync_tab.obj_cache, sync);

	kt_stat_dec(thr, kt_stat_sync_objects);
	kt_stat_inc(thr, kt_stat_sync_free);
}

void kt_sync_acquire(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	kt_tab_sync_t *sync;

#if KT_DEBUG
	kt_trace_add_event(thr, kt_event_acquire, kt_compress(pc));
	kt_clk_tick(&thr->clk, thr->id);
#endif /* KT_DEBUG */

	sync = kt_sync_ensure_created(thr, pc, addr);
	if (sync == NULL)
		return;
	kt_acquire(thr, pc, sync);
	kt_spin_unlock(&sync->tab.lock);
}

void kt_sync_release(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	kt_tab_sync_t *sync;

#if KT_DEBUG
	kt_trace_add_event(thr, kt_event_release, kt_compress(pc));
	kt_clk_tick(&thr->clk, thr->id);
#endif /* KT_DEBUG */

	sync = kt_sync_ensure_created(thr, pc, addr);
	if (sync == NULL)
		return;
	kt_release(thr, pc, sync);
	kt_spin_unlock(&sync->tab.lock);
}

void kt_acquire(kt_thr_t *thr, uptr_t pc, kt_tab_sync_t *sync)
{
	kt_stat_inc(thr, kt_stat_acquire);
	kt_clk_acquire(&thr->clk, &sync->clk);
}

void kt_release(kt_thr_t *thr, uptr_t pc, kt_tab_sync_t *sync)
{
	kt_stat_inc(thr, kt_stat_release);
	kt_clk_acquire(&sync->clk, &thr->clk);
}
