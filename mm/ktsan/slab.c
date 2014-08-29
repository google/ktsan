#include "ktsan.h"

#include <linux/slab.h>

void kt_slab_alloc(kt_thr_t *thr, uptr_t pc, uptr_t addr, size_t size)
{

}

void kt_slab_free(kt_thr_t *thr, uptr_t pc, uptr_t addr, size_t size)
{
	kt_tab_slab_t *slab;
	int i;
	kt_tab_sync_t *sync;

	slab = kt_tab_access(&kt_ctx.slab_tab, addr, NULL, true);

	if (slab == NULL)
		return;

	BUG_ON(slab->sync_num > KT_MAX_SYNC_PER_SLAB_OBJ);
	BUG_ON(slab->sync_num < 0);

	for (i = 0; i < slab->sync_num; i++) {
		sync = kt_tab_access(&kt_ctx.sync_tab,
			slab->syncs[i], NULL, true);
		BUG_ON(sync == NULL);

		spin_unlock(&sync->tab.lock);
		kt_cache_free(&kt_ctx.sync_tab.obj_cache, sync);

		kt_thr_stat_dec(thr, kt_stat_sync_objects);
		kt_thr_stat_inc(thr, kt_stat_sync_free);
	}

	spin_unlock(&slab->tab.lock);
	kt_cache_free(&kt_ctx.slab_tab.obj_cache, slab);

	kt_thr_stat_dec(thr, kt_stat_slab_objects);
	kt_thr_stat_inc(thr, kt_stat_slab_free);
}
