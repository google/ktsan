#include "ktsan.h"

#include <linux/gfp.h>
#include <linux/kernel.h>
#include <linux/mm_types.h>
#include <linux/mm.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/slab_def.h>

void ktsan_alloc_page(struct page *page, unsigned int order,
		     gfp_t flags, int node)
{
	struct page *shadow;
	int pages = 1 << order;
	int i;

	if (flags & (__GFP_HIGHMEM | __GFP_NOTRACK))
		return;

	shadow = alloc_pages_node(node, flags | __GFP_NOTRACK,
			order + KT_SHADOW_SLOTS_LOG);
	BUG_ON(!shadow);

	memset(page_address(shadow), 0,
	       PAGE_SIZE * (1 << (order + KT_SHADOW_SLOTS_LOG)));

	for (i = 0; i < pages; i++)
		page[i].shadow = page_address(&shadow[i * KT_SHADOW_SLOTS]);
}
EXPORT_SYMBOL(ktsan_alloc_page);

void ktsan_free_page(struct page *page, unsigned int order)
{
	struct page *shadow;
	int pages = 1 << order;
	int i;
	unsigned long page_addr, first_obj_addr, obj_addr;
	struct kmem_cache *cache;

	page_addr = (unsigned long)page_address(page);
	first_obj_addr = (unsigned long)page->s_mem;
	if (PageSlab(page)) {
		cache = page->slab_cache;
		for (obj_addr = first_obj_addr;
		     obj_addr < page_addr + (PAGE_SIZE << order);
		     obj_addr += cache->size)
			ktsan_memblock_free((void *)obj_addr, cache->size);
	}
	ktsan_memblock_free((void *)page_addr, PAGE_SIZE << order);

	if (!page[0].shadow)
		return;

	shadow = virt_to_page(page[0].shadow);

	for (i = 0; i < pages; i++)
		page[i].shadow = NULL;

	__free_pages(shadow, order);
}
EXPORT_SYMBOL(ktsan_free_page);

void ktsan_split_page(struct page *page, unsigned int order)
{
	struct page *shadow;

	if (!page[0].shadow)
		return;

	shadow = virt_to_page(page[0].shadow);
	split_page(shadow, order);
}
EXPORT_SYMBOL(ktsan_split_page);

void kt_shadow_clear(uptr_t addr, size_t size)
{
	void *shadow_beg;
	void *shadow_end;
	size_t shadow_size;

	shadow_beg = kt_shadow_get(addr);
	shadow_end = kt_shadow_get(addr);

	BUG_ON(shadow_beg == NULL && shadow_end != NULL);
	BUG_ON(shadow_beg != NULL && shadow_end == NULL);

	if (shadow_beg == NULL && shadow_end == NULL)
		return;

	BUG_ON(shadow_beg > shadow_end);

	shadow_size = (uptr_t)shadow_end - (uptr_t)shadow_beg;
	memset(shadow_beg, 0, shadow_size);
}
