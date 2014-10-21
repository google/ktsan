#include "ktsan.h"

#include <linux/irqflags.h>
#include <linux/list.h>
#include <linux/preempt.h>

struct kt_percpu_sync_s {
	uptr_t addr;
	struct list_head list;
};

static void kt_percpu_track_enable(kt_thr_t *thr)
{
	thr->track_percpu = true;
}

static void kt_percpu_track_try_disable(kt_thr_t *thr)
{
	if (!irqs_disabled() && !preempt_count())
		thr->track_percpu = false;

	/* TODO(xairy): release all percpu syncs. */
}

void kt_percpu_acquire(kt_thr_t *thr, uptr_t addr)
{

}

void kt_preempt_disable(kt_thr_t *thr)
{
	kt_percpu_track_enable(thr);
}

void kt_preempt_enable(kt_thr_t *thr)
{
	kt_percpu_track_try_disable(thr);
}

void kt_irq_disable(kt_thr_t *thr)
{
	kt_percpu_track_enable(thr);
}

void kt_irq_enable(kt_thr_t *thr)
{
	kt_percpu_track_try_disable(thr);
}
