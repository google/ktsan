#include "ktsan.h"

#include <linux/atomic.h>
#include <linux/spinlock.h>

void kt_thread_fence(kt_thr_t* thr, uptr_t pc, ktsan_memory_order_t mo)
{
	if (mo == ktsan_memory_order_acquire ||
	    mo == ktsan_memory_order_acq_rel) {
		kt_clk_acquire(&thr->clk, &thr->acquire_clk);

#if KT_DEBUG
		kt_trace_add_event(thr, kt_event_type_membar_acquire, pc);
		kt_clk_tick(&thr->clk, thr->id);
#endif
	}

	kt_thread_fence_no_ktsan(mo);

	if (mo == ktsan_memory_order_release ||
	    mo == ktsan_memory_order_acq_rel) {
		kt_clk_acquire(&thr->release_clk, &thr->clk);

#if KT_DEBUG
		kt_trace_add_event(thr, kt_event_type_membar_release, pc);
		kt_clk_tick(&thr->clk, thr->id);
#endif
	}
}

static void kt_atomic_pre_op(kt_thr_t *thr, uptr_t pc, kt_tab_sync_t *sync,
		ktsan_memory_order_t mo, bool read, bool write)
{
	if (mo == ktsan_memory_order_acquire ||
	    mo == ktsan_memory_order_acq_rel) {
		kt_clk_acquire(&thr->clk, &sync->clk);
#if KT_DEBUG
		kt_trace_add_event(thr, kt_event_type_acquire, pc);
		kt_clk_tick(&thr->clk, thr->id);
#endif /* KT_DEBUG */
		kt_thread_fence_no_ktsan(ktsan_memory_order_acquire);
	} else if (read) {
		kt_clk_acquire(&thr->acquire_clk, &sync->clk);
#if KT_DEBUG
		kt_trace_add_event(thr, kt_event_type_nonmat_acquire, pc);
		kt_clk_tick(&thr->clk, thr->id);
#endif /* KT_DEBUG */
	}
}

static void kt_atomic_post_op(kt_thr_t *thr, uptr_t pc, kt_tab_sync_t *sync,
		ktsan_memory_order_t mo, bool read, bool write)
{
	if (mo == ktsan_memory_order_release ||
	    mo == ktsan_memory_order_acq_rel) {
		kt_thread_fence_no_ktsan(ktsan_memory_order_release);
		kt_clk_acquire(&sync->clk, &thr->clk);
#if KT_DEBUG
		kt_trace_add_event(thr, kt_event_type_release, pc);
		kt_clk_tick(&thr->clk, thr->id);
#endif /* KT_DEBUG */
	} else if (write) {
		kt_clk_acquire(&sync->clk, &thr->release_clk);
#if KT_DEBUG
		kt_trace_add_event(thr, kt_event_type_nonmat_release, pc);
		kt_clk_tick(&thr->clk, thr->id);
#endif /* KT_DEBUG */
	}
}

#define KT_ATOMIC_OP(op, ad, mo, read, write)				\
do {									\
	kt_tab_sync_t *sync;						\
									\
	sync = kt_sync_ensure_created(thr, pc, (ad));			\
									\
	kt_atomic_pre_op(thr, pc, sync, mo, read, write);		\
									\
	(op);								\
									\
	kt_atomic_post_op(thr, pc, sync, mo, read, write);		\
									\
	kt_spin_unlock(&sync->tab.lock);				\
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
