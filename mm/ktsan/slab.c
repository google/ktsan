#include "ktsan.h"

#include <linux/slab.h>

void kt_slab_alloc(kt_thr_t *thr, uptr_t pc, uptr_t addr, size_t size)
{

}

void kt_slab_free(kt_thr_t *thr, uptr_t pc, uptr_t addr, size_t size)
{
	kt_tab_slab_t *slab;
	kt_tab_sync_t *sync;

	slab = kt_tab_access(&kt_ctx.slab_tab, addr, NULL, true);

	if (slab == NULL)
		return;

	for (sync = slab->head; sync; sync = sync->next) {
		sync = kt_tab_access(&kt_ctx.sync_tab,
			sync->tab.key, NULL, true);
		BUG_ON(sync == NULL);

		spin_unlock(&sync->tab.lock);
		kt_cache_free(&kt_ctx.sync_tab.obj_cache, sync);

		kt_stat_dec(thr, kt_stat_sync_objects);
		kt_stat_inc(thr, kt_stat_sync_free);
	}

	spin_unlock(&slab->tab.lock);
	kt_cache_free(&kt_ctx.slab_tab.obj_cache, slab);

	kt_stat_dec(thr, kt_stat_slab_objects);
	kt_stat_inc(thr, kt_stat_slab_free);
}
