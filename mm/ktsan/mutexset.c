#include "ktsan.h"

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
