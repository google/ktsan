#include "ktsan.h"

#include <linux/atomic.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/spinlock.h>

void kt_thr_pool_init(void)
{
	kt_thr_pool_t *pool = &kt_ctx.thr_pool;
	int i;

	kt_cache_init(&pool->cache, sizeof(kt_thr_t), KT_MAX_THREAD_ID);
	memset(pool->thrs, 0, sizeof(pool->thrs));
	for (i = 0; i < KT_MAX_THREAD_ID - 1; i++)
		pool->ids[i] = i + 1;
	pool->ids[KT_MAX_THREAD_ID - 1] = -1;
	pool->free_head = 0;
	pool->quarantine_head = -1;
	spin_lock_init(&pool->lock);
}

static void kt_thr_drain_quarantine(void)
{
	kt_thr_pool_t *pool = &kt_ctx.thr_pool;
	int prev_quarantine_head;

	BUG_ON(pool->quarantine_head == -1); /* Out of memory. */

	while (pool->quarantine_head != -1) {
		prev_quarantine_head = pool->quarantine_head;

		BUG_ON(pool->thrs[pool->quarantine_head] == NULL);
		kt_cache_free(&pool->cache, pool->thrs[pool->quarantine_head]);
		pool->thrs[pool->quarantine_head] = NULL;
		pool->quarantine_head = pool->ids[pool->quarantine_head];

		pool->ids[prev_quarantine_head] = pool->free_head;
		pool->free_head = prev_quarantine_head;
	}
}

kt_thr_t *kt_thr_create(kt_thr_t *thr, int kid)
{
	kt_thr_pool_t *pool = &kt_ctx.thr_pool;
	kt_thr_t *new;

	spin_lock(&pool->lock);

	BUG_ON(pool->free_head < -1);
	BUG_ON(pool->free_head >= KT_MAX_THREAD_ID);

	/* If the list of free thr objects is empty, free all from quarantine. */
	if (pool->free_head == -1) {
		kt_thr_drain_quarantine();
	}

	new = kt_cache_alloc(&pool->cache);
	BUG_ON(new == NULL);

	BUG_ON(pool->free_head == -1);
	new->id = pool->free_head;
	pool->thrs[pool->free_head] = new;
	pool->free_head = pool->ids[pool->free_head];

	spin_unlock(&pool->lock);

	new->kid = kid;
	kt_atomic32_pure_set(&new->inside, 0);
	new->cpu = NULL;
	kt_clk_init(thr, &new->clk);
	kt_trace_init(&new->trace);

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
	pool->ids[old->id] = pool->quarantine_head;
	pool->quarantine_head = old->id;
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

	KT_ATOMIC_64_SET(&thr->cpu, &cpu);
}

void kt_thr_stop(kt_thr_t *thr)
{
	void *cpu = NULL;

	KT_ATOMIC_64_SET(&thr->cpu, &cpu);
}

void kt_thr_wakeup(kt_thr_t *thr, kt_thr_t *other)
{
	kt_clk_acquire(thr, &other->clk, &thr->clk);
}
