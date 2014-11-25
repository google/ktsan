#include "ktsan.h"

#include <linux/spinlock.h>

void kt_rcu_read_lock(kt_thr_t *thr, uptr_t pc)
{
	kt_trace_add_event(thr, kt_event_type_rcu_read_lock, pc);
	kt_clk_tick(&thr->clk, thr->id);
}

void kt_rcu_read_unlock(kt_thr_t *thr, uptr_t pc)
{
	kt_trace_add_event(thr, kt_event_type_rcu_read_unlock, pc);
	kt_clk_tick(&thr->clk, thr->id);

	spin_lock(&kt_ctx.rcu_lock);
	kt_clk_acquire(thr, &kt_ctx.rcu_clk, &thr->clk);
	spin_unlock(&kt_ctx.rcu_lock);
}

void kt_rcu_synchronize(kt_thr_t *thr, uptr_t pc)
{
	kt_trace_add_event(thr, kt_event_type_rcu_synchronize, pc);
	kt_clk_tick(&thr->clk, thr->id);

	spin_lock(&kt_ctx.rcu_lock);
	kt_clk_acquire(thr, &thr->clk, &kt_ctx.rcu_clk);
	spin_unlock(&kt_ctx.rcu_lock);
}

void kt_rcu_callback(kt_thr_t *thr, uptr_t pc)
{
	kt_trace_add_event(thr, kt_event_type_rcu_callback, pc);
	kt_clk_tick(&thr->clk, thr->id);

	spin_lock(&kt_ctx.rcu_lock);
	kt_clk_acquire(thr, &thr->clk, &kt_ctx.rcu_clk);
	spin_unlock(&kt_ctx.rcu_lock);
}

void kt_rcu_read_lock_bh(kt_thr_t *thr, uptr_t pc)
{
	kt_trace_add_event(thr, kt_event_type_rcu_read_lock, pc);
	kt_clk_tick(&thr->clk, thr->id);
}

void kt_rcu_read_unlock_bh(kt_thr_t *thr, uptr_t pc)
{
	kt_trace_add_event(thr, kt_event_type_rcu_read_unlock, pc);
	kt_clk_tick(&thr->clk, thr->id);

	spin_lock(&kt_ctx.rcu_bh_lock);
	kt_clk_acquire(thr, &kt_ctx.rcu_bh_clk, &thr->clk);
	spin_unlock(&kt_ctx.rcu_bh_lock);
}

void kt_rcu_synchronize_bh(kt_thr_t *thr, uptr_t pc)
{
	kt_trace_add_event(thr, kt_event_type_rcu_synchronize, pc);
	kt_clk_tick(&thr->clk, thr->id);

	spin_lock(&kt_ctx.rcu_bh_lock);
	kt_clk_acquire(thr, &thr->clk, &kt_ctx.rcu_bh_clk);
	spin_unlock(&kt_ctx.rcu_bh_lock);
}

void kt_rcu_callback_bh(kt_thr_t *thr, uptr_t pc)
{
	kt_trace_add_event(thr, kt_event_type_rcu_callback, pc);
	kt_clk_tick(&thr->clk, thr->id);

	spin_lock(&kt_ctx.rcu_bh_lock);
	kt_clk_acquire(thr, &thr->clk, &kt_ctx.rcu_bh_clk);
	spin_unlock(&kt_ctx.rcu_bh_lock);
}

void kt_rcu_read_lock_sched(kt_thr_t *thr, uptr_t pc)
{
	kt_trace_add_event(thr, kt_event_type_rcu_read_lock, pc);
	kt_clk_tick(&thr->clk, thr->id);
}

void kt_rcu_read_unlock_sched(kt_thr_t *thr, uptr_t pc)
{
	kt_trace_add_event(thr, kt_event_type_rcu_read_unlock, pc);
	kt_clk_tick(&thr->clk, thr->id);

	spin_lock(&kt_ctx.rcu_sched_lock);
	kt_clk_acquire(thr, &kt_ctx.rcu_sched_clk, &thr->clk);
	spin_unlock(&kt_ctx.rcu_sched_lock);
}

void kt_rcu_synchronize_sched(kt_thr_t *thr, uptr_t pc)
{
	kt_trace_add_event(thr, kt_event_type_rcu_synchronize, pc);
	kt_clk_tick(&thr->clk, thr->id);

	spin_lock(&kt_ctx.rcu_sched_lock);
	kt_clk_acquire(thr, &thr->clk, &kt_ctx.rcu_sched_clk);
	spin_unlock(&kt_ctx.rcu_sched_lock);
}

void kt_rcu_callback_sched(kt_thr_t *thr, uptr_t pc)
{
	kt_trace_add_event(thr, kt_event_type_rcu_callback, pc);
	kt_clk_tick(&thr->clk, thr->id);

	spin_lock(&kt_ctx.rcu_sched_lock);
	kt_clk_acquire(thr, &thr->clk, &kt_ctx.rcu_sched_clk);
	spin_unlock(&kt_ctx.rcu_sched_lock);
}

void kt_rcu_assign_pointer(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	kt_trace_add_event(thr, kt_event_type_rcu_assign_pointer, pc);
	kt_clk_tick(&thr->clk, thr->id);

	kt_sync_release(thr, pc, addr);
}

void kt_rcu_dereference(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	kt_trace_add_event(thr, kt_event_type_rcu_dereference, pc);
	kt_clk_tick(&thr->clk, thr->id);

	kt_sync_acquire(thr, pc, addr);
}
