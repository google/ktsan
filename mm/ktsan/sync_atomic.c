#include "ktsan.h"

#include <linux/atomic.h>
#include <linux/spinlock.h>

void kt_membar_acquire(kt_thr_t *thr, uptr_t pc)
{
	kt_clk_acquire(&thr->clk, &thr->acquire_clk);

	kt_trace_add_event(thr, kt_event_type_membar_acquire, pc);
	kt_clk_tick(&thr->clk, thr->id);
}

void kt_membar_release(kt_thr_t *thr, uptr_t pc)
{
	kt_clk_acquire(&thr->release_clk, &thr->clk);

	kt_trace_add_event(thr, kt_event_type_membar_release, pc);
	kt_clk_tick(&thr->clk, thr->id);
}

void kt_membar_acq_rel(kt_thr_t *thr, uptr_t pc)
{
	kt_membar_acquire(thr, pc);
	kt_membar_release(thr, pc);
}

/* FIXME(xairy): remove mmmo. */
#define KT_ATOMIC_OP(op, ad, mo, nmmo)					\
	do {								\
		kt_tab_sync_t *sync;					\
									\
		sync = kt_sync_ensure_created(thr, (ad));		\
									\
		if ((mo) == ktsan_memory_order_acquire ||		\
		    (mo) == ktsan_memory_order_acq_rel) {		\
			kt_clk_acquire(&thr->clk, &sync->clk);		\
			kt_trace_add_event(thr,				\
				kt_event_type_acquire, pc);		\
			kt_clk_tick(&thr->clk, thr->id);		\
		}							\
		if ((nmmo) == ktsan_memory_order_acquire ||		\
		    (nmmo) == ktsan_memory_order_acq_rel)	{	\
			kt_clk_acquire(&thr->acquire_clk, &sync->clk);	\
			kt_trace_add_event(thr,				\
				kt_event_type_nonmat_acquire, pc);	\
			kt_clk_tick(&thr->clk, thr->id);		\
		}							\
									\
		(op);							\
									\
		if ((mo) == ktsan_memory_order_release ||		\
		    (mo) == ktsan_memory_order_acq_rel) {		\
			kt_clk_acquire(&sync->clk, &thr->clk);		\
			kt_trace_add_event(thr,				\
				kt_event_type_release, pc);		\
			kt_clk_tick(&thr->clk, thr->id);		\
		}							\
		if ((nmmo) == ktsan_memory_order_release ||		\
		    (nmmo) == ktsan_memory_order_acq_rel) {		\
			kt_clk_acquire(&sync->clk, &thr->release_clk);	\
			kt_trace_add_event(thr,				\
				kt_event_type_nonmat_release, pc);	\
			kt_clk_tick(&thr->clk, thr->id);		\
		}							\
									\
		spin_unlock(&sync->tab.lock);				\
	} while (0)

void kt_atomic8_store(kt_thr_t *thr, uptr_t pc,
		void *addr, u8 value, ktsan_memory_order_t mo)
{
	KT_ATOMIC_OP(kt_atomic8_store_no_ktsan(addr, value),
			(uptr_t)addr, mo, ktsan_memory_order_acquire);
}

void kt_atomic16_store(kt_thr_t *thr, uptr_t pc,
		void *addr, u16 value, ktsan_memory_order_t mo)
{
	KT_ATOMIC_OP(kt_atomic16_store_no_ktsan(addr, value),
			(uptr_t)addr, mo, ktsan_memory_order_acquire);
}

void kt_atomic32_store(kt_thr_t *thr, uptr_t pc,
		void *addr, u32 value, ktsan_memory_order_t mo)
{
	KT_ATOMIC_OP(kt_atomic32_store_no_ktsan(addr, value),
			(uptr_t)addr, mo, ktsan_memory_order_acquire);
}

void kt_atomic64_store(kt_thr_t *thr, uptr_t pc,
		void *addr, u64 value, ktsan_memory_order_t mo)
{
	KT_ATOMIC_OP(kt_atomic64_store_no_ktsan(addr, value),
			(uptr_t)addr, mo, ktsan_memory_order_acquire);
}

u8 kt_atomic8_load(kt_thr_t *thr, uptr_t pc,
		void *addr, ktsan_memory_order_t mo)
{
	u8 rv;

	KT_ATOMIC_OP(rv = kt_atomic8_load_no_ktsan(addr),
			(uptr_t)addr, mo, ktsan_memory_order_acquire);

	return rv;
}

u16 kt_atomic16_load(kt_thr_t *thr, uptr_t pc,
		void *addr, ktsan_memory_order_t mo)
{
	u16 rv;

	KT_ATOMIC_OP(rv = kt_atomic16_load_no_ktsan(addr),
			(uptr_t)addr, mo, ktsan_memory_order_acquire);

	return rv;
}

u32 kt_atomic32_load(kt_thr_t *thr, uptr_t pc,
		void *addr, ktsan_memory_order_t mo)
{
	u32 rv;

	KT_ATOMIC_OP(rv = kt_atomic32_load_no_ktsan(addr),
			(uptr_t)addr, mo, ktsan_memory_order_acquire);

	return rv;
}

u64 kt_atomic64_load(kt_thr_t *thr, uptr_t pc,
		void *addr, ktsan_memory_order_t mo)
{
	u64 rv;

	KT_ATOMIC_OP(rv = kt_atomic64_load_no_ktsan(addr),
			(uptr_t)addr, mo, ktsan_memory_order_acquire);

	return rv;
}

u8 kt_atomic8_exchange(kt_thr_t *thr, uptr_t pc,
		void *addr, u8 value, ktsan_memory_order_t mo)
{
	u8 rv;

	KT_ATOMIC_OP(rv = kt_atomic8_exchange_no_ktsan(addr, value),
		(uptr_t)addr, mo, ktsan_memory_order_relaxed);

	return rv;
}

u16 kt_atomic16_exchange(kt_thr_t *thr, uptr_t pc,
		void *addr, u16 value, ktsan_memory_order_t mo)
{
	u16 rv;

	KT_ATOMIC_OP(rv = kt_atomic16_exchange_no_ktsan(addr, value),
		(uptr_t)addr, mo, ktsan_memory_order_relaxed);

	return rv;
}

u32 kt_atomic32_exchange(kt_thr_t *thr, uptr_t pc,
		void *addr, u32 value, ktsan_memory_order_t mo)
{
	u32 rv;

	KT_ATOMIC_OP(rv = kt_atomic32_exchange_no_ktsan(addr, value),
		(uptr_t)addr, mo, ktsan_memory_order_relaxed);

	return rv;
}

u64 kt_atomic64_exchange(kt_thr_t *thr, uptr_t pc,
		void *addr, u64 value, ktsan_memory_order_t mo)
{
	u64 rv;

	KT_ATOMIC_OP(rv = kt_atomic64_exchange_no_ktsan(addr, value),
		(uptr_t)addr, mo, ktsan_memory_order_relaxed);

	return rv;
}

u8 kt_atomic8_compare_exchange(kt_thr_t *thr, uptr_t pc,
		void *addr, u8 old, u8 new, ktsan_memory_order_t mo)
{
	u8 rv;

	KT_ATOMIC_OP(rv = kt_atomic8_compare_exchange_no_ktsan(addr, old, new),
		(uptr_t)addr, mo, ktsan_memory_order_relaxed);

	return rv;
}

u16 kt_atomic16_compare_exchange(kt_thr_t *thr, uptr_t pc,
		void *addr, u16 old, u16 new, ktsan_memory_order_t mo)
{
	u16 rv;

	KT_ATOMIC_OP(rv = kt_atomic16_compare_exchange_no_ktsan(addr, old, new),
		(uptr_t)addr, mo, ktsan_memory_order_relaxed);

	return rv;
}

u32 kt_atomic32_compare_exchange(kt_thr_t *thr, uptr_t pc,
		void *addr, u32 old, u32 new, ktsan_memory_order_t mo)
{
	u32 rv;

	KT_ATOMIC_OP(rv = kt_atomic32_compare_exchange_no_ktsan(addr, old, new),
		(uptr_t)addr, mo, ktsan_memory_order_relaxed);

	return rv;
}

u64 kt_atomic64_compare_exchange(kt_thr_t *thr, uptr_t pc,
		void *addr, u64 old, u64 new, ktsan_memory_order_t mo)
{
	u64 rv;

	KT_ATOMIC_OP(rv = kt_atomic64_compare_exchange_no_ktsan(addr, old, new),
		(uptr_t)addr, mo, ktsan_memory_order_relaxed);

	return rv;
}

u8 kt_atomic8_fetch_add(kt_thr_t *thr, uptr_t pc,
		void *addr, u8 value, ktsan_memory_order_t mo)
{
	u8 rv;

	KT_ATOMIC_OP(rv = kt_atomic8_fetch_add_no_ktsan(addr, value),
		(uptr_t)addr, mo, ktsan_memory_order_relaxed);

	return rv;
}

u16 kt_atomic16_fetch_add(kt_thr_t *thr, uptr_t pc,
		void *addr, u16 value, ktsan_memory_order_t mo)
{
	u16 rv;

	KT_ATOMIC_OP(rv = kt_atomic16_fetch_add_no_ktsan(addr, value),
		(uptr_t)addr, mo, ktsan_memory_order_relaxed);

	return rv;
}

u32 kt_atomic32_fetch_add(kt_thr_t *thr, uptr_t pc,
		void *addr, u32 value, ktsan_memory_order_t mo)
{
	u32 rv;

	KT_ATOMIC_OP(rv = kt_atomic32_fetch_add_no_ktsan(addr, value),
		(uptr_t)addr, mo, ktsan_memory_order_relaxed);

	return rv;
}

u64 kt_atomic64_fetch_add(kt_thr_t *thr, uptr_t pc,
		void *addr, u64 value, ktsan_memory_order_t mo)
{
	u64 rv;

	KT_ATOMIC_OP(rv = kt_atomic64_fetch_add_no_ktsan(addr, value),
		(uptr_t)addr, mo, ktsan_memory_order_relaxed);

	return rv;
}

/* FIXME(xairy). */

void kt_bitop_set_bit(kt_thr_t *thr, uptr_t pc, uptr_t addr, long nr)
{
	KT_ATOMIC_OP(
		kt_bitop_set_bit_no_ktsan((void *)addr, nr), addr,
		ktsan_memory_order_relaxed, ktsan_memory_order_release);
}

void kt_bitop_clear_bit(kt_thr_t *thr, uptr_t pc, uptr_t addr, long nr)
{
	KT_ATOMIC_OP(
		kt_bitop_clear_bit_no_ktsan((void *)addr, nr), addr,
		ktsan_memory_order_relaxed, ktsan_memory_order_release);
}

void kt_bitop_change_bit(kt_thr_t *thr, uptr_t pc, uptr_t addr, long nr)
{
	KT_ATOMIC_OP(
		kt_bitop_change_bit_no_ktsan((void *)addr, nr), addr,
		ktsan_memory_order_relaxed, ktsan_memory_order_acq_rel);
}

int kt_bitop_test_and_set_bit(kt_thr_t *thr, uptr_t pc, uptr_t addr, long nr)
{
	int rv;

	KT_ATOMIC_OP(
		rv = kt_bitop_test_and_set_bit_no_ktsan((void *)addr, nr),
		addr, ktsan_memory_order_acq_rel, ktsan_memory_order_relaxed);

	return rv;
}

int kt_bitop_test_and_clear_bit(kt_thr_t *thr, uptr_t pc, uptr_t addr, long nr)
{
	int rv;

	KT_ATOMIC_OP(
		rv = kt_bitop_test_and_clear_bit_no_ktsan((void *)addr, nr),
		addr, ktsan_memory_order_acq_rel, ktsan_memory_order_relaxed);

	return rv;
}

int kt_bitop_test_and_change_bit(kt_thr_t *thr, uptr_t pc, uptr_t addr, long nr)
{
	int rv;

	KT_ATOMIC_OP(
		rv = kt_bitop_test_and_change_bit_no_ktsan((void *)addr, nr),
		addr, ktsan_memory_order_acq_rel, ktsan_memory_order_relaxed);

	return rv;
}

int kt_bitop_test_and_set_bit_lock(kt_thr_t *thr, uptr_t pc, uptr_t addr, long nr)
{
	int rv;

	KT_ATOMIC_OP(
		rv = kt_bitop_test_and_set_bit_lock_no_ktsan((void *)addr, nr),
		addr, ktsan_memory_order_acq_rel, ktsan_memory_order_relaxed);

	return rv;
}

void kt_bitop_clear_bit_unlock(kt_thr_t *thr, uptr_t pc, uptr_t addr, long nr)
{
	KT_ATOMIC_OP(
		kt_bitop_clear_bit_unlock_no_ktsan((void *)addr, nr),
		addr, ktsan_memory_order_release, ktsan_memory_order_relaxed);
}
