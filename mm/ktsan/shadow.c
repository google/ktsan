#include "ktsan.h"

#include <linux/gfp.h>
#include <linux/kernel.h>
#include <linux/mm_types.h>
#include <linux/mm.h>
#include <linux/printk.h>

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
