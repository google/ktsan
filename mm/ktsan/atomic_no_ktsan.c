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
