#include "ktsan.h"

#include <linux/kernel.h>
#include <linux/sched.h>

void ktsan_thr_create(ktsan_thr_t *thr, int thr_id, ktsan_thr_t *parent)
{
	memset(thr, 0, sizeof(*thr));
	thr->clk = ktsan_clk_create(thr);
	thr->id = thr_id;
}

void ktsan_thr_finish(ktsan_thr_t *thr)
{
	/* TODO(dvyukov): call me */
	ktsan_clk_destroy(thr, thr->clk);
	memset(thr, 0, sizeof(*thr));
}

void ktsan_thr_start(ktsan_thr_t *thr, int cpu)
{
	/*REPEAT_N_AND_STOP(10) pr_err(
		"TSan: Thread #%d started on cpu #%d.\n", thread_id, cpu); */
}

void ktsan_thr_stop(ktsan_thr_t *thr, int cpu)
{
}
