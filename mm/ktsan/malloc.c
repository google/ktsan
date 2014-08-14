#include "ktsan.h"

#include <linux/slab.h>

/* TODO: kmalloc/kfree hooks? */

void ktsan_slab_alloc(struct kmem_cache *cache, void *obj)
{
}

void ktsan_slab_free(struct kmem_cache *cache, void *obj)
{
}
