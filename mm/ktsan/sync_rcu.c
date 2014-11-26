#include "ktsan.h"

#include <linux/spinlock.h>

void kt_rcu_read_lock(kt_thr_t *thr, uptr_t pc, kt_rcu_type_t type)
{
	kt_trace_add_event(thr, kt_event_type_rcu_read_lock, pc);
	kt_clk_tick(&thr->clk, thr->id);
}

void kt_rcu_read_unlock(kt_thr_t *thr, uptr_t pc, kt_rcu_type_t type)
{
	kt_trace_add_event(thr, kt_event_type_rcu_read_unlock, pc);
	kt_clk_tick(&thr->clk, thr->id);

	spin_lock(&kt_ctx.rcu_locks[type]);
	kt_clk_acquire(thr, &kt_ctx.rcu_clks[type], &thr->clk);
	spin_unlock(&kt_ctx.rcu_locks[type]);
}

void kt_rcu_synchronize(kt_thr_t *thr, uptr_t pc, kt_rcu_type_t type)
{
	kt_trace_add_event(thr, kt_event_type_rcu_synchronize, pc);
	kt_clk_tick(&thr->clk, thr->id);

	spin_lock(&kt_ctx.rcu_locks[type]);
	kt_clk_acquire(thr, &thr->clk, &kt_ctx.rcu_clks[type]);
	spin_unlock(&kt_ctx.rcu_locks[type]);
}

void kt_rcu_callback(kt_thr_t *thr, uptr_t pc, kt_rcu_type_t type)
{
	kt_trace_add_event(thr, kt_event_type_rcu_callback, pc);
	kt_clk_tick(&thr->clk, thr->id);

	spin_lock(&kt_ctx.rcu_locks[type]);
	kt_clk_acquire(thr, &thr->clk, &kt_ctx.rcu_clks[type]);
	spin_unlock(&kt_ctx.rcu_locks[type]);
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
