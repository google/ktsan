#include "ktsan.h"

#include <linux/kernel.h>
#include <linux/spinlock.h>

static inline void kt_trace_follow(kt_trace_t *trace, unsigned long beg,
				   unsigned long end, kt_trace_state_t *state)
{
	unsigned long i;
	kt_event_t *event;

	for (i = beg; i <= end; i++) {
		event = &trace->events[i];
		if (event->type == kt_event_type_func_enter) {
			BUG_ON(state->stack.size + 1 == KT_MAX_STACK_FRAMES);
			state->stack.pc[state->stack.size] = event->data;
			state->stack.size++;
		} else if (event->type == kt_event_type_func_exit) {
			BUG_ON(state->stack.size <= 0);
			state->stack.size--;
		} else if (event->type == kt_event_type_thr_start) {
			BUG_ON(state->cpu_id != -1);
			state->cpu_id = event->data;
		} else if (event->type == kt_event_type_thr_stop) {
			BUG_ON(state->cpu_id != event->data);
			state->cpu_id = -1;
		}
	}
}

static inline void kt_trace_switch(kt_trace_t *trace, kt_time_t clock)
{
	unsigned part, prev_part;
	kt_trace_part_header_t *header, *prev_header;
	unsigned long beg, end;

	kt_spin_lock(&trace->lock);

	part = trace->position / KT_TRACE_PART_SIZE;
	header = &trace->headers[part];
	prev_part = (part == 0) ? (KT_TRACE_PARTS - 1) : (part - 1);
	prev_header = &trace->headers[prev_part];

	memcpy(&header->state, &prev_header->state, sizeof(header->state));
	beg = prev_part * KT_TRACE_PART_SIZE;
	end = (prev_part + 1) * KT_TRACE_PART_SIZE - 1;
	kt_trace_follow(trace, beg, end, &header->state);

	header->clock = clock;

	kt_spin_unlock(&trace->lock);
}

void kt_trace_init(kt_trace_t *trace)
{
	int i;

	memset(trace, 0, sizeof(*trace));
	for (i = 0; i < KT_TRACE_PARTS; i++)
		trace->headers[i].state.cpu_id = -1;
	kt_spin_init(&trace->lock);
}

void kt_trace_add_event(kt_thr_t *thr, kt_event_type_t type, u32 data)
{
	kt_trace_t *trace;
	kt_time_t clock;
	kt_event_t event;

	trace = &thr->trace;
	clock = kt_clk_get(&thr->clk, thr->id);

	trace->position = clock % KT_TRACE_SIZE;

	if ((trace->position % KT_TRACE_PART_SIZE) == 0)
		kt_trace_switch(trace, clock);

	event.type = (int)type;
	event.data = data;
	trace->events[trace->position] = event;
}

void kt_trace_restore_state(kt_thr_t *thr, kt_time_t clock,
				kt_trace_state_t *state)
{
	kt_trace_t *trace;
	unsigned part;
	kt_trace_part_header_t *header;
	unsigned long beg, end;
	kt_event_t *event;

	trace = &thr->trace;
	part = (clock % KT_TRACE_SIZE) / KT_TRACE_PART_SIZE;
	header = &trace->headers[part];

	kt_spin_lock(&trace->lock);

	if (header->clock > clock) {
		state->stack.size = 0;
		state->cpu_id = -1;
		kt_spin_unlock(&trace->lock);
		return;
	}

	memcpy(state, &header->state, sizeof(*state));
	end = clock % KT_TRACE_SIZE;
	beg = round_down(end, KT_TRACE_PART_SIZE);
	kt_trace_follow(trace, beg, end, state);

	event = &trace->events[end];
	if (event->type != kt_event_type_func_enter &&
	    event->type != kt_event_type_func_exit) {
		BUG_ON(state->stack.size + 1 == KT_MAX_STACK_FRAMES);
		state->stack.pc[state->stack.size] = event->data;
		state->stack.size++;
	}

	kt_spin_unlock(&trace->lock);
}

void kt_trace_dump(kt_trace_t *trace, uptr_t beg, uptr_t end)
{
	unsigned long i;
	kt_event_t *event;
	uptr_t pc;

	for (i = beg; i <= end; i++) {
		event = &trace->events[i % KT_TRACE_SIZE];
		if (event->type == kt_event_type_func_enter) {
			pc = kt_pc_decompress(event->data);
			pr_err(" i: %lu, enter  , pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} else if (event->type == kt_event_type_func_exit) {
			pc = kt_pc_decompress(event->data);
			pr_err(" i: %lu, exit   , pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} else if (event->type == kt_event_type_thr_stop) {
			pr_err(" i: %lu, stop   , cpu: %d\n", i, event->data);
		} else if (event->type == kt_event_type_thr_start) {
			pr_err(" i: %lu, start  , cpu: %d\n", i, event->data);
#if KT_DEBUG
		} else if (event->type == kt_event_type_lock) {
			pc = kt_pc_decompress(event->data);
			pr_err(" i: %lu, lock   , pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} else if (event->type == kt_event_type_unlock) {
			pc = kt_pc_decompress(event->data);
			pr_err(" i: %lu, unlock , pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} else if (event->type == kt_event_type_preempt_enable) {
			pc = kt_pc_decompress(event->data);
			pr_err(" i: %lu, prm on , pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} else if (event->type == kt_event_type_preempt_disable) {
			pc = kt_pc_decompress(event->data);
			pr_err(" i: %lu, prm off, pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} else if (event->type == kt_event_type_irq_enable) {
			pc = kt_pc_decompress(event->data);
			pr_err(" i: %lu, irq on , pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} else if (event->type == kt_event_type_irq_disable) {
			pc = kt_pc_decompress(event->data);
			pr_err(" i: %lu, irq off, pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} else if (event->type == kt_event_type_acquire) {
			pc = kt_pc_decompress(event->data);
			pr_err(" i: %lu, acquire, pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} else if (event->type == kt_event_type_release) {
			pc = kt_pc_decompress(event->data);
			pr_err(" i: %lu, release, pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} else if (event->type == kt_event_type_nonmat_acquire) {
			pc = kt_pc_decompress(event->data);
			pr_err(" i: %lu, nm acq , pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} else if (event->type == kt_event_type_nonmat_release) {
			pc = kt_pc_decompress(event->data);
			pr_err(" i: %lu, nm rel , pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} else if (event->type == kt_event_type_membar_acquire) {
			pc = kt_pc_decompress(event->data);
			pr_err(" i: %lu, mb acq , pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} else if (event->type == kt_event_type_membar_release) {
			pc = kt_pc_decompress(event->data);
			pr_err(" i: %lu, mb rel , pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} else if (event->type == kt_event_type_event_enable) {
			pc = kt_pc_decompress(event->data);
			pr_err(" i: %lu, evt on , pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} else if (event->type == kt_event_type_event_disable) {
			pc = kt_pc_decompress(event->data);
			pr_err(" i: %lu, evt off, pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
#endif /* KT_DEBUG */
		}
	}
}
