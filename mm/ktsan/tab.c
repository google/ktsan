#include "ktsan.h"

#include <linux/kernel.h>
#include <linux/slab.h>

void kt_tab_init(kt_tab_t *tab, unsigned size, unsigned objsize)
{
	kt_tab_part_t *part;
	unsigned i;

	tab->size = size;
	tab->objsize = objsize;
	tab->parts = kmalloc_array(size, sizeof(*tab->parts), GFP_KERNEL);
	for (i = 0; i < size; i++) {
		part = &tab->parts[i];
		spin_lock_init(&part->lock);
		part->head = NULL;
	}
}

void kt_tab_destroy(kt_tab_t *tab)
{
	kfree(tab->parts);
	tab->parts = NULL;
}

void *kt_tab_access(kt_tab_t *tab, uptr_t key, bool *created, bool destroy)
{
	return NULL;
}
