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
	pool->head = 0;
	spin_lock_init(&pool->lock);
}

kt_thr_t *kt_thr_create(kt_thr_t *thr, int kid)
{
	kt_thr_pool_t *pool = &kt_ctx.thr_pool;
	kt_thr_t *new;

	spin_lock(&pool->lock);

	new = kt_cache_alloc(&pool->cache);
	BUG_ON(new == NULL); /* Out of memory. */

	BUG_ON(pool->head == -1); /* Out of ids. */
	new->id = pool->head;
	pool->thrs[pool->head] = new;
	pool->head = pool->ids[pool->head];

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
	pool->thrs[old->id] = NULL;
	pool->ids[old->id] = pool->head;
	pool->head = old->id;
	kt_cache_free(&kt_ctx.thr_pool.cache, old);
	spin_unlock(&pool->lock);

	kt_stat_inc(thr, kt_stat_thread_destroy);
	kt_stat_dec(thr, kt_stat_threads);
}

kt_thr_t *kt_thr_get(int id)
{
	kt_thr_pool_t *pool = &kt_ctx.thr_pool;
	void *thr;

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
