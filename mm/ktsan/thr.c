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
	pool->new_pid = 0;
	INIT_LIST_HEAD(&pool->quarantine);
	pool->quarantine_size = 0;
	kt_spin_init(&pool->lock);
}

kt_thr_t *kt_thr_create(kt_thr_t *thr, int pid)
{
	kt_thr_pool_t *pool = &kt_ctx.thr_pool;
	kt_thr_t *new;
	int i;

	kt_spin_lock(&pool->lock);
	if ((new = kt_cache_alloc(&pool->cache)) != NULL) {
		new->id = pool->new_id;
		pool->new_id++;
		pool->thrs[new->id] = new;
	} else if (pool->quarantine_size) {
		new = list_first_entry(&pool->quarantine,
				kt_thr_t, quarantine_list);
		list_del(&new->quarantine_list);
		pool->quarantine_size--;
	} else {
		pr_err("KTSAN: maximum number of threads is reached\n");
		BUG();
	}
	/* Kernel does not assign PIDs to some threads, give them fake
	 * negative PIDs so that they are distinguishable in reports. */
	if (pid == 0)
		pid = --pool->new_pid;
	kt_spin_unlock(&pool->lock);

	new->pid = pid;
	new->inside = 0;
	new->cpu = NULL;
	kt_clk_init(&new->clk);
	kt_clk_init(&new->acquire_clk);
	new->acquire_active = 0;
	kt_clk_init(&new->release_clk);
	new->release_active = 0;
	new->stack.size = 0;
	kt_trace_init(&new->trace);
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
	new->interrupt_depth = 0;

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
	BUG_ON(old->interrupt_depth != 0);

	kt_spin_lock(&pool->lock);
	list_add_tail(&old->quarantine_list, &pool->quarantine);
	pool->quarantine_size++;
	kt_spin_unlock(&pool->lock);

	kt_stat_inc(thr, kt_stat_thread_destroy);
	kt_stat_dec(thr, kt_stat_threads);
}

kt_thr_t *kt_thr_get(int id)
{
	kt_thr_pool_t *pool = &kt_ctx.thr_pool;
	void *thr;

	BUG_ON(id < 0);
	BUG_ON(id >= KT_MAX_THREAD_COUNT);
	kt_spin_lock(&pool->lock);
	thr = pool->thrs[id];
	kt_spin_unlock(&pool->lock);

	return thr;
}

void kt_thr_start(kt_thr_t *thr, uptr_t pc)
{
	kt_trace_add_event(thr, kt_event_thr_start,
		smp_processor_id() | ((u32)thr->pid << 16));
	kt_clk_tick(&thr->clk, thr->id);

	thr->cpu = this_cpu_ptr(kt_ctx.cpus);
	BUG_ON(thr->cpu->thr != NULL);
	thr->cpu->thr = thr;
}

void kt_thr_stop(kt_thr_t *thr, uptr_t pc)
{
	BUG_ON(thr->event_disable_depth != 0);

	/* Current thread might be rescheduled even if preemption is disabled
	   (for example using might_sleep()). Therefore, percpu syncs won't
	   be released before thread switching. Release them here. */
	kt_percpu_release(thr, pc);

	kt_trace_add_event(thr, kt_event_thr_stop, smp_processor_id());
	kt_clk_tick(&thr->clk, thr->id);

	BUG_ON(thr->cpu == NULL);
	BUG_ON(thr->cpu->thr != thr);
	thr->cpu->thr = NULL;
	thr->cpu = NULL;
}

void kt_thr_wakeup(kt_thr_t *thr, kt_thr_t *other)
{
	kt_clk_acquire(&other->clk, &thr->clk);
}

/* Returns true if events were enabled before the call. */
bool kt_thr_event_disable(kt_thr_t *thr, uptr_t pc, unsigned long *flags)
{
#if KT_DEBUG
	kt_trace_add_event(thr, kt_event_event_disable, kt_compress(pc));
	kt_clk_tick(&thr->clk, thr->id);
	if (thr->event_disable_depth == 0)
		thr->last_event_disable_time = kt_clk_get(&thr->clk, thr->id);
#endif /* KT_DEBUG */

	thr->event_disable_depth++;
	BUG_ON(thr->event_disable_depth >= 3);

	if (thr->event_disable_depth - 1 == 0) {
                /* Disable interrupts as well. Otherwise all events
		   that happen in interrupts will be ignored. */
                thr->irq_flags_before_disable = *flags;
                /* Set all disabled in *flags. */
                *flags = arch_local_irq_save();
	}

	return (thr->event_disable_depth - 1 == 0);
}

/* Returns true if events became enabled after the call. */
bool kt_thr_event_enable(kt_thr_t *thr, uptr_t pc, unsigned long *flags)
{
#if KT_DEBUG
	kt_trace_add_event(thr, kt_event_event_enable, kt_compress(pc));
	kt_clk_tick(&thr->clk, thr->id);
	if (thr->event_disable_depth - 1 == 0)
		thr->last_event_enable_time = kt_clk_get(&thr->clk, thr->id);
#endif /* KT_DEBUG */

	thr->event_disable_depth--;
	BUG_ON(thr->event_disable_depth < 0);

	if (thr->event_disable_depth == 0) {
		BUG_ON(!arch_irqs_disabled());
                *flags = thr->irq_flags_before_disable; 
	}

	return (thr->event_disable_depth == 0);
}

void kt_thr_interrupt(kt_thr_t *thr, uptr_t pc, kt_interrupted_t *state)
{
	BUG_ON(state->thr != NULL);
	state->thr = thr;

	BUG_ON(thr->event_disable_depth);
	/* FIXME: fails during boot.
	 * How can we receive an interrupt when interrupts are disabled?
	 * We probably miss some enable of interrupt.
	 * BUG_ON(thr->irqs_disabled);
	 */

	memcpy(&state->stack.pc[0], &thr->stack.pc[0],
		thr->stack.size * sizeof(thr->stack.pc[0]));
	state->stack.size = thr->stack.size;
	thr->stack.size = 0;

	state->mutexset = thr->mutexset;
	thr->mutexset.size = 0;

	state->acquire_active = thr->acquire_active;
	if (thr->acquire_active) {
		thr->acquire_active = 0;
		state->acquire_clk = thr->acquire_clk;
	}
	state->release_active = thr->release_active;
	if (thr->release_active) {
		thr->release_active = 0;
		state->release_clk = thr->release_clk;
	}

	state->read_disable_depth = thr->read_disable_depth;
	thr->read_disable_depth = 0;
	state->report_disable_depth = thr->report_disable_depth;
	thr->report_disable_depth = 0;
	state->preempt_disable_depth = thr->preempt_disable_depth;
	thr->preempt_disable_depth = 0;

	list_replace_init(&thr->percpu_list, &state->percpu_list);

	memcpy(&state->seqcount, &thr->seqcount, sizeof(state->seqcount));
	memcpy(&state->seqcount_pc, &thr->seqcount_pc,
		sizeof(state->seqcount_pc));
	memset(&thr->seqcount, 0, sizeof(thr->seqcount));
	memset(&thr->seqcount_pc, 0, sizeof(thr->seqcount_pc));
	state->seqcount_ignore = thr->seqcount_ignore;
	thr->seqcount_ignore = 0;

	/* This resets stack and mutexset during trace replay. */
	kt_trace_add_event(thr, kt_event_interrupt, 0);
	kt_clk_tick(&thr->clk, thr->id);
}

void kt_thr_resume(kt_thr_t *thr, uptr_t pc, kt_interrupted_t *state)
{
	int i;

	BUG_ON(state->thr != thr);
	state->thr = NULL;

	BUG_ON(thr->mutexset.size);
	BUG_ON(thr->event_disable_depth);
	BUG_ON(thr->read_disable_depth);
	BUG_ON(thr->report_disable_depth);
	BUG_ON(thr->preempt_disable_depth);
	BUG_ON(thr->seqcount[0]);
	BUG_ON(thr->seqcount_ignore);

	/* This resets stack and mutexset during trace replay. */
	kt_trace_add_event(thr, kt_event_interrupt, 0);
	kt_clk_tick(&thr->clk, thr->id);
	thr->stack.size = 0;
	for (i = 0; i < state->stack.size; i++)
		kt_func_entry(thr, kt_decompress(state->stack.pc[i]));

	thr->mutexset = state->mutexset;
	for (i = 0; i < thr->mutexset.size; i++) {
		kt_locked_mutex_t *mtx = &thr->mutexset.mtx[i];

		kt_trace_add_event2(thr, mtx->write ? kt_event_lock :
			kt_event_rlock, mtx->uid, mtx->stack);
		kt_clk_tick(&thr->clk, thr->id);
	}

	thr->acquire_active = state->acquire_active;
	if (thr->acquire_active)
		thr->acquire_clk = state->acquire_clk;
	thr->release_active = state->release_active;
	if (thr->release_active)
		thr->release_clk = state->release_clk;

	thr->read_disable_depth = state->read_disable_depth;
	thr->report_disable_depth = state->report_disable_depth;
	thr->preempt_disable_depth = state->preempt_disable_depth;

	kt_percpu_release(thr, pc);
	list_replace_init(&state->percpu_list, &thr->percpu_list);

	memcpy(&thr->seqcount, &state->seqcount, sizeof(thr->seqcount));
	memcpy(&thr->seqcount_pc, &state->seqcount_pc,
		sizeof(thr->seqcount_pc));
	thr->seqcount_ignore = state->seqcount_ignore;
}
