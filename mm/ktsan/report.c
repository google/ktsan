#include "ktsan.h"

#include <linux/printk.h>
#include <linux/thread_info.h>
#include <linux/sort.h>
#include <linux/spinlock.h>

#define MAX_FUNCTION_NAME_SIZE (128)

static kt_spinlock_t kt_report_lock;

static unsigned long racy_pc[1024];
static unsigned nracy_pc;

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

void kt_report_sync_usage(void)
{
	int sync_objects_count = 0;
	int sync_entries_count = 0;
	kt_tab_part_t *part;
	kt_tab_obj_t *obj;
	kt_tab_sync_t *sync;
	int i, p, curr;
	uptr_t curr_pc;
	static int counter = 0;

	if (counter++ % 128 != 0)
		return;

	for (p = 0; p < kt_ctx.sync_tab.size; p++) {
		part = &kt_ctx.sync_tab.parts[p];
		kt_spin_lock(&part->lock);
		for (obj = part->head; obj != NULL; obj = obj->link) {
			sync = (kt_tab_sync_t *)obj;
			sync_objects[sync_objects_count++] = sync->pc;
		}
		kt_spin_unlock(&part->lock);
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
	pr_err("Most syncs created at (totally %d):\n", sync_objects_count);
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
	int i, n;
	unsigned long newpc, oldpc;
	char function[MAX_FUNCTION_NAME_SIZE];
	kt_thr_t *old;
	kt_trace_state_t state;

	if (new->report_disable_depth != 0)
		return;

	newpc = info->strip_addr;
	if (kt_supp_suppressed(newpc))
		return;

	old = kt_thr_get(info->old.tid);
	BUG_ON(old == NULL);
	kt_trace_restore_state(old, info->old.clock, &state);
	/* We use newpc/oldpc pair for report deduplication (see racy_pc).
	 * If we fail to restore second stack, use newpc/newpc pair instead.
	 * This is better than reporting tons of reports with missing stack.
	 */
	oldpc = newpc;
	if (state.stack.size > 0) {
		oldpc = kt_pc_decompress(state.stack.pc[state.stack.size - 1]);
		if (kt_supp_suppressed(oldpc))
			return;
	}
	n = kt_atomic32_load_no_ktsan(&nracy_pc);
	for (i = 0; i < n; i += 2) {
		if (newpc == racy_pc[i] && oldpc == racy_pc[i + 1])
			return;
	}

	sprintf(function, "%pS", (void *)newpc);
	for (i = 0; i < MAX_FUNCTION_NAME_SIZE; i++) {
		if (function[i] == '+') {
			function[i] = '\0';
			break;
		}
	}

	kt_spin_lock(&kt_report_lock);

	if (nracy_pc < ARRAY_SIZE(racy_pc)) {
		racy_pc[nracy_pc] = newpc;
		racy_pc[nracy_pc + 1] = oldpc;
		kt_atomic32_store_no_ktsan(&nracy_pc, nracy_pc + 2);
	}

	pr_err("==================================================================\n");
	pr_err("ThreadSanitizer: data-race in %s\n", function);
	pr_err("\n");

	pr_err("%s of size %d by thread T%d (K%d, CPU%d):\n",
		info->new.read ? "Read" : "Write", (1 << info->new.size),
		info->new.tid, new->kid, smp_processor_id());
	kt_stack_print_current(newpc);
	pr_err("\n");

	if (state.cpu_id == -1) {
		pr_err("Previous %s of size %d by thread T%d (K%d):\n",
			info->old.read ? "read" : "write",
			(1 << info->old.size), info->old.tid, old->kid);
	} else {
		pr_err("Previous %s of size %d by thread T%d (K%d, CPU%d):\n",
			info->old.read ? "read" : "write",
			(1 << info->old.size), info->old.tid,
			old->kid, state.cpu_id);
	}
	kt_stack_print(&state.stack);
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

	kt_spin_unlock(&kt_report_lock);
}

void kt_report_bad_mtx_unlock(kt_thr_t *new, kt_tab_sync_t *sync, uptr_t strip)
{
	kt_thr_t *old;
	kt_trace_state_t state;

	BUG_ON(sync->lock_tid == -1);
	BUG_ON(sync->lock_tid == new->id);

	old = kt_thr_get(sync->lock_tid);
	BUG_ON(old == NULL);
	kt_trace_restore_state(old, sync->last_lock_time, &state);

	pr_err("ThreadSanitizer: mutex unlocked in a different thread\n");

	pr_err("Unlock by T%d (K%d, CPU%d):\n",
		new->id, new->kid, smp_processor_id());
	kt_stack_print_current(strip);

	if (state.cpu_id == -1) {
		pr_err("Previous lock by T%d (K%d):\n", old->id, old->kid);
	} else {
		pr_err("Previous lock by T%d (K%d, CPU%d):\n",
			old->id, old->kid, state.cpu_id);
	}
	kt_stack_print(&state.stack);
}
