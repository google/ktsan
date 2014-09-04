#include "ktsan.h"

#include <linux/atomic.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/spinlock.h>

/* thr == NULL when thread #0 is being created. */
void kt_thr_create(kt_thr_t *thr, uptr_t pc, kt_thr_t *new, int tid)
{
	memset(new, 0, sizeof(*new));

	new->id = tid;
	kt_clk_init(thr, &new->clk);

	/*if (thr != NULL) {
		pr_err("! created thread #%d by thread #%d\n",
			(int)tid,  (int)thr->id);
		kt_print_current_stack_trace((uptr_t)_RET_IP_);
		pr_err("\n");
	}*/

	if (thr == NULL)
		return;

	kt_clk_acquire(thr, &new->clk, &thr->clk);

	kt_stat_inc(thr, kt_stat_thread_create);
	kt_stat_inc(thr, kt_stat_threads);
}

void kt_thr_finish(kt_thr_t *thr, uptr_t pc, kt_thr_t *old)
{
	kt_clk_destroy(thr, &old->clk);

	kt_stat_inc(thr, kt_stat_thread_destroy);
	kt_stat_dec(thr, kt_stat_threads);
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
