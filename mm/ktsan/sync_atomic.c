#include "ktsan.h"

#include <linux/atomic.h>
#include <linux/spinlock.h>

void kt_thread_fence(kt_thr_t* thr, uptr_t pc, ktsan_memory_order_t mo)
{
	if (mo == ktsan_memory_order_acquire ||
	    mo == ktsan_memory_order_acq_rel) {
		if (thr->acquire_active) {
#if KT_DEBUG
			kt_trace_add_event(thr, kt_event_type_membar_acquire,
						kt_pc_compress(pc));
			kt_clk_tick(&thr->clk, thr->id);
#endif
			kt_clk_acquire(&thr->clk, &thr->acquire_clk);
		}
	}

	kt_thread_fence_no_ktsan(mo);

	if (mo == ktsan_memory_order_release ||
	    mo == ktsan_memory_order_acq_rel) {
#if KT_DEBUG
		kt_trace_add_event(thr, kt_event_type_membar_release,
					kt_pc_compress(pc));
		kt_clk_tick(&thr->clk, thr->id);
#endif
		if (thr->release_active)
			kt_clk_acquire(&thr->release_clk, &thr->clk);
		else
			kt_clk_set(&thr->release_clk, &thr->clk);
		thr->release_active = KT_TAME_COUNTER_LIMIT;
	}
}

static kt_tab_sync_t *kt_atomic_pre_op(kt_thr_t *thr, uptr_t pc, uptr_t addr,
	ktsan_memory_order_t mo, bool read, bool write, kt_tab_sync_t *sync)
{
	/* This will catch races between atomic operations and non-atomic
	 * writes (in particular with kfree).
	 */
	if (write)
		kt_access(thr, pc, addr, 0, true);

	if (mo == ktsan_memory_order_release ||
	    mo == ktsan_memory_order_acq_rel)
		kt_thread_fence_no_ktsan(ktsan_memory_order_release);

	if (mo == ktsan_memory_order_release ||
	    mo == ktsan_memory_order_acq_rel) {
		if (sync == NULL)
			sync = kt_sync_ensure_created(thr, pc, addr);
		if (sync == NULL)
			return NULL;
#if KT_DEBUG
		kt_trace_add_event(thr, kt_event_type_release,
					kt_pc_compress(pc));
		kt_clk_tick(&thr->clk, thr->id);
#endif /* KT_DEBUG */
		kt_clk_acquire(&sync->clk, &thr->clk);
	} else if (write) {
		if (thr->release_active) {
			if (sync == NULL)
				sync = kt_sync_ensure_created(thr, pc, addr);
			if (sync == NULL)
				return NULL;
#if KT_DEBUG
			kt_trace_add_event(thr, kt_event_type_nonmat_release,
						kt_pc_compress(pc));
			kt_clk_tick(&thr->clk, thr->id);
#endif /* KT_DEBUG */
			kt_clk_acquire(&sync->clk, &thr->release_clk);
		}
	}

	return sync;
}

static kt_tab_sync_t *kt_atomic_post_op(kt_thr_t *thr, uptr_t pc, uptr_t addr,
	ktsan_memory_order_t mo, bool read, bool write, kt_tab_sync_t *sync)
{
	if (mo == ktsan_memory_order_acquire ||
	    mo == ktsan_memory_order_acq_rel)
		kt_thread_fence_no_ktsan(ktsan_memory_order_acquire);

	if (sync == NULL) {
		sync = kt_tab_access(&kt_ctx.sync_tab, addr, NULL, false);
		if (sync == NULL)
			return NULL;
	}

	if (mo == ktsan_memory_order_acquire ||
	    mo == ktsan_memory_order_acq_rel) {
#if KT_DEBUG
		kt_trace_add_event(thr, kt_event_type_acquire,
					kt_pc_compress(pc));
		kt_clk_tick(&thr->clk, thr->id);
#endif /* KT_DEBUG */
		kt_clk_acquire(&thr->clk, &sync->clk);
	} else if (read) {
#if KT_DEBUG
		kt_trace_add_event(thr, kt_event_type_nonmat_acquire,
					kt_pc_compress(pc));
		kt_clk_tick(&thr->clk, thr->id);
#endif /* KT_DEBUG */
		if (thr->acquire_active)
			kt_clk_acquire(&thr->acquire_clk, &sync->clk);
		else
			kt_clk_set(&thr->acquire_clk, &sync->clk);
		thr->acquire_active = KT_TAME_COUNTER_LIMIT;
	}

	/* This will catch races between atomic operations and non-atomic
	 * writes (in particular with kfree).
	 */
	if (read && !write)
		kt_access(thr, pc, addr, 0, true);

	return sync;
}

#define KT_ATOMIC_OP(op, ad, mo, read, write)				\
do {									\
	kt_tab_sync_t *sync = NULL;					\
									\
	sync = kt_atomic_pre_op(thr, pc, ad, mo, read, write, sync);	\
									\
	(op);								\
									\
	sync = kt_atomic_post_op(thr, pc, ad, mo, read, write, sync);	\
									\
	if (sync != NULL)						\
		kt_spin_unlock(&sync->tab.lock);			\
} while (0)

void kt_atomic8_store(kt_thr_t *thr, uptr_t pc,
		void *addr, u8 value, ktsan_memory_order_t mo)
{
	KT_ATOMIC_OP(kt_atomic8_store_no_ktsan(addr, value),
			(uptr_t)addr, mo, false, true);
}

void kt_atomic16_store(kt_thr_t *thr, uptr_t pc,
		void *addr, u16 value, ktsan_memory_order_t mo)
{
	KT_ATOMIC_OP(kt_atomic16_store_no_ktsan(addr, value),
			(uptr_t)addr, mo, false, true);
}

void kt_atomic32_store(kt_thr_t *thr, uptr_t pc,
		void *addr, u32 value, ktsan_memory_order_t mo)
{
	KT_ATOMIC_OP(kt_atomic32_store_no_ktsan(addr, value),
			(uptr_t)addr, mo, false, true);
}

void kt_atomic64_store(kt_thr_t *thr, uptr_t pc,
		void *addr, u64 value, ktsan_memory_order_t mo)
{
	KT_ATOMIC_OP(kt_atomic64_store_no_ktsan(addr, value),
			(uptr_t)addr, mo, false, true);
}

u8 kt_atomic8_load(kt_thr_t *thr, uptr_t pc,
		void *addr, ktsan_memory_order_t mo)
{
	u8 rv;

	KT_ATOMIC_OP(rv = kt_atomic8_load_no_ktsan(addr),
			(uptr_t)addr, mo, true, false);

	return rv;
}

u16 kt_atomic16_load(kt_thr_t *thr, uptr_t pc,
		void *addr, ktsan_memory_order_t mo)
{
	u16 rv;

	KT_ATOMIC_OP(rv = kt_atomic16_load_no_ktsan(addr),
			(uptr_t)addr, mo, true, false);

	return rv;
}

u32 kt_atomic32_load(kt_thr_t *thr, uptr_t pc,
		void *addr, ktsan_memory_order_t mo)
{
	u32 rv;

	KT_ATOMIC_OP(rv = kt_atomic32_load_no_ktsan(addr),
			(uptr_t)addr, mo, true, false);

	return rv;
}

u64 kt_atomic64_load(kt_thr_t *thr, uptr_t pc,
		void *addr, ktsan_memory_order_t mo)
{
	u64 rv;

	KT_ATOMIC_OP(rv = kt_atomic64_load_no_ktsan(addr),
			(uptr_t)addr, mo, true, false);

	return rv;
}

u8 kt_atomic8_exchange(kt_thr_t *thr, uptr_t pc,
		void *addr, u8 value, ktsan_memory_order_t mo)
{
	u8 rv;

	KT_ATOMIC_OP(rv = kt_atomic8_exchange_no_ktsan(addr, value),
		(uptr_t)addr, mo, true, true);

	return rv;
}

u16 kt_atomic16_exchange(kt_thr_t *thr, uptr_t pc,
		void *addr, u16 value, ktsan_memory_order_t mo)
{
	u16 rv;

	KT_ATOMIC_OP(rv = kt_atomic16_exchange_no_ktsan(addr, value),
		(uptr_t)addr, mo, true, true);

	return rv;
}

u32 kt_atomic32_exchange(kt_thr_t *thr, uptr_t pc,
		void *addr, u32 value, ktsan_memory_order_t mo)
{
	u32 rv;

	KT_ATOMIC_OP(rv = kt_atomic32_exchange_no_ktsan(addr, value),
		(uptr_t)addr, mo, true, true);

	return rv;
}

u64 kt_atomic64_exchange(kt_thr_t *thr, uptr_t pc,
		void *addr, u64 value, ktsan_memory_order_t mo)
{
	u64 rv;

	KT_ATOMIC_OP(rv = kt_atomic64_exchange_no_ktsan(addr, value),
		(uptr_t)addr, mo, true, true);

	return rv;
}

u8 kt_atomic8_compare_exchange(kt_thr_t *thr, uptr_t pc,
		void *addr, u8 old, u8 new, ktsan_memory_order_t mo)
{
	u8 rv;

	KT_ATOMIC_OP(rv = kt_atomic8_compare_exchange_no_ktsan(addr, old, new),
		(uptr_t)addr, mo, true, true);

	return rv;
}

u16 kt_atomic16_compare_exchange(kt_thr_t *thr, uptr_t pc,
		void *addr, u16 old, u16 new, ktsan_memory_order_t mo)
{
	u16 rv;

	KT_ATOMIC_OP(rv = kt_atomic16_compare_exchange_no_ktsan(addr, old, new),
		(uptr_t)addr, mo, true, true);

	return rv;
}

u32 kt_atomic32_compare_exchange(kt_thr_t *thr, uptr_t pc,
		void *addr, u32 old, u32 new, ktsan_memory_order_t mo)
{
	u32 rv;

	KT_ATOMIC_OP(rv = kt_atomic32_compare_exchange_no_ktsan(addr, old, new),
		(uptr_t)addr, mo, true, true);

	return rv;
}

u64 kt_atomic64_compare_exchange(kt_thr_t *thr, uptr_t pc,
		void *addr, u64 old, u64 new, ktsan_memory_order_t mo)
{
	u64 rv;

	KT_ATOMIC_OP(rv = kt_atomic64_compare_exchange_no_ktsan(addr, old, new),
		(uptr_t)addr, mo, true, true);

	return rv;
}

u8 kt_atomic8_fetch_add(kt_thr_t *thr, uptr_t pc,
		void *addr, u8 value, ktsan_memory_order_t mo)
{
	u8 rv;

	KT_ATOMIC_OP(rv = kt_atomic8_fetch_add_no_ktsan(addr, value),
		(uptr_t)addr, mo, true, true);

	return rv;
}

u16 kt_atomic16_fetch_add(kt_thr_t *thr, uptr_t pc,
		void *addr, u16 value, ktsan_memory_order_t mo)
{
	u16 rv;

	KT_ATOMIC_OP(rv = kt_atomic16_fetch_add_no_ktsan(addr, value),
		(uptr_t)addr, mo, true, true);

	return rv;
}

u32 kt_atomic32_fetch_add(kt_thr_t *thr, uptr_t pc,
		void *addr, u32 value, ktsan_memory_order_t mo)
{
	u32 rv;

	KT_ATOMIC_OP(rv = kt_atomic32_fetch_add_no_ktsan(addr, value),
		(uptr_t)addr, mo, true, true);

	return rv;
}

u64 kt_atomic64_fetch_add(kt_thr_t *thr, uptr_t pc,
		void *addr, u64 value, ktsan_memory_order_t mo)
{
	u64 rv;

	KT_ATOMIC_OP(rv = kt_atomic64_fetch_add_no_ktsan(addr, value),
		(uptr_t)addr, mo, true, true);

	return rv;
}

void kt_atomic_set_bit(kt_thr_t *thr, uptr_t pc,
		void *addr, long nr, ktsan_memory_order_t mo)
{
	KT_ATOMIC_OP(kt_atomic_set_bit_no_ktsan(addr, nr),
		(uptr_t)addr, mo, false, true);
}

void kt_atomic_clear_bit(kt_thr_t *thr, uptr_t pc,
		void *addr, long nr, ktsan_memory_order_t mo)
{
	KT_ATOMIC_OP(kt_atomic_clear_bit_no_ktsan(addr, nr),
		(uptr_t)addr, mo, false, true);
}

void kt_atomic_change_bit(kt_thr_t *thr, uptr_t pc,
		void *addr, long nr, ktsan_memory_order_t mo)
{
	KT_ATOMIC_OP(kt_atomic_change_bit_no_ktsan(addr, nr),
		(uptr_t)addr, mo, true, true);
}

int kt_atomic_fetch_set_bit(kt_thr_t *thr, uptr_t pc,
		void *addr, long nr, ktsan_memory_order_t mo)
{
	int rv;

	KT_ATOMIC_OP(rv = kt_atomic_fetch_set_bit_no_ktsan(addr, nr),
		(uptr_t)addr, mo, true, true);

	return rv;
}

int kt_atomic_fetch_clear_bit(kt_thr_t *thr, uptr_t pc,
		void *addr, long nr, ktsan_memory_order_t mo)
{
	int rv;

	KT_ATOMIC_OP(rv = kt_atomic_fetch_clear_bit_no_ktsan(addr, nr),
		(uptr_t)addr, mo, true, true);

	return rv;
}

int kt_atomic_fetch_change_bit(kt_thr_t *thr, uptr_t pc,
		void *addr, long nr, ktsan_memory_order_t mo)
{
	int rv;

	KT_ATOMIC_OP(rv = kt_atomic_fetch_change_bit_no_ktsan(addr, nr),
		(uptr_t)addr, mo, true, true);

	return rv;
}
