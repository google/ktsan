#include "ktsan.h"

#include <linux/gfp.h>
#include <linux/kernel.h>
#include <linux/slab.h>

kt_clk_t *kt_clk_create(kt_thr_t *thr)
{
	kt_clk_t *clk;

	clk = kzalloc(sizeof(*clk), GFP_KERNEL);
	return clk;
}

void kt_clk_destroy(kt_thr_t *thr, kt_clk_t *clk)
{
	kfree(clk);
}

void kt_clk_acquire(kt_thr_t *thr, kt_clk_t *dst, kt_clk_t *src)
{
	int i;

	for (i = 0; i < KT_MAX_THREAD_ID; i++)
		dst->time[i] = max(dst->time[i], src->time[i]);
}
