#include "ktsan.h"

void kt_memblock_alloc(kt_thr_t *thr, uptr_t pc, uptr_t addr, size_t size)
{
	kt_shadow_clear(addr, size);
}

void kt_memblock_free(kt_thr_t *thr, uptr_t pc, uptr_t addr, size_t size)
{
	kt_tab_memblock_t *memblock;
	kt_tab_sync_t *sync, *next;

	memblock = kt_tab_access(&kt_ctx.memblock_tab, addr, NULL, true);

	if (memblock == NULL)
		return;

	sync = memblock->head;

	spin_unlock(&memblock->tab.lock);
	kt_cache_free(&kt_ctx.memblock_tab.obj_cache, memblock);

	kt_stat_dec(thr, kt_stat_memblock_objects);
	kt_stat_inc(thr, kt_stat_memblock_free);

	while (sync) {
		sync = kt_tab_access(&kt_ctx.sync_tab,
			sync->tab.key, NULL, true);
		BUG_ON(sync == NULL);

		next = sync->next;

		spin_unlock(&sync->tab.lock);
		kt_cache_free(&kt_ctx.sync_tab.obj_cache, sync);

		kt_stat_dec(thr, kt_stat_sync_objects);
		kt_stat_inc(thr, kt_stat_sync_free);

		sync = next;
	}

	kt_shadow_clear(addr, size);
}
