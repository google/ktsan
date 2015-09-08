#include "ktsan.h"

#include <linux/jhash.h>
#include <linux/kernel.h>

#define KT_STACK_HASH_SEED 0x9747b28c

/* Only available during early boot. */
void __init kt_stack_depot_init(kt_stack_depot_t *depot)
{
	unsigned i;

	for (i = 0; i < KT_STACK_DEPOT_PARTS; i++) {
		depot->parts[i] = NULL;
	}
	kt_cache_init(&depot->stack_cache, KT_STACK_DEPOT_MEMORY_LIMIT, 1);
	depot->stack_offset = 0;
	kt_spin_init(&depot->lock);
}

/* Only available during early boot. */
void __init kt_stack_depot_destroy(kt_stack_depot_t *depot)
{
	kt_cache_destroy(&depot->stack_cache);
}

static inline u32 kt_stack_hash(kt_stack_t *stack)
{
	return jhash2((u32 *)stack->pc,
			stack->size * sizeof(stack->pc[0]) / sizeof(u32),
			KT_STACK_HASH_SEED);
}

static inline bool kt_stack_compare(kt_stack_t *first, kt_stack_t *second)
{
	if (first->size != second->size)
		return false;
	return memcmp(&first->pc, &second->pc,
			sizeof(first->pc[0]) * first->size);
}

static inline kt_stack_t *kt_stack_depot_obj_to_stack(kt_stack_depot_obj_t *obj)
{
	return (kt_stack_t *)&obj->stack_size;
}

static inline kt_stack_depot_obj_t *kt_stack_depot_lookup(
			kt_stack_depot_obj_t *head, kt_stack_t *stack)
{
	kt_stack_depot_obj_t *obj;
	u32 hash = kt_stack_hash(stack);

	for (obj = head; obj; obj = obj->link) {
		if (obj->hash != hash)
			continue;
		if (!kt_stack_compare(kt_stack_depot_obj_to_stack(obj), stack))
			continue;
		return obj;
	}

	return NULL;
}

static inline kt_stack_handle_t kt_stack_depot_obj_to_handle(
			kt_stack_depot_t *depot, kt_stack_depot_obj_t *obj)
{
	return ((uptr_t)obj - depot->stack_cache.base) / sizeof(u32);
}

static inline kt_stack_depot_obj_t *kt_stack_depot_handle_to_obj(
			kt_stack_depot_t *depot, kt_stack_handle_t handle)
{
	return (kt_stack_depot_obj_t *)(depot->stack_cache.base +
					handle * sizeof(u32));
}

kt_stack_handle_t kt_stack_depot_save(kt_stack_depot_t *depot,
					kt_stack_t *stack)
{
	u32 hash;
	kt_stack_depot_obj_t **part;
	kt_stack_depot_obj_t *obj, *new;

	BUG_ON(stack->size == 0);

	hash = kt_stack_hash(stack);
	part = &depot->parts[hash % KT_STACK_DEPOT_PARTS];

	obj = kt_stack_depot_lookup(smp_load_acquire(part), stack);
	if (obj)
		return kt_stack_depot_obj_to_handle(depot, obj);

	kt_spin_lock(&depot->lock);

	obj = kt_stack_depot_lookup(*part, stack);
	if (!obj) {
		new = (kt_stack_depot_obj_t *)(depot->stack_cache.base + 
						depot->stack_offset);
		depot->stack_offset += sizeof(*new) +
			sizeof(stack->pc[0]) * stack->size;
		BUG_ON(depot->stack_offset > KT_STACK_DEPOT_MEMORY_LIMIT);

		new->link = obj;
		new->hash = hash;
		new->stack_size = stack->size;
		memcpy(&new->stack_pc[0], &stack->pc[0],
				sizeof(stack->pc[0]) * stack->size);

		smp_store_release(part, new);

		obj = new;
	}

	kt_spin_unlock(&depot->lock);

	return kt_stack_depot_obj_to_handle(depot, obj);
}

kt_stack_t *kt_stack_depot_get(kt_stack_depot_t *depot,
				kt_stack_handle_t handle)
{
	kt_stack_depot_obj_t *obj = kt_stack_depot_handle_to_obj(depot, handle);
	return kt_stack_depot_obj_to_stack(obj);
}
