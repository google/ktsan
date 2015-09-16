#include "ktsan.h"

#include <linux/kernel.h>
#include <linux/stacktrace.h>

/* TODO: remove this function, it cannot be used without frame pointers. */
void kt_stack_save_current(kt_stack_t *stack, unsigned long strip_addr)
{
	unsigned long entries[KT_MAX_STACK_FRAMES];
	unsigned int beg = 0, end, i;

	struct stack_trace trace_info = {
		.nr_entries = 0,
		.entries = entries,
		.max_entries = KT_MAX_STACK_FRAMES,
		.skip = 0
	};
	save_stack_trace(&trace_info);

	/* The last frame is always 0xffffffffffffffff. */
	end = trace_info.nr_entries - 1;
	while (entries[beg] != strip_addr && beg < end)
		beg++;

	/* Save stack frames in reversed order (deepest first). */
	for (i = 0; i < end - beg; i++)
		stack->pc[i] = kt_compress(entries[end - 1 - i]);
	stack->size = end - beg;
}

void kt_stack_print(kt_stack_t *stack)
{
	int i;
	long pc;

	for (i = stack->size - 1; i >= 0; i--) {
		pc = kt_decompress(stack->pc[i]);
		pr_err(" [<%p>] %pS\n", (void *)pc, (void *)pc);
	}
	pr_err("\n");
}

/* TODO: remove this function, it cannot be used without frame pointers. */
void kt_stack_print_current(unsigned long strip_addr)
{
	kt_stack_t stack;

	kt_stack_save_current(&stack, strip_addr);
	kt_stack_print(&stack);
}
