#include "ktsan.h"

#include <linux/atomic.h>
#include <linux/spinlock.h>

int kt_atomic32_read(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	spinlock_t *lock;
	int rv;

	lock = kt_lock_get_and_lock(thr, addr);
	kt_sync_acquire(thr, pc, addr);
	rv = kt_atomic32_read_no_ktsan((const void *)addr);
	spin_unlock(lock);

	return rv;
}

void kt_atomic32_set(kt_thr_t *thr, uptr_t pc, uptr_t addr, int value)
{
	spinlock_t *lock;

	lock = kt_lock_get_and_lock(thr, addr);
	kt_atomic32_set_no_ktsan((void *)addr, value);
	kt_sync_release(thr, pc, addr);
	spin_unlock(lock);
}

void kt_atomic32_add(kt_thr_t *thr, uptr_t pc, uptr_t addr, int value)
{
	spinlock_t *lock;

	lock = kt_lock_get_and_lock(thr, addr);
	kt_sync_acquire(thr, pc, addr);
	kt_atomic32_add_no_ktsan((void *)addr, value);
	kt_sync_release(thr, pc, addr);
	spin_unlock(lock);
}

void kt_atomic32_sub(kt_thr_t *thr, uptr_t pc, uptr_t addr, int value)
{
	spinlock_t *lock;

	lock = kt_lock_get_and_lock(thr, addr);
	kt_sync_acquire(thr, pc, addr);
	kt_atomic32_sub_no_ktsan((void *)addr, value);
	kt_sync_release(thr, pc, addr);
	spin_unlock(lock);
}

int kt_atomic32_sub_and_test(kt_thr_t *thr, uptr_t pc, uptr_t addr, int value)
{
	spinlock_t *lock;
	int rv;

	lock = kt_lock_get_and_lock(thr, addr);
	kt_sync_acquire(thr, pc, addr);
	rv = kt_atomic32_sub_and_test_no_ktsan((void *)addr, value);
	kt_sync_release(thr, pc, addr);
	spin_unlock(lock);

	return rv;
}

int kt_atomic32_add_negative(kt_thr_t *thr, uptr_t pc, uptr_t addr, int value)
{
	spinlock_t *lock;
	int rv;

	lock = kt_lock_get_and_lock(thr, addr);
	kt_sync_acquire(thr, pc, addr);
	rv = kt_atomic32_add_negative_no_ktsan((void *)addr, value);
	kt_sync_release(thr, pc, addr);
	spin_unlock(lock);

	return rv;
}

void kt_atomic32_inc(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	spinlock_t *lock;

	lock = kt_lock_get_and_lock(thr, addr);
	kt_sync_acquire(thr, pc, addr);
	kt_atomic32_inc_no_ktsan((void *)addr);
	kt_sync_release(thr, pc, addr);
	spin_unlock(lock);
}

void kt_atomic32_dec(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	spinlock_t *lock;

	lock = kt_lock_get_and_lock(thr, addr);
	kt_sync_acquire(thr, pc, addr);
	kt_atomic32_dec_no_ktsan((void *)addr);
	kt_sync_release(thr, pc, addr);
	spin_unlock(lock);
}

int kt_atomic32_inc_and_test(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	spinlock_t *lock;
	int rv;

	lock = kt_lock_get_and_lock(thr, addr);
	kt_sync_acquire(thr, pc, addr);
	rv = kt_atomic32_inc_and_test_no_ktsan((void *)addr);
	kt_sync_release(thr, pc, addr);
	spin_unlock(lock);

	return rv;
}

int kt_atomic32_dec_and_test(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	spinlock_t *lock;
	int rv;

	lock = kt_lock_get_and_lock(thr, addr);
	kt_sync_acquire(thr, pc, addr);
	rv = kt_atomic32_dec_and_test_no_ktsan((void *)addr);
	kt_sync_release(thr, pc, addr);
	spin_unlock(lock);

	return rv;
}

long kt_atomic64_read(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	spinlock_t *lock;
	long rv;

	lock = kt_lock_get_and_lock(thr, addr);
	kt_sync_acquire(thr, pc, addr);
	rv = kt_atomic64_read_no_ktsan((const void *)addr);
	spin_unlock(lock);

	return rv;
}

void kt_atomic64_set(kt_thr_t *thr, uptr_t pc, uptr_t addr, long value)
{
	spinlock_t *lock;

	lock = kt_lock_get_and_lock(thr, addr);
	kt_atomic64_set_no_ktsan((void *)addr, value);
	kt_sync_release(thr, pc, addr);
	spin_unlock(lock);
}

void kt_atomic64_add(kt_thr_t *thr, uptr_t pc, uptr_t addr, long value)
{
	spinlock_t *lock;

	lock = kt_lock_get_and_lock(thr, addr);
	kt_sync_acquire(thr, pc, addr);
	kt_atomic64_add_no_ktsan((void *)addr, value);
	kt_sync_release(thr, pc, addr);
	spin_unlock(lock);
}

void kt_atomic64_sub(kt_thr_t *thr, uptr_t pc, uptr_t addr, long value)
{
	spinlock_t *lock;

	lock = kt_lock_get_and_lock(thr, addr);
	kt_sync_acquire(thr, pc, addr);
	kt_atomic64_sub_no_ktsan((void *)addr, value);
	kt_sync_release(thr, pc, addr);
	spin_unlock(lock);
}

int kt_atomic64_sub_and_test(kt_thr_t *thr, uptr_t pc, uptr_t addr, long value)
{
	spinlock_t *lock;
	int rv;

	lock = kt_lock_get_and_lock(thr, addr);
	kt_sync_acquire(thr, pc, addr);
	rv = kt_atomic64_sub_and_test_no_ktsan((void *)addr, value);
	kt_sync_release(thr, pc, addr);
	spin_unlock(lock);

	return rv;
}

int kt_atomic64_add_negative(kt_thr_t *thr, uptr_t pc, uptr_t addr, long value)
{
	spinlock_t *lock;
	int rv;

	lock = kt_lock_get_and_lock(thr, addr);
	kt_sync_acquire(thr, pc, addr);
	rv = kt_atomic64_add_negative_no_ktsan((void *)addr, value);
	kt_sync_release(thr, pc, addr);
	spin_unlock(lock);

	return rv;
}

void kt_atomic64_inc(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	spinlock_t *lock;

	lock = kt_lock_get_and_lock(thr, addr);
	kt_sync_acquire(thr, pc, addr);
	kt_atomic64_inc_no_ktsan((void *)addr);
	kt_sync_release(thr, pc, addr);
	spin_unlock(lock);
}

void kt_atomic64_dec(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	spinlock_t *lock;

	lock = kt_lock_get_and_lock(thr, addr);
	kt_sync_acquire(thr, pc, addr);
	kt_atomic64_dec_no_ktsan((void *)addr);
	kt_sync_release(thr, pc, addr);
	spin_unlock(lock);
}

int kt_atomic64_inc_and_test(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	spinlock_t *lock;
	int rv;

	lock = kt_lock_get_and_lock(thr, addr);
	kt_sync_acquire(thr, pc, addr);
	rv = kt_atomic64_inc_and_test_no_ktsan((void *)addr);
	kt_sync_release(thr, pc, addr);
	spin_unlock(lock);

	return rv;
}

int kt_atomic64_dec_and_test(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	spinlock_t *lock;
	int rv;

	lock = kt_lock_get_and_lock(thr, addr);
	kt_sync_acquire(thr, pc, addr);
	rv = kt_atomic64_dec_and_test_no_ktsan((void *)addr);
	kt_sync_release(thr, pc, addr);
	spin_unlock(lock);

	return rv;
}

s64 kt_atomic64_xchg(kt_thr_t *thr, uptr_t pc, uptr_t addr, s64 value)
{
	spinlock_t *lock;
	s64 rv;

	lock = kt_lock_get_and_lock(thr, addr);
	kt_sync_acquire(thr, pc, addr);
	rv = kt_atomic64_xchg_no_ktsan((void *)addr, value);
	kt_sync_release(thr, pc, addr);
	spin_unlock(lock);

	return rv;
}

s32 kt_atomic32_xchg(kt_thr_t *thr, uptr_t pc, uptr_t addr, s32 value)
{
	spinlock_t *lock;
	s32 rv;

	lock = kt_lock_get_and_lock(thr, addr);
	kt_sync_acquire(thr, pc, addr);
	rv = kt_atomic32_xchg_no_ktsan((void *)addr, value);
	kt_sync_release(thr, pc, addr);
	spin_unlock(lock);

	return rv;
}

s16 kt_atomic16_xchg(kt_thr_t *thr, uptr_t pc, uptr_t addr, s16 value)
{
	/* TODO(xairy). */
	return kt_atomic16_xchg_no_ktsan((void *)addr, value);
}

s64 kt_atomic64_cmpxchg(kt_thr_t *thr, uptr_t pc, uptr_t addr, s64 old, s64 new)
{
	spinlock_t *lock;
	s64 rv;

	lock = kt_lock_get_and_lock(thr, addr);
	kt_sync_acquire(thr, pc, addr);
	rv = kt_atomic64_cmpxchg_no_ktsan((void *)addr, old, new);
	kt_sync_release(thr, pc, addr);
	spin_unlock(lock);

	return rv;
}

s32 kt_atomic32_cmpxchg(kt_thr_t *thr, uptr_t pc, uptr_t addr, s32 old, s32 new)
{
	spinlock_t *lock;
	s32 rv;

	lock = kt_lock_get_and_lock(thr, addr);
	kt_sync_acquire(thr, pc, addr);
	rv = kt_atomic32_cmpxchg_no_ktsan((void *)addr, old, new);
	kt_sync_release(thr, pc, addr);
	spin_unlock(lock);

	return rv;
}

s16 kt_atomic16_cmpxchg(kt_thr_t *thr, uptr_t pc, uptr_t addr, s16 old, s16 new)
{
	spinlock_t *lock;
	s16 rv;

	lock = kt_lock_get_and_lock(thr, addr);
	kt_sync_acquire(thr, pc, addr);
	rv = kt_atomic16_cmpxchg_no_ktsan((void *)addr, old, new);
	kt_sync_release(thr, pc, addr);
	spin_unlock(lock);

	return rv;
}

s8 kt_atomic8_cmpxchg(kt_thr_t *thr, uptr_t pc, uptr_t addr, s8 old, s8 new)
{
	/* TODO(xairy). */
	return kt_atomic8_cmpxchg_no_ktsan((void *)addr, old, new);
}

s64 kt_atomic64_xadd(kt_thr_t *thr, uptr_t pc, uptr_t addr, s64 value)
{
	spinlock_t *lock;
	s64 rv;

	lock = kt_lock_get_and_lock(thr, addr);
	kt_sync_acquire(thr, pc, addr);
	rv = kt_atomic64_xadd_no_ktsan((void *)addr, value);
	kt_sync_release(thr, pc, addr);
	spin_unlock(lock);

	return rv;
}

s32 kt_atomic32_xadd(kt_thr_t *thr, uptr_t pc, uptr_t addr, s32 value)
{
	spinlock_t *lock;
	s32 rv;

	lock = kt_lock_get_and_lock(thr, addr);
	kt_sync_acquire(thr, pc, addr);
	rv = kt_atomic32_xadd_no_ktsan((void *)addr, value);
	kt_sync_release(thr, pc, addr);
	spin_unlock(lock);

	return rv;
}

s16 kt_atomic16_xadd(kt_thr_t *thr, uptr_t pc, uptr_t addr, s16 value)
{
	spinlock_t *lock;
	s16 rv;

	lock = kt_lock_get_and_lock(thr, addr);
	kt_sync_acquire(thr, pc, addr);
	rv = kt_atomic16_xadd_no_ktsan((void *)addr, value);
	kt_sync_release(thr, pc, addr);
	spin_unlock(lock);

	return rv;
}
