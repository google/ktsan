#include "ktsan.h"

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/slab_def.h>
#include <linux/spinlock.h>

/* XXX(xairy): move somewhere? */
static uptr_t addr_to_memblock_addr(uptr_t addr)
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

		memblock_addr = addr_to_memblock_addr(addr);
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
	kt_clk_tick(&thr->clk, thr->id);
	kt_sync_acquire(thr, pc, addr);
}

void kt_mtx_pre_unlock(kt_thr_t *thr, uptr_t pc, uptr_t addr, bool wr)
{
	kt_clk_tick(&thr->clk, thr->id);
	kt_sync_release(thr, pc, addr);
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
