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

int u64_cmp(const void *a, const void *b)
{
	if (*(u64 *)a < *(u64 *)b)
		return -1;
	else if (*(u64 *)a > *(u64 *)b)
		return 1;
	return 0;
}

int sync_entry_cmp(const void *a, const void *b)
{
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
	static int counter; /* = 0 */

	if (counter++ % 8 != 0)
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

static void kt_print_mutexset(kt_mutexset_t *set)
{
	kt_locked_mutex_t *mtx;
	int i;

	for (i = 0; i < set->size; i++) {
		mtx = &set->mtx[i];
		pr_err("Mutex %llu is %slocked here:\n",
			mtx->uid, mtx->write ? "" : "read ");
		kt_stack_print(kt_stack_depot_get(
			&kt_ctx.stack_depot, mtx->stack), 0);
	}
}

static void print_mop(bool new, bool wr, uptr_t addr, int sz, int pid, int cpu)
{
	pr_err("%s at 0x%p of size %d by thread %d on CPU %d:\n",
		new ? (wr ? "Write" : "Read") :
			(wr ? "Previous write" : "Previous read"),
		(void *)addr, sz, pid, cpu);
}

void kt_report_race(kt_thr_t *new, kt_race_info_t *info)
{
	int i, n;
	kt_thr_t *old;
	uptr_t new_pc, old_pc;
	kt_trace_state_t old_state;
	char function[MAX_FUNCTION_NAME_SIZE];

	if (new->report_disable_depth != 0)
		return;

	BUG_ON(new->stack.size == 0);
	new_pc = kt_decompress(kt_trace_last_data(new));
	if (kt_supp_suppressed(new_pc))
		return;

	old = kt_thr_get(info->old.tid);
	BUG_ON(old == NULL);
	kt_trace_restore_state(old, info->old.clock, &old_state);
	if (old_state.stack.size == 0)
		return;
	old_pc = kt_decompress(
			old_state.stack.pc[old_state.stack.size - 1]);
	if (kt_supp_suppressed(old_pc))
		return;

	n = kt_atomic32_load_no_ktsan(&nracy_pc);
	for (i = 0; i < n; i += 2) {
		if (new_pc == racy_pc[i] && old_pc == racy_pc[i + 1])
			return;
	}

	sprintf(function, "%pS", (void *)new_pc);
	for (i = 0; i < MAX_FUNCTION_NAME_SIZE; i++) {
		if (function[i] == '+') {
			function[i] = '\0';
			break;
		}
	}

	kt_spin_lock(&kt_report_lock);

	if (nracy_pc < ARRAY_SIZE(racy_pc)) {
		racy_pc[nracy_pc] = new_pc;
		racy_pc[nracy_pc + 1] = old_pc;
		kt_atomic32_store_no_ktsan(&nracy_pc, nracy_pc + 2);
	}

	pr_err("==================================================================\n");
	pr_err("ThreadSanitizer: data-race in %s\n\n", function);

	print_mop(true, !info->new.read, info->addr, (1 << info->new.size),
		new->pid, smp_processor_id());
	kt_stack_print(&new->stack, new_pc);

	print_mop(false, !info->old.read, info->addr, (1 << info->old.size),
		old_state.pid, old_state.cpu_id);
	kt_stack_print(&old_state.stack, 0);

	if (new->mutexset.size) {
		pr_err("Mutexes locked by thread %d:\n", new->pid);
		kt_print_mutexset(&new->mutexset);
	}

	if (old_state.mutexset.size) {
		pr_err("Mutexes locked by thread %d:\n", old_state.pid);
		kt_print_mutexset(&old_state.mutexset);
	}

#if KT_DEBUG
	pr_err("Thread %d clock: {T%d: %lu, T%d: %lu}\n", new->pid,
			new->pid, kt_clk_get(&new->clk, new->id),
			old->pid, kt_clk_get(&new->clk, old->id));
	pr_err("Thread %d clock: {T%d: %lu}\n", old->pid,
			old->pid, (unsigned long)info->old.clock);
#endif /* KT_DEBUG */

#if KT_DEBUG_TRACE
	pr_err("\n");
	pr_err("Thread %d trace:\n", new->pid);
	kt_trace_dump(&new->trace, kt_clk_get(&new->clk, new->id) - 50,
				kt_clk_get(&new->clk, new->id));

	pr_err("\n");
	pr_err("Thread %d trace:\n", old->pid);
	kt_trace_dump(&old->trace, kt_clk_get(&new->clk, old->id) - 20,
				(uptr_t)info->old.clock + 50);
#endif /* KT_DEBUG_TRACE */
	pr_err("==================================================================\n");

	kt_stat_inc(kt_stat_reports);

	kt_spin_unlock(&kt_report_lock);
}

void kt_report_bad_mtx_unlock(kt_thr_t *new, uptr_t pc, kt_tab_sync_t *sync)
{
	kt_thr_t *old;
	kt_trace_state_t state;

	BUG_ON(sync->lock_tid == -1);
	BUG_ON(sync->lock_tid == new->id);

	old = kt_thr_get(sync->lock_tid);
	BUG_ON(old == NULL);
	kt_trace_restore_state(old, sync->last_lock_time, &state);
	if (state.stack.size == 0)
		return;

	pr_err("==================================================================\n");

	pr_err("ThreadSanitizer: mutex unlocked in a different thread\n");
	pr_err("\n");

	pr_err("Unlock by thread %d on CPU %d:\n",
		new->pid, smp_processor_id());
	kt_stack_print(&new->stack, pc);
	pr_err("\n");

	pr_err("Previous lock by thread %d on CPU %d:\n",
		state.pid, state.cpu_id);
	kt_stack_print(&state.stack, 0);
	pr_err("\n");

	pr_err("==================================================================\n");
}
