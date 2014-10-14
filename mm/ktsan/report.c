#include "ktsan.h"

#include <linux/printk.h>
#include <linux/thread_info.h>
#include <linux/spinlock.h>

#define MAX_FUNCTION_NAME_SIZE (128)

DEFINE_SPINLOCK(kt_report_lock);

void kt_report_race(kt_thr_t *new, kt_race_info_t *info)
{
	int i;
	char function[MAX_FUNCTION_NAME_SIZE];
	kt_thr_t *old;
	kt_stack_t stack;

	sprintf(function, "%pS", (void *)info->strip_addr);
	for (i = 0; i < MAX_FUNCTION_NAME_SIZE; i++) {
		if (function[i] == '+') {
			function[i] = '\0';
			break;
		}
	}

	spin_lock(&kt_report_lock);

	/* TODO(xairy): print kernel thread id in a report. */
	pr_err("==================================================================\n");
	pr_err("ThreadSanitizer: data-race in %s\n", function);
	pr_err("\n");

	pr_err("%s of size %d by thread T%d (K%d):\n",
		info->new.read ? "Read" : "Write",
		(1 << info->new.size), info->new.tid, new->kid);
	kt_stack_print_current(info->strip_addr);
	pr_err("DBG: cpu = %lx\n", (uptr_t)new->cpu);
	pr_err("\n");

	/* FIXME(xairy): stack might be wrong if id was reassigned. */
	old = kt_thr_get(info->old.tid);

	if (old == NULL) {
		pr_err("Previous %s of size %d by thread T%d:\n",
			info->old.read ? "read" : "write",
			(1 << info->old.size), info->old.tid);
		pr_err("No stack available.\n");
	} else {
		pr_err("Previous %s of size %d by thread T%d (K%d):\n",
			info->old.read ? "read" : "write",
			(1 << info->old.size), info->old.tid, old->kid);
		kt_trace_restore_stack(old, info->old.clock, &stack);
		kt_stack_print(&stack);
		pr_err("DBG: cpu = %lx\n", (uptr_t)old->cpu);
	}
	pr_err("\n");

	pr_err("DBG: addr: %lx\n", info->addr);
	pr_err("DBG: first offset: %d, second offset: %d\n",
		(int)info->old.offset, (int)info->new.offset);
	pr_err("DBG: T%d clock: {T%d: %lu, T%d: %lu}\n", new->id,
			new->id, kt_clk_get(&new->clk, new->id),
			old->id, kt_clk_get(&new->clk, old->id));
	pr_err("DBG: T%d clock: {T%d: %lu}\n", old->id,
			old->id, (unsigned long)info->old.clock);

	pr_err("==================================================================\n");

	spin_unlock(&kt_report_lock);
}
