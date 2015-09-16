#include "ktsan.h"

#include <linux/preempt.h>

static void kt_tame_acquire_and_release_clocks(kt_thr_t *thr)
{
	if (thr->acquire_active)
		thr->acquire_active--;

	if (thr->release_active)
		thr->release_active--;
}

void kt_func_entry(kt_thr_t *thr, uptr_t pc)
{
	kt_stat_inc(thr, kt_stat_func_entry);
	kt_trace_add_event(thr, kt_event_func_enter, kt_compress(pc));
	kt_clk_tick(&thr->clk, thr->id);

	thr->stack.pc[thr->stack.size++] = pc;
	BUG_ON(thr->stack.size >= KT_MAX_STACK_FRAMES);

	kt_tame_acquire_and_release_clocks(thr);
}

void kt_func_exit(kt_thr_t *thr)
{
	kt_stat_inc(thr, kt_stat_func_exit);
	kt_trace_add_event(thr, kt_event_func_exit, 0);
	kt_clk_tick(&thr->clk, thr->id);

	thr->stack.size--;
	BUG_ON(thr->stack.size < 0);

	kt_tame_acquire_and_release_clocks(thr);
}
