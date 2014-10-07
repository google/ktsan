#include "ktsan.h"

void kt_func_entry(kt_thr_t *thr, uptr_t pc)
{
	thr->call_depth++;
	BUG_ON(thr->call_depth >= KT_MAX_STACK_FRAMES);

	kt_trace_add_event(thr, kt_event_type_func_enter, pc);
	kt_clk_tick(&thr->clk, thr->id);
}

void kt_func_exit(kt_thr_t *thr)
{
	thr->call_depth--;
	BUG_ON(thr->call_depth < 0);

	kt_trace_add_event(thr, kt_event_type_func_exit, 0);
	kt_clk_tick(&thr->clk, thr->id);
}
