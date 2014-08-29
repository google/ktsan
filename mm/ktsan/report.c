#include <linux/bug.h>
#include <linux/printk.h>
#include <linux/sched.h>
#include <linux/stacktrace.h>
#include <linux/thread_info.h>

#include <asm/page.h>
#include <asm/page_64.h>

#include "ktsan.h"

#define MAX_FUNCTION_NAME_SIZE (128)

static unsigned int compress_and_save_stack_trace(unsigned int *output,
			unsigned int max_entries, unsigned long strip_addr)
{
	unsigned long stack[KT_MAX_STACK_TRACE_FRAMES];
	unsigned int entries;
	unsigned int beg = 0, end, i;

	struct stack_trace trace_info = {
		.nr_entries = 0,
		.entries = stack,
		.max_entries = KT_MAX_STACK_TRACE_FRAMES,
		.skip = 0
	};
	save_stack_trace(&trace_info);
	entries = trace_info.nr_entries;

	while (stack[beg] != strip_addr && beg < entries)
		beg++;
	end = (entries - beg <= max_entries) ? entries : beg + max_entries;

	for (i = 0; i < end - beg; i++)
		output[i] = stack[beg + i] & UINT_MAX;
	return end - beg;
}

static void print_compressed_stack_trace(unsigned int *stack,
					 unsigned int entries)
{
	unsigned int i;
	unsigned long frame;

	for (i = 0; i < entries; i++) {
		if (stack[i] == UINT_MAX || stack[i] == 0)
			break;
		frame = (ULONG_MAX - UINT_MAX) | stack[i];
		pr_err(" [<%p>] %pS\n", (void *)frame, (void *)frame);
	}
}

void kt_print_current_stack_trace(unsigned long strip_addr)
{
	unsigned int stack[KT_MAX_STACK_TRACE_FRAMES];
	unsigned int entries = compress_and_save_stack_trace(&stack[0],
		KT_MAX_STACK_TRACE_FRAMES, strip_addr);
	print_compressed_stack_trace(&stack[0], entries);
}

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

	pr_err("==================================================================\n");
	pr_err("ThreadSanitizer: data-race in %s\n", function);
	pr_err("%s of size %d by thread T%d:\n",
		info->new.read ? "Read" : "Write",
		(1 << info->new.size), info->new.tid);
	kt_print_current_stack_trace(info->strip_addr); /* FIXME: ret ip */

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
