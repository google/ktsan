#include "ktsan.h"

#include <linux/atomic.h>

int kt_atomic32_read(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	/* TODO(xairy). */
	return kt_atomic32_read_no_ktsan((const void *)addr);
}

void kt_atomic32_set(kt_thr_t *thr, uptr_t pc, uptr_t addr, int value)
{
	/* TODO(xairy). */
	kt_atomic32_set_no_ktsan((void *)addr, value);
}

void kt_atomic32_add(kt_thr_t *thr, uptr_t pc, uptr_t addr, int value)
{
	/* TODO(xairy). */
	kt_atomic32_add_no_ktsan((void *)addr, value);
}

void kt_atomic32_sub(kt_thr_t *thr, uptr_t pc, uptr_t addr, int value)
{
	/* TODO(xairy). */
	kt_atomic32_sub_no_ktsan((void *)addr, value);
}

int kt_atomic32_sub_and_test(kt_thr_t *thr, uptr_t pc, uptr_t addr, int value)
{
	/* TODO(xairy). */
	return kt_atomic32_sub_and_test_no_ktsan((void *)addr, value);
}

int kt_atomic32_add_negative(kt_thr_t *thr, uptr_t pc, uptr_t addr, int value)
{
	/* TODO(xairy). */
	return kt_atomic32_add_negative_no_ktsan((void *)addr, value);
}

void kt_atomic32_inc(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	/* TODO(xairy). */
	kt_atomic32_inc_no_ktsan((void *)addr);
}

void kt_atomic32_dec(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	/* TODO(xairy). */
	kt_atomic32_dec_no_ktsan((void *)addr);
}

int kt_atomic32_inc_and_test(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	/* TODO(xairy). */
	return kt_atomic32_inc_and_test_no_ktsan((void *)addr);
}

int kt_atomic32_dec_and_test(kt_thr_t *thr, uptr_t pc, uptr_t addr)
{
	/* TODO(xairy). */
	return kt_atomic32_dec_and_test_no_ktsan((void *)addr);
}
