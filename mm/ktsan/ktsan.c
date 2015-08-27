#include "ktsan.h"

#include <linux/atomic.h>
#include <linux/irqflags.h>
#include <linux/kernel.h>
#include <linux/nmi.h>
#include <linux/preempt.h>
#include <linux/printk.h>
#include <linux/sched.h>

int ktsan_glob_sync[ktsan_glob_sync_type_count];
EXPORT_SYMBOL(ktsan_glob_sync);

kt_ctx_t kt_ctx;

#define DISABLE_INTERRUPTS(flags)	\
	preempt_disable();		\
	flags = arch_local_irq_save();	\
	stop_nmi()			\
/**/

#define ENABLE_INTERRUPTS(flags)	\
	restart_nmi();			\
	arch_local_irq_restore(flags);	\
	preempt_enable()		\
/**/

#define IN_INTERRUPT()			\
	 (in_nmi())			\
/**/

/* The handle_scheduler flag enables handling events
   that come from the scheduler internals.
   The handle_disabled flag enables handling events
   even if events were disabled with ktsan_disable(). */
#define ENTER(handle_scheduler, handle_disabled)			\
	kt_thr_t *thr;							\
	uptr_t pc;							\
	unsigned long kt_flags;						\
	int kt_inside_was;						\
	bool event_handled;						\
									\
	thr = NULL;							\
	kt_inside_was = -1;						\
	event_handled = false;						\
									\
	DISABLE_INTERRUPTS(kt_flags);					\
									\
	if (!kt_ctx.enabled)						\
		goto exit;						\
									\
	/* Ignore reports from some interrupts for now. */		\
	if (IN_INTERRUPT())						\
		goto exit;						\
									\
	/* TODO(xairy): check if we even need theese checks. */		\
	if (!current)							\
		goto exit;						\
	if (!current->ktsan.thr)					\
		goto exit;						\
									\
	thr = current->ktsan.thr;					\
	pc = (uptr_t)_RET_IP_;						\
									\
	if (thr->event_disable_depth != 0 && !(handle_disabled))	\
		goto exit;						\
									\
	if (thr->cpu == NULL && !(handle_scheduler))			\
		goto exit;						\
									\
	kt_inside_was = kt_atomic32_compare_exchange_no_ktsan(		\
				&thr->inside, 0, 1);			\
	if (kt_inside_was != 0) {					\
		goto exit;						\
	}								\
									\
	event_handled = true;						\
/**/

#define LEAVE()								\
	/* thr might become NULL in ktsan_thread_destroy. */		\
	thr = current->ktsan.thr;					\
									\
	if (thr) {							\
		kt_inside_was =	kt_atomic32_compare_exchange_no_ktsan(	\
					&thr->inside, 1, 0);		\
		BUG_ON(kt_inside_was != 1);				\
	}								\
									\
exit:									\
	ENABLE_INTERRUPTS(kt_flags);					\
/**/

void __init ktsan_init_early(void)
{
	kt_ctx_t *ctx = &kt_ctx;

	kt_tab_init(&ctx->sync_tab, 10007,
		    sizeof(kt_tab_sync_t), KT_MAX_SYNC_COUNT);
	kt_tab_init(&ctx->memblock_tab, 10007,
		    sizeof(kt_tab_memblock_t), KT_MAX_MEMBLOCK_COUNT);
	kt_cache_init(&ctx->percpu_sync_cache,
		      sizeof(kt_percpu_sync_t), KT_MAX_PERCPU_SYNC_COUNT);

	kt_tab_init(&ctx->test_tab, 13, sizeof(kt_tab_test_t), 20);

	kt_thr_pool_init();
}

void ktsan_init(void)
{
	kt_ctx_t *ctx;
	kt_thr_t *thr;
	int inside;

	ctx = &kt_ctx;

	thr = kt_thr_create(NULL, current->pid);
	kt_thr_start(thr, (uptr_t)_RET_IP_);
	current->ktsan.thr = thr;

	BUG_ON(ctx->enabled);
	inside = atomic_cmpxchg(&thr->inside, 0, 1);
	BUG_ON(inside != 0);

	ctx->cpus = alloc_percpu(kt_cpu_t);
	kt_stat_init();
	kt_tests_init();

	/* These stats were not recorded in kt_thr_create. */
	kt_stat_inc(thr, kt_stat_thread_create);
	kt_stat_inc(thr, kt_stat_threads);

	inside = atomic_cmpxchg(&thr->inside, 1, 0);
	BUG_ON(inside != 1);
	ctx->enabled = 1;

	pr_err("ktsan: enabled.\n");
	pr_err("ktsan memory usage: %lu GB + shadow.\n",
		(KT_MAX_SYNC_COUNT * sizeof(kt_tab_sync_t) +
		 KT_MAX_MEMBLOCK_COUNT * sizeof(kt_tab_memblock_t) +
		 KT_MAX_PERCPU_SYNC_COUNT * sizeof(kt_percpu_sync_t) +
		 KT_MAX_THREAD_COUNT * sizeof(kt_thr_t)) >> 30);
	pr_err("             syncs: %lu MB\n",
		(KT_MAX_SYNC_COUNT * sizeof(kt_tab_sync_t)) >> 20);
	pr_err("          memblock: %lu MB\n",
		(KT_MAX_MEMBLOCK_COUNT * sizeof(kt_tab_memblock_t)) >> 20);
	pr_err("      percpu syncs: %lu MB\n",
		(KT_MAX_PERCPU_SYNC_COUNT * sizeof(kt_percpu_sync_t)) >> 20);
	pr_err("              thrs: %lu MB\n",
		(KT_MAX_THREAD_COUNT * sizeof(kt_thr_t)) >> 20);
}

void ktsan_print_diagnostics(void)
{
	ENTER(false, true);
	LEAVE();

	pr_err("# # # # # # # # # # ktsan diagnostics # # # # # # # # # #\n");
	pr_err("\n");

#if KT_DEBUG_TRACE
	if (thr != NULL) {
		kt_time_t time;
		time = kt_clk_get(&thr->clk, thr->id);
		kt_trace_dump(&thr->trace, (time - 512) % KT_TRACE_SIZE, time);
		pr_err("\n");
	}
#endif /* KT_DEBUG_TRACE */

	pr_err("Runtime:\n");
	pr_err(" runtime active:                %s\n", event_handled ? "+" : "-");
	if (!event_handled) {
		pr_err(" kt_ctx.enabled:                %s\n",
			(kt_ctx.enabled) ? "+" : "-");
		pr_err(" !IN_INTERRUPT():               %s\n",
			(!IN_INTERRUPT()) ? "+" : "-");
		pr_err(" current:                       %s\n",
			(current) ? "+" : "-");
		pr_err(" current->ktsan.thr:            %s\n",
			(current->ktsan.thr) ? "+" : "-");
		if (thr != NULL) {
			pr_err(" thr->event_disable_depth == 0: %s\n",
				(thr->event_disable_depth == 0) ? "+" : "-");
			pr_err(" thr->cpu != NULL:              %s\n",
				(thr->cpu != NULL) ? "+" : "-");
		}
		pr_err(" kt_inside_was == 0:            %s\n",
			(kt_inside_was == 0) ? "+" : "-");
	}

	pr_err("\n");

	if (thr != NULL) {
		pr_err("Thread:\n");
		pr_err(" thr->id:                    %d\n", thr->id);
		pr_err(" thr->kid:                   %d\n", thr->kid);
		pr_err(" thr->inside:                %d\n",
			kt_atomic32_load_no_ktsan((void *)&thr->inside));
		pr_err(" thr->call_depth:            %d\n", thr->call_depth);
		pr_err(" thr->report_disable_depth:  %d\n",
			thr->report_disable_depth);
		pr_err(" thr->preempt_disable_depth: %d\n",
			thr->preempt_disable_depth);
		pr_err(" thr->event_disable_depth:   %d\n",
			thr->event_disable_depth);
		pr_err(" thr->irqs_disabled:         %s\n",
			thr->irqs_disabled ? "+" : "-");
		pr_err("\n");
	}

	pr_err("Stack trace:\n");
	kt_stack_print_current(_RET_IP_);
	pr_err("\n");

#if KT_DEBUG
	if (thr != NULL) {
		kt_stack_t stack;

		pr_err("Last event disable:\n");
		kt_trace_restore_stack(thr,
				thr->last_event_disable_time, &stack);
		kt_stack_print(&stack);
		pr_err("\n");

		pr_err("Last event enable:\n");
		kt_trace_restore_stack(thr,
				thr->last_event_enable_time, &stack);
		kt_stack_print(&stack);
		pr_err("\n");

		pr_err("Thread start:\n");
		kt_stack_print(&thr->start_stack);
		pr_err("\n");
	}
#endif /* KT_DEBUG */

	pr_err("# # # # # # # # # # # # # # # # # # # # # # # # # # # # #\n");
}

/* FIXME(xairy): not sure if this is the best place for this
   function, but it requires access to ENTER and LEAVE. */
void kt_tests_run(void)
{
	ENTER(false, false);
	kt_tests_run_noinst();
	LEAVE();
	kt_tests_run_inst();
}

void ktsan_thr_create(struct ktsan_thr_s *new, int kid)
{
	ENTER(true, true);
	new->thr = kt_thr_create(thr, kid);
	LEAVE();
}

void ktsan_thr_destroy(struct ktsan_thr_s *old)
{
	ENTER(true, true);
	kt_thr_destroy(thr, old->thr);
	old->thr = NULL;
	LEAVE();
}

void ktsan_thr_start(void)
{
	ENTER(true, true);
	kt_thr_start(thr, pc);
	LEAVE();
}

void ktsan_thr_stop(void)
{
	ENTER(true, true);
	kt_thr_stop(thr, pc);
	LEAVE();
}

void ktsan_thr_event_disable(void)
{
	ENTER(false, true);
	kt_thr_event_disable(thr, pc);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_thr_event_disable);

void ktsan_thr_event_enable(void)
{
	ENTER(false, true);
	kt_thr_event_enable(thr, pc);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_thr_event_enable);

void ktsan_report_disable(void)
{
	ENTER(false, false);
	kt_report_disable(thr);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_report_disable);

void ktsan_report_enable(void)
{
	ENTER(false, false);
	kt_report_enable(thr);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_report_enable);

void ktsan_sync_acquire(void *addr)
{
	ENTER(false, false);
	kt_trace_add_event(thr, kt_event_type_acquire, pc);
	kt_clk_tick(&thr->clk, thr->id);
	kt_sync_acquire(thr, pc, (uptr_t)addr);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_sync_acquire);

void ktsan_sync_release(void *addr)
{
	ENTER(false, false);
	kt_trace_add_event(thr, kt_event_type_release, pc);
	kt_clk_tick(&thr->clk, thr->id);
	kt_sync_release(thr, pc, (uptr_t)addr);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_sync_release);

void ktsan_memblock_alloc(void *addr, unsigned long size)
{
	ENTER(false, true);
	kt_memblock_alloc(thr, pc, (uptr_t)addr, (size_t)size);
	LEAVE();
}

void ktsan_memblock_free(void *addr, unsigned long size)
{
	ENTER(false, true);
	kt_memblock_free(thr, pc, (uptr_t)addr, (size_t)size);
	LEAVE();
}

void ktsan_mtx_pre_lock(void *addr, bool write, bool try)
{
	ENTER(false, true);

	if (kt_thr_event_disable(thr, pc)) {
		thr->irq_flags_before_mtx = kt_flags;
		/* Set all disabled in kt_flags. */
		kt_flags = arch_local_irq_save();

		kt_mtx_pre_lock(thr, pc, (uptr_t)addr, write, try);
	}

	LEAVE();
}
EXPORT_SYMBOL(ktsan_mtx_pre_lock);

void ktsan_mtx_post_lock(void *addr, bool write, bool try, bool success)
{
	ENTER(false, true);

	if (kt_thr_event_enable(thr, pc)) {
		kt_mtx_post_lock(thr, pc, (uptr_t)addr, write, try, success);

		kt_flags = thr->irq_flags_before_mtx;
	}

	LEAVE();
}
EXPORT_SYMBOL(ktsan_mtx_post_lock);

void ktsan_mtx_pre_unlock(void *addr, bool write)
{
	ENTER(false, true);

	if (kt_thr_event_disable(thr, pc)) {
		thr->irq_flags_before_mtx = kt_flags;
		/* Set all disabled in kt_flags. */
		kt_flags = arch_local_irq_save();

		kt_mtx_pre_unlock(thr, pc, (uptr_t)addr, write);
	}

	LEAVE();
}
EXPORT_SYMBOL(ktsan_mtx_pre_unlock);

void ktsan_mtx_post_unlock(void *addr, bool write)
{
	ENTER(false, true);

	if (kt_thr_event_enable(thr, pc)) {
		kt_mtx_post_unlock(thr, pc, (uptr_t)addr, write);

		kt_flags = thr->irq_flags_before_mtx;
	}

	LEAVE();
}
EXPORT_SYMBOL(ktsan_mtx_post_unlock);

void ktsan_thread_fence(ktsan_memory_order_t mo)
{
	ENTER(false, false);
	kt_thread_fence(thr, pc, mo);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_thread_fence);

void ktsan_atomic8_store(void *addr, u8 value, ktsan_memory_order_t mo)
{
	ENTER(false, false);
	kt_atomic8_store(thr, pc, addr, value, mo);
	LEAVE();

	if (!event_handled)
		return kt_atomic8_store_no_ktsan(addr, value);
}
EXPORT_SYMBOL(ktsan_atomic8_store);

void ktsan_atomic16_store(void *addr, u16 value, ktsan_memory_order_t mo)
{
	ENTER(false, false);
	kt_atomic16_store(thr, pc, addr, value, mo);
	LEAVE();

	if (!event_handled)
		return kt_atomic16_store_no_ktsan(addr, value);
}
EXPORT_SYMBOL(ktsan_atomic16_store);

void ktsan_atomic32_store(void *addr, u32 value, ktsan_memory_order_t mo)
{
	ENTER(false, false);
	kt_atomic32_store(thr, pc, addr, value, mo);
	LEAVE();

	if (!event_handled)
		return kt_atomic32_store_no_ktsan(addr, value);
}
EXPORT_SYMBOL(ktsan_atomic32_store);

void ktsan_atomic64_store(void *addr, u64 value, ktsan_memory_order_t mo)
{
	ENTER(false, false);
	kt_atomic64_store(thr, pc, addr, value, mo);
	LEAVE();

	if (!event_handled)
		return kt_atomic64_store_no_ktsan(addr, value);
}
EXPORT_SYMBOL(ktsan_atomic64_store);

u8 ktsan_atomic8_load(void *addr, ktsan_memory_order_t mo)
{
	u8 rv;

	ENTER(false, false);
	rv = kt_atomic8_load(thr, pc, addr, mo);
	LEAVE();

	if (!event_handled)
		return kt_atomic8_load_no_ktsan(addr);
	return rv;
}
EXPORT_SYMBOL(ktsan_atomic8_load);

u16 ktsan_atomic16_load(void *addr, ktsan_memory_order_t mo)
{
	u16 rv;

	ENTER(false, false);
	rv = kt_atomic16_load(thr, pc, addr, mo);
	LEAVE();

	if (!event_handled)
		return kt_atomic16_load_no_ktsan(addr);
	return rv;
}
EXPORT_SYMBOL(ktsan_atomic16_load);

u32 ktsan_atomic32_load(void *addr, ktsan_memory_order_t mo)
{
	u32 rv;

	ENTER(false, false);
	rv = kt_atomic32_load(thr, pc, addr, mo);
	LEAVE();

	if (!event_handled)
		return kt_atomic32_load_no_ktsan(addr);
	return rv;
}
EXPORT_SYMBOL(ktsan_atomic32_load);

u64 ktsan_atomic64_load(void *addr, ktsan_memory_order_t mo)
{
	u64 rv;

	ENTER(false, false);
	rv = kt_atomic64_load(thr, pc, addr, mo);
	LEAVE();

	if (!event_handled)
		return kt_atomic64_load_no_ktsan(addr);
	return rv;
}
EXPORT_SYMBOL(ktsan_atomic64_load);

u8 ktsan_atomic8_exchange(void *addr, u8 value, ktsan_memory_order_t mo)
{
	u8 rv;

	ENTER(false, false);
	rv = kt_atomic8_exchange(thr, pc, addr, value, mo);
	LEAVE();

	if (!event_handled)
		return kt_atomic8_exchange_no_ktsan(addr, value);
	return rv;
}
EXPORT_SYMBOL(ktsan_atomic8_exchange);

u16 ktsan_atomic16_exchange(void *addr, u16 value, ktsan_memory_order_t mo)
{
	u16 rv;

	ENTER(false, false);
	rv = kt_atomic16_exchange(thr, pc, addr, value, mo);
	LEAVE();

	if (!event_handled)
		return kt_atomic16_exchange_no_ktsan(addr, value);
	return rv;
}
EXPORT_SYMBOL(ktsan_atomic16_exchange);

u32 ktsan_atomic32_exchange(void *addr, u32 value, ktsan_memory_order_t mo)
{
	u32 rv;

	ENTER(false, false);
	rv = kt_atomic32_exchange(thr, pc, addr, value, mo);
	LEAVE();

	if (!event_handled)
		return kt_atomic32_exchange_no_ktsan(addr, value);
	return rv;
}
EXPORT_SYMBOL(ktsan_atomic32_exchange);

u64 ktsan_atomic64_exchange(void *addr, u64 value, ktsan_memory_order_t mo)
{
	u64 rv;

	ENTER(false, false);
	rv = kt_atomic64_exchange(thr, pc, addr, value, mo);
	LEAVE();

	if (!event_handled)
		return kt_atomic64_exchange_no_ktsan(addr, value);
	return rv;
}
EXPORT_SYMBOL(ktsan_atomic64_exchange);

u8 ktsan_atomic8_compare_exchange(void *addr, u8 old, u8 new,
					ktsan_memory_order_t mo)
{
	u8 rv;

	ENTER(false, false);
	rv = kt_atomic8_compare_exchange(thr, pc, addr, old, new, mo);
	LEAVE();

	if (!event_handled)
		return kt_atomic8_compare_exchange_no_ktsan(addr, old, new);
	return rv;
}
EXPORT_SYMBOL(ktsan_atomic8_compare_exchange);

u16 ktsan_atomic16_compare_exchange(void *addr, u16 old, u16 new,
					ktsan_memory_order_t mo)
{
	u16 rv;

	ENTER(false, false);
	rv = kt_atomic16_compare_exchange(thr, pc, addr, old, new, mo);
	LEAVE();

	if (!event_handled)
		return kt_atomic16_compare_exchange_no_ktsan(addr, old, new);
	return rv;
}
EXPORT_SYMBOL(ktsan_atomic16_compare_exchange);

u32 ktsan_atomic32_compare_exchange(void *addr, u32 old, u32 new,
					ktsan_memory_order_t mo)
{
	u32 rv;

	ENTER(false, false);
	rv = kt_atomic32_compare_exchange(thr, pc, addr, old, new, mo);
	LEAVE();

	if (!event_handled)
		return kt_atomic32_compare_exchange_no_ktsan(addr, old, new);
	return rv;
}
EXPORT_SYMBOL(ktsan_atomic32_compare_exchange);

u64 ktsan_atomic64_compare_exchange(void *addr, u64 old, u64 new,
					ktsan_memory_order_t mo)
{
	u64 rv;

	ENTER(false, false);
	rv = kt_atomic64_compare_exchange(thr, pc, addr, old, new, mo);
	LEAVE();

	if (!event_handled)
		return kt_atomic64_compare_exchange_no_ktsan(addr, old, new);
	return rv;
}
EXPORT_SYMBOL(ktsan_atomic64_compare_exchange);

u8 ktsan_atomic8_fetch_add(void *addr, u8 value, ktsan_memory_order_t mo)
{
	u8 rv;

	ENTER(false, false);
	rv = kt_atomic8_fetch_add(thr, pc, addr, value, mo);
	LEAVE();

	if (!event_handled)
		return kt_atomic8_fetch_add_no_ktsan(addr, value);
	return rv;
}
EXPORT_SYMBOL(ktsan_atomic8_fetch_add);

u16 ktsan_atomic16_fetch_add(void *addr, u16 value, ktsan_memory_order_t mo)
{
	u16 rv;

	ENTER(false, false);
	rv = kt_atomic16_fetch_add(thr, pc, addr, value, mo);
	LEAVE();

	if (!event_handled)
		return kt_atomic16_fetch_add_no_ktsan(addr, value);
	return rv;
}
EXPORT_SYMBOL(ktsan_atomic16_fetch_add);

u32 ktsan_atomic32_fetch_add(void *addr, u32 value, ktsan_memory_order_t mo)
{
	u32 rv;

	ENTER(false, false);
	rv = kt_atomic32_fetch_add(thr, pc, addr, value, mo);
	LEAVE();

	if (!event_handled)
		return kt_atomic32_fetch_add_no_ktsan(addr, value);
	return rv;
}
EXPORT_SYMBOL(ktsan_atomic32_fetch_add);

u64 ktsan_atomic64_fetch_add(void *addr, u64 value, ktsan_memory_order_t mo)
{
	u64 rv;

	ENTER(false, false);
	rv = kt_atomic64_fetch_add(thr, pc, addr, value, mo);
	LEAVE();

	if (!event_handled)
		return kt_atomic64_fetch_add_no_ktsan(addr, value);
	return rv;
}
EXPORT_SYMBOL(ktsan_atomic64_fetch_add);

void ktsan_atomic_set_bit(void *addr, long nr, ktsan_memory_order_t mo)
{
	ENTER(false, false);
	kt_atomic_set_bit(thr, pc, addr, nr, mo);
	LEAVE();

	if (!event_handled)
		kt_atomic_set_bit_no_ktsan(addr, nr);
}
EXPORT_SYMBOL(ktsan_atomic_set_bit);

void ktsan_atomic_clear_bit(void *addr, long nr, ktsan_memory_order_t mo)
{
	ENTER(false, false);
	kt_atomic_clear_bit(thr, pc, addr, nr, mo);
	LEAVE();

	if (!event_handled)
		kt_atomic_clear_bit_no_ktsan(addr, nr);
}
EXPORT_SYMBOL(ktsan_atomic_clear_bit);

void ktsan_atomic_change_bit(void *addr, long nr, ktsan_memory_order_t mo)
{
	ENTER(false, false);
	kt_atomic_change_bit(thr, pc, addr, nr, mo);
	LEAVE();

	if (!event_handled)
		kt_atomic_change_bit_no_ktsan(addr, nr);
}
EXPORT_SYMBOL(ktsan_atomic_change_bit);

int ktsan_atomic_fetch_set_bit(void *addr, long nr, ktsan_memory_order_t mo)
{
	int rv;

	ENTER(false, false);
	rv = kt_atomic_fetch_set_bit(thr, pc, addr, nr, mo);
	LEAVE();

	if (!event_handled)
		return kt_atomic_fetch_set_bit_no_ktsan(addr, nr);
	return rv;
}
EXPORT_SYMBOL(ktsan_atomic_fetch_set_bit);

int ktsan_atomic_fetch_clear_bit(void *addr, long nr, ktsan_memory_order_t mo)
{
	int rv;

	ENTER(false, false);
	rv = kt_atomic_fetch_clear_bit(thr, pc, addr, nr, mo);
	LEAVE();

	if (!event_handled)
		return kt_atomic_fetch_clear_bit_no_ktsan(addr, nr);
	return rv;
}
EXPORT_SYMBOL(ktsan_atomic_fetch_clear_bit);

int ktsan_atomic_fetch_change_bit(void *addr, long nr, ktsan_memory_order_t mo)
{
	int rv;

	ENTER(false, false);
	rv = kt_atomic_fetch_change_bit(thr, pc, addr, nr, mo);
	LEAVE();

	if (!event_handled)
		return kt_atomic_fetch_change_bit_no_ktsan(addr, nr);
	return rv;
}
EXPORT_SYMBOL(ktsan_atomic_fetch_change_bit);

void ktsan_preempt_add(int value)
{
	ENTER(false, false);
	kt_preempt_add(thr, pc, value);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_preempt_add);

void ktsan_preempt_sub(int value)
{
	ENTER(false, false);
	kt_preempt_sub(thr, pc, value);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_preempt_sub);

void ktsan_irq_disable(void)
{
	ENTER(false, false);
	kt_irq_disable(thr, pc);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_irq_disable);

void ktsan_irq_enable(void)
{
	ENTER(false, false);
	kt_irq_enable(thr, pc);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_irq_enable);

void ktsan_irq_save(void)
{
	ENTER(false, false);
	kt_irq_save(thr, pc);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_irq_save);

void ktsan_irq_restore(unsigned long flags)
{
	ENTER(false, false);
	kt_irq_restore(thr, pc, flags);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_irq_restore);

void ktsan_percpu_acquire(void *addr)
{
	ENTER(false, false);
	kt_percpu_acquire(thr, pc, (uptr_t)addr);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_percpu_acquire);

void ktsan_read1(void *addr)
{
	ENTER(false, false);
	kt_access(thr, pc, (uptr_t)addr, 0, true);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_read1);

void ktsan_read2(void *addr)
{
	ENTER(false, false);
	kt_access(thr, pc, (uptr_t)addr, 1, true);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_read2);

void ktsan_read4(void *addr)
{
	ENTER(false, false);
	kt_access(thr, pc, (uptr_t)addr, 2, true);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_read4);

void ktsan_read8(void *addr)
{
	ENTER(false, false);
	kt_access(thr, pc, (uptr_t)addr, 3, true);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_read8);

void ktsan_read16(void *addr)
{
	ENTER(false, false);
	kt_access(thr, pc, (uptr_t)addr, 3, true);
	kt_access(thr, pc, (uptr_t)addr + 8, 3, true);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_read16);

void ktsan_read_range(void *addr, size_t sz)
{
	ENTER(false, false);
	kt_access_range(thr, pc, (uptr_t)addr, sz, true);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_read_range);

void ktsan_write1(void *addr)
{
	ENTER(false, false);
	kt_access(thr, pc, (uptr_t)addr, 0, false);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_write1);

void ktsan_write2(void *addr)
{
	ENTER(false, false);
	kt_access(thr, pc, (uptr_t)addr, 1, false);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_write2);

void ktsan_write4(void *addr)
{
	ENTER(false, false);
	kt_access(thr, pc, (uptr_t)addr, 2, false);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_write4);

void ktsan_write8(void *addr)
{
	ENTER(false, false);
	kt_access(thr, pc, (uptr_t)addr, 3, false);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_write8);

void ktsan_write16(void *addr)
{
	ENTER(false, false);
	kt_access(thr, pc, (uptr_t)addr, 3, false);
	kt_access(thr, pc, (uptr_t)addr + 8, 3, false);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_write16);

void ktsan_write_range(void *addr, size_t sz)
{
	ENTER(false, false);
	kt_access_range(thr, pc, (uptr_t)addr, sz, false);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_write_range);

void ktsan_func_entry(void *call_pc)
{
	ENTER(false, true);
	kt_func_entry(thr, (uptr_t)call_pc);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_func_entry);

void ktsan_func_exit(void)
{
	ENTER(false, true);
	kt_func_exit(thr);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_func_exit);

void __tsan_read1(unsigned long addr) __attribute__ ((alias("ktsan_read1")));
EXPORT_SYMBOL(__tsan_read1);

void __tsan_read2(unsigned long addr) __attribute__ ((alias("ktsan_read2")));
EXPORT_SYMBOL(__tsan_read2);

void __tsan_read4(unsigned long addr) __attribute__ ((alias("ktsan_read4")));
EXPORT_SYMBOL(__tsan_read4);

void __tsan_read8(unsigned long addr) __attribute__ ((alias("ktsan_read8")));
EXPORT_SYMBOL(__tsan_read8);

void __tsan_read16(unsigned long addr) __attribute__ ((alias("ktsan_read16")));
EXPORT_SYMBOL(__tsan_read16);

void __tsan_read_range(unsigned long addr, unsigned long size)
	__attribute__ ((alias("ktsan_read_range")));
EXPORT_SYMBOL(__tsan_read_range);

void __tsan_write1(unsigned long addr) __attribute__ ((alias("ktsan_write1")));
EXPORT_SYMBOL(__tsan_write1);

void __tsan_write2(unsigned long addr) __attribute__ ((alias("ktsan_write2")));
EXPORT_SYMBOL(__tsan_write2);

void __tsan_write4(unsigned long addr) __attribute__ ((alias("ktsan_write4")));
EXPORT_SYMBOL(__tsan_write4);

void __tsan_write8(unsigned long addr) __attribute__ ((alias("ktsan_write8")));
EXPORT_SYMBOL(__tsan_write8);

void __tsan_write16(unsigned long addr) __attribute__ ((alias("ktsan_write16")));
EXPORT_SYMBOL(__tsan_write16);

void __tsan_write_range(unsigned long addr, unsigned long size)
	__attribute__ ((alias("ktsan_write_range")));
EXPORT_SYMBOL(__tsan_write_range);

void __tsan_func_entry(unsigned long addr) __attribute__ ((alias("ktsan_func_entry")));
EXPORT_SYMBOL(__tsan_func_entry);

void __tsan_func_exit(unsigned long addr) __attribute__ ((alias("ktsan_func_exit")));
EXPORT_SYMBOL(__tsan_func_exit);
