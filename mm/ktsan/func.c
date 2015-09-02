#include "ktsan.h"

#include <linux/preempt.h>

static void kt_tame_acquire_and_release_clocks(kt_thr_t *thr)
{
	/* Don't tame clocks in interrupts, since it might happen
	   between a barrier and a relaxed memory operation. */
	if (in_interrupt())
		return;

	if (thr->acquire_active)
		thr->acquire_active--;

	if (thr->release_active)
		thr->release_active--;
}

void kt_func_entry(kt_thr_t *thr, uptr_t pc)
{
	kt_trace_add_event(thr, kt_event_type_func_enter, kt_pc_compress(pc));
	kt_clk_tick(&thr->clk, thr->id);

	thr->call_depth++;
	BUG_ON(thr->call_depth >= KT_MAX_STACK_FRAMES);

	kt_tame_acquire_and_release_clocks(thr);
}

void kt_func_exit(kt_thr_t *thr)
{
	kt_trace_add_event(thr, kt_event_type_func_exit, 0);
	kt_clk_tick(&thr->clk, thr->id);

	thr->call_depth--;
	BUG_ON(thr->call_depth < 0);

	kt_tame_acquire_and_release_clocks(thr);
}
