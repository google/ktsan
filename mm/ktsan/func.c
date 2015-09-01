#include "ktsan.h"

void kt_func_entry(kt_thr_t *thr, uptr_t pc)
{
	kt_trace_add_event(thr, kt_event_type_func_enter, kt_pc_compress(pc));
	kt_clk_tick(&thr->clk, thr->id);

	thr->call_depth++;
	BUG_ON(thr->call_depth >= KT_MAX_STACK_FRAMES);
}

void kt_func_exit(kt_thr_t *thr)
{
	kt_trace_add_event(thr, kt_event_type_func_exit, 0);
	kt_clk_tick(&thr->clk, thr->id);

	thr->call_depth--;
	BUG_ON(thr->call_depth < 0);
}
