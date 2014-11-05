#undef CONFIG_KTSAN

#include <linux/atomic.h>

int kt_atomic32_read_no_ktsan(const void *addr)
{
	return atomic_read((const atomic_t *)addr);
}

void kt_atomic32_set_no_ktsan(void *addr, int value)
{
	atomic_set((atomic_t *)addr, value);
}

void kt_atomic32_add_no_ktsan(void *addr, int value)
{
	atomic_add(value, (atomic_t *)addr);
}

void kt_atomic32_sub_no_ktsan(void *addr, int value)
{
	atomic_sub(value, (atomic_t *)addr);
}

int kt_atomic32_sub_and_test_no_ktsan(void *addr, int value)
{
	return atomic_sub_and_test(value, (atomic_t *)addr);
}

int kt_atomic32_add_negative_no_ktsan(void *addr, int value)
{
	return atomic_add_negative(value, (atomic_t *)addr);
}

void kt_atomic32_inc_no_ktsan(void *addr)
{
	atomic_inc((atomic_t *)addr);
}

void kt_atomic32_dec_no_ktsan(void *addr)
{
	atomic_dec((atomic_t *)addr);
}

int kt_atomic32_inc_and_test_no_ktsan(void *addr)
{
	return atomic_inc_and_test((atomic_t *)addr);
}

int kt_atomic32_dec_and_test_no_ktsan(void *addr)
{
	return atomic_dec_and_test((atomic_t *)addr);
}

long kt_atomic64_read_no_ktsan(const void *addr)
{
	return atomic64_read((const atomic64_t *)addr);
}

void kt_atomic64_set_no_ktsan(void *addr, long value)
{
	atomic64_set((atomic64_t *)addr, value);
}

void kt_atomic64_add_no_ktsan(void *addr, long value)
{
	atomic64_add(value, (atomic64_t *)addr);
}

void kt_atomic64_sub_no_ktsan(void *addr, long value)
{
	atomic64_sub(value, (atomic64_t *)addr);
}

int kt_atomic64_sub_and_test_no_ktsan(void *addr, long value)
{
	return atomic64_sub_and_test(value, (atomic64_t *)addr);
}

int kt_atomic64_add_negative_no_ktsan(void *addr, long value)
{
	return atomic64_add_negative(value, (atomic64_t *)addr);
}

void kt_atomic64_inc_no_ktsan(void *addr)
{
	atomic64_inc((atomic64_t *)addr);
}

void kt_atomic64_dec_no_ktsan(void *addr)
{
	atomic64_dec((atomic64_t *)addr);
}

int kt_atomic64_inc_and_test_no_ktsan(void *addr)
{
	return atomic64_inc_and_test((atomic64_t *)addr);
}

int kt_atomic64_dec_and_test_no_ktsan(void *addr)
{
	return atomic64_dec_and_test((atomic64_t *)addr);
}

s64 kt_atomic64_xchg_no_ktsan(void *addr, s64 value)
{
	return xchg((s64 *)addr, value);
}

s32 kt_atomic32_xchg_no_ktsan(void *addr, s32 value)
{
	return xchg((s32 *)addr, value);
}

s16 kt_atomic16_xchg_no_ktsan(void *addr, s16 value)
{
	return xchg((s16 *)addr, value);
}

s64 kt_atomic64_cmpxchg_no_ktsan(void *addr, s64 old, s64 new)
{
	return cmpxchg((s64 *)addr, old, new);
}

s32 kt_atomic32_cmpxchg_no_ktsan(void *addr, s32 old, s32 new)
{
	return cmpxchg((s32 *)addr, old, new);
}

s16 kt_atomic16_cmpxchg_no_ktsan(void *addr, s16 old, s16 new)
{
	return cmpxchg((s16 *)addr, old, new);
}

s8 kt_atomic8_cmpxchg_no_ktsan(void *addr, s8 old, s8 new)
{
	return cmpxchg((s8 *)addr, old, new);
}

s64 kt_atomic64_xadd_no_ktsan(void *addr, s64 value)
{
	return xadd((s64 *)addr, value);
}

s32 kt_atomic32_xadd_no_ktsan(void *addr, s32 value)
{
	return xadd((s32 *)addr, value);
}

s16 kt_atomic16_xadd_no_ktsan(void *addr, s16 value)
{
	return xadd((s16 *)addr, value);
}
