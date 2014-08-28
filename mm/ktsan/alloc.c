#include "ktsan.h"

#include <linux/bootmem.h>
#include <linux/gfp.h>
#include <linux/memblock.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/spinlock.h>

struct kt_cache_obj_s {
	int link;
};

typedef struct kt_cache_obj_s kt_cache_obj_t;

#define INDEX_TO_ADDR(n) (cache->addr + (n) * cache->obj_size)
#define INDEX_TO_OBJ(n) ((kt_cache_obj_t *)INDEX_TO_ADDR(n))
#define ADDR_TO_INDEX(addr) (((addr) - cache->addr) / cache->obj_size)

/* Only available during early boot. */
static uptr_t alloc_memory(uptr_t size)
{
	uptr_t found;

	found = memblock_alloc(size, PAGE_SIZE);
	BUG_ON(found == 0);
	return (uptr_t)(phys_to_virt(found));
}

/* Only available during early boot. */
static void free_memory(uptr_t addr, uptr_t size)
{
	int rv;

	rv = memblock_free(addr, size);
	BUG_ON(rv == 0);
}

void kt_cache_init(kt_cache_t *cache, size_t obj_size, size_t obj_max_num)
{
	int i;

	cache->obj_size = round_up(obj_size, sizeof(unsigned long));
	cache->obj_max_num = obj_max_num;
	cache->obj_num = 0;

	cache->space = cache->obj_size * cache->obj_max_num;
	cache->addr = alloc_memory(cache->space);

	BUG_ON(cache->obj_max_num == 0);

	for (i = 0; i < cache->obj_max_num - 1; i++)
		INDEX_TO_OBJ(i)->link = i + 1;
	INDEX_TO_OBJ(cache->obj_max_num - 1)->link = -1;

	cache->head = 0;
	spin_lock_init(&cache->lock);
}

void kt_cache_destroy(kt_cache_t *cache)
{
	free_memory(cache->addr, cache->space);
}

static void *kt_cache_alloc_impl(kt_cache_t *cache)
{
	void *obj;

	if (cache->head == -1)
		return NULL;

	obj = (void *)INDEX_TO_ADDR(cache->head);
	cache->head = INDEX_TO_OBJ(cache->head)->link;
	cache->obj_num++;

	return obj;
}

void *kt_cache_alloc(kt_cache_t *cache)
{
	void *obj;

	spin_lock(&cache->lock);

	BUG_ON(cache->head < -1);
	BUG_ON(cache->head >= cache->obj_max_num);

	obj = kt_cache_alloc_impl(cache);

	BUG_ON(cache->head < -1);
	BUG_ON(cache->head >= cache->obj_max_num);

	spin_unlock(&cache->lock);

	return obj;
}

static void kt_cache_free_impl(kt_cache_t *cache, uptr_t addr)
{
	int index;

	index = ADDR_TO_INDEX(addr);
	INDEX_TO_OBJ(index)->link = cache->head;
	cache->head = index;
	cache->obj_num--;
}

void kt_cache_free(kt_cache_t *cache, void *obj)
{
	uptr_t addr = (uptr_t)obj;

	spin_lock(&cache->lock);

	BUG_ON(cache->head < -1);
	BUG_ON(cache->head >= cache->obj_max_num);

	BUG_ON(addr < cache->addr);
	BUG_ON(addr >= cache->addr + cache->space);
	BUG_ON((addr - cache->addr) % cache->obj_size != 0);

	kt_cache_free_impl(cache, addr);

	BUG_ON(cache->head < -1);
	BUG_ON(cache->head >= cache->obj_max_num);

	spin_unlock(&cache->lock);
}
