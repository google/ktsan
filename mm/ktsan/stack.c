#include "ktsan.h"

#include <linux/kernel.h>
#include <linux/stacktrace.h>

static inline unsigned int pc_compress(unsigned long pc)
{
	return (pc & UINT_MAX);
}

static inline unsigned long pc_decompress(unsigned int pc)
{
	return ((ULONG_MAX - UINT_MAX) | pc);
}

static void kt_stack_save_current(kt_stack_t *stack, unsigned long strip_addr)
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

	end = trace_info.nr_entries;
	while (entries[beg] != strip_addr && beg < end)
		beg++;

	for (i = 0; i < end - beg; i++)
		stack->pc[i] = pc_compress(entries[beg + i]);
	stack->size = end - beg;
}

static void kt_stack_print(kt_stack_t *stack)
{
	int i;
	long pc;

	for (i = 0; i < stack->size; i++) {
		if (stack->pc[i] == UINT_MAX || stack->pc[i] == 0)
			break;
		pc = pc_decompress(stack->pc[i]);
		pr_err(" [<%p>] %pS\n", (void *)pc, (void *)pc);
	}
}

void kt_stack_print_current(unsigned long strip_addr)
{
	kt_stack_t stack;
	kt_stack_save_current(&stack, strip_addr);
	kt_stack_print(&stack);
}
