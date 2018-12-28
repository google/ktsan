#undef CONFIG_KTSAN

#include <linux/atomic.h>
#include <linux/bitops.h>

#include <asm/barrier.h>

void kt_thread_fence_no_ktsan(ktsan_memory_order_t mo)
{
	switch (mo) {
	case ktsan_memory_order_acquire:
		rmb();
		break;
	case ktsan_memory_order_release:
		wmb();
		break;
	case ktsan_memory_order_acq_rel:
		mb();
		break;
	default:
		break;
	}
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

void kt_atomic_set_bit_no_ktsan(void *addr, long nr)
{
	set_bit(nr, addr);
}

void kt_atomic_clear_bit_no_ktsan(void *addr, long nr)
{
	clear_bit(nr, addr);
}

void kt_atomic_change_bit_no_ktsan(void *addr, long nr)
{
	change_bit(nr, addr);
}

int kt_atomic_fetch_set_bit_no_ktsan(void *addr, long nr)
{
	return test_and_set_bit(nr, addr);
}

int kt_atomic_fetch_clear_bit_no_ktsan(void *addr, long nr)
{
	return test_and_clear_bit(nr, addr);
}

int kt_atomic_fetch_change_bit_no_ktsan(void *addr, long nr)
{
	return test_and_change_bit(nr, addr);
}
