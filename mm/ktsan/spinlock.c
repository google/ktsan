#include "ktsan.h"

void kt_spin_init(kt_spinlock_t *l)
{
	l->state = 0;
}

void kt_spin_lock(kt_spinlock_t *l)
{
	for (;;) {
		if (kt_atomic8_exchange_no_ktsan(&l->state, 1) == 0)
			return;
		while (kt_atomic8_load_no_ktsan(&l->state) != 0)
			cpu_relax();
	}
}

void kt_spin_unlock(kt_spinlock_t *l)
{
	kt_thread_fence_no_ktsan(ktsan_memory_order_release);
	kt_atomic8_store_no_ktsan(&l->state, 0);
}

int kt_spin_is_locked(kt_spinlock_t *l)
{
	return kt_atomic8_load_no_ktsan(&l->state) != 0;
}
