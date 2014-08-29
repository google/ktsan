#include "ktsan.h"

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/slab.h>

/* Only available during early boot. */
void __init kt_tab_init(kt_tab_t *tab, unsigned size,
			unsigned obj_size, unsigned obj_max_num)
{
	kt_tab_part_t *part;
	unsigned i;

	tab->size = size;
	tab->objsize = obj_size;

	kt_cache_init(&tab->parts_cache, sizeof(*tab->parts) * size, 1);
	tab->parts = kt_cache_alloc(&tab->parts_cache);
	BUG_ON(tab->parts == NULL);

	for (i = 0; i < size; i++) {
		part = &tab->parts[i];
		spin_lock_init(&part->lock);
		part->head = NULL;
	}

	kt_cache_init(&tab->obj_cache, obj_size, obj_max_num);
}

/* Only available during early boot. */
void __init kt_tab_destroy(kt_tab_t *tab)
{
	kt_cache_destroy(&tab->obj_cache);
	kt_cache_destroy(&tab->parts_cache);
	tab->parts = NULL;
}

static inline void *kt_part_access(kt_tab_t *tab, kt_tab_part_t *part,
				   uptr_t key, bool *created, bool destroy)
{
	kt_tab_obj_t *obj;
	kt_tab_obj_t *prev;

	for (prev = NULL, obj = part->head; obj; prev = obj, obj = obj->link)
		if (obj->key == key)
			break;

	/* Get object if exists. */
	if (created == NULL && destroy == false) {
		if (obj) {
			spin_lock(&obj->lock);
			return obj;
		}
		return NULL;
	}

	/* Remove object from table if exists. */
	if (created == NULL && destroy == true) {
		if (obj) {
			if (!prev)
				part->head = obj->link;
			else
				prev->link = obj->link;

			spin_lock(&obj->lock);
			return obj;
		}
		return NULL;
	}

	/* Create object if not exists. */
	if (created != NULL && destroy == false) {
		if (!obj) {
			obj = kt_cache_alloc(&tab->obj_cache);
			if (!obj)
				return NULL;

			spin_lock_init(&obj->lock);
			obj->link = part->head;
			part->head = obj;
			obj->key = key;

			*created = true;
		} else {
			*created = false;
		}

		spin_lock(&obj->lock);
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
	unsigned int hash;
	kt_tab_part_t *part;
	void *result;
	kt_tab_obj_t *obj;

	BUG_ON(created != NULL && destroy == true);

	hash = key % tab->size;
	part = &tab->parts[hash];

	spin_lock(&part->lock);

	for (obj = part->head; obj != NULL; obj = obj->link)
		BUG_ON((uptr_t)obj < PAGE_OFFSET);

	result = kt_part_access(tab, part, key, created, destroy);

	for (obj = part->head; obj != NULL; obj = obj->link)
		BUG_ON((uptr_t)obj < PAGE_OFFSET);

	spin_unlock(&part->lock);

	return result;
}
