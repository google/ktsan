#include "ktsan.h"

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

static uptr_t reserve_memory(uptr_t size)
{
	uptr_t start, end, found;
	int rv;

	start = 0;
	end = memblock_phys_mem_size();
	found = memblock_find_in_range(start, end, size, PAGE_SIZE);
	BUG_ON(found == 0);
	rv = memblock_reserve(found, size);
	BUG_ON(rv != 0);
	return (uptr_t)(phys_to_virt(found));
}

static void free_memory(uptr_t addr, uptr_t size)
{
	int rv;

	rv = memblock_free(addr, size);
	BUG_ON(rv == 0);
}

void kt_cache_create(kt_cache_t *cache, size_t obj_size)
{
	unsigned int i;

	cache->space = 128 * (1 << 20);
	cache->addr = reserve_memory(cache->space);

	cache->obj_size = round_up(obj_size, sizeof(unsigned long));
	cache->obj_max_num = cache->space / cache->obj_size;
	cache->obj_num = 0;

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

void *kt_cache_alloc(kt_cache_t *cache)
{
	unsigned long flags;
	void *obj;

	spin_lock_irqsave(&cache->lock, flags);

	if (cache->head == -1) {
		spin_unlock_irqrestore(&cache->lock, flags);
		return NULL;
	}

	obj = (void *)INDEX_TO_ADDR(cache->head);
	cache->head = INDEX_TO_OBJ(cache->head)->link;
	cache->obj_num++;

	spin_unlock_irqrestore(&cache->lock, flags);

	return obj;
}

void kt_cache_free(kt_cache_t *cache, void *obj)
{
	unsigned long addr;
	unsigned long flags;
	int index;

	spin_lock_irqsave(&cache->lock, flags);

	addr = (unsigned long)obj;
	index = ADDR_TO_INDEX(addr);
	INDEX_TO_OBJ(index)->link = cache->head;
	cache->head = index;
	cache->obj_num--;

	spin_unlock_irqrestore(&cache->lock, flags);
}
