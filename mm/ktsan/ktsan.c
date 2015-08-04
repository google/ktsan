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

/* If scheduler is false the events generated from
   the scheduler internals will be ignored. */
#define ENTER(scheduler)					\
	kt_thr_t *thr;						\
	uptr_t pc;						\
	unsigned long kt_flags;					\
	int kt_inside_was;					\
	bool event_handled;					\
								\
	thr = NULL;						\
	kt_inside_was = -1;					\
	event_handled = false;					\
								\
	DISABLE_INTERRUPTS(kt_flags);				\
								\
	if (!kt_ctx.enabled)					\
		goto exit;					\
								\
	/* Ignore reports from some interrupts for now. */	\
	if (IN_INTERRUPT())					\
		goto exit;					\
								\
	/* TODO(xairy): check if we even need theese checks. */	\
	if (!current)						\
		goto exit;					\
	if (!current->ktsan.thr)				\
		goto exit;					\
								\
	thr = current->ktsan.thr;				\
	pc = (uptr_t)_RET_IP_;					\
								\
	if (thr->cpu == NULL && !(scheduler))			\
		goto exit;					\
								\
	kt_inside_was = kt_atomic32_cmpxchg_no_ktsan(		\
				&thr->inside, 0, 1);		\
	if (kt_inside_was != 0) {				\
		goto exit;					\
	}							\
								\
	event_handled = true;					\
/**/

#define LEAVE()							\
	/* thr might become NULL in ktsan_thread_destroy. */	\
	thr = current->ktsan.thr;				\
								\
	if (thr) {						\
		kt_inside_was =	kt_atomic32_cmpxchg_no_ktsan(	\
					&thr->inside, 1, 0);	\
		BUG_ON(kt_inside_was != 1);			\
	}							\
								\
exit:								\
	ENABLE_INTERRUPTS(kt_flags);				\
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
}

void ktsan_print_diagnostics(void)
{
	ENTER(false);
	LEAVE();

	pr_err("#! ktsan runtime is %s!\n",
		event_handled ? "active" : "not active");
	if (!event_handled) {
		pr_err("  kt_ctx.enabled:      %s\n",
			(kt_ctx.enabled) ? "+" : "-");
		pr_err("  !IN_INTERRUPT():     %s\n",
			(!IN_INTERRUPT()) ? "+" : "-");
		pr_err("  current:             %s\n",
			(current) ? "+" : "-");
		pr_err("  current->ktsan.thr : %s\n",
			(current->ktsan.thr) ? "+" : "-");
		pr_err("  thr->cpu != NULL:    %s\n",
			(thr->cpu != NULL) ? "+" : "-");
		pr_err("  kt_inside_was == 0:  %s\n",\
			(kt_inside_was == 0) ? "+" : "-");
		pr_err("Stack trace:\n");
		kt_stack_print_current(_RET_IP_);
	}
}

/* FIXME(xairy): not sure if this is the best place for this
   function, but it requires access to ENTER and LEAVE. */
void kt_tests_run(void)
{
	ENTER(false);
	kt_tests_run_noinst();
	LEAVE();
	kt_tests_run_inst();
}

void ktsan_thr_create(struct ktsan_thr_s *new, int kid)
{
	ENTER(true);
	new->thr = kt_thr_create(thr, kid);
	LEAVE();
}

void ktsan_thr_destroy(struct ktsan_thr_s *old)
{
	ENTER(true);
	kt_thr_destroy(thr, old->thr);
	old->thr = NULL;
	LEAVE();
}

void ktsan_thr_start(void)
{
	ENTER(true);
	kt_thr_start(thr, pc);
	LEAVE();
}

void ktsan_thr_stop(void)
{
	ENTER(true);
	kt_thr_stop(thr, pc);
	LEAVE();
}

void ktsan_report_disable(void)
{
	ENTER(false);
	kt_report_disable(thr);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_report_disable);

void ktsan_report_enable(void)
{
	ENTER(false);
	kt_report_enable(thr);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_report_enable);

void ktsan_sync_acquire(void *addr)
{
	ENTER(false);
	kt_trace_add_event(thr, kt_event_type_acquire, pc);
	kt_clk_tick(&thr->clk, thr->id);
	kt_sync_acquire(thr, pc, (uptr_t)addr);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_sync_acquire);

void ktsan_sync_release(void *addr)
{
	ENTER(false);
	kt_trace_add_event(thr, kt_event_type_release, pc);
	kt_clk_tick(&thr->clk, thr->id);
	kt_sync_release(thr, pc, (uptr_t)addr);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_sync_release);

void ktsan_memblock_alloc(void *addr, unsigned long size)
{
	ENTER(false);
	kt_memblock_alloc(thr, pc, (uptr_t)addr, (size_t)size);
	LEAVE();
}

void ktsan_memblock_free(void *addr, unsigned long size)
{
	ENTER(false);
	kt_memblock_free(thr, pc, (uptr_t)addr, (size_t)size);
	LEAVE();
}

void ktsan_mtx_pre_lock(void *addr, bool write, bool try)
{
	ENTER(false);
	kt_mtx_pre_lock(thr, pc, (uptr_t)addr, write, try);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_mtx_pre_lock);

void ktsan_mtx_post_lock(void *addr, bool write, bool try)
{
	ENTER(false);
	kt_mtx_post_lock(thr, pc, (uptr_t)addr, write, try);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_mtx_post_lock);

void ktsan_mtx_pre_unlock(void *addr, bool write)
{
	ENTER(false);
	kt_mtx_pre_unlock(thr, pc, (uptr_t)addr, write);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_mtx_pre_unlock);

void ktsan_membar_acquire(void)
{
	ENTER(false);
	kt_membar_acquire(thr, pc);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_membar_acquire);

void ktsan_membar_release(void)
{
	ENTER(false);
	kt_membar_release(thr, pc);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_membar_release);

void ktsan_membar_acq_rel(void)
{
	ENTER(false);
	kt_membar_acq_rel(thr, pc);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_membar_acq_rel);

void ktsan_atomic8_store(void *addr, u8 value, ktsan_memory_order_t mo)
{
	ENTER(false);
	kt_atomic8_store(thr, pc, addr, value, mo);
	LEAVE();

	if (!event_handled)
		return kt_atomic8_store_no_ktsan(addr, value);
}
EXPORT_SYMBOL(ktsan_atomic8_store);

void ktsan_atomic16_store(void *addr, u16 value, ktsan_memory_order_t mo)
{
	ENTER(false);
	kt_atomic16_store(thr, pc, addr, value, mo);
	LEAVE();

	if (!event_handled)
		return kt_atomic16_store_no_ktsan(addr, value);
}
EXPORT_SYMBOL(ktsan_atomic16_store);

void ktsan_atomic32_store(void *addr, u32 value, ktsan_memory_order_t mo)
{
	ENTER(false);
	kt_atomic32_store(thr, pc, addr, value, mo);
	LEAVE();

	if (!event_handled)
		return kt_atomic32_store_no_ktsan(addr, value);
}
EXPORT_SYMBOL(ktsan_atomic32_store);

void ktsan_atomic64_store(void *addr, u64 value, ktsan_memory_order_t mo)
{
	ENTER(false);
	kt_atomic64_store(thr, pc, addr, value, mo);
	LEAVE();

	if (!event_handled)
		return kt_atomic64_store_no_ktsan(addr, value);
}
EXPORT_SYMBOL(ktsan_atomic64_store);

u8 ktsan_atomic8_load(void *addr, ktsan_memory_order_t mo)
{
	u8 rv;

	ENTER(false);
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

	ENTER(false);
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

	ENTER(false);
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

	ENTER(false);
	rv = kt_atomic64_load(thr, pc, addr, mo);
	LEAVE();

	if (!event_handled)
		return kt_atomic64_load_no_ktsan(addr);
	return rv;
}
EXPORT_SYMBOL(ktsan_atomic64_load);

void ktsan_atomic32_add(void *addr, int value)
{
	ENTER(false);
	kt_atomic32_add(thr, pc, (uptr_t)addr, value);
	LEAVE();

	if (!event_handled)
		kt_atomic32_add_no_ktsan(addr, value);
}
EXPORT_SYMBOL(ktsan_atomic32_add);

void ktsan_atomic32_sub(void *addr, int value)
{
	ENTER(false);
	kt_atomic32_sub(thr, pc, (uptr_t)addr, value);
	LEAVE();

	if (!event_handled)
		kt_atomic32_sub_no_ktsan(addr, value);
}
EXPORT_SYMBOL(ktsan_atomic32_sub);

int ktsan_atomic32_sub_and_test(void *addr, int value)
{
	int rv;

	ENTER(false);
	rv = kt_atomic32_sub_and_test(thr, pc, (uptr_t)addr, value);
	LEAVE();

	if (!event_handled)
		return kt_atomic32_sub_and_test_no_ktsan(addr, value);
	return rv;
}
EXPORT_SYMBOL(ktsan_atomic32_sub_and_test);

int ktsan_atomic32_add_negative(void *addr, int value)
{
	int rv;

	ENTER(false);
	rv = kt_atomic32_add_negative(thr, pc, (uptr_t)addr, value);
	LEAVE();

	if (!event_handled)
		return kt_atomic32_add_negative_no_ktsan(addr, value);
	return rv;
}
EXPORT_SYMBOL(ktsan_atomic32_add_negative);

void ktsan_atomic32_inc(void *addr)
{
	ENTER(false);
	kt_atomic32_inc(thr, pc, (uptr_t)addr);
	LEAVE();

	if (!event_handled)
		kt_atomic32_inc_no_ktsan(addr);
}
EXPORT_SYMBOL(ktsan_atomic32_inc);

void ktsan_atomic32_dec(void *addr)
{
	ENTER(false);
	kt_atomic32_dec(thr, pc, (uptr_t)addr);
	LEAVE();

	if (!event_handled)
		kt_atomic32_dec_no_ktsan(addr);
}
EXPORT_SYMBOL(ktsan_atomic32_dec);

int ktsan_atomic32_inc_and_test(void *addr)
{
	int rv;

	ENTER(false);
	rv = kt_atomic32_inc_and_test(thr, pc, (uptr_t)addr);
	LEAVE();

	if (!event_handled)
		return kt_atomic32_inc_and_test_no_ktsan(addr);
	return rv;
}
EXPORT_SYMBOL(ktsan_atomic32_inc_and_test);

int ktsan_atomic32_dec_and_test(void *addr)
{
	int rv;

	ENTER(false);
	rv = kt_atomic32_dec_and_test(thr, pc, (uptr_t)addr);
	LEAVE();

	if (!event_handled)
		return kt_atomic32_dec_and_test_no_ktsan(addr);
	return rv;
}
EXPORT_SYMBOL(ktsan_atomic32_dec_and_test);

void ktsan_atomic64_add(void *addr, long value)
{
	ENTER(false);
	kt_atomic64_add(thr, pc, (uptr_t)addr, value);
	LEAVE();

	if (!event_handled)
		kt_atomic64_add_no_ktsan(addr, value);
}
EXPORT_SYMBOL(ktsan_atomic64_add);

void ktsan_atomic64_sub(void *addr, long value)
{
	ENTER(false);
	kt_atomic64_sub(thr, pc, (uptr_t)addr, value);
	LEAVE();

	if (!event_handled)
		kt_atomic64_sub_no_ktsan(addr, value);
}
EXPORT_SYMBOL(ktsan_atomic64_sub);

int ktsan_atomic64_sub_and_test(void *addr, long value)
{
	int rv;

	ENTER(false);
	rv = kt_atomic64_sub_and_test(thr, pc, (uptr_t)addr, value);
	LEAVE();

	if (!event_handled)
		return kt_atomic64_sub_and_test_no_ktsan(addr, value);
	return rv;
}
EXPORT_SYMBOL(ktsan_atomic64_sub_and_test);

int ktsan_atomic64_add_negative(void *addr, long value)
{
	int rv;

	ENTER(false);
	rv = kt_atomic64_add_negative(thr, pc, (uptr_t)addr, value);
	LEAVE();

	if (!event_handled)
		return kt_atomic64_add_negative_no_ktsan(addr, value);
	return rv;
}
EXPORT_SYMBOL(ktsan_atomic64_add_negative);

void ktsan_atomic64_inc(void *addr)
{
	ENTER(false);
	kt_atomic64_inc(thr, pc, (uptr_t)addr);
	LEAVE();

	if (!event_handled)
		kt_atomic64_inc_no_ktsan(addr);
}
EXPORT_SYMBOL(ktsan_atomic64_inc);

void ktsan_atomic64_dec(void *addr)
{
	ENTER(false);
	kt_atomic64_dec(thr, pc, (uptr_t)addr);
	LEAVE();

	if (!event_handled)
		kt_atomic64_dec_no_ktsan(addr);
}
EXPORT_SYMBOL(ktsan_atomic64_dec);

int ktsan_atomic64_inc_and_test(void *addr)
{
	int rv;

	ENTER(false);
	rv = kt_atomic64_inc_and_test(thr, pc, (uptr_t)addr);
	LEAVE();

	if (!event_handled)
		return kt_atomic64_inc_and_test_no_ktsan(addr);
	return rv;
}
EXPORT_SYMBOL(ktsan_atomic64_inc_and_test);

int ktsan_atomic64_dec_and_test(void *addr)
{
	int rv;

	ENTER(false);
	rv = kt_atomic64_dec_and_test(thr, pc, (uptr_t)addr);
	LEAVE();

	if (!event_handled)
		return kt_atomic64_dec_and_test_no_ktsan(addr);
	return rv;
}
EXPORT_SYMBOL(ktsan_atomic64_dec_and_test);

s64 ktsan_atomic64_xchg(void *addr, s64 value)
{
	s64 rv;

	ENTER(false);
	rv = kt_atomic64_xchg(thr, pc, (uptr_t)addr, value);
	LEAVE();

	if (!event_handled)
		return kt_atomic64_xchg_no_ktsan(addr, value);

	return rv;
}
EXPORT_SYMBOL(ktsan_atomic64_xchg);

s32 ktsan_atomic32_xchg(void *addr, s32 value)
{
	s32 rv;

	ENTER(false);
	rv = kt_atomic32_xchg(thr, pc, (uptr_t)addr, value);
	LEAVE();

	if (!event_handled)
		return kt_atomic32_xchg_no_ktsan(addr, value);
	return rv;
}
EXPORT_SYMBOL(ktsan_atomic32_xchg);

s16 ktsan_atomic16_xchg(void *addr, s16 value)
{
	s16 rv;

	ENTER(false);
	rv = kt_atomic16_xchg(thr, pc, (uptr_t)addr, value);
	LEAVE();

	if (!event_handled)
		return kt_atomic16_xchg_no_ktsan(addr, value);
	return rv;
}
EXPORT_SYMBOL(ktsan_atomic16_xchg);

s64 ktsan_atomic64_cmpxchg(void *addr, s64 old, s64 new)
{
	s64 rv;

	ENTER(false);
	rv = kt_atomic64_cmpxchg(thr, pc, (uptr_t)addr, old, new);
	LEAVE();

	if (!event_handled)
		return kt_atomic64_cmpxchg_no_ktsan(addr, old, new);
	return rv;
}
EXPORT_SYMBOL(ktsan_atomic64_cmpxchg);

s32 ktsan_atomic32_cmpxchg(void *addr, s32 old, s32 new)
{
	s32 rv;

	ENTER(false);
	rv = kt_atomic32_cmpxchg(thr, pc, (uptr_t)addr, old, new);
	LEAVE();

	if (!event_handled)
		return kt_atomic32_cmpxchg_no_ktsan(addr, old, new);
	return rv;
}
EXPORT_SYMBOL(ktsan_atomic32_cmpxchg);

s16 ktsan_atomic16_cmpxchg(void *addr, s16 old, s16 new)
{
	s16 rv;

	ENTER(false);
	rv = kt_atomic16_cmpxchg(thr, pc, (uptr_t)addr, old, new);
	LEAVE();

	if (!event_handled)
		return kt_atomic16_cmpxchg_no_ktsan(addr, old, new);
	return rv;
}
EXPORT_SYMBOL(ktsan_atomic16_cmpxchg);

s8 ktsan_atomic8_cmpxchg(void *addr, s8 old, s8 new)
{
	s8 rv;

	ENTER(false);
	rv = kt_atomic8_cmpxchg(thr, pc, (uptr_t)addr, old, new);
	LEAVE();

	if (!event_handled)
		return kt_atomic8_cmpxchg_no_ktsan(addr, old, new);
	return rv;
}
EXPORT_SYMBOL(ktsan_atomic8_cmpxchg);

s64 ktsan_atomic64_xadd(void *addr, s64 value)
{
	s64 rv;

	ENTER(false);
	rv = kt_atomic64_xadd(thr, pc, (uptr_t)addr, value);
	LEAVE();

	if (!event_handled)
		return kt_atomic64_xadd_no_ktsan(addr, value);
	return rv;
}
EXPORT_SYMBOL(ktsan_atomic64_xadd);

s32 ktsan_atomic32_xadd(void *addr, s32 value)
{
	s32 rv;

	ENTER(false);
	rv = kt_atomic32_xadd(thr, pc, (uptr_t)addr, value);
	LEAVE();

	if (!event_handled)
		return kt_atomic32_xadd_no_ktsan(addr, value);
	return rv;
}
EXPORT_SYMBOL(ktsan_atomic32_xadd);

s16 ktsan_atomic16_xadd(void *addr, s16 value)
{
	s16 rv;

	ENTER(false);
	rv = kt_atomic16_xadd(thr, pc, (uptr_t)addr, value);
	LEAVE();

	if (!event_handled)
		return kt_atomic16_xadd_no_ktsan(addr, value);
	return rv;
}
EXPORT_SYMBOL(ktsan_atomic16_xadd);

void ktsan_bitop_set_bit(void *addr, long nr)
{
	ENTER(false);
	kt_bitop_set_bit(thr, pc, (uptr_t)addr, nr);
	LEAVE();

	if (!event_handled)
		kt_bitop_set_bit_no_ktsan(addr, nr);
}
EXPORT_SYMBOL(ktsan_bitop_set_bit);

void ktsan_bitop_clear_bit(void *addr, long nr)
{
	ENTER(false);
	kt_bitop_clear_bit(thr, pc, (uptr_t)addr, nr);
	LEAVE();

	if (!event_handled)
		kt_bitop_clear_bit_no_ktsan(addr, nr);
}
EXPORT_SYMBOL(ktsan_bitop_clear_bit);

void ktsan_bitop_change_bit(void *addr, long nr)
{
	ENTER(false);
	kt_bitop_change_bit(thr, pc, (uptr_t)addr, nr);
	LEAVE();

	if (!event_handled)
		kt_bitop_change_bit_no_ktsan(addr, nr);
}
EXPORT_SYMBOL(ktsan_bitop_change_bit);

int ktsan_bitop_test_and_set_bit(void *addr, long nr)
{
	int rv;

	ENTER(false);
	rv = kt_bitop_test_and_set_bit(thr, pc, (uptr_t)addr, nr);
	LEAVE();

	if (!event_handled)
		return kt_bitop_test_and_set_bit_no_ktsan(addr, nr);
	return rv;
}
EXPORT_SYMBOL(ktsan_bitop_test_and_set_bit);

int ktsan_bitop_test_and_clear_bit(void *addr, long nr)
{
	int rv;

	ENTER(false);
	rv = kt_bitop_test_and_clear_bit(thr, pc, (uptr_t)addr, nr);
	LEAVE();

	if (!event_handled)
		return kt_bitop_test_and_clear_bit_no_ktsan(addr, nr);
	return rv;
}
EXPORT_SYMBOL(ktsan_bitop_test_and_clear_bit);

int ktsan_bitop_test_and_change_bit(void *addr, long nr)
{
	int rv;

	ENTER(false);
	rv = kt_bitop_test_and_change_bit(thr, pc, (uptr_t)addr, nr);
	LEAVE();

	if (!event_handled)
		return kt_bitop_test_and_change_bit_no_ktsan(addr, nr);
	return rv;
}
EXPORT_SYMBOL(ktsan_bitop_test_and_change_bit);

int ktsan_bitop_test_and_set_bit_lock(void *addr, long nr)
{
	int rv;

	ENTER(false);
	rv = kt_bitop_test_and_set_bit_lock(thr, pc, (uptr_t)addr, nr);
	LEAVE();

	if (!event_handled)
		return kt_bitop_test_and_set_bit_lock_no_ktsan(addr, nr);
	return rv;
}
EXPORT_SYMBOL(ktsan_bitop_test_and_set_bit_lock);

void ktsan_bitop_clear_bit_unlock(void *addr, long nr)
{
	ENTER(false);
	kt_bitop_clear_bit_unlock(thr, pc, (uptr_t)addr, nr);
	LEAVE();

	if (!event_handled)
		kt_bitop_clear_bit_unlock_no_ktsan(addr, nr);
}
EXPORT_SYMBOL(ktsan_bitop_clear_bit_unlock);

void ktsan_preempt_add(int value)
{
	ENTER(false);
	kt_preempt_add(thr, pc, value);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_preempt_add);

void ktsan_preempt_sub(int value)
{
	ENTER(false);
	kt_preempt_sub(thr, pc, value);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_preempt_sub);

void ktsan_irq_disable(void)
{
	ENTER(false);
	kt_irq_disable(thr, pc);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_irq_disable);

void ktsan_irq_enable(void)
{
	ENTER(false);
	kt_irq_enable(thr, pc);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_irq_enable);

void ktsan_irq_save(void)
{
	ENTER(false);
	kt_irq_save(thr, pc);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_irq_save);

void ktsan_irq_restore(unsigned long flags)
{
	ENTER(false);
	kt_irq_restore(thr, pc, flags);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_irq_restore);

void ktsan_percpu_acquire(void *addr)
{
	ENTER(false);
	kt_percpu_acquire(thr, pc, (uptr_t)addr);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_percpu_acquire);

void ktsan_read1(void *addr)
{
	ENTER(false);
	kt_access(thr, pc, (uptr_t)addr, 0, true);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_read1);

void ktsan_read2(void *addr)
{
	ENTER(false);
	kt_access(thr, pc, (uptr_t)addr, 1, true);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_read2);

void ktsan_read4(void *addr)
{
	ENTER(false);
	kt_access(thr, pc, (uptr_t)addr, 2, true);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_read4);

void ktsan_read8(void *addr)
{
	ENTER(false);
	kt_access(thr, pc, (uptr_t)addr, 3, true);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_read8);

void ktsan_read16(void *addr)
{
	ENTER(false);
	kt_access(thr, pc, (uptr_t)addr, 3, true);
	kt_access(thr, pc, (uptr_t)addr + 8, 3, true);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_read16);

void ktsan_write1(void *addr)
{
	ENTER(false);
	kt_access(thr, pc, (uptr_t)addr, 0, false);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_write1);

void ktsan_write2(void *addr)
{
	ENTER(false);
	kt_access(thr, pc, (uptr_t)addr, 1, false);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_write2);

void ktsan_write4(void *addr)
{
	ENTER(false);
	kt_access(thr, pc, (uptr_t)addr, 2, false);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_write4);

void ktsan_write8(void *addr)
{
	ENTER(false);
	kt_access(thr, pc, (uptr_t)addr, 3, false);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_write8);

void ktsan_write16(void *addr)
{
	ENTER(false);
	kt_access(thr, pc, (uptr_t)addr, 3, false);
	kt_access(thr, pc, (uptr_t)addr + 8, 3, false);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_write16);

void ktsan_func_entry(void *call_pc)
{
	ENTER(false);
	pc = (uptr_t)__builtin_return_address(1);
	kt_func_entry(thr, pc);
	LEAVE();
}
EXPORT_SYMBOL(ktsan_func_entry);

void ktsan_func_exit(void)
{
	ENTER(false);
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

void __tsan_func_entry(unsigned long addr) __attribute__ ((alias("ktsan_func_entry")));
EXPORT_SYMBOL(__tsan_func_entry);

void __tsan_func_exit(unsigned long addr) __attribute__ ((alias("ktsan_func_exit")));
EXPORT_SYMBOL(__tsan_func_exit);
