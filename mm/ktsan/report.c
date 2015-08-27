#include "ktsan.h"

#include <linux/printk.h>
#include <linux/thread_info.h>
#include <linux/sort.h>
#include <linux/spinlock.h>

#define MAX_FUNCTION_NAME_SIZE (128)

DEFINE_SPINLOCK(kt_report_lock);

unsigned long last;

#if KT_DEBUG

uptr_t sync_objects[KT_MAX_SYNC_COUNT];

struct sync_entry_s {
	uptr_t pc;
	int count;
};

typedef struct sync_entry_s sync_entry_t;

sync_entry_t sync_entries[KT_MAX_SYNC_COUNT];

int u64_cmp(const void *a, const void *b) {
	if (*(u64 *)a < *(u64 *)b)
		return -1;
	else if (*(u64 *)a > *(u64 *)b)
		return 1;
	return 0;
}

int sync_entry_cmp(const void *a, const void *b) {
	sync_entry_t *sa = (sync_entry_t *)a;
	sync_entry_t *sb = (sync_entry_t *)b;
	return sb->count - sa->count;
}

static void kt_report_sync_usage(void)
{
	int sync_objects_count = 0;
	int sync_entries_count = 0;
	kt_tab_part_t *part;
	kt_tab_obj_t *obj;
	kt_tab_sync_t *sync;
	int i, p, curr;
	uptr_t curr_pc;
	static int counter = 0;

	if (counter++ % 64 != 0)
		return;

	for (p = 0; p < kt_ctx.sync_tab.size; p++) {
		part = &kt_ctx.sync_tab.parts[p];
		spin_lock(&part->lock);
		for (obj = part->head; obj != NULL; obj = obj->link) {
			sync = (kt_tab_sync_t *)obj;
			sync_objects[sync_objects_count++] = sync->pc;
		}
		spin_unlock(&part->lock);
	}

	sort(&sync_objects[0], sync_objects_count,
		sizeof(uptr_t), &u64_cmp, NULL);

	i = 0;
	while (i < sync_objects_count) {
		curr = 0;
		curr_pc = sync_objects[i];
		while (i < sync_objects_count && sync_objects[i] == curr_pc) {
			i++;
			curr++;
		}
		sync_entries[sync_entries_count].pc = curr_pc;
		sync_entries[sync_entries_count].count = curr;
		sync_entries_count++;
	}

	sort(&sync_entries[0], sync_entries_count, sizeof(sync_entry_t),
			&sync_entry_cmp, NULL);

	pr_err("\n");
	pr_err("Most syncs created at:\n");
	for (i = 0; i < 32; i++) {
		pr_err(" %6d [<%p>] %pS\n", sync_entries[i].count,
			(void *)sync_entries[i].pc, (void *)sync_entries[i].pc);
	}
}

#endif /* KT_DEBUG */

void kt_report_disable(kt_thr_t *thr)
{
	thr->report_disable_depth++;
}

void kt_report_enable(kt_thr_t *thr)
{
	thr->report_disable_depth--;
	BUG_ON(thr->report_disable_depth < 0);
}

void kt_report_race(kt_thr_t *new, kt_race_info_t *info)
{
	int i;
	char function[MAX_FUNCTION_NAME_SIZE];
	kt_thr_t *old;
	kt_stack_t stack;

	if (new->report_disable_depth != 0)
		return;

	sprintf(function, "%pS", (void *)info->strip_addr);
	for (i = 0; i < MAX_FUNCTION_NAME_SIZE; i++) {
		if (function[i] == '+') {
			function[i] = '\0';
			break;
		}
	}

	spin_lock(&kt_report_lock);

	if (info->addr == last) {
		spin_unlock(&kt_report_lock);
		return;
	}
	last = info->addr;

	pr_err("==================================================================\n");
	pr_err("ThreadSanitizer: data-race in %s\n", function);
	pr_err("\n");

	pr_err("%s of size %d by thread T%d (K%d):\n",
		info->new.read ? "Read" : "Write",
		(1 << info->new.size), info->new.tid, new->kid);
	kt_stack_print_current(info->strip_addr);
	pr_err("DBG: cpu = %lx\n", (uptr_t)new->cpu);
	pr_err("DBG: cpu id = %d\n", smp_processor_id());
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

#if KT_DEBUG_TRACE
	pr_err("\n");
	pr_err("T%d trace:\n", old->id);
	kt_trace_dump(&old->trace, kt_clk_get(&new->clk, old->id) - 20,
			(uptr_t)info->old.clock + 30);

	pr_err("\n");
	pr_err("T%d trace:\n", new->id);
	kt_trace_dump(&new->trace, kt_clk_get(&new->clk, new->id) - 30,
				kt_clk_get(&new->clk, new->id) + 30);
#endif /* KT_DEBUG_TRACE */

#if KT_DEBUG
	kt_report_sync_usage();
#endif /* KT_DEBUG */

	pr_err("==================================================================\n");

	kt_stat_inc(new, kt_stat_reports);

	spin_unlock(&kt_report_lock);
}
