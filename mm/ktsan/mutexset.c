#include "ktsan.h"

void kt_mutexset_init(kt_mutexset_t *set)
{
	set->size = 0;
}

void kt_mutexset_lock(kt_mutexset_t *set, u64 uid, kt_stack_handle_t stk,
	bool wr)
{
	kt_locked_mutex_t *mtx;

	BUG_ON(set->size >= ARRAY_SIZE(set->mtx));
	mtx = &set->mtx[set->size++];
	mtx->uid = uid;
	mtx->write = wr;
	mtx->stack = stk;
}

void kt_mutexset_unlock(kt_mutexset_t *set, u64 uid, bool wr)
{
	int i;

	for (i = set->size - 1; i >= 0; i--) {
		if (set->mtx[i].uid == uid) {
			BUG_ON(set->mtx[i].write != wr);
			set->mtx[i] = set->mtx[set->size - 1];
			set->size--;
			return;
		}
	}
	BUG();
}

/* Add mutex to mutexset and add event to trace. */
void kt_mutex_lock(kt_thr_t *thr, uptr_t pc, u64 sync_uid, bool write)
{
	kt_stack_handle_t stack_handle;

	/* Temporary push the pc onto stack so that it is recorded. */
	kt_func_entry(thr, pc);
	stack_handle = kt_stack_depot_save(&kt_ctx.stack_depot, &thr->stack);
	kt_trace_add_event2(thr, write ? kt_event_lock : kt_event_rlock,
				sync_uid, stack_handle);
	kt_mutexset_lock(&thr->mutexset, sync_uid, stack_handle, write);
	kt_func_exit(thr);
}

/* Remove mutex from mutexset and add event to trace. */
void kt_mutex_unlock(kt_thr_t *thr, u64 sync_uid, bool write)
{
	kt_trace_add_event(thr, write ? kt_event_unlock : kt_event_runlock,
				sync_uid);
	kt_mutexset_unlock(&thr->mutexset, sync_uid, write);
}
