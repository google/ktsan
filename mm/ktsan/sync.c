#include "ktsan.h"

#include <linux/kernel.h>
#include <linux/spinlock.h>

/* XXX: for debugging. */
#define KT_MGK(x, y) x ## y
#define KT_MGK2(x, y) KT_MGK(x, y)
#define REPEAT_N_AND_STOP(n) \
	static int KT_MGK2(scary_, __LINE__); if (++KT_MGK2(scary_, __LINE__) < (n))

void kt_sync_acquire(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	kt_sync_t *sync;
	bool created;

	static int i = 0;
	i++;
	if (i > 3000 && i < 3010) {
		pr_err("%d\n", i);
		print_current_stack_trace(_RET_IP_);
		pr_err("\n");
	}

	sync = kt_tab_access(&kt_ctx.synctab, addr, &created, false);
	if (sync != NULL)
		spin_unlock(&sync->tab.lock);

	if (created) {
		//REPEAT_N_AND_STOP(10) pr_err("### 2\n");
	}
}

void kt_sync_release(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	kt_sync_t *sync;
	bool created;

	//sync = kt_tab_access(&kt_ctx.synctab, addr, &created, false);
	//spin_unlock(&sync->tab.lock);
	/*if (created) {
	}*/
}

void kt_mtx_pre_lock(kt_thr_t *thr, uptr_t pc, uptr_t addr, bool wr, bool try)
{
}

void kt_mtx_post_lock(kt_thr_t *thr, uptr_t pc, uptr_t addr, bool wr, bool try)
{
	kt_clk_tick(&thr->clk, thr->id);

	kt_sync_acquire(thr, pc, addr);

/*
	unsigned long addr = (unsigned long)lock;
	int thread_id = current_thread_id();
	spinlock_t *spin_lock = (spinlock_t *)lock;

	REPEAT_N_AND_STOP(20) pr_err(
		"TSan: Thread #%d locked %lu.\n", thread_id, addr);

	if (!spin_lock->clock) {
		spin_lock->clock = kzalloc(
			sizeof(unsigned long)*KTSAN_MAX_THREAD_ID, GFP_KERNEL);
		spin_lock->clock =
			(void *)__get_free_pages(GFP_KERNEL | __GFP_NOTRACK, 3);
	}
	//BUG_ON(!spin_lock->clock);
*/
}

void kt_mtx_pre_unlock(kt_thr_t *thr, uptr_t pc, uptr_t addr, bool wr)
{
	kt_clk_tick(&thr->clk, thr->id);

	kt_sync_release(thr, pc, addr);

/*
	unsigned long addr = (unsigned long)lock;
	int thread_id = current_thread_id();
	REPEAT_N_AND_STOP(20) pr_err(
		"TSan: Thread #%d unlocked %lu.\n", thread_id, addr);
*/
}
