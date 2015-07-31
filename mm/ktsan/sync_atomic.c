#include "ktsan.h"

#include <linux/atomic.h>
#include <linux/spinlock.h>

enum kt_memory_order_e {
	kt_memory_order_relaxed_acquire,
	kt_memory_order_relaxed_release,
	kt_memory_order_relaxed_acq_rel,
	kt_memory_order_acquire,
	kt_memory_order_release,
	kt_memory_order_acq_rel,
};

#define KT_ATOMIC_OP(op, ad, mo)					\
	do {								\
		kt_tab_sync_t *sync;					\
									\
		sync = kt_sync_ensure_created(thr, (ad));		\
									\
		if ((mo) == kt_memory_order_acquire ||			\
		    (mo) == kt_memory_order_acq_rel)			\
			kt_clk_acquire(&thr->clk, &sync->clk);		\
		if ((mo) == kt_memory_order_relaxed_acquire ||		\
		    (mo) == kt_memory_order_relaxed_acq_rel)		\
			kt_clk_acquire(&thr->acquire_clk, &sync->clk);	\
									\
		(op);							\
									\
		if ((mo) == kt_memory_order_release ||			\
		    (mo) == kt_memory_order_acq_rel)			\
			kt_clk_acquire(&sync->clk, &thr->clk);		\
		if ((mo) == kt_memory_order_relaxed_release ||		\
		    (mo) == kt_memory_order_relaxed_acq_rel)		\
			kt_clk_acquire(&sync->clk, &thr->release_clk);	\
									\
		spin_unlock(&sync->tab.lock);				\
									\
		kt_trace_add_event(thr, kt_event_type_atomic_op, pc);	\
		kt_clk_tick(&thr->clk, thr->id);			\
	} while (0)

int kt_atomic32_read(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	int rv;

	KT_ATOMIC_OP(rv = kt_atomic32_read_no_ktsan((const void *)addr),
			addr, kt_memory_order_acquire);

	return rv;
}

void kt_atomic32_set(kt_thr_t *thr, uptr_t pc, uptr_t addr, int value)
{
	KT_ATOMIC_OP(kt_atomic32_set_no_ktsan((void *)addr, value),
			addr, kt_memory_order_relaxed_release);
}

void kt_atomic32_add(kt_thr_t *thr, uptr_t pc, uptr_t addr, int value)
{
	KT_ATOMIC_OP(kt_atomic32_add_no_ktsan((void *)addr, value),
			addr, kt_memory_order_relaxed_acq_rel);
}

void kt_atomic32_sub(kt_thr_t *thr, uptr_t pc, uptr_t addr, int value)
{
	KT_ATOMIC_OP(kt_atomic32_sub_no_ktsan((void *)addr, value),
			addr, kt_memory_order_relaxed_acq_rel);
}

int kt_atomic32_sub_and_test(kt_thr_t *thr, uptr_t pc, uptr_t addr, int value)
{
	int rv;

	KT_ATOMIC_OP(rv = kt_atomic32_sub_and_test_no_ktsan((void *)addr, value),
			addr, kt_memory_order_acq_rel);

	return rv;
}

int kt_atomic32_add_negative(kt_thr_t *thr, uptr_t pc, uptr_t addr, int value)
{
	int rv;

	KT_ATOMIC_OP(rv = kt_atomic32_add_negative_no_ktsan((void *)addr, value),
			addr, kt_memory_order_acq_rel);

	return rv;
}

void kt_atomic32_inc(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	KT_ATOMIC_OP(kt_atomic32_inc_no_ktsan((void *)addr),
			addr, kt_memory_order_relaxed_acq_rel);
}

void kt_atomic32_dec(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	KT_ATOMIC_OP(kt_atomic32_dec_no_ktsan((void *)addr),
			addr, kt_memory_order_relaxed_acq_rel);
}

int kt_atomic32_inc_and_test(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	int rv;

	KT_ATOMIC_OP(rv = kt_atomic32_inc_and_test_no_ktsan((void *)addr),
			addr, kt_memory_order_acq_rel);

	return rv;
}

int kt_atomic32_dec_and_test(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	int rv;

	KT_ATOMIC_OP(rv = kt_atomic32_dec_and_test_no_ktsan((void *)addr),
			addr, kt_memory_order_acq_rel);

	return rv;
}

long kt_atomic64_read(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	long rv;

	KT_ATOMIC_OP(rv = kt_atomic64_read_no_ktsan((void *)addr),
			addr, kt_memory_order_acquire);

	return rv;
}

void kt_atomic64_set(kt_thr_t *thr, uptr_t pc, uptr_t addr, long value)
{
	KT_ATOMIC_OP(kt_atomic64_set_no_ktsan((void *)addr, value),
			addr, kt_memory_order_relaxed_release);
}

void kt_atomic64_add(kt_thr_t *thr, uptr_t pc, uptr_t addr, long value)
{
	KT_ATOMIC_OP(kt_atomic64_add_no_ktsan((void *)addr, value),
			addr, kt_memory_order_relaxed_acq_rel);
}

void kt_atomic64_sub(kt_thr_t *thr, uptr_t pc, uptr_t addr, long value)
{
	KT_ATOMIC_OP(kt_atomic64_sub_no_ktsan((void *)addr, value),
			addr, kt_memory_order_relaxed_acq_rel);
}

int kt_atomic64_sub_and_test(kt_thr_t *thr, uptr_t pc, uptr_t addr, long value)
{
	int rv;

	KT_ATOMIC_OP(rv = kt_atomic64_sub_and_test_no_ktsan((void *)addr, value),
			addr, kt_memory_order_acq_rel);

	return rv;
}

int kt_atomic64_add_negative(kt_thr_t *thr, uptr_t pc, uptr_t addr, long value)
{
	int rv;

	KT_ATOMIC_OP(rv = kt_atomic64_add_negative_no_ktsan((void *)addr, value),
			addr, kt_memory_order_acq_rel);

	return rv;
}

void kt_atomic64_inc(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	KT_ATOMIC_OP(kt_atomic64_inc_no_ktsan((void *)addr),
			addr, kt_memory_order_relaxed_acq_rel);
}

void kt_atomic64_dec(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	KT_ATOMIC_OP(kt_atomic64_dec_no_ktsan((void *)addr),
			addr, kt_memory_order_relaxed_acq_rel);
}

int kt_atomic64_inc_and_test(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	int rv;

	KT_ATOMIC_OP(rv = kt_atomic64_inc_and_test_no_ktsan((void *)addr),
			addr, kt_memory_order_acq_rel);

	return rv;
}

int kt_atomic64_dec_and_test(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	int rv;

	KT_ATOMIC_OP(rv = kt_atomic64_dec_and_test_no_ktsan((void *)addr),
			addr, kt_memory_order_acq_rel);

	return rv;
}

s64 kt_atomic64_xchg(kt_thr_t *thr, uptr_t pc, uptr_t addr, s64 value)
{
	s64 rv;

	KT_ATOMIC_OP(rv = kt_atomic64_xchg_no_ktsan((void *)addr, value),
			addr, kt_memory_order_acq_rel);

	return rv;
}

s32 kt_atomic32_xchg(kt_thr_t *thr, uptr_t pc, uptr_t addr, s32 value)
{
	s32 rv;

	KT_ATOMIC_OP(rv = kt_atomic32_xchg_no_ktsan((void *)addr, value),
			addr, kt_memory_order_acq_rel);

	return rv;
}

s16 kt_atomic16_xchg(kt_thr_t *thr, uptr_t pc, uptr_t addr, s16 value)
{
	s16 rv;

	KT_ATOMIC_OP(rv = kt_atomic16_xchg_no_ktsan((void *)addr, value),
			addr, kt_memory_order_acq_rel);

	return rv;
}

s64 kt_atomic64_cmpxchg(kt_thr_t *thr, uptr_t pc, uptr_t addr, s64 old, s64 new)
{
	s64 rv;

	KT_ATOMIC_OP(rv = kt_atomic64_cmpxchg_no_ktsan((void *)addr, old, new),
			addr, kt_memory_order_acq_rel);

	return rv;
}

s32 kt_atomic32_cmpxchg(kt_thr_t *thr, uptr_t pc, uptr_t addr, s32 old, s32 new)
{
	s32 rv;

	KT_ATOMIC_OP(rv = kt_atomic32_cmpxchg_no_ktsan((void *)addr, old, new),
			addr, kt_memory_order_acq_rel);

	return rv;
}

s16 kt_atomic16_cmpxchg(kt_thr_t *thr, uptr_t pc, uptr_t addr, s16 old, s16 new)
{
	s16 rv;

	KT_ATOMIC_OP(rv = kt_atomic16_cmpxchg_no_ktsan((void *)addr, old, new),
			addr, kt_memory_order_acq_rel);

	return rv;
}

s8 kt_atomic8_cmpxchg(kt_thr_t *thr, uptr_t pc, uptr_t addr, s8 old, s8 new)
{
	s8 rv;

	KT_ATOMIC_OP(rv = kt_atomic8_cmpxchg_no_ktsan((void *)addr, old, new),
			addr, kt_memory_order_acq_rel);

	return rv;
}

s64 kt_atomic64_xadd(kt_thr_t *thr, uptr_t pc, uptr_t addr, s64 value)
{
	s64 rv;

	KT_ATOMIC_OP(rv = kt_atomic64_xadd_no_ktsan((void *)addr, value),
			addr, kt_memory_order_acq_rel);

	return rv;
}

s32 kt_atomic32_xadd(kt_thr_t *thr, uptr_t pc, uptr_t addr, s32 value)
{
	s32 rv;

	KT_ATOMIC_OP(rv = kt_atomic32_xadd_no_ktsan((void *)addr, value),
			addr, kt_memory_order_acq_rel);

	return rv;
}

s16 kt_atomic16_xadd(kt_thr_t *thr, uptr_t pc, uptr_t addr, s16 value)
{
	s16 rv;

	KT_ATOMIC_OP(rv = kt_atomic16_xadd_no_ktsan((void *)addr, value),
			addr, kt_memory_order_acq_rel);

	return rv;
}

void kt_bitop_set_bit(kt_thr_t *thr, uptr_t pc, uptr_t addr, long nr)
{
	KT_ATOMIC_OP(kt_bitop_set_bit_no_ktsan((void *)addr, nr),
			addr + nr / 8, kt_memory_order_relaxed_release);
}

void kt_bitop_clear_bit(kt_thr_t *thr, uptr_t pc, uptr_t addr, long nr)
{
	KT_ATOMIC_OP(kt_bitop_clear_bit_no_ktsan((void *)addr, nr),
			addr + nr / 8, kt_memory_order_relaxed_release);
}

void kt_bitop_change_bit(kt_thr_t *thr, uptr_t pc, uptr_t addr, long nr)
{
	KT_ATOMIC_OP(kt_bitop_change_bit_no_ktsan((void *)addr, nr),
			addr + nr / 9, kt_memory_order_relaxed_release);
}

int kt_bitop_test_and_set_bit(kt_thr_t *thr, uptr_t pc, uptr_t addr, long nr)
{
	int rv;

	KT_ATOMIC_OP(rv = kt_bitop_test_and_set_bit_no_ktsan((void *)addr, nr),
			addr + nr / 8, kt_memory_order_acq_rel);

	return rv;
}

int kt_bitop_test_and_clear_bit(kt_thr_t *thr, uptr_t pc, uptr_t addr, long nr)
{
	int rv;

	KT_ATOMIC_OP(rv = kt_bitop_test_and_clear_bit_no_ktsan((void *)addr, nr),
			addr + nr / 8, kt_memory_order_acq_rel);

	return rv;
}

int kt_bitop_test_and_change_bit(kt_thr_t *thr, uptr_t pc, uptr_t addr, long nr)
{
	int rv;

	KT_ATOMIC_OP(rv = kt_bitop_test_and_change_bit_no_ktsan((void *)addr, nr),
			addr + nr / 8, kt_memory_order_acq_rel);

	return rv;
}

void kt_membar_acquire(kt_thr_t *thr)
{
	kt_clk_acquire(&thr->clk, &thr->acquire_clk);
}

void kt_membar_release(kt_thr_t *thr)
{
	kt_clk_acquire(&thr->release_clk, &thr->clk);
}

void kt_membar_acq_rel(kt_thr_t *thr)
{
	kt_clk_acquire(&thr->clk, &thr->acquire_clk);
	kt_clk_acquire(&thr->release_clk, &thr->clk);
}
