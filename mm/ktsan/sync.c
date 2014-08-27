#include "ktsan.h"

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

/* XXX(xairy): move somewhere? */
static uptr_t addr_to_slab_obj_addr(uptr_t addr)
{
	struct page *page;
	struct kmem_cache *cache;
	u32 offset;
	u32 idx;
	uptr_t obj_addr;

	if (!virt_addr_valid(addr))
		return 0;
	page = virt_to_head_page((void *)addr);
	if (!PageSlab(page))
		return 0;
	cache = page->slab_cache;
	offset = addr - (uptr_t)page->s_mem;
	idx = reciprocal_divide(offset, cache->reciprocal_buffer_size);
	obj_addr = (uptr_t)(page->s_mem + cache->size * idx);
	return obj_addr;
}

static void kt_sync_add(kt_tab_slab_t *slab, uptr_t sync)
{
	int i;

	BUG_ON(slab->sync_num > KT_MAX_SYNC_PER_SLAB_OBJ);
	BUG_ON(slab->sync_num < 0);

	for (i = 0; i < slab->sync_num; i++)
		if (slab->syncs[i] == sync)
			return;

	if (slab->sync_num < KT_MAX_SYNC_PER_SLAB_OBJ) {
		slab->syncs[slab->sync_num] = sync;
		slab->sync_num++;
	}

	BUG_ON(slab->sync_num > KT_MAX_SYNC_PER_SLAB_OBJ);
	BUG_ON(slab->sync_num < 0);
}

static void kt_sync_ensure_created(kt_thr_t *thr, uptr_t addr)
{
	kt_tab_sync_t *sync;
	bool created;
	uptr_t slab_obj_addr;
	kt_tab_slab_t *slab;

	sync = kt_tab_access(&kt_ctx.synctab, addr, &created, false);
	BUG_ON(sync == NULL); /* Ran out of memory. */

	if (created) {
		kt_thr_stat_inc(thr, kt_stat_sync_objects);

		slab_obj_addr = addr_to_slab_obj_addr(addr);
		slab = kt_tab_access(&kt_ctx.slabtab,
			slab_obj_addr, &created, false);
		BUG_ON(slab == NULL); /* Ran out of memory. */

		if (created) {
			kt_thr_stat_inc(thr, kt_stat_slab_objects);
			slab->sync_num = 0;
		}

		kt_sync_add(slab, addr);
		spin_unlock(&slab->tab.lock);
	}

	spin_unlock(&sync->tab.lock);
}

void kt_sync_acquire(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	kt_sync_ensure_created(thr, addr);
}

void kt_sync_release(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	kt_sync_ensure_created(thr, addr);
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
