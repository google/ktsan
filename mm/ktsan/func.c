#include "ktsan.h"

void kt_func_entry(kt_thr_t *thr, uptr_t pc)
{
	kt_trace_add_event(thr, kt_event_type_func_enter, pc);
	kt_clk_tick(&thr->clk, thr->id);
}

void kt_func_exit(kt_thr_t *thr, uptr_t pc)
{
	kt_trace_add_event(thr, kt_event_type_func_exit, pc);
	kt_clk_tick(&thr->clk, thr->id);
}
