#include "ktsan.h"

#include <linux/irqflags.h>
#include <linux/list.h>
#include <linux/preempt.h>

void kt_percpu_release(kt_thr_t *thr, uptr_t pc)
{
	struct list_head *entry, *tmp;
	kt_percpu_sync_t *sync;

	list_for_each_safe(entry, tmp, &thr->percpu_list) {
		sync = list_entry(entry, kt_percpu_sync_t, list);
		list_del_init(entry);

		kt_trace_add_event(thr, kt_event_type_release, pc);
		kt_clk_tick(&thr->clk, thr->id);
		kt_sync_release(thr, pc, sync->addr);

		kt_cache_free(&kt_ctx.percpu_sync_cache, sync);
	}
}

void kt_percpu_acquire(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	kt_percpu_sync_t *percpu_sync;

	/* Since a sync object may be the first field of a per-cpu structure,
	   the per-cpu sync is bound to the second byte of the structure. */
	addr += 1;

	/* This BUG_ON is failing since a pointer to a per-cpu structure
	   may be acquired via &__get_cpu_var(...) before disabling
	   irqs/preemption without actually accessing the structure itself. */
	/* BUG_ON(thr->preempt_depth == 0 && !thr->irqs_disabled); */

	kt_trace_add_event(thr, kt_event_type_acquire, pc);
	kt_clk_tick(&thr->clk, thr->id);
	kt_sync_acquire(thr, pc, addr);

	list_for_each_entry(percpu_sync, &thr->percpu_list, list)
		if (percpu_sync->addr == addr)
			return;

	percpu_sync = kt_cache_alloc(&kt_ctx.percpu_sync_cache);
	BUG_ON(percpu_sync == NULL);
	percpu_sync->addr = addr;
	INIT_LIST_HEAD(&percpu_sync->list);
	list_add(&percpu_sync->list, &thr->percpu_list);
}

static void kt_percpu_try_release(kt_thr_t *thr, uptr_t pc)
{
	if (thr->preempt_depth > 0 || thr->irqs_disabled)
		return;

	kt_percpu_release(thr, pc);
}

void kt_preempt_add(kt_thr_t *thr, uptr_t pc, int value)
{
	kt_trace_add_event(thr, kt_event_type_preempt_disable, pc);
	kt_clk_tick(&thr->clk, thr->id);

	thr->preempt_depth += value;
}

void kt_preempt_sub(kt_thr_t *thr, uptr_t pc, int value)
{
	kt_trace_add_event(thr, kt_event_type_preempt_enable, pc);
	kt_clk_tick(&thr->clk, thr->id);

	thr->preempt_depth -= value;
	BUG_ON(thr->preempt_depth < 0);
	kt_percpu_try_release(thr, pc);
}

void kt_irq_disable(kt_thr_t *thr, uptr_t pc)
{
	kt_trace_add_event(thr, kt_event_type_irq_disable, pc);
	kt_clk_tick(&thr->clk, thr->id);

	thr->irqs_disabled = true;
}

void kt_irq_enable(kt_thr_t *thr, uptr_t pc)
{
	kt_trace_add_event(thr, kt_event_type_irq_enable, pc);
	kt_clk_tick(&thr->clk, thr->id);

	thr->irqs_disabled = false;
	kt_percpu_try_release(thr, pc);
}

void kt_irq_save(kt_thr_t *thr, uptr_t pc)
{
	kt_irq_disable(thr, pc);
}

void kt_irq_restore(kt_thr_t *thr, uptr_t pc, unsigned long flags)
{
	if (!irqs_disabled_flags(flags))
		kt_irq_enable(thr, pc);
}
