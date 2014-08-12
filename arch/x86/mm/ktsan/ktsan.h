#ifndef __X86_MM_KTSAN_KTSAN_H
#define __X86_MM_KTSAN_KTSAN_H

#include <linux/ktsan.h>

#define KTSAN_SHADOW_SLOTS_LOG 2
#define KTSAN_SHADOW_SLOTS (1 << KTSAN_SHADOW_SLOTS_LOG)

#define KTSAN_GRAIN 8

#define KTSAN_THREAD_ID_BITS     13
#define KTSAN_CLOCK_BITS         42

typedef unsigned long ktsan_time_t;

struct ktsan_clk_s {
	ktsan_time_t time[KTSAN_MAX_THREAD_ID];
};

struct shadow {
	unsigned long thread_id : KTSAN_THREAD_ID_BITS;
	unsigned long clock     : KTSAN_CLOCK_BITS;
	unsigned long offset    : 3;
	unsigned long size      : 2;
	unsigned long is_read   : 1;
	unsigned long is_atomic : 1;
	unsigned long is_freed  : 1;
};

/*
 * Clocks
 */
ktsan_clk_t *ktsan_clk_create(ktsan_thr_t *thr);
void ktsan_clk_destroy(ktsan_thr_t *thr, ktsan_clk_t *clk);
void ktsan_clk_acquire(ktsan_thr_t *thr, ktsan_clk_t *dst, ktsan_clk_t *src);

static inline
ktsan_time_t ktsan_clk_get(ktsan_clk_t *clk, int tid)
{
	return clk->time[tid];
}

static inline
void ktsan_clk_tick(ktsan_clk_t *clk, int tid)
{
	clk->time[tid]++;
}

/* Fow testing purposes. */
void ktsan_access_memory(unsigned long addr, size_t size, bool is_read);

#define KTSAN_MAX_STACK_TRACE_FRAMES 64

struct race_info {
	unsigned long addr;
	struct shadow old;
	struct shadow new;
	unsigned long strip_addr;
};

void report_race(struct race_info* info);

#endif /* __X86_MM_KTSAN_KTSAN_H */
