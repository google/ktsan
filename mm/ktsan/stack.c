#include "ktsan.h"

#include <linux/kernel.h>
#include <linux/stacktrace.h>

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
