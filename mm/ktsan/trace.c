#include "ktsan.h"

#include <linux/kernel.h>
#include <linux/spinlock.h>

static inline void kt_trace_switch(kt_trace_t *trace)
{
	unsigned part;
	kt_part_header_t *header;
	unsigned long strip_addr;

	spin_lock(&trace->lock);

	part = trace->position / KT_TRACE_PART_SIZE;
	header = &trace->headers[part];

	/* Remove ktsan_* and kt_* frames from stack. */
	strip_addr = (uptr_t)__builtin_return_address(2);
	kt_stack_save_current(&header->stack, strip_addr);
	/* TODO: save time. */

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

	if (trace->setup == 0) {
		kt_trace_switch(trace);
		trace->setup = 1;
	}

	if ((trace->position % KT_TRACE_PART_SIZE) == 0)
		kt_trace_switch(trace);

	event.type = (int)type;
	event.pc = kt_pc_compress(addr);
	trace->events[trace->position] = event;
}

/* Saves restored stack to *stack. */
void kt_trace_restore_stack(kt_thr_t *thr, kt_stack_t *stack)
{
	kt_trace_t *trace;
	kt_time_t clock;
	unsigned part;
	kt_part_header_t *header;
	unsigned long beg, end, i;
	kt_event_t *event;

	trace = &thr->trace;
	clock = kt_clk_get(&thr->clk, thr->id);

	spin_lock(&trace->lock);

	part = (clock % KT_TRACE_SIZE) / KT_TRACE_PART_SIZE;
	BUG_ON(part >= KT_TRACE_PARTS);
	header = &trace->headers[part];
	/*BUG_ON(clock < header->clock);*/

	end = clock % KT_TRACE_SIZE;
	beg = round_down(end, KT_TRACE_PART_SIZE);

	memcpy(stack, &header->stack, sizeof(*stack));

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
			pr_err("  i: %lu, enter, pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} else if (event->type == kt_event_type_func_exit) {
			pc = kt_pc_decompress(event->pc);
			pr_err("  i: %lu, exit, pc: [<%p>] %pS\n",
				i, (void *)pc, (void *)pc);
		} /*else if (event->type == kt_event_type_mop) {
			pc = kt_pc_decompress(event->pc);
			pr_err("  i: %lu, mop, pc: %p\n",
				i, (void *)pc);
		} else if (event->type == kt_event_type_lock) {
			pr_err("  i: %lu, lock, pc: %lx\n",
				i, kt_pc_decompress(event->pc));
		} else if (event->type == kt_event_type_unlock) {
			pr_err("  i: %lu, unlock, pc: %lx\n",
				i, kt_pc_decompress(event->pc));
		}*/
	}
	pr_err("\n");
}
