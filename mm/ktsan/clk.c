#include "ktsan.h"

#include <linux/gfp.h>
#include <linux/kernel.h>
#include <linux/slab.h>

ktsan_clk_t *ktsan_clk_create(ktsan_thr_t *thr)
{
	ktsan_clk_t *clk;

	clk = kzalloc(sizeof(*clk), GFP_KERNEL);
	return clk;
}

void ktsan_clk_destroy(ktsan_thr_t *thr, ktsan_clk_t *clk)
{
	kfree(clk);
}

void ktsan_clk_acquire(ktsan_thr_t *thr, ktsan_clk_t *dst, ktsan_clk_t *src)
{
	int i;

	for (i = 0; i < KTSAN_MAX_THREAD_ID; i++)
		dst->time[i] = max(dst->time[i], src->time[i]);
}
