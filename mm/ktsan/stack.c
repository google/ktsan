#include "ktsan.h"

#include <linux/kernel.h>
#include <linux/stacktrace.h>

void kt_stack_copy(kt_stack_t *dst, kt_stack_t *src)
{
	/* Faster than *dst = *src, since copies only the needed frames. */
	memcpy(&dst->pc[0], &src->pc[0], src->size * sizeof(src->pc[0]));
	dst->size = src->size;
}

void kt_stack_print(kt_stack_t *stack, uptr_t top_pc)
{
	int i;
	long pc;

	if (top_pc)
		pr_err(" [<%p>] %pS\n", (void *)top_pc, (void *)top_pc);
	for (i = stack->size - 1; i >= 0; i--) {
		pc = kt_decompress(stack->pc[i]);
		pr_err(" [<%p>] %pS\n", (void *)pc, (void *)pc);
	}
	pr_err("\n");
}

#if KT_DEBUG
/* The following functions can't work reliably without frame pointers and are
 * for debugging only (consider removing -fomit-frame-pointer from Makefile
 * locally if you use them). */
void kt_stack_print_current(unsigned long strip_addr)
{
	kt_stack_t stack;

	kt_stack_save_current(&stack, strip_addr);
	kt_stack_print(&stack, 0);
}

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
#endif
