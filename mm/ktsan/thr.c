#include "ktsan.h"

#include <linux/atomic.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/sched.h>
#include <linux/spinlock.h>

void kt_thr_pool_init(void)
{
	kt_thr_pool_t *pool = &kt_ctx.thr_pool;

	kt_cache_init(&pool->cache, sizeof(kt_thr_t), KT_MAX_THREAD_COUNT);
	memset(pool->thrs, 0, sizeof(pool->thrs));
	pool->new_id = 0;
	INIT_LIST_HEAD(&pool->quarantine);
	pool->quarantine_size = 0;
	spin_lock_init(&pool->lock);
}

kt_thr_t *kt_thr_create(kt_thr_t *thr, int kid)
{
	kt_thr_pool_t *pool = &kt_ctx.thr_pool;
	kt_thr_t *new;
	int i;

	spin_lock(&pool->lock);

	if (pool->quarantine_size > KT_QUARANTINE_SIZE) {
		new = list_first_entry(&pool->quarantine,
				kt_thr_t, quarantine_list);
		list_del(&new->quarantine_list);
		pool->quarantine_size--;
	} else {
		new = kt_cache_alloc(&pool->cache);
		BUG_ON(new == NULL);
		new->id = pool->new_id;
		pool->new_id++;
		pool->thrs[new->id] = new;
	}

	spin_unlock(&pool->lock);

	new->kid = kid;
	kt_atomic32_store_no_ktsan(&new->inside, 0);
	new->cpu = NULL;
	kt_clk_init(&new->clk);
	kt_clk_init(&new->acquire_clk);
	kt_clk_init(&new->release_clk);
	kt_trace_init(&new->trace);
	new->call_depth = 0;
	new->read_disable_depth = 0;
	new->event_disable_depth = 0;
	new->report_disable_depth = 0;
	new->preempt_disable_depth = 0;
	new->irqs_disabled = false;
	INIT_LIST_HEAD(&new->quarantine_list);
	INIT_LIST_HEAD(&new->percpu_list);
	for (i = 0; i < ARRAY_SIZE(new->seqcount); i++) {
		new->seqcount[i] = 0;
		new->seqcount_pc[i] = 0;
	}
	new->seqcount_ignore = 0;

	/* thr == NULL when thread #0 is being initialized. */
	if (thr == NULL)
		return new;

	kt_clk_acquire(&new->clk, &thr->clk);

	kt_stat_inc(thr, kt_stat_thread_create);
	kt_stat_inc(thr, kt_stat_threads);

	return new;
}

void kt_thr_destroy(kt_thr_t *thr, kt_thr_t *old)
{
	kt_thr_pool_t *pool = &kt_ctx.thr_pool;
	int i;

	BUG_ON(old->event_disable_depth != 0);
	for (i = 0; i < ARRAY_SIZE(old->seqcount); i++) {
		if (old->seqcount[i] != 0)
			kt_seqcount_bug(old, 0, "acquired seqlock on thr end");
	}
	if (old->read_disable_depth != 0)
		kt_seqcount_bug(old, 0, "read_disable_depth on thr end");
	BUG_ON(old->seqcount_ignore != 0);

	spin_lock(&pool->lock);
	list_add_tail(&old->quarantine_list, &pool->quarantine);
	pool->quarantine_size++;
	spin_unlock(&pool->lock);

	kt_stat_inc(thr, kt_stat_thread_destroy);
	kt_stat_dec(thr, kt_stat_threads);
}

kt_thr_t *kt_thr_get(int id)
{
	kt_thr_pool_t *pool = &kt_ctx.thr_pool;
	void *thr;

	BUG_ON(id < 0);
	BUG_ON(id >= KT_MAX_THREAD_COUNT);
	spin_lock(&pool->lock);
	thr = pool->thrs[id];
	spin_unlock(&pool->lock);

	return thr;
}

void kt_thr_start(kt_thr_t *thr, uptr_t pc)
{
	kt_trace_add_event(thr, kt_event_type_thr_start, pc);
	kt_clk_tick(&thr->clk, thr->id);

	thr->cpu = this_cpu_ptr(kt_ctx.cpus);

#if KT_DEBUG
	kt_stack_save_current(&thr->start_stack, _RET_IP_);
#endif
}

void kt_thr_stop(kt_thr_t *thr, uptr_t pc)
{
	BUG_ON(thr->event_disable_depth != 0);

	kt_trace_add_event(thr, kt_event_type_thr_stop, pc);
	kt_clk_tick(&thr->clk, thr->id);

	/* Current thread might be rescheduled even if preemption is disabled
	   (for example using might_sleep()). Therefore, percpu syncs won't
	   be released before thread switching. Release them here. */
	kt_percpu_release(thr, pc);
	kt_percpu_list_clean(thr, pc);

	thr->cpu = NULL;
}

void kt_thr_wakeup(kt_thr_t *thr, kt_thr_t *other)
{
	kt_clk_acquire(&other->clk, &thr->clk);
}

/* Returns true if events were enabled before the call. */
bool kt_thr_event_disable(kt_thr_t *thr, uptr_t pc)
{
	kt_trace_add_event(thr, kt_event_type_event_disable, pc);
	kt_clk_tick(&thr->clk, thr->id);
#if KT_DEBUG
	if (thr->event_disable_depth == 0)
		thr->last_event_disable_time = kt_clk_get(&thr->clk, thr->id);
#endif
	thr->event_disable_depth++;
	BUG_ON(thr->event_disable_depth >= 3);
	return (thr->event_disable_depth - 1 == 0);
}

/* Returns true if events became enabled after the call. */
bool kt_thr_event_enable(kt_thr_t *thr, uptr_t pc)
{
	kt_trace_add_event(thr, kt_event_type_event_enable, pc);
	kt_clk_tick(&thr->clk, thr->id);
#if KT_DEBUG
	if (thr->event_disable_depth - 1 == 0)
		thr->last_event_enable_time = kt_clk_get(&thr->clk, thr->id);
#endif
	thr->event_disable_depth--;
	BUG_ON(thr->event_disable_depth < 0);
	return (thr->event_disable_depth == 0);
}
