#ifndef __X86_MM_KTSAN_KTSAN_H
#define __X86_MM_KTSAN_KTSAN_H

#include <linux/ktsan.h>
#include <linux/spinlock.h>

#define KTSAN_SHADOW_SLOTS_LOG 2
#define KTSAN_SHADOW_SLOTS (1 << KTSAN_SHADOW_SLOTS_LOG)

#define KTSAN_GRAIN 8

#define KTSAN_THREAD_ID_BITS     13
#define KTSAN_CLOCK_BITS         42

#define KTSAN_MAX_THREAD_ID 4096

typedef unsigned long uptr_t;
typedef unsigned long ktsan_time_t;

typedef struct ktsan_tab_s	ktsan_tab_t;
typedef struct ktsan_tab_obj_s	ktsan_tab_obj_t;
typedef struct ktsan_tab_part_s	ktsan_tab_part_t;
typedef struct ktsan_sync_s	ktsan_sync_t;
typedef struct ktsan_ctx_s	ktsan_ctx_t;

struct ktsan_clk_s {
	ktsan_time_t time[KTSAN_MAX_THREAD_ID];
};

struct shadow {
	unsigned long tid	: KTSAN_THREAD_ID_BITS;
	unsigned long clock	: KTSAN_CLOCK_BITS;
	unsigned long offset	: 3;
	unsigned long size	: 2;
	unsigned long read	: 1;
};

struct ktsan_tab_obj_s {
	spinlock_t		lock;
	ktsan_tab_obj_t		*link;
	uptr_t			addr;
};

struct ktsan_tab_part_s {
	spinlock_t		lock;
	ktsan_tab_obj_t		*head;
};

struct ktsan_tab_s {
	unsigned		size;
	unsigned		objsize;
	ktsan_tab_part_t	*parts;
};

struct ktsan_sync_s {
	ktsan_tab_obj_t		tab;
	ktsan_clk_t		clk;
};

struct ktsan_ctx_s {
	int			enabled;
	ktsan_tab_t		synctab;
};

extern ktsan_ctx_t ktsan_ctx;

/*
 * Clocks.
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

/*
 * Synchronization.
 */
void ktsan_acquire(ktsan_thr_t *thr, uptr_t pc, uptr_t addr);
void ktsan_release(ktsan_thr_t *thr, uptr_t pc, uptr_t addr);
void ktsan_pre_lock(ktsan_thr_t *thr, uptr_t pc, uptr_t addr,
		    bool write, bool try);
void ktsan_post_lock(ktsan_thr_t *thr, uptr_t pc, uptr_t addr,
		     bool write, bool try);
void ktsan_pre_unlock(ktsan_thr_t *thr, uptr_t pc, uptr_t addr, bool write);

/*
 * Hash table. Maps an address to an arbitrary object.
 * The object must start with ktsan_tab_obj_t.
 */
void ktsan_tab_init(ktsan_tab_t *tab, unsigned size, unsigned objsize);
void ktsan_tab_destroy(ktsan_tab_t *tab);
void *ktsan_tab_access(ktsan_tab_t *tab, uptr_t key,
		       bool *created, bool destroy);

/*
 * Generic memory access.
 */
void ktsan_access(ktsan_thr_t *thr, uptr_t pc, uptr_t addr,
		  size_t size, bool read);

#define KTSAN_MAX_STACK_TRACE_FRAMES 64

struct race_info {
	unsigned long addr;
	struct shadow old;
	struct shadow new;
	unsigned long strip_addr;
};

void report_race(struct race_info *info);

#endif /* __X86_MM_KTSAN_KTSAN_H */
