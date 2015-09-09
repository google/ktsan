#include "ktsan.h"

#include <linux/list.h>
#include <linux/mmzone.h>
#include <linux/spinlock.h>

kt_tab_sync_t *kt_sync_ensure_created(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	uptr_t page_structs_start, page_structs_end;
	kt_tab_sync_t *sync;
	bool created;
	uptr_t memblock_addr;

	/* Ignore all atomics that are stored inside page_struct structs.
	   This significantly reduces the number of syncs objects. This might
	   potentially lead to false positives, but none were observed.
	   Works only with UMA. */
	page_structs_start = (uptr_t)pfn_to_page(NODE_DATA(0)->node_start_pfn);
	page_structs_end = (uptr_t)(pfn_to_page(NODE_DATA(0)->node_start_pfn)
					+ NODE_DATA(0)->node_spanned_pages);
	if (page_structs_start <= addr && addr < page_structs_end)
		return NULL;

	sync = kt_tab_access(&kt_ctx.sync_tab, addr, &created, false);
#if KT_DEBUG
	if (sync == NULL)
		kt_report_sync_usage();
#endif /* KT_DEBUG */
	BUG_ON(sync == NULL); /* Ran out of memory. */

	if (created) {
		kt_clk_init(&sync->clk);
		sync->lock_tid = -1;
		INIT_LIST_HEAD(&sync->list);
		sync->pc = pc;

		memblock_addr = kt_memblock_addr(addr);
		kt_memblock_add_sync(thr, memblock_addr, sync);

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

	sync = kt_sync_ensure_created(thr, pc, addr);
	if (sync == NULL)
		return;
	kt_clk_acquire(&thr->clk, &sync->clk);
	kt_spin_unlock(&sync->tab.lock);
}

void kt_sync_release(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	kt_tab_sync_t *sync;

	sync = kt_sync_ensure_created(thr, pc, addr);
	if (sync == NULL)
		return;
	kt_clk_acquire(&sync->clk, &thr->clk);
	kt_spin_unlock(&sync->tab.lock);
}
