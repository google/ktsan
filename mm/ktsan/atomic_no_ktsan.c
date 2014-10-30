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
