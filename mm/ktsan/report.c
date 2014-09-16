#include "ktsan.h"

#include <linux/printk.h>
#include <linux/thread_info.h>

#define MAX_FUNCTION_NAME_SIZE (128)

void kt_report_race(kt_race_info_t *info)
{
	int i;
	char function[MAX_FUNCTION_NAME_SIZE];

	sprintf(function, "%pS", (void *)info->strip_addr);
	for (i = 0; i < MAX_FUNCTION_NAME_SIZE; i++) {
		if (function[i] == '+') {
			function[i] = '\0';
			break;
		}
	}

	/* TODO(xairy): print kernel thread id in a report. */
	pr_err("==================================================================\n");
	pr_err("ThreadSanitizer: data-race in %s\n", function);
	pr_err("%s of size %d by thread T%d:\n",
		info->new.read ? "Read" : "Write",
		(1 << info->new.size), info->new.tid);
	kt_stack_print_current(info->strip_addr); /* FIXME: ret ip */

	pr_err("Previous %s of size %d by thread T%d\n",
		info->old.read ? "read" : "write",
		(1 << info->old.size), info->old.tid);

	pr_err("DBG: addr: %lx\n", info->addr);
	pr_err("DBG: first offset: %d, second offset: %d\n",
		(int)info->old.offset, (int)info->new.offset);
	pr_err("DBG: first clock: %lu, second clock: %lu\n",
		(unsigned long)info->old.clock, (unsigned long)info->new.clock);

	/* TODO. */
	pr_err("==================================================================\n");
}
