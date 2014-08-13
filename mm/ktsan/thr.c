#include "ktsan.h"

#include <linux/kernel.h>
#include <linux/sched.h>

void kt_thr_create(kt_thr_t *thr, uptr_t pc, kt_thr_t *new, int tid)
{
	new->id = tid;
	kt_clk_init(thr, &new->clk);
}

void kt_thr_finish(kt_thr_t *thr, uptr_t pc)
{
	/* TODO(dvyukov): call me */
	kt_clk_destroy(thr, &thr->clk);
}

void kt_thr_start(kt_thr_t *thr, uptr_t pc)
{
	/* thr->cpu = this_cpu_ptr(kt_ctx.cpus); */
}

void kt_thr_stop(kt_thr_t *thr, uptr_t pc)
{
	/* thr->cpu = NULL; */
}
