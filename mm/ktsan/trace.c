#include "ktsan.h"

#include <linux/kernel.h>
#include <linux/spinlock.h>

static inline void kt_trace_follow(kt_trace_t *trace, unsigned long beg,
				   unsigned long end, kt_trace_state_t *state)
{
	unsigned long i;
	kt_stack_handle_t stk;
	kt_event_t event;

	for (i = beg; i <= end; i++) {
		event = trace->events[i];
		if (event.type == kt_event_func_enter) {
			BUG_ON(state->stack.size + 1 == KT_MAX_STACK_FRAMES);
			state->stack.pc[state->stack.size] = event.data;
			state->stack.size++;
		} else if (event.type == kt_event_func_exit) {
			BUG_ON(state->stack.size <= 0);
			state->stack.size--;
		} else if (event.type == kt_event_thr_start) {
			int cpu = event.data & 0xffff;
			int pid = (s32)(u32)(event.data >> 16);
			BUG_ON(state->cpu_id != -1);
			state->cpu_id = cpu;
			state->pid = pid;
		} else if (event.type == kt_event_thr_stop) {
			BUG_ON(state->cpu_id != event.data);
			state->cpu_id = -1;
		} else if (event.type == kt_event_lock ||
				event.type == kt_event_rlock) {
			stk = *(u64*)&trace->events[++i];
			kt_mutexset_lock(&state->mutexset, event.data, stk,
				event.type == kt_event_lock);
		} else if (event.type == kt_event_unlock) {
			kt_mutexset_unlock(&state->mutexset, event.data, true);
		} else if (event.type == kt_event_runlock) {
			kt_mutexset_unlock(&state->mutexset, event.data, false);
		} else if (event.type == kt_event_interrupt) {
			state->stack.size = 0;
			state->mutexset.size = 0;
		}
	}
}

void kt_trace_switch(kt_thr_t *thr)
{
	kt_trace_t *trace;
	kt_time_t clock;
	unsigned part;
	kt_trace_part_header_t *header;

	trace = &thr->trace;
	clock = kt_clk_get(&thr->clk, thr->id);
	kt_spin_lock(&trace->lock);
	part = (clock % KT_TRACE_SIZE) / KT_TRACE_PART_SIZE;
	header = &trace->headers[part];
	header->state.stack = thr->stack;
	header->state.mutexset = thr->mutexset;
	header->state.pid = thr->pid;
	/* -1 for case we are called from kt_thr_start. */
	header->state.cpu_id = thr->cpu ? smp_processor_id() : -1;
	header->clock = clock;
	kt_spin_unlock(&trace->lock);
}

void kt_trace_add_event2(kt_thr_t *thr, kt_event_type_t type, u64 data,
	u64 data2)
{
	kt_time_t clock;
	unsigned pos;

	clock = kt_clk_get(&thr->clk, thr->id);
	if (((clock + 1) % KT_TRACE_PART_SIZE) == 0) {
		/* The trace would switch between the two data items.
		 * Push a fake event to precent it. */
		kt_trace_add_event(thr, kt_event_nop, 0);
		kt_clk_tick(&thr->clk, thr->id);
	}
	kt_trace_add_event(thr, type, data);
	kt_clk_tick(&thr->clk, thr->id);

	clock = kt_clk_get(&thr->clk, thr->id);
	pos = clock % KT_TRACE_SIZE;
	BUG_ON((pos % KT_TRACE_PART_SIZE) == 0);
	BUG_ON(sizeof(kt_event_t) != sizeof(data2));
	*(u64*)&thr->trace.events[pos] = data2;
}

void kt_trace_init(kt_trace_t *trace)
{
	int i;

	memset(trace, 0, sizeof(*trace));
	for (i = 0; i < KT_TRACE_PARTS; i++)
		trace->headers[i].state.cpu_id = -1;
	kt_spin_init(&trace->lock);
}

u64 kt_trace_last_data(kt_thr_t *thr)
{
	kt_time_t clock;
	kt_event_t event;

	clock = kt_clk_get(&thr->clk, thr->id) - 1;
	event = thr->trace.events[clock % KT_TRACE_SIZE];
	BUG_ON(event.type != kt_event_mop);
	return event.data;
}

void kt_trace_restore_state(kt_thr_t *thr, kt_time_t clock,
				kt_trace_state_t *state)
{
	kt_trace_t *trace;
	unsigned part;
	kt_trace_part_header_t *header;
	unsigned long beg, end;
	kt_event_t event;

	trace = &thr->trace;
	part = (clock % KT_TRACE_SIZE) / KT_TRACE_PART_SIZE;
	header = &trace->headers[part];

	kt_spin_lock(&trace->lock);

	if (header->clock > clock) {
		kt_spin_unlock(&trace->lock);
		memset(state, 0, sizeof(*state));
		state->cpu_id = -1;
		return;
	}

	memcpy(state, &header->state, sizeof(*state));
	end = clock % KT_TRACE_SIZE;
	beg = round_down(end, KT_TRACE_PART_SIZE);
	kt_trace_follow(trace, beg, end, state);

	event = trace->events[end];
	if (event.type != kt_event_nop
	    && event.type != kt_event_func_enter
	    && event.type != kt_event_func_exit
	    && event.type != kt_event_thr_stop
	    && event.type != kt_event_thr_start
	    && event.type != kt_event_lock
	    && event.type != kt_event_unlock
	    && event.type != kt_event_rlock
	    && event.type != kt_event_runlock
	) {
		BUG_ON(state->stack.size + 1 == KT_MAX_STACK_FRAMES);
		state->stack.pc[state->stack.size] = event.data;
		state->stack.size++;
	}

	kt_spin_unlock(&trace->lock);
}

void kt_trace_dump(kt_trace_t *trace, uptr_t beg, uptr_t end)
{
	unsigned long i;
	kt_event_t event;
	uptr_t pc;

	for (i = beg; i <= end; i++) {
		event = trace->events[i % KT_TRACE_SIZE];
		if (event.type == kt_event_mop) {
			pc = kt_decompress(event.data);
			pr_err(" i: %lu, access , pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} else if (event.type == kt_event_func_enter) {
			pc = kt_decompress(event.data);
			pr_err(" i: %lu, enter  , pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} else if (event.type == kt_event_func_exit) {
			pc = kt_decompress(event.data);
			pr_err(" i: %lu, exit   , pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} else if (event.type == kt_event_thr_stop) {
			int cpu = event.data & 0xffff;
			int pid = (s32)(u32)(event.data >> 16);
			pr_err(" i: %lu, stop   , cpu: %d, thread: %d\n",
				i, cpu, pid);
		} else if (event.type == kt_event_thr_start) {
			int cpu = event.data & 0xffff;
			int pid = (s32)(u32)(event.data >> 16);
			pr_err(" i: %lu, start  , cpu: %d, thread: %d\n",
				i, cpu, pid);
		} else if (event.type == kt_event_lock) {
			pr_err(" i: %lu, lock   , mutex: %llu\n",
				i, (u64)event.data);
			i++; /* consume stack id */
		} else if (event.type == kt_event_unlock) {
			pr_err(" i: %lu, unlock , mutex: %llu\n",
				i, (u64)event.data);
		} else if (event.type == kt_event_rlock) {
			pr_err(" i: %lu, rlock  , mutex: %llu\n",
				i, (u64)event.data);
			i++; /* consume stack id */
		} else if (event.type == kt_event_runlock) {
			pr_err(" i: %lu, runlock, mutex: %llu\n",
				i, (u64)event.data);
		} else if (event.type == kt_event_interrupt) {
			pr_err(" i: %lu, interrupt\n", i);
#if KT_DEBUG
		} else if (event.type == kt_event_preempt_enable) {
			pc = kt_decompress(event.data);
			pr_err(" i: %lu, prm on , pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} else if (event.type == kt_event_preempt_disable) {
			pc = kt_decompress(event.data);
			pr_err(" i: %lu, prm off, pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} else if (event.type == kt_event_irq_enable) {
			pc = kt_decompress(event.data);
			pr_err(" i: %lu, irq on , pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} else if (event.type == kt_event_irq_disable) {
			pc = kt_decompress(event.data);
			pr_err(" i: %lu, irq off, pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} else if (event.type == kt_event_acquire) {
			pc = kt_decompress(event.data);
			pr_err(" i: %lu, acquire, pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} else if (event.type == kt_event_release) {
			pc = kt_decompress(event.data);
			pr_err(" i: %lu, release, pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} else if (event.type == kt_event_nonmat_acquire) {
			pc = kt_decompress(event.data);
			pr_err(" i: %lu, nm acq , pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} else if (event.type == kt_event_nonmat_release) {
			pc = kt_decompress(event.data);
			pr_err(" i: %lu, nm rel , pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} else if (event.type == kt_event_membar_acquire) {
			pc = kt_decompress(event.data);
			pr_err(" i: %lu, mb acq , pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} else if (event.type == kt_event_membar_release) {
			pc = kt_decompress(event.data);
			pr_err(" i: %lu, mb rel , pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} else if (event.type == kt_event_event_enable) {
			pc = kt_decompress(event.data);
			pr_err(" i: %lu, evt on , pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} else if (event.type == kt_event_event_disable) {
			pc = kt_decompress(event.data);
			pr_err(" i: %lu, evt off, pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
#endif /* KT_DEBUG */
		}
	}
}
