#include "ktsan.h"

#include <linux/kernel.h>
#include <linux/spinlock.h>

static inline void kt_trace_follow(kt_trace_t *trace, unsigned long beg,
				   unsigned long end, kt_stack_t *stack)
{
	unsigned long i;
	kt_event_t *event;

	for (i = beg; i <= end; i++) {
		event = &trace->events[i];
		if (event->type == kt_event_type_func_enter) {
			BUG_ON(stack->size + 1 == KT_MAX_STACK_FRAMES);
			stack->pc[stack->size] = event->pc;
			stack->size++;
		} else if (event->type == kt_event_type_func_exit) {
			BUG_ON(stack->size <= 0);
			stack->size--;
		}
	}
}

static inline void kt_trace_switch(kt_trace_t *trace, kt_time_t clock)
{
	unsigned part, prev_part;
	kt_part_header_t *header, *prev_header;
	unsigned long beg, end;

	spin_lock(&trace->lock);

	part = trace->position / KT_TRACE_PART_SIZE;
	header = &trace->headers[part];
	prev_part = (part == 0) ? (KT_TRACE_PARTS - 1) : (part - 1);
	prev_header = &trace->headers[prev_part];

	memcpy(&header->stack, &prev_header->stack, sizeof(header->stack));
	beg = prev_part * KT_TRACE_PART_SIZE;
	end = (prev_part + 1) * KT_TRACE_PART_SIZE - 1;
	kt_trace_follow(trace, beg, end, &header->stack);

	header->clock = clock;

	spin_unlock(&trace->lock);
}

void kt_trace_init(kt_trace_t *trace)
{
	memset(trace, 0, sizeof(*trace));
	spin_lock_init(&trace->lock);
}

void kt_trace_add_event(kt_thr_t *thr, kt_event_type_t type, uptr_t addr)
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
	event.pc = kt_pc_compress(addr);
	trace->events[trace->position] = event;
}

/* Saves restored stack to *stack. */
void kt_trace_restore_stack(kt_thr_t *thr, kt_time_t clock, kt_stack_t *stack)
{
	kt_trace_t *trace;
	unsigned part;
	kt_part_header_t *header;
	unsigned long beg, end;
	kt_event_t *event;

	trace = &thr->trace;
	part = (clock % KT_TRACE_SIZE) / KT_TRACE_PART_SIZE;
	header = &trace->headers[part];

	spin_lock(&trace->lock);

	if (header->clock > clock) {
		stack->size = 0;
		spin_unlock(&trace->lock);
		return;
	}

	memcpy(stack, &header->stack, sizeof(*stack));
	end = clock % KT_TRACE_SIZE;
	beg = round_down(end, KT_TRACE_PART_SIZE);
	kt_trace_follow(trace, beg, end, stack);

	event = &trace->events[end];
	if (event->type == kt_event_type_mop) {
		BUG_ON(stack->size + 1 == KT_MAX_STACK_FRAMES);
		stack->pc[stack->size] = event->pc;
		stack->size++;
	}

	spin_unlock(&trace->lock);
}

void kt_trace_dump(kt_trace_t *trace, uptr_t beg, uptr_t end)
{
	unsigned long i;
	kt_event_t *event;
	uptr_t pc;

	pr_err("Trace dump:\n");
	for (i = beg; i <= end; i++) {
		event = &trace->events[i];
		if (event->type == kt_event_type_func_enter) {
			pc = kt_pc_decompress(event->pc);
			pr_err(" i: %lu, enter  , pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} else if (event->type == kt_event_type_func_exit) {
			pc = kt_pc_decompress(event->pc);
			pr_err(" i: %lu, exit   , pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} else if (event->type == kt_event_type_lock) {
			pc = kt_pc_decompress(event->pc);
			pr_err(" i: %lu, lock   , pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} else if (event->type == kt_event_type_unlock) {
			pc = kt_pc_decompress(event->pc);
			pr_err(" i: %lu, unlock , pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} else if (event->type == kt_event_type_thr_stop) {
			pc = kt_pc_decompress(event->pc);
			pr_err(" i: %lu, stop   , pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} else if (event->type == kt_event_type_thr_start) {
			pc = kt_pc_decompress(event->pc);
			pr_err(" i: %lu, start  , pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} else if (event->type == kt_event_type_preempt_enable) {
			pc = kt_pc_decompress(event->pc);
			pr_err(" i: %lu, prm on , pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} else if (event->type == kt_event_type_preempt_disable) {
			pc = kt_pc_decompress(event->pc);
			pr_err(" i: %lu, prm off, pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} else if (event->type == kt_event_type_irq_enable) {
			pc = kt_pc_decompress(event->pc);
			pr_err(" i: %lu, irq on , pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} else if (event->type == kt_event_type_irq_disable) {
			pc = kt_pc_decompress(event->pc);
			pr_err(" i: %lu, irq off, pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} else if (event->type == kt_event_type_acquire) {
			pc = kt_pc_decompress(event->pc);
			pr_err(" i: %lu, acquire, pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} else if (event->type == kt_event_type_release) {
			pc = kt_pc_decompress(event->pc);
			pr_err(" i: %lu, release, pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} else if (event->type == kt_event_type_nonmat_acquire) {
			pc = kt_pc_decompress(event->pc);
			pr_err(" i: %lu, nm acq , pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} else if (event->type == kt_event_type_nonmat_release) {
			pc = kt_pc_decompress(event->pc);
			pr_err(" i: %lu, nm rel , pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} else if (event->type == kt_event_type_membar_acquire) {
			pc = kt_pc_decompress(event->pc);
			pr_err(" i: %lu, mb acq , pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} else if (event->type == kt_event_type_membar_release) {
			pc = kt_pc_decompress(event->pc);
			pr_err(" i: %lu, mb rel , pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} else if (event->type == kt_event_type_event_enable) {
			pc = kt_pc_decompress(event->pc);
			pr_err(" i: %lu, evt on , pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} else if (event->type == kt_event_type_event_disable) {
			pc = kt_pc_decompress(event->pc);
			pr_err(" i: %lu, evt off, pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} /*else if (event->type == kt_event_type_mop) {
			pc = kt_pc_decompress(event->pc);
			pr_err(" i: %lu, mop, pc: %p\n",
				i, (void *)pc);
		} */
	}
}
