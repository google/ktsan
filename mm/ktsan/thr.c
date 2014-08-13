#include "ktsan.h"

#include <linux/kernel.h>
#include <linux/sched.h>

void kt_thr_create(kt_thr_t *thr, uptr_t pc, ktsan_thr_t *new, int tid)
{
	memset(new, 0, sizeof(*new));
	new->clk = kt_clk_create(thr);
	new->id = tid;
}

void kt_thr_finish(kt_thr_t *thr, uptr_t pc)
{
	/* TODO(dvyukov): call me */
	kt_clk_destroy(thr, thr->clk);
	memset(thr, 0, sizeof(*thr));
}

void kt_thr_start(kt_thr_t *thr, uptr_t pc)
{
	int cpu;

	cpu = smp_processor_id();
	(void)cpu;
}

void kt_thr_stop(kt_thr_t *thr, uptr_t pc)
{
	int cpu;

	cpu = smp_processor_id();
	(void)cpu;
}
