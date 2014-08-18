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

	tab->objnum = 0;
	kt_cache_create(&tab->cache, objsize);
}

/* Called in tests only. */
void kt_tab_destroy(kt_tab_t *tab)
{
	kt_tab_obj_t *obj;
	kt_tab_part_t *part;
	unsigned i;

	for (i = 0; i < tab->size; i++) {
		part = &tab->parts[i];
		for (obj = part->head; obj; obj = obj->link)
			kfree(obj);
	}

	kfree(tab->parts);
	tab->parts = NULL;

	kt_cache_destroy(&tab->cache);
}

static inline void *kt_part_access(kt_tab_t *tab, kt_tab_part_t *part,
				   uptr_t key, bool *created, bool destroy)
{
	kt_tab_obj_t *obj;
	kt_tab_obj_t *prev;

	for (prev = NULL, obj = part->head; obj; prev = obj, obj = obj->link)
		if (obj->key == key)
			break;

	if (created == NULL && destroy == false) {
		if (obj) {
			spin_lock(&obj->lock); /* Correct lock type? */
			return obj;
		}
		return NULL;
	}

	if (created == NULL && destroy == true) {
		if (obj) {
			/* Remove object from table. */
			if (prev == NULL)
				part->head = obj->link;
			else
				prev->link = obj->link;

			tab->objnum--;

			spin_lock(&obj->lock); /* Correct lock type? */
			return obj;
		}
		return NULL;
	}

	if (created != NULL && destroy == false) {
		if (!obj) {
			/* Create object. */
			obj = kt_cache_alloc(&tab->cache);
			if (!obj)
				return NULL;

			spin_lock_init(&obj->lock);
			obj->link = part->head;
			part->head = obj;
			obj->key = key;

			tab->objnum++;

			*created = true;
		} else {
			*created = false;
		}

		spin_lock(&obj->lock); /* Correct lock type? */
		return obj;
	}

	BUG_ON(true); /* Unreachable. */
	return NULL;
}

/*
 * When (created == NULL) and (destroy == false)
 *      returns the object if it exists, returns NULL otherwise.
 * When (created == NULL) and (destroy == true)
 *      removes the object from the table if it exists and returns it,
 *      returns NULL otherwise.
 *      The object must be freed by the caller via kt_cache_free.
 * When (created != NULL) and (destory == false)
 *      creates an object if it doesn't exist and returns it.
 *      Sets *created = false if the object existed, *c = true otherwise.
 * Parameters (created != NULL) and (destroy == true) are incorrect.
 * The returned object is always locked via spin_lock(object->lock).
 */
void *kt_tab_access(kt_tab_t *tab, uptr_t key, bool *created, bool destroy)
{
	unsigned long flags;
	unsigned int hash;
	kt_tab_part_t *part;
	void *result;

	BUG_ON(created != NULL && destroy == true);

	hash = key % tab->size;
	part = &tab->parts[hash];

	spin_lock_irqsave(&part->lock, flags);
	result = kt_part_access(tab, part, key, created, destroy);
	spin_unlock_irqrestore(&part->lock, flags);

	return result;
}
