#include "ktsan.h"

#include <linux/bootmem.h>
#include <linux/gfp.h>
#include <linux/memblock.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/spinlock.h>

/* Only available during early boot. */
void __init kt_cache_init(kt_cache_t *cache, size_t obj_size,
			  size_t obj_max_num)
{
	phys_addr_t paddr;
	uptr_t obj;
	int i;

	BUG_ON(obj_size == 0);
	BUG_ON(obj_max_num == 0);

	obj_size = round_up(obj_size, sizeof(unsigned long));
	cache->mem_size = obj_size * obj_max_num;

	paddr = memblock_alloc(cache->mem_size, PAGE_SIZE);
	BUG_ON(paddr == 0);
	cache->base = (uptr_t)(phys_to_virt(paddr));

	for (i = 0, obj = cache->base; i < obj_max_num-1; i++, obj += obj_size)
		*(void **)obj = (void *)(obj + obj_size);
	*(void **)obj = NULL;

	cache->head = (void *)cache->base;
	kt_spin_init(&cache->lock);
}

/* Only available during early boot. */
void __init kt_cache_destroy(kt_cache_t *cache)
{
	int rv;

	rv = memblock_free(cache->base, cache->mem_size);
	BUG_ON(rv == 0);
}

void *kt_cache_alloc(kt_cache_t *cache)
{
	void *obj;

	kt_spin_lock(&cache->lock);
	obj = cache->head;
	if (obj)
		cache->head = *(void **)obj;
	kt_spin_unlock(&cache->lock);

	return obj;
}

void kt_cache_free(kt_cache_t *cache, void *obj)
{
	kt_spin_lock(&cache->lock);
	*(void **)obj = cache->head;
	cache->head = obj;
	kt_spin_unlock(&cache->lock);
}
