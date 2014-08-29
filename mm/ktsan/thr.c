#include "ktsan.h"

#include <linux/atomic.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/spinlock.h>

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
	void *cpu = this_cpu_ptr(kt_ctx.cpus);

	/*if (thr->id == 1) {
	pr_err("start %d on %d: %lx -> %lx\n",
		thr->id, smp_processor_id(),
		(uptr_t)thr->cpu, (uptr_t)cpu);
	kt_print_current_stack_trace((u64)_RET_IP_);
	pr_err("\n");
	}*/

	KT_ATOMIC_64_SET(&thr->cpu, &cpu);
}

void kt_thr_stop(kt_thr_t *thr, uptr_t pc)
{
	void *cpu = NULL;

	/*if (thr->id == 1) {
	pr_err("stop %d on %d: %lx -> %lx\n",
		thr->id, smp_processor_id(),
		(uptr_t)thr->cpu, (uptr_t)cpu);
	kt_print_current_stack_trace((u64)_RET_IP_);
	pr_err("\n");
	}*/

	KT_ATOMIC_64_SET(&thr->cpu, &cpu);
}
