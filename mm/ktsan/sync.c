#include "ktsan.h"

#include <linux/list.h>
#include <linux/spinlock.h>

kt_tab_sync_t *kt_sync_ensure_created(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	kt_tab_sync_t *sync;
	bool created;
	uptr_t memblock_addr;

	sync = kt_tab_access(&kt_ctx.sync_tab, addr, &created, false);
	BUG_ON(sync == NULL); /* Ran out of memory. */

	if (created) {
		kt_clk_init(&sync->clk);
		sync->lock_tid = -1;
		INIT_LIST_HEAD(&sync->list);
		sync->pc = pc;

		memblock_addr = kt_memblock_addr(addr);
		kt_memblock_add_sync(thr, memblock_addr, sync);

		kt_stat_inc(thr, kt_stat_sync_objects);
		kt_stat_inc(thr, kt_stat_sync_alloc);
	}

	return sync;
}

void kt_sync_destroy(kt_thr_t *thr, uptr_t addr)
{
	kt_tab_sync_t *sync;

	sync = kt_tab_access(&kt_ctx.sync_tab, addr, NULL, true);
	BUG_ON(sync == NULL);

	spin_unlock(&sync->tab.lock);
	kt_cache_free(&kt_ctx.sync_tab.obj_cache, sync);

	kt_stat_dec(thr, kt_stat_sync_objects);
	kt_stat_inc(thr, kt_stat_sync_free);
}

void kt_sync_acquire(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	kt_tab_sync_t *sync;

	sync = kt_sync_ensure_created(thr, pc, addr);
	kt_clk_acquire(&thr->clk, &sync->clk);
	spin_unlock(&sync->tab.lock);
}

void kt_sync_release(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	kt_tab_sync_t *sync;

	sync = kt_sync_ensure_created(thr, pc, addr);
	kt_clk_acquire(&sync->clk, &thr->clk);
	spin_unlock(&sync->tab.lock);
}

void kt_seqcount_begin(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	int i;

	if (thr->seqcount_ignore)
		return;

	/* Find a slot for this seqcount and store it. */
	BUG_ON(addr == 0);
	for (i = 0; i < ARRAY_SIZE(thr->seqcount); i++) {
		if (thr->seqcount[i] == 0) {
			thr->seqcount[i] = addr;
			thr->seqcount_pc[i] = pc;
			break;
		}
	}
	if (i == ARRAY_SIZE(thr->seqcount))
		kt_seqcount_bug(thr, addr, "seqcount overflow");

	thr->read_disable_depth++;
	if (thr->read_disable_depth > ARRAY_SIZE(thr->seqcount) + 1)
		kt_seqcount_bug(thr, addr, "read_disable_depth overflow");
}

void kt_seqcount_end(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	int i;

	if (thr->seqcount_ignore)
		return;

	/* Find and remove the seqcount (reversed to support nested locks). */
	BUG_ON(addr == 0);
	for (i = ARRAY_SIZE(thr->seqcount) - 1; i >= 0; i--) {
		if (thr->seqcount[i] == addr) {
			thr->seqcount[i] = 0;
			thr->seqcount_pc[i] = 0;
			break;
		}
	}
	if (i < 0)
		kt_seqcount_bug(thr, addr, "seqcount is not acquired");

	thr->read_disable_depth--;
	if (thr->read_disable_depth < 0)
		kt_seqcount_bug(thr, addr, "read_disable_depth underflow");
}

void kt_seqcount_ignore_begin(kt_thr_t *thr, uptr_t pc)
{
	// This is counter-measure against fs/namei.c.
	BUG_ON(thr->seqcount_ignore);
	thr->seqcount_ignore = 1;
	thr->read_disable_depth++;
	if (thr->read_disable_depth > ARRAY_SIZE(thr->seqcount) + 1)
		kt_seqcount_bug(thr, 0, "read_disable_depth overflow");
}

void kt_seqcount_ignore_end(kt_thr_t *thr, uptr_t pc)
{
	BUG_ON(!thr->seqcount_ignore);
	thr->seqcount_ignore = 0;
	thr->read_disable_depth--;
	if (thr->read_disable_depth < 0)
		kt_seqcount_bug(thr, 0, "read_disable_depth underflow");
}

void kt_seqcount_bug(kt_thr_t *thr, uptr_t addr, const char *what)
{
	int i;

	pr_err("kt_seqcount_bug: %s\n", what);
	pr_err(" seqlock=%p read_disable_depth=%d\n",
		(void*)addr, thr->read_disable_depth);
	for (i = 0; i < ARRAY_SIZE(thr->seqcount); i++)
		pr_err(" slot #%d: %p [<%p>] %pS\n", i, (void*)thr->seqcount[i],
			(void*)thr->seqcount_pc[i], (void*)thr->seqcount_pc[i]);
	BUG();
}
