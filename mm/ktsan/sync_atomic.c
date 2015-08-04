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
		if ((mo) == ktsan_memory_order_acquire ||			\
		    (mo) == ktsan_memory_order_acq_rel) {			\
			kt_clk_acquire(&thr->clk, &sync->clk);		\
			kt_trace_add_event(thr,				\
				kt_event_type_acquire, pc);		\
			kt_clk_tick(&thr->clk, thr->id);		\
		}							\
		if ((nmmo) == ktsan_memory_order_acquire ||		\
		    (nmmo) == ktsan_memory_order_acq_rel)	{		\
			kt_clk_acquire(&thr->acquire_clk, &sync->clk);	\
			kt_trace_add_event(thr,				\
				kt_event_type_nonmat_acquire, pc);	\
			kt_clk_tick(&thr->clk, thr->id);		\
		}							\
									\
		(op);							\
									\
		if ((mo) == ktsan_memory_order_release ||			\
		    (mo) == ktsan_memory_order_acq_rel) {			\
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

/* FIXME(xairy). */

void kt_atomic32_add(kt_thr_t *thr, uptr_t pc, uptr_t addr, int value)
{
	KT_ATOMIC_OP(kt_atomic32_add_no_ktsan((void *)addr, value),
		addr, ktsan_memory_order_relaxed, ktsan_memory_order_acq_rel);
}

void kt_atomic32_sub(kt_thr_t *thr, uptr_t pc, uptr_t addr, int value)
{
	KT_ATOMIC_OP(kt_atomic32_sub_no_ktsan((void *)addr, value),
		addr, ktsan_memory_order_relaxed, ktsan_memory_order_acq_rel);
}

int kt_atomic32_sub_and_test(kt_thr_t *thr, uptr_t pc, uptr_t addr, int value)
{
	int rv;

	KT_ATOMIC_OP(
		rv = kt_atomic32_sub_and_test_no_ktsan((void *)addr, value),
		addr, ktsan_memory_order_acq_rel, ktsan_memory_order_relaxed);

	return rv;
}

int kt_atomic32_add_negative(kt_thr_t *thr, uptr_t pc, uptr_t addr, int value)
{
	int rv;

	KT_ATOMIC_OP(
		rv = kt_atomic32_add_negative_no_ktsan((void *)addr, value),
		addr, ktsan_memory_order_acq_rel, ktsan_memory_order_relaxed);

	return rv;
}

void kt_atomic32_inc(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	KT_ATOMIC_OP(kt_atomic32_inc_no_ktsan((void *)addr),
		addr, ktsan_memory_order_relaxed, ktsan_memory_order_acq_rel);
}

void kt_atomic32_dec(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	KT_ATOMIC_OP(kt_atomic32_dec_no_ktsan((void *)addr),
		addr, ktsan_memory_order_relaxed, ktsan_memory_order_acq_rel);
}

int kt_atomic32_inc_and_test(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	int rv;

	KT_ATOMIC_OP(rv = kt_atomic32_inc_and_test_no_ktsan((void *)addr),
		addr, ktsan_memory_order_acq_rel, ktsan_memory_order_relaxed);

	return rv;
}

int kt_atomic32_dec_and_test(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	int rv;

	KT_ATOMIC_OP(rv = kt_atomic32_dec_and_test_no_ktsan((void *)addr),
		addr, ktsan_memory_order_acq_rel, ktsan_memory_order_relaxed);

	return rv;
}

void kt_atomic64_add(kt_thr_t *thr, uptr_t pc, uptr_t addr, long value)
{
	KT_ATOMIC_OP(kt_atomic64_add_no_ktsan((void *)addr, value),
		addr, ktsan_memory_order_relaxed, ktsan_memory_order_acq_rel);
}

void kt_atomic64_sub(kt_thr_t *thr, uptr_t pc, uptr_t addr, long value)
{
	KT_ATOMIC_OP(kt_atomic64_sub_no_ktsan((void *)addr, value),
		addr, ktsan_memory_order_relaxed, ktsan_memory_order_acq_rel);
}

int kt_atomic64_sub_and_test(kt_thr_t *thr, uptr_t pc, uptr_t addr, long value)
{
	int rv;

	KT_ATOMIC_OP(
		rv = kt_atomic64_sub_and_test_no_ktsan((void *)addr, value),
		addr, ktsan_memory_order_acq_rel, ktsan_memory_order_relaxed);

	return rv;
}

int kt_atomic64_add_negative(kt_thr_t *thr, uptr_t pc, uptr_t addr, long value)
{
	int rv;

	KT_ATOMIC_OP(
		rv = kt_atomic64_add_negative_no_ktsan((void *)addr, value),
		addr, ktsan_memory_order_acq_rel, ktsan_memory_order_relaxed);

	return rv;
}

void kt_atomic64_inc(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	KT_ATOMIC_OP(kt_atomic64_inc_no_ktsan((void *)addr),
		addr, ktsan_memory_order_relaxed, ktsan_memory_order_acq_rel);
}

void kt_atomic64_dec(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	KT_ATOMIC_OP(kt_atomic64_dec_no_ktsan((void *)addr),
		addr, ktsan_memory_order_relaxed, ktsan_memory_order_acq_rel);
}

int kt_atomic64_inc_and_test(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	int rv;

	KT_ATOMIC_OP(rv = kt_atomic64_inc_and_test_no_ktsan((void *)addr),
		addr, ktsan_memory_order_acq_rel, ktsan_memory_order_relaxed);

	return rv;
}

int kt_atomic64_dec_and_test(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	int rv;

	KT_ATOMIC_OP(rv = kt_atomic64_dec_and_test_no_ktsan((void *)addr),
		addr, ktsan_memory_order_acq_rel, ktsan_memory_order_relaxed);

	return rv;
}

s64 kt_atomic64_xchg(kt_thr_t *thr, uptr_t pc, uptr_t addr, s64 value)
{
	s64 rv;

	KT_ATOMIC_OP(rv = kt_atomic64_xchg_no_ktsan((void *)addr, value),
		addr, ktsan_memory_order_acq_rel, ktsan_memory_order_relaxed);

	return rv;
}

s32 kt_atomic32_xchg(kt_thr_t *thr, uptr_t pc, uptr_t addr, s32 value)
{
	s32 rv;

	KT_ATOMIC_OP(rv = kt_atomic32_xchg_no_ktsan((void *)addr, value),
		addr, ktsan_memory_order_acq_rel, ktsan_memory_order_relaxed);

	return rv;
}

s16 kt_atomic16_xchg(kt_thr_t *thr, uptr_t pc, uptr_t addr, s16 value)
{
	s16 rv;

	KT_ATOMIC_OP(rv = kt_atomic16_xchg_no_ktsan((void *)addr, value),
		addr, ktsan_memory_order_acq_rel, ktsan_memory_order_relaxed);

	return rv;
}

s64 kt_atomic64_cmpxchg(kt_thr_t *thr, uptr_t pc, uptr_t addr, s64 old, s64 new)
{
	s64 rv;

	KT_ATOMIC_OP(rv = kt_atomic64_cmpxchg_no_ktsan((void *)addr, old, new),
		addr, ktsan_memory_order_acq_rel, ktsan_memory_order_relaxed);

	return rv;
}

s32 kt_atomic32_cmpxchg(kt_thr_t *thr, uptr_t pc, uptr_t addr, s32 old, s32 new)
{
	s32 rv;

	KT_ATOMIC_OP(rv = kt_atomic32_cmpxchg_no_ktsan((void *)addr, old, new),
		addr, ktsan_memory_order_acq_rel, ktsan_memory_order_relaxed);

	return rv;
}

s16 kt_atomic16_cmpxchg(kt_thr_t *thr, uptr_t pc, uptr_t addr, s16 old, s16 new)
{
	s16 rv;

	KT_ATOMIC_OP(rv = kt_atomic16_cmpxchg_no_ktsan((void *)addr, old, new),
		addr, ktsan_memory_order_acq_rel, ktsan_memory_order_relaxed);

	return rv;
}

s8 kt_atomic8_cmpxchg(kt_thr_t *thr, uptr_t pc, uptr_t addr, s8 old, s8 new)
{
	s8 rv;

	KT_ATOMIC_OP(rv = kt_atomic8_cmpxchg_no_ktsan((void *)addr, old, new),
		addr, ktsan_memory_order_acq_rel, ktsan_memory_order_relaxed);

	return rv;
}

s64 kt_atomic64_xadd(kt_thr_t *thr, uptr_t pc, uptr_t addr, s64 value)
{
	s64 rv;

	KT_ATOMIC_OP(rv = kt_atomic64_xadd_no_ktsan((void *)addr, value),
		addr, ktsan_memory_order_acq_rel, ktsan_memory_order_relaxed);

	return rv;
}

s32 kt_atomic32_xadd(kt_thr_t *thr, uptr_t pc, uptr_t addr, s32 value)
{
	s32 rv;

	KT_ATOMIC_OP(rv = kt_atomic32_xadd_no_ktsan((void *)addr, value),
		addr, ktsan_memory_order_acq_rel, ktsan_memory_order_relaxed);

	return rv;
}

s16 kt_atomic16_xadd(kt_thr_t *thr, uptr_t pc, uptr_t addr, s16 value)
{
	s16 rv;

	KT_ATOMIC_OP(rv = kt_atomic16_xadd_no_ktsan((void *)addr, value),
		addr, ktsan_memory_order_acq_rel, ktsan_memory_order_relaxed);

	return rv;
}

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
