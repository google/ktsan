#include "ktsan.h"

#include <linux/atomic.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/sched.h>
#include <linux/spinlock.h>

void kt_thr_pool_init(void)
{
	kt_thr_pool_t *pool = &kt_ctx.thr_pool;

	kt_cache_init(&pool->cache, sizeof(kt_thr_t), KT_MAX_THREAD_ID);
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

	spin_lock(&pool->lock);

	if (pool->quarantine_size > KT_QUARANTINE_SIZE) {
		new = list_first_entry(&pool->quarantine, kt_thr_t, list);
		list_del(&new->list);
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
	kt_atomic32_pure_set(&new->inside, 0);
	new->cpu = NULL;
	kt_clk_init(thr, &new->clk);
	kt_trace_init(&new->trace);
	new->call_depth = 0;
	INIT_LIST_HEAD(&new->list);

	/* thr == NULL when thread #0 is being initialized. */
	if (thr == NULL)
		return new;

	kt_clk_acquire(thr, &new->clk, &thr->clk);

	kt_stat_inc(thr, kt_stat_thread_create);
	kt_stat_inc(thr, kt_stat_threads);

	return new;
}

void kt_thr_destroy(kt_thr_t *thr, kt_thr_t *old)
{
	kt_thr_pool_t *pool = &kt_ctx.thr_pool;

	spin_lock(&pool->lock);
	list_add_tail(&old->list, &pool->quarantine);
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
	BUG_ON(id >= KT_MAX_THREAD_ID);
	spin_lock(&pool->lock);
	thr = pool->thrs[id];
	spin_unlock(&pool->lock);

	return thr;
}

void kt_thr_start(kt_thr_t *thr)
{
	void *cpu = this_cpu_ptr(kt_ctx.cpus);

	kt_trace_add_event(thr, kt_event_type_thr_start, 0);
	kt_clk_tick(&thr->clk, thr->id);

	KT_ATOMIC_64_SET(&thr->cpu, &cpu);
}

void kt_thr_stop(kt_thr_t *thr)
{
	void *cpu = NULL;

	kt_trace_add_event(thr, kt_event_type_thr_stop, 0);
	kt_clk_tick(&thr->clk, thr->id);

	KT_ATOMIC_64_SET(&thr->cpu, &cpu);
}

void kt_thr_wakeup(kt_thr_t *thr, kt_thr_t *other)
{
	kt_clk_acquire(thr, &other->clk, &thr->clk);
}
