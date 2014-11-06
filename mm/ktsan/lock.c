#include "ktsan.h"

#include <linux/list.h>
#include <linux/spinlock.h>

static kt_tab_lock_t *kt_lock_ensure_created(kt_thr_t *thr, uptr_t addr)
{
	kt_tab_lock_t *lock;
	bool created;
	uptr_t memblock_addr;

	lock = kt_tab_access(&kt_ctx.lock_tab, addr, &created, false);
	BUG_ON(lock == NULL); /* Ran out of memory. */

	if (created) {
		spin_lock_init(&lock->lock);
		INIT_LIST_HEAD(&lock->list);

		memblock_addr = kt_memblock_addr(addr);
		kt_memblock_add_lock(thr, memblock_addr, lock);

		kt_stat_inc(thr, kt_stat_lock_objects);
		kt_stat_inc(thr, kt_stat_lock_alloc);
	}

	return lock;
}

spinlock_t *kt_lock_get_and_lock(kt_thr_t *thr, uptr_t addr)
{
	kt_tab_lock_t *lock;

	lock = kt_lock_ensure_created(thr, addr);
	spin_lock(&lock->lock);
	spin_unlock(&lock->tab.lock);

	return &lock->lock;
}

void kt_lock_destroy(kt_thr_t *thr, uptr_t addr)
{
	kt_tab_lock_t *lock;

	lock = kt_tab_access(&kt_ctx.lock_tab, addr, NULL, true);
	BUG_ON(lock == NULL);

	spin_unlock(&lock->tab.lock);
	kt_cache_free(&kt_ctx.lock_tab.obj_cache, lock);

	kt_stat_dec(thr, kt_stat_lock_objects);
	kt_stat_inc(thr, kt_stat_lock_free);
}
