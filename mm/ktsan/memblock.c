#include "ktsan.h"

#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/slab_def.h>

uptr_t kt_memblock_addr(uptr_t addr)
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

void kt_memblock_alloc(kt_thr_t *thr, uptr_t pc, uptr_t addr, size_t size)
{
	/* FIXME(xairy): imitate access instead. */
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

	/* FIXME(xairy): imitate access instead. */
	kt_shadow_clear(addr, size);
}
