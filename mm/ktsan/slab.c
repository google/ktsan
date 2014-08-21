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

	slab = kt_tab_access(&kt_ctx.slabtab, addr, NULL, true);

	if (slab == NULL)
		return;

	BUG_ON(!spin_is_locked(&slab->tab.lock));

	for (i = 0; i < slab->head; i++) {
		sync = kt_tab_access(&kt_ctx.synctab, slab->syncs[i], NULL, true);
		if (sync == NULL)
			continue;
		BUG_ON(!spin_is_locked(&sync->tab.lock));
		spin_unlock(&sync->tab.lock);
		kt_cache_free(&kt_ctx.synctab.cache, sync);

		kt_thr_stat_dec(thr, kt_stat_sync_objects);
	}

	spin_unlock(&slab->tab.lock);
	kt_cache_free(&kt_ctx.slabtab.cache, slab);

	kt_thr_stat_dec(thr, kt_stat_slab_objects);
}
