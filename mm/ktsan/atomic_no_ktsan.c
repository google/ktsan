#undef CONFIG_KTSAN

#include <linux/atomic.h>
#include <linux/bitops.h>

u8 kt_atomic8_load_no_ktsan(void *addr)
{
	return *(volatile u8 *)addr;
}

u16 kt_atomic16_load_no_ktsan(void *addr)
{
	return *(volatile u16 *)addr;
}

u32 kt_atomic32_load_no_ktsan(void *addr)
{
	return *(volatile u32 *)addr;
}

u64 kt_atomic64_load_no_ktsan(void *addr)
{
	return *(volatile u64 *)addr;
}

void kt_atomic8_store_no_ktsan(void *addr, u8 value)
{
	*(volatile u8 *)addr = value;
}

void kt_atomic16_store_no_ktsan(void *addr, u16 value)
{
	*(volatile u16 *)addr = value;
}

void kt_atomic32_store_no_ktsan(void *addr, u32 value)
{
	*(volatile u32 *)addr = value;
}

void kt_atomic64_store_no_ktsan(void *addr, u64 value)
{
	*(volatile u64 *)addr = value;
}

u8 kt_atomic8_exchange_no_ktsan(void *addr, u8 value)
{
	return xchg((u8 *)addr, value);
}

u16 kt_atomic16_exchange_no_ktsan(void *addr, u16 value)
{
	return xchg((u16 *)addr, value);
}

u32 kt_atomic32_exchange_no_ktsan(void *addr, u32 value)
{
	return xchg((u32 *)addr, value);
}

u64 kt_atomic64_exchange_no_ktsan(void *addr, u64 value)
{
	return xchg((u64 *)addr, value);
}

u8 kt_atomic8_compare_exchange_no_ktsan(void *addr, u8 old, u8 new)
{
	return cmpxchg((u8 *)addr, old, new);
}

u16 kt_atomic16_compare_exchange_no_ktsan(void *addr, u16 old, u16 new)
{
	return cmpxchg((u16 *)addr, old, new);
}

u32 kt_atomic32_compare_exchange_no_ktsan(void *addr, u32 old, u32 new)
{
	return cmpxchg((u32 *)addr, old, new);
}

u64 kt_atomic64_compare_exchange_no_ktsan(void *addr, u64 old, u64 new)
{
	return cmpxchg((u64 *)addr, old, new);
}

u8 kt_atomic8_fetch_add_no_ktsan(void *addr, u8 value)
{
	return xadd((u8 *)addr, value);
}

u16 kt_atomic16_fetch_add_no_ktsan(void *addr, u16 value)
{
	return xadd((u16 *)addr, value);
}

u32 kt_atomic32_fetch_add_no_ktsan(void *addr, u32 value)
{
	return xadd((u32 *)addr, value);
}

u64 kt_atomic64_fetch_add_no_ktsan(void *addr, u64 value)
{
	return xadd((u64 *)addr, value);
}

/* FIXME(xairy). */

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

void kt_bitop_set_bit_no_ktsan(void *addr, long nr)
{
	set_bit(nr, addr);
}

void kt_bitop_clear_bit_no_ktsan(void *addr, long nr)
{
	clear_bit(nr, addr);
}

void kt_bitop_change_bit_no_ktsan(void *addr, long nr)
{
	change_bit(nr, addr);
}

int kt_bitop_test_and_set_bit_no_ktsan(void *addr, long nr)
{
	return test_and_set_bit(nr, addr);
}

int kt_bitop_test_and_clear_bit_no_ktsan(void *addr, long nr)
{
	return test_and_clear_bit(nr, addr);
}

int kt_bitop_test_and_change_bit_no_ktsan(void *addr, long nr)
{
	return test_and_change_bit(nr, addr);
}

int kt_bitop_test_and_set_bit_lock_no_ktsan(void *addr, long nr)
{
	return test_and_set_bit_lock(nr, addr);
}

void kt_bitop_clear_bit_unlock_no_ktsan(void *addr, long nr)
{
	clear_bit_unlock(nr, addr);
}
