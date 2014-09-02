#undef CONFIG_KTSAN

#include <linux/atomic.h>

int kt_atomic32_pure_read(const void *addr)
{
	return atomic_read((const atomic_t *)addr);
}

void kt_atomic32_pure_set(void *addr, int value)
{
	atomic_set((atomic_t *)addr, value);
}
